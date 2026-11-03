"""Install the EU-only units (Evan's "EU Units" folder + the EU datamine rows in scripts/data/eu_units/).

    python scripts/eu_units.py check   --root ROOT                       # exit 1 unless ROOT has them all
    python scripts/eu_units.py install --root ROOT [--backup-dir DIR]    # idempotent; deploy/ needs --backup-dir

ROOT is a deploy-shaped folder (mst/, archive/, game_content/content/, game_content/mst/), e.g. a QA copy or
deploy itself.  install:

  * server tables   appends the units', skills', leader/extra skills', evolutions', CGS and item rows to
                    ROOT/mst/*.json (each file's own layout byte for byte) and the units to ROOT/archive/unit.json;
  * client tables   stages the merged tables as new download versions (VERSIONS) in ROOT/game_content/content/
                    <TABLE>/ -- the client takes these tables only over the version/download channel;
  * text            reissues the seven dictionaries the names and descriptions live in under new zip names and
                    stages F_RESOURCE_MST listing them (a changed zip under an old name is never re-fetched);
  * art             copies thumbnails, full art and item icons, and the battle sprites (below).

BATTLE SPRITES.  A unit is drawn from unit_anime_<id>.png cut up by unit_cgg_<id>.csv (the part layout) and
played by the idle/move/atk cgs files.  The folder has the sheets and cgs files but NO cgg for any unit, and no
copy exists in the 2022 asset pack, the decoded MSTs, the EU datamine or the wiki (searched 2026-11-01).
  - 12 sheets are pixel-identical to a Global unit's (IDENTICAL_SHEETS): that unit's cgg and cgs are used, so
    they animate exactly as the original.
  - 187 of the other 212 have their REAL set since 2026-11-03: the EU client's own downloaded cgg (Evan's
    app-data dump, staged in EU_SPRITES = tools/eu_sprites) beside its sheet, which is pixel-identical to the
    folder's; real_sprite() takes a set only when every part lies inside the sheet and every cgs step names an
    existing frame.  3 dump sets fail that (710145, 730215, 750157: parts outside the sheet) and 22 units have no
    complete dump set.  (5 of the 12 twins also have a dump set; the twin branch keeps the Global twin's.)
  - Those 25 keep an AUTHORED stand-in: a static sprite cut from the unit's full art (alpha box, fitted to
    SPRITE_MAX px, feet on the anchor) and a cgg of identical one-part frames, as many as the folder's cgs files
    address, so those cgs files install unchanged and a real cgg can later replace the authored one alone.
    The folder's own sheets are kept beside the install as unit_anime_<id>.eu_parts.png for that day.

Versions are above every version a client here has cached (deploy's staged ones and the round-5 S19 package's
F_UNIT_MST 1087 / F_LEADER_SKILL_MST 751 / F_RESOURCE_MST 3276; it also took 1088 from the round-8 v8
smoke, made before the Home crops were found on the art).  The round-5 #14 crop (unit_mst_corrections)
and #22-3 text (sgtext_corrections) are NOT folded in; staging either later must build on these versions.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sgtext_corrections as sgt  # noqa: E402
import unit_mst_corrections as umc  # noqa: E402

REPO = HERE.parent
DATA = HERE / 'data' / 'eu_units'
EU = Path(os.environ.get('BF_EU_UNITS', r'C:/Users/Evan/BF/EU Units'))
# The EU client's own downloaded sprite sets (Evan's app-data dump, 2026-11-03; tools/eu_sprites/SOURCE.txt): the
# part layouts the EU folder lacks, for 192 units, beside sheets pixel-identical to the folder's.
EU_SPRITES = Path(os.environ.get('BF_EU_SPRITES', REPO / 'tools' / 'eu_sprites'))
SPRITE_MAX = 150

# table -> (deploy mst file, file key, encode key, id key, staged version)
TABLES = {
    'F_UNIT_MST': ('unit_mst.json', '2r9cNSdt', '7nL1WTUb', 'pn16CNah', 1089),
    'F_SKILL_MST': ('skill_mst.json', 'wkCyV73D', 'JA03wvHG', 'nj9Lw7mV', 1537),
    'F_SKILL_LEVEL_MST': ('skill_level_mst.json', 'zLIvD5o2', 'rF97pVdT', 'nj9Lw7mV', 1488),
    'F_LEADER_SKILL_MST': ('leader_skill_mst.json', '4dE8UKcw', 'HGYZ1Ag5', 'oS3kTZ2W', 752),
    'F_EXTRA_PASSIVE_SKILL_MST': ('extra_passive_skill_mst.json', 'kP4pTJ7n', 'PDGK5sk3', 'cP83zNsv', 772),
    'F_UNIT_EVO_MST': ('unit_evo_mst.json', 'Ef6oG0mz', 'R9IryC5v', 'pn16CNah', 598),
    'F_UNIT_CGS_MST': ('unit_cgs_mst.json', '6rjmY7tV', 'Y7ezNyn2', 'pn16CNah', 812),
    'F_ITEM_MST': ('item_mst.json', '83JWTCGy', 'houmK14R', 'kixHbe54', 1029),
    'F_UNIT_COMMENT_MST': ('unit_comment_mst.json', '2npmC9Nw', 'Apc5eIw6', 'pn16CNah', 707),
}
RESOURCE_VERSION = 3277
# dictionary family -> the zip base its text goes into (the highest numbered one present is reissued as n+1;
# 498 of leaderSkillDescription is the round-5 #22-3 candidate, so this one skips past it)
DICTIONARIES = {
    'units': 'sgtext_units', 'skillName': 'sgtext_skillName', 'SkillDescription': 'sgtext_SkillDescription',
    'leaderSkillName': 'sgtext_leaderSkillName', 'leaderSkillDescription': 'sgtext_leaderSkillDescription',
    'extraPassiveSkill_Name': 'sgtext_extraPassiveSkill_Name', 'extraPassiveSkill_DESC': 'sgtext_extraPassiveSkill_DESC',
    'itemMaterialName': 'sgtext_itemMaterialName', 'itemMaterialShortDesc': 'sgtext_itemMaterialShortDesc',
    'itemMaterialLongDesc': 'sgtext_itemMaterialLongDesc',
}
RESERVED = {'sgtext_leaderSkillDescription': {498}}
# EU unit -> Global unit whose unit_anime sheet is pixel-identical (md5 of decoded RGBA, 2026-11-01)
IDENTICAL_SHEETS = {710195: 810177, 710196: 810178, 710197: 810178, 720195: 820176, 720196: 820176,
                    720197: 820176, 740167: 840608, 750195: 850177, 750196: 850178, 750197: 850178,
                    750247: 740247, 760206: 860187}


def load(name):
    return json.loads((DATA / name).read_text(encoding='utf-8'))


# ---- files ---------------------------------------------------------------------------------------------------

def put(path: Path, data: bytes, log: list, backup: Path | None, root: Path):
    """Write by temp + rename (never through a hard link); back the old file up first when asked."""
    if path.is_file() and path.read_bytes() == data:
        return
    if backup is not None and path.is_file():
        dest = backup / path.relative_to(root)
        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, dest)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix='.eu_units_')
    with os.fdopen(fd, 'wb') as f:
        f.write(data)
    os.replace(tmp, path)
    log.append(str(path.relative_to(root)))


def layout(raw: bytes):
    """How a deploy JSON file is written, recovered from its own bytes: (dump function)."""
    text = raw.decode('utf-8-sig')
    bom = raw.startswith(b'\xef\xbb\xbf')
    nl = '\r\n' if '\r\n' in text[:4000] else '\n'
    doc = json.loads(text)

    def std(d):
        s = json.dumps(d, indent=1, separators=(',', ': '), ensure_ascii=False).replace('\n', nl) + nl
        return (b'\xef\xbb\xbf' if bom else b'') + s.encode('utf-8')

    def per_line(d):
        (key, rows), = d.items()
        body = (',' + nl).join('    ' + json.dumps(r, ensure_ascii=False) for r in rows)
        s = '{' + nl + f'  "{key}": [' + nl + body + nl + '  ]' + nl + '}' + nl
        return (b'\xef\xbb\xbf' if bom else b'') + s.encode('utf-8')

    def indented(indent, ascii_):
        def dump(d):
            s = json.dumps(d, indent=indent, ensure_ascii=ascii_).replace('\n', nl) + nl
            return (b'\xef\xbb\xbf' if bom else b'') + s.encode('utf-8')
        return dump

    for fn in (std, per_line) + tuple(indented(i, a) for i in (1, 2, 4) for a in (False, True)):
        try:
            if fn(doc) == raw:
                return doc, fn
        except Exception:  # noqa: BLE001
            pass
    raise SystemExit('unrecognised JSON layout; refusing to rewrite it')


def merged_rows(rows: list, new: list, idk: str) -> tuple[list, int]:
    """The table with this data's rows: an existing row with one of its ids is REPLACED (the data owns those
    ids, so a rebuilt row reaches the table), the rest appended.  Returns (rows, rows changed or added)."""
    by_id = {r[idk]: r for r in new}
    changed = sum(1 for r in rows if r[idk] in by_id and r != by_id[r[idk]])
    out = [by_id.get(r[idk], r) for r in rows]
    have = {r[idk] for r in rows}
    add = [r for r in new if r[idk] not in have]
    return out + add, changed + len(add)


# ---- sprites ---------------------------------------------------------------------------------------------------

def cgs_frames(path: Path) -> int:
    best = -1
    if path.is_file():
        for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
            head = line.split(',')[0].strip()
            if head.lstrip('-').isdigit():
                best = max(best, int(head))
    return best + 1


def real_sprite(uid: int) -> dict | None:
    """The EU client's own set for `uid` from EU_SPRITES ({'sheet', 'cgg', 'idle', 'atk', 'move'?} bytes), or None.
    A set counts only when every part lies inside the sheet and every cgs step names a frame the part file has
    (a frame is a cgg LINE; the first column is not a frame number -- Global's are all 0)."""
    names = {'sheet': f'unit_anime_{uid}.png', 'cgg': f'unit_cgg_{uid}.csv', 'idle': f'unit_idle_cgs_{uid}.csv',
             'atk': f'unit_atk_cgs_{uid}.csv', 'move': f'unit_move_cgs_{uid}.csv'}
    if not all((EU_SPRITES / names[k]).is_file() for k in ('sheet', 'cgg', 'idle', 'atk')):
        return None
    out = {k: (EU_SPRITES / n).read_bytes() for k, n in names.items() if (EU_SPRITES / n).is_file()}
    W, H = Image.open(io.BytesIO(out['sheet'])).size
    lines = [line for line in out['cgg'].decode('ascii', 'replace').splitlines() if line.strip()]
    for line in lines:
        v = line.strip().rstrip(',').split(',')
        for p in range(int(v[1])):
            part = v[2 + p * 11: 13 + p * 11]
            if len(part) < 11:
                return None
            sx, sy, w, h = map(int, part[6:10])
            if sx + w > W or sy + h > H:
                return None
    for kind in ('idle', 'atk', 'move'):
        for line in out.get(kind, b'').decode('ascii', 'replace').splitlines():
            if line.strip() and int(line.split(',')[0]) >= len(lines):
                return None
    return out


def authored_sprite(uid: int) -> tuple[bytes, int, int]:
    im = Image.open(EU / 'Full Art' / f'unit_ills_full_{uid}.png').convert('RGBA')
    box = im.getchannel('A').getbbox() or (0, 0) + im.size
    im = im.crop(box)
    k = min(SPRITE_MAX / im.width, SPRITE_MAX / im.height, 1.0)
    im = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, format='PNG', optimize=False)
    return buf.getvalue(), im.width, im.height


def authored_battle_portrait(uid: int) -> bytes:
    """unit_ills_battle_<id>.png (108x108, the battle panel's portrait).  The folder has none, and Home asks
    for it for every owned unit (a missing one is a login-time download error, client run v8).  Global's is
    the thumbnail's picture without the element frame, so: the thumbnail inside its frame, scaled up."""
    im = Image.open(EU / 'Thumbnails' / f'unit_ills_thum_{uid}.png').convert('RGBA')
    w, h = im.size
    inner = im.crop((round(w * 0.07), round(h * 0.07), round(w * 0.93), round(h * 0.93)))
    buf = io.BytesIO()
    inner.resize((108, 108), Image.LANCZOS).save(buf, format='PNG', optimize=False)
    return buf.getvalue()


def authored_cgg(frames: int, w: int, h: int) -> bytes:
    """`frame,parts,` then per part x,y,0,0,100,0,srcX,srcY,w,h,0 (a Global cgg's layout), feet on the anchor."""
    part = f'{-(w // 2)},{-h},0,0,100,0,0,0,{w},{h},0,'
    return ''.join(f'{i},1,{part}\r\n' for i in range(max(1, frames))).encode('ascii')


def authored_cgs(total: int) -> bytes:
    return f'0,0,0,{max(1, total)},\r\n'.encode('ascii')


def reissue_dictionaries(root: Path, text: dict, version: int, log: list, backup: Path | None, check_only: bool,
                         reserved: dict = RESERVED, force: bool = False) -> list[str]:
    """Append `text` ({family: {MST key: English}}) to the dictionaries the root's F_RESOURCE_MST lists: each family
    with a missing key is reissued once under the next free number (skipping `reserved`), and F_RESOURCE_MST is
    staged as `version` naming the new zips.  A key already present is left alone.  Returns the problems (check)."""
    problems = []
    content = root / 'game_content' / 'content'
    sg = content / 'sgtext'
    reissued = False
    resource_rows = sgt.read_resource(root / 'game_content' / 'mst')
    staged_res = content / 'F_RESOURCE_MST' / 'manifest.json'
    if staged_res.is_file():
        rv = json.loads(staged_res.read_text(encoding='utf-8'))['version']
        resource_rows = [r for name in sorted((content / 'F_RESOURCE_MST').glob(f'Ver{rv}_*.dat'))
                         for r in umc.decode_part(name.read_bytes(), umc.Spec('F_RESOURCE_MST', '', '', sgt.RESOURCE_FILE_KEY,
                                                                                    sgt.RESOURCE_ENC_KEY, '', ()))]
    paths = [r[sgt.RESOURCE_PATH] for r in resource_rows]
    for family, entries in text.items():
        base = DICTIONARIES[family]
        listed = [p for p in paths if re.fullmatch(rf'/?sgtext/{base}_\d+\.zip', p)]   # rows read '/sgtext/...'
        if not listed:
            problems.append(f'{base}: not listed in F_RESOURCE_MST')
            continue
        cur = listed[0]
        lead = '/' if cur.startswith('/') else ''
        data = (content / cur.lstrip('/')).read_bytes()
        csv_name, csv, info_zip = sgt.decode_zip(data)
        existing = sgt.rows_of_csv(csv)
        missing = {k: v for k, v in entries.items() if k.encode('utf-8') not in existing}
        if not missing:
            continue
        if check_only:
            problems.append(f'{cur}: {len(missing)} key(s) missing')
            continue
        n = int(re.search(r'_(\d+)\.zip$', cur).group(1)) + 1
        while n in reserved.get(base, set()) or (sg / f'{base}_{n}.zip').exists():
            n += 1
        nl = b'\r\n' if b'\r\n' in csv[:2000] else b'\n'
        cols = csv.split(nl, 1)[0].count(b'^')
        lines = [k.encode('utf-8') + b'^' + b'^'.join([v.replace('\n', ' ').encode('utf-8')] * cols)
                 for k, v in missing.items()]
        new_csv = csv.rstrip(b'\r\n') + nl + nl.join(lines) + nl
        name = f'{base}_{n}'
        put(sg / f'{name}.zip', sgt.encode_zip(f'{name}.csv', new_csv, info_zip), log, backup, root)
        reissued = True
        for r in resource_rows:
            if r[sgt.RESOURCE_PATH] == cur:
                r[sgt.RESOURCE_PATH] = f'{lead}sgtext/{name}.zip'
    if not check_only:
        res_dir = content / 'F_RESOURCE_MST'
        rman = res_dir / 'manifest.json'
        current = json.loads(rman.read_text(encoding='utf-8'))['version'] if rman.is_file() else 0
        if current < version or force or reissued:
            parts = sgt.encode_resource(resource_rows, version)
            for nm, blob in parts.items():
                put(res_dir / nm, blob, log, backup, root)
            put(rman, umc.manifest_bytes(version, len(parts), len(resource_rows),
                                         umc.Spec('F_RESOURCE_MST', '', '', sgt.RESOURCE_FILE_KEY, sgt.RESOURCE_ENC_KEY,
                                                  '', ())), log, backup, root)
    return problems


# ---- install ----------------------------------------------------------------------------------------------------

def install_tables(root: Path, data: Path, tables: dict, log: list, backup: Path | None, check_only: bool) -> list[str]:
    """Merge data/<file> into each ROOT/mst table of `tables` ({table: (file, file key, encode key, id key, version)})
    and stage the merged table as that download version.  Returns the problems (check)."""
    problems = []
    content = root / 'game_content' / 'content'
    for table, (fname, fkey, ekey, idk, version) in tables.items():
        new = json.loads((data / fname).read_text(encoding='utf-8')) if (data / fname).is_file() else []
        src = root / 'mst' / fname
        raw = src.read_bytes()
        doc, dump = layout(raw)
        (wrapper, rows), = doc.items()
        merged, added = merged_rows(rows, new, idk)
        spec = umc.Spec(table, fname, wrapper, fkey, ekey, idk, ())
        folder = content / table
        manifest = folder / 'manifest.json'
        staged = json.loads(manifest.read_text(encoding='utf-8')).get('version', 0) if manifest.is_file() else 0
        # Refuse BEFORE writing anything: a client that cached a version never fetches it again, so new content
        # under an already staged number would split server and client (2026-11-01: a refusal that came after
        # the server-table write left deploy's unit_mst.json ahead of the staged F_UNIT_MST).
        current = None
        if staged >= version and not check_only:
            current = [r for name in sorted(folder.glob(f'Ver{staged}_*.dat')) for r in umc.decode_part(name.read_bytes(), spec)]
            if current != merged:
                raise SystemExit(f'{table}: version {staged} is staged with different rows; raise its version in TABLES')
        if check_only:
            if added:
                problems.append(f'{fname}: {added} row(s) missing')
        else:
            put(src, dump({wrapper: merged}), log, backup, root)
        if current is not None:
            continue
        if check_only:
            if staged < version:
                problems.append(f'{table}: staged version {staged} < {version}')
            continue
        for name, blob in umc.encode_parts(merged, version, spec).items():
            put(folder / name, blob, log, backup, root)
        put(manifest, umc.manifest_bytes(version, len(umc.encode_parts(merged, version, spec)), len(merged), spec),
            log, backup, root)
    return problems


def install_archive(root: Path, records: list, log: list, backup: Path | None, check_only: bool, what: str) -> list[str]:
    """Merge `records` into ROOT/archive/unit.json (a record with one of their ids is replaced, the rest appended)."""
    archive = root / 'archive' / 'unit.json'
    araw = archive.read_bytes()
    adoc, adump = layout(araw)
    mine = {u['id']: u for u in records}
    merged = [mine.get(u['id'], u) for u in adoc] + [u for u in mine.values() if u['id'] not in {x['id'] for x in adoc}]
    if check_only:
        return [f'archive/unit.json: {what} records missing or different'] if merged != adoc else []
    if merged != adoc:
        put(archive, adump(merged), log, backup, root)
    return []


def install(root: Path, backup: Path | None, check_only=False) -> list[str]:
    log, problems = [], []
    content = root / 'game_content' / 'content'
    units = load('unit_mst.json')
    info = {u['pn16CNah']: u for u in units}

    # server + client tables, the archive
    problems += install_tables(root, DATA, TABLES, log, backup, check_only)
    problems += install_archive(root, load('archive_unit.json'), log, backup, check_only, 'EU')

    # text: reissue each dictionary family once, with the new keys appended
    problems += reissue_dictionaries(root, load('text.json'), RESOURCE_VERSION, log, backup, check_only,
                                     force=bool(log))

    # art
    img, cgg_dir, cgs_dir = content / 'unit' / 'img', content / 'unit' / 'cgg', content / 'unit' / 'cgs'
    for uid_s in info:
        uid = int(uid_s)
        for sub, pre in (('Thumbnails', 'unit_ills_thum'), ('Full Art', 'unit_ills_full')):
            src = EU / sub / f'{pre}_{uid}.png'
            dst = img / f'{pre}_{uid}.png'
            if check_only:
                if not dst.is_file():
                    problems.append(f'{dst.name} missing')
            elif src.is_file():
                put(dst, src.read_bytes(), log, backup, root)
        portrait = img / f'unit_ills_battle_{uid}.png'
        if check_only:
            if not portrait.is_file():
                problems.append(f'{portrait.name} missing')
        else:
            put(portrait, authored_battle_portrait(uid), log, backup, root)
        twin = IDENTICAL_SHEETS.get(uid)
        if twin:
            pairs = [(img / f'unit_anime_{uid}.png', EU / 'Spritesheets' / f'unit_anime_{uid}.png'),
                     (cgg_dir / f'unit_cgg_{uid}.csv', cgg_dir / f'unit_cgg_{twin}.csv')]
            for kind in ('idle', 'move', 'atk'):
                src = cgs_dir / f'unit_{kind}_cgs_{twin}.csv'
                if not src.is_file() and kind == 'move':
                    src = cgs_dir / f'unit_idle_cgs_{twin}.csv'
                pairs.append((cgs_dir / f'unit_{kind}_cgs_{uid}.csv', src))
            for dst, src in pairs:
                if check_only:
                    if not dst.is_file():
                        problems.append(f'{dst.name} missing')
                elif src.is_file():
                    put(dst, src.read_bytes(), log, backup, root)
            continue
        cgs_src = {k: EU / 'Spritesheets' / f'unit_{k}_cgs_{uid}.csv' for k in ('idle', 'move', 'atk')}
        frames = max(cgs_frames(p) for p in cgs_src.values())
        if check_only:
            for name in (f'unit_anime_{uid}.png', f'unit_cgg_{uid}.csv'):
                if not ((img if name.endswith('png') else cgg_dir) / name).is_file():
                    problems.append(f'{name} missing')
            continue
        real = real_sprite(uid)
        if real:
            # the unit's own sheet and part layout: it animates as in the EU client
            put(img / f'unit_anime_{uid}.png', real['sheet'], log, backup, root)
            put(cgg_dir / f'unit_cgg_{uid}.csv', real['cgg'], log, backup, root)
            for kind in ('idle', 'move', 'atk'):
                data = real.get(kind) or (cgs_src[kind].read_bytes() if cgs_src[kind].is_file() else authored_cgs(60))
                put(cgs_dir / f'unit_{kind}_cgs_{uid}.csv', data, log, backup, root)
            continue
        png, w, h = authored_sprite(uid)
        put(img / f'unit_anime_{uid}.png', png, log, backup, root)
        put(cgg_dir / f'unit_cgg_{uid}.csv', authored_cgg(frames, w, h), log, backup, root)
        sheet = EU / 'Spritesheets' / f'unit_anime_{uid}.png'
        if sheet.is_file():
            put(root / 'game_content' / 'eu_parts' / f'unit_anime_{uid}.eu_parts.png', sheet.read_bytes(), log, backup, root)
        total = 60
        for kind, src in cgs_src.items():
            dst = cgs_dir / f'unit_{kind}_cgs_{uid}.csv'
            put(dst, src.read_bytes() if src.is_file() else authored_cgs(total), log, backup, root)
    for item in load('item_mst.json') if (DATA / 'item_mst.json').is_file() else []:
        name = item['Mt3Y0bo5']
        src = EU / 'Items' / name
        if src.is_file() and not check_only:
            put(content / 'item' / name, src.read_bytes(), log, backup, root)
    return problems if check_only else log


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('command', choices=['check', 'install'])
    ap.add_argument('--root', type=Path, required=True)
    ap.add_argument('--backup-dir', type=Path)
    a = ap.parse_args(argv)
    root = a.root.resolve()
    if a.command == 'check':
        problems = install(root, None, check_only=True)
        print('complete' if not problems else f'{len(problems)} problem(s):\n  ' + '\n  '.join(problems[:30]))
        return 1 if problems else 0
    if root == (REPO / 'deploy').resolve() and a.backup_dir is None:
        sys.exit('installing into deploy/ needs --backup-dir (every replaced file is copied there first)')
    log = install(root, a.backup_dir.resolve() if a.backup_dir else None)
    print(f'{len(log)} file(s) written' + (f'; backups in {a.backup_dir}' if a.backup_dir else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
