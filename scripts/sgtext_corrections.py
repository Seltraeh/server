"""Maintained corrections to the client's own text dictionaries (sgtext_*.zip) and the F_RESOURCE_MST that lists them.

    python scripts/sgtext_corrections.py stage --content-root ROOT --version 3278 [--mirror-from SOURCE_ROOT]

The unit screen draws names and leader-skill text from the client's dictionaries, not from the MSTs' English
(round 5: an F_LEADER_SKILL_MST text change downloaded but never showed).  A dictionary is a zip holding one
CSV, base64-encoded in 64-character lines; the CSV's first line names the dictionary and every other line is
`<MST key>^<English>^<other languages...>`.  The client lists the zips to download in F_RESOURCE_MST (one
path per row, key ZwstRU92), fetches a listed zip it does not have (existsLocalFile has no hash check, so a
changed zip under the SAME name is never fetched again) and extracts it into its sgtext dictionary.  So a
correction ships as a dictionary under a NEW file name (`<name>_<n+1>.zip`, its CSV renamed to match) plus a
new F_RESOURCE_MST version whose row names it instead of the old zip.

stage    reads F_RESOURCE_MST from SOURCE_ROOT (default deploy/game_content): the STAGED table in
         content/F_RESOURCE_MST when there is one (round 8: 3277, the EU units' ten reissued dictionaries), else
         the shipped mst/Ver3275.  For each corrected dictionary it takes the one zip that table lists (round 8:
         sgtext_leaderSkillDescription_499, the EU reissue of the shipped 497), applies CORRECTIONS (each English
         value must be its known OLD value or already its NEW one; anything else is refused), checks the decoded
         CSV differs in exactly those fields, and writes into ROOT -- never deploy/:
           content/sgtext/<name>_<n+1>.zip
           content/F_RESOURCE_MST/Ver<version>_hdt2N6mr_<i>.dat + manifest.json   (same row count and order,
                                                                                  one path changed)
         Files are written only by exclusive temporary file + rename (unit_mst_corrections.replace_into), so a
         hard-link mirror's source is never written through; a different file already under a target name is a
         refused collision.  Deploy files are hashed before and after.

CORRECTIONS (each with its evidence):

  MST_LEADERSKILLS_801271_DESCRIPTION (Galtier 860428, #22-3): "<br>" -> ". " in the English field only.  The
        unit screen's two-line leader-skill ticker draws a "<br>" text as a first piece of raw characters
        0-252 (line 1, then line 2 up to "...20% damage reductio") and only later the remainder beside line 1,
        so line 2 reads as cut off (reproduced, round 5).  Durumn 840278 joins its Guild Raid part with ". " and
        scrolls to its end on one line.  Every word is kept.
"""
from __future__ import annotations

import argparse
import base64
import io
import json
import re
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path

from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad

sys.path.insert(0, str(Path(__file__).resolve().parent))
from unit_mst_corrections import (DEPLOY, REPO, Refused, inside_deploy, mirror, replace_into, sha,  # noqa: E402
                                  tree_hashes)

RESOURCE_TABLE, RESOURCE_FILE_KEY, RESOURCE_ENC_KEY = 'F_RESOURCE_MST', 'hdt2N6mr', 'z5dp9GX8'
RESOURCE_PATH = 'ZwstRU92'          # the row's only column: the content path of a dictionary zip
RESOURCE_SOURCE_VERSION, RESOURCE_SOURCE_PARTS = 3275, 2
WRAP = 64                           # base64 line width the shipped dictionaries use


@dataclass(frozen=True)
class Correction:
    dictionary: str     # zip stem without the version, e.g. sgtext_leaderSkillDescription
    version: int        # the shipped version this correction was first made against (the source is now
                        # whatever version F_RESOURCE_MST lists, see stage)
    key: str
    old: str            # English field
    new: str


_HEAD = ('50% boost to all parameters, boosts Atk and Def when BB gauge is over 50%, probable damage reduction to 1, '
         'probable resistance against 1 KO attack & considerably boosts ABP and CBP gain')
_RAID = '25% boost to all parameters of Dark types & 20% damage reduction from Dark, Light types in Guild Raid'
CORRECTIONS = (Correction('sgtext_leaderSkillDescription', 497, 'MST_LEADERSKILLS_801271_DESCRIPTION',
                          f'{_HEAD}<br>{_RAID}', f'{_HEAD}. {_RAID}'),)
# sha256 prefixes of the dictionaries a correction may start from: the shipped one, and the round-8 EU reissue
# (scripts/eu_units.py, which skipped 498 for this correction).
SOURCE_ZIP_SHA = {'sgtext_leaderSkillDescription_497.zip': '5e9d92095b1417f3',
                  'sgtext_leaderSkillDescription_499.zip': 'fa50d174afc13235'}


def decode_zip(data: bytes) -> tuple[str, bytes, zipfile.ZipInfo]:
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        infos = [i for i in z.infolist() if i.filename.lower().endswith('.csv')]
        if len(infos) != 1:
            raise Refused(f'a dictionary zip holds exactly one CSV, found {len(infos)}')
        raw = z.read(infos[0])
    return infos[0].filename, base64.b64decode(raw), infos[0]


def encode_zip(csv_name: str, csv: bytes, like: zipfile.ZipInfo) -> bytes:
    s = base64.b64encode(csv)
    body = b''.join(s[i:i + WRAP] + b'\n' for i in range(0, len(s), WRAP))
    info = zipfile.ZipInfo(csv_name, date_time=like.date_time)
    info.compress_type, info.external_attr, info.create_system = zipfile.ZIP_DEFLATED, like.external_attr, like.create_system
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w') as z:
        z.writestr(info, body)
    return out.getvalue()


def rows_of_csv(csv: bytes) -> dict[bytes, bytes]:
    rows = {}
    for line in csv.split(b'\n')[1:]:
        if line:
            k, _, rest = line.partition(b'^')
            if k in rows:
                raise Refused(f'duplicate key {k!r}')
            rows[k] = rest
    return rows


def correct_csv(csv: bytes, fixes: list[Correction]) -> tuple[bytes, list[str]]:
    """The corrected CSV: only the English fields of the corrected keys change."""
    lines = csv.split(b'\n')
    notes, changed = [], set()
    for c in fixes:
        hits = [i for i, line in enumerate(lines) if line.startswith(c.key.encode() + b'^')]
        if len(hits) != 1:
            raise Refused(f'{c.key}: {len(hits)} rows, expected exactly one')
        fields = lines[hits[0]].split(b'^')
        english = fields[1].decode('utf-8')
        if english == c.new:
            notes.append(f'{c.key} already corrected')
            continue
        if english != c.old:
            raise Refused(f'{c.key} English is {english!r}: neither the known old nor the new value, refusing')
        fields[1] = c.new.encode('utf-8')
        lines[hits[0]] = b'^'.join(fields)
        changed.add(c.key.encode())
        notes.append(f'{c.key}: {c.old!r} -> {c.new!r}')
    out = b'\n'.join(lines)
    a, b = rows_of_csv(csv), rows_of_csv(out)
    diff = {k for k in a.keys() | b.keys() if a.get(k) != b.get(k)}
    if diff != changed or lines[0] != csv.split(b'\n')[0]:
        raise Refused(f'decoded difference {sorted(diff)} is not exactly the corrected keys {sorted(changed)}')
    for k in changed:
        if a[k].split(b'^')[1:] != b[k].split(b'^')[1:]:
            raise Refused(f'{k!r}: a non-English field changed')
    return out, notes


def resource_key() -> bytes:
    return RESOURCE_ENC_KEY.encode()[:16].ljust(16, b'\x00')


def read_resource(mst_dir: Path) -> list[dict]:
    rows = []
    for i in range(1, RESOURCE_SOURCE_PARTS + 1):
        raw = (mst_dir / f'Ver{RESOURCE_SOURCE_VERSION}_{RESOURCE_FILE_KEY}_{i}.dat').read_bytes()
        rows += json.loads(unpad(AES.new(resource_key(), AES.MODE_ECB).decrypt(base64.b64decode(raw)), 16))
    return rows


def resource_base(source: Path) -> tuple[int, int, list[dict]]:
    """(version, parts, rows) of the F_RESOURCE_MST a client gets from SOURCE: the staged one when there is one."""
    folder = source / 'content' / RESOURCE_TABLE
    if (folder / 'manifest.json').is_file():
        man = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
        rows = []
        for i in range(1, man['file_count'] + 1):
            raw = (folder / f'Ver{man["version"]}_{RESOURCE_FILE_KEY}_{i}.dat').read_bytes()
            rows += json.loads(unpad(AES.new(resource_key(), AES.MODE_ECB).decrypt(base64.b64decode(raw)), 16))
        if len(rows) != man['record_num']:
            raise Refused(f'staged F_RESOURCE_MST {man["version"]}: {len(rows)} rows, manifest says {man["record_num"]}')
        return man['version'], man['file_count'], rows
    return RESOURCE_SOURCE_VERSION, RESOURCE_SOURCE_PARTS, read_resource(source / 'mst')


def encode_resource(rows: list[dict], version: int, parts: int = RESOURCE_SOURCE_PARTS) -> dict[str, bytes]:
    per = -(-len(rows) // parts)      # the shipped split: equal row counts
    out = {}
    for i in range(parts):
        chunk = json.dumps(rows[i * per:(i + 1) * per], ensure_ascii=False, separators=(',', ':')).encode('utf-8')
        out[f'Ver{version}_{RESOURCE_FILE_KEY}_{i + 1}.dat'] = base64.b64encode(
            AES.new(resource_key(), AES.MODE_ECB).encrypt(pad(chunk, 16)))
    return out


def put(path: Path, data: bytes, lines: list[str]) -> None:
    """Write a new file by exclusive temp + rename; identical bytes are kept; different bytes are refused."""
    if path.exists():
        if path.read_bytes() != data:
            raise Refused(f'collision: {path} exists with different bytes')
        lines.append(f'{path.name}: identical (kept)')
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    replace_into(path, data)
    lines.append(f'{path.name}: written, sha256 {sha(data)}')


def stage(content_root: Path, version: int, mirror_from: Path | None = None) -> str:
    root = Path(content_root).resolve()
    if inside_deploy(root):
        raise Refused(f'{root} is inside deploy/: staging goes to a disposable content root')
    source = Path(mirror_from).resolve() if mirror_from else DEPLOY / 'game_content'
    base_version, base_parts, rows = resource_base(source)
    if version <= base_version:
        # A client never fetches a version it already holds, so a correction needs a number above the table
        # it would otherwise keep.
        raise Refused(f'version {version} must be above the F_RESOURCE_MST it replaces ({base_version})')
    watched = tree_hashes(DEPLOY / 'game_content' / 'mst') | tree_hashes(DEPLOY / 'game_content' / 'content' / 'sgtext')
    lines = []
    if mirror_from:
        lines.append(f'{mirror(source, root)} hard links {source} -> {root}')
    paths = [r.get(RESOURCE_PATH) for r in rows]
    for dictionary, fixes in {c.dictionary: [x for x in CORRECTIONS if x.dictionary == c.dictionary]
                              for c in CORRECTIONS}.items():
        listed = [x for x in paths if re.fullmatch(rf'/sgtext/{re.escape(dictionary)}_(\d+)\.zip', x or '')]
        if len(listed) != 1:
            raise Refused(f'F_RESOURCE_MST must list exactly one {dictionary} zip, found {listed}')
        n = int(re.fullmatch(r'.*_(\d+)\.zip', listed[0]).group(1))
        old_name, new_name = f'{dictionary}_{n}.zip', f'{dictionary}_{n + 1}.zip'
        old_path, new_path = f'/sgtext/{old_name}', f'/sgtext/{new_name}'
        if new_path in paths:
            raise Refused(f'F_RESOURCE_MST already lists {new_path}')
        data = (source / 'content' / 'sgtext' / old_name).read_bytes()
        known = SOURCE_ZIP_SHA.get(old_name)
        if not known or not sha(data).startswith(known):
            raise Refused(f'{old_name} is not a dictionary this correction was checked against (SOURCE_ZIP_SHA)')
        csv_name, csv, info = decode_zip(data)
        fixed, notes = correct_csv(csv, fixes)
        lines += notes
        new_zip = encode_zip(new_name[:-4] + '.csv', fixed, info)
        back_name, back, _ = decode_zip(new_zip)
        if back != fixed or back_name != new_name[:-4] + '.csv':
            raise Refused('dictionary round trip failed')
        put(root / 'content' / 'sgtext' / new_name, new_zip, lines)
        rows[paths.index(old_path)] = {RESOURCE_PATH: new_path}
    parts = encode_resource(rows, version, base_parts)
    folder = root / 'content' / RESOURCE_TABLE
    for name, blob in parts.items():
        put(folder / name, blob, lines)
    manifest = {'table': RESOURCE_TABLE, 'file_key': RESOURCE_FILE_KEY, 'version': version,
                'file_count': len(parts), 'record_num': len(rows)}
    mbytes = json.dumps(manifest, indent=1).replace('\n', '\r\n').encode('utf-8')
    if (folder / 'manifest.json').exists() and (folder / 'manifest.json').read_bytes() != mbytes:
        old = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
        if old.get('version', 0) >= version:
            raise Refused(f'staged F_RESOURCE_MST manifest is already version {old.get("version")}')
        replace_into(folder / 'manifest.json', mbytes)
        lines.append(f'manifest.json: version {version} (replaced by rename)')
    else:
        put(folder / 'manifest.json', mbytes, lines)
    back = []
    for name in parts:
        back += json.loads(unpad(AES.new(resource_key(), AES.MODE_ECB).decrypt(base64.b64decode(
            (folder / name).read_bytes())), 16))
    if back != rows:
        raise Refused('F_RESOURCE_MST round trip failed')
    if tree_hashes(DEPLOY / 'game_content' / 'mst') | tree_hashes(DEPLOY / 'game_content' / 'content' / 'sgtext') != watched:
        raise Refused('deploy content changed during the run')
    lines.append(f'F_RESOURCE_MST v{version}: {len(rows)} rows, {len(parts)} parts; deploy content unchanged')
    return '\n'.join(lines)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('stage')
    s.add_argument('--content-root', type=Path, required=True)
    s.add_argument('--version', type=int, required=True)
    s.add_argument('--mirror-from', type=Path)
    a = ap.parse_args(argv)
    try:
        print(stage(a.content_root, a.version, a.mirror_from))
        return 0
    except Refused as ex:
        print(f'refused: {ex}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
