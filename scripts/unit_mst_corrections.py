"""Maintained MST corrections and the downloadable F_* files that carry them to the client.

    python scripts/unit_mst_corrections.py [--table F_UNIT_MST] propose --out DIR/unit_mst.json
    python scripts/unit_mst_corrections.py [--table F_UNIT_MST] stage --rows DIR/unit_mst.json --content-root ROOT
                                                 --version 1087 [--mirror-from SOURCE_CONTENT_ROOT]

Tables are Specs in SPECS (today only F_UNIT_MST, from deploy/mst/unit_mst.json); everything below applies to
each the same way.

The client takes UnitMst ONLY through the version/download channel (Initialize carries no UnitMst, and no
server code reads home_img_pos), so a correction reaches players as (1) a corrected deploy/mst/unit_mst.json,
which is the generator's input, and (2) a new F_UNIT_MST version staged beside it.  This script produces
both in places it is given -- never deploy/ -- so a correction can be kept, reviewed and regenerated
without deploying it.

propose  applies CORRECTIONS to a unit_mst.json byte-for-byte: only the corrected value's characters change
         (CRLF line endings and every other byte are kept), and the parsed result must differ from the source
         in exactly the corrected (unit, key) pairs.  Each correction's current value must be its known OLD
         value (applied) or its NEW value (already applied: idempotent); any other value -- a source this
         script does not know -- is refused.  An existing --out is never overwritten: identical bytes are
         reported unchanged, different bytes refused.
stage    writes F_UNIT_MST version N (base64(AES-ECB(rows JSON, PKCS#7)) under the table's encode key, rows
         split into complete JSON arrays exactly as tools/gen_mst_download.py does) plus its manifest into
         ROOT/content/F_UNIT_MST.  ROOT is typically a hard-link mirror (--mirror-from makes one), so nothing is
         ever written THROUGH an existing file: new files and the manifest are written to a temporary name and
         renamed into place, which replaces the directory entry and leaves a linked source inode untouched.  An
         existing part with the same bytes is kept; one with different bytes is a version collision and is
         refused, as is a version below the staged manifest's.  The version is always explicit.  Source and
         deploy staging hashes are checked before and after.

CORRECTIONS (each with its evidence):

  50254 Goddess Tilith, home_img_pos 1W9CxaFK: 245,-5,135,369 -> 298,48,180,492.  The shipped rectangle put her
        face off the right edge of the Home squad panel (reproduced round 3, P1_06_home.png).  The new one
        keeps the 50253 fix's 180x492 (both arts are 906x606 with the figure at the same height) centred on
        her face; accepted in the S19 client on Home and the unit screens (round 3, P2_04_home_before_after.png,
        P2_info_sheet.png).  Seria 10233 and Tilith 50253 were checked unchanged.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad

REPO = Path(__file__).resolve().parents[1]
DEPLOY = (REPO / 'deploy').resolve()
CHUNK_BYTES = 3 * 1024 * 1024   # as tools/gen_mst_download.py


@dataclass(frozen=True)
class Correction:
    unit: str       # the row's id (a unit id for F_UNIT_MST, a leader-skill id for F_LEADER_SKILL_MST)
    key: str
    old: str
    new: str


@dataclass(frozen=True)
class Spec:
    """One downloadable table: its deploy/mst source, wire names and the corrections it carries."""
    table: str          # F_* name, also the staging folder
    source: str         # deploy/mst file (the generator's input; staged versions equal its rows)
    wrapper: str        # the source file's row-list key
    file_key: str       # Ver<version>_<file_key>.dat -- tools/mst_download_map.json / libgame.so strings
    enc_key: str        # the table's encode key (same source); UTF-8, zero-padded to 16 bytes
    id: str             # the row id key
    corrections: tuple
    pristine_sha: str = ''          # the source the corrections were accepted against
    corrected_sha: str = ''         # its corrected bytes
    accepted_parts: dict = None     # version -> part hashes the client was accepted with

    @property
    def deploy_stage(self) -> Path:
        return DEPLOY / 'game_content' / 'content' / self.table


# 50254 Goddess Tilith (round 3/4; see the module docstring).
UNIT = Spec('F_UNIT_MST', 'unit_mst.json', '2r9cNSdt', '2r9cNSdt', '7nL1WTUb', 'pn16CNah',
            (Correction('50254', '1W9CxaFK', '245,-5,135,369', '298,48,180,492'),),
            pristine_sha='193d3b9fd13dd24211221c079ce353c8075a28dc755d4dda4d9f7754b38ba1bc',
            corrected_sha='0c38f8311dd4f66fd9830954b4b44ce922a1ac5a1b0ee0097781049e6a7dd0e1',
            accepted_parts={1087: ['2a5d342e8932e6ccbfb22ebe0b2f5518c3462acf01e982a333a09f51882ed758']})

# F_LEADER_SKILL_MST was tried for #22-3 (round 5) and withdrawn: the unit screen draws leader-skill text
# from the client's own dictionary (sgtext_leaderSkillDescription_497.zip, keyed by the skill id), so a
# changed F_LEADER_SKILL_MST description is never shown (v751 test, docs/CLAUDE_FRESH_ROUND5_HANDOFF_2026-10-30.md).
# A table is added here only with a correction the client is shown to read.
SPECS = {s.table: s for s in (UNIT,)}

# The unit table's names, kept for callers written against the first version of this tool.
TABLE, WRAPPER, FILE_KEY, ENC_KEY, ID = UNIT.table, UNIT.wrapper, UNIT.file_key, UNIT.enc_key, UNIT.id
CORRECTIONS, DEPLOY_STAGE = UNIT.corrections, UNIT.deploy_stage
PRISTINE_SHA, CORRECTED_SHA, ACCEPTED_PARTS = UNIT.pristine_sha, UNIT.corrected_sha, UNIT.accepted_parts


class Refused(Exception):
    """A refused correction or staging; the message says what was checked."""


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def rows_of(data: bytes, spec: Spec = UNIT) -> list[dict]:
    doc = json.loads(data.decode('utf-8'))
    rows = doc.get(spec.wrapper) if isinstance(doc, dict) else doc
    if not isinstance(rows, list):
        raise Refused(f'not a {spec.source} document (no "{spec.wrapper}" row list)')
    return rows


def inside_deploy(path: Path) -> bool:
    p = Path(path).resolve()
    return p == DEPLOY or DEPLOY in p.parents


# ---------------------------------------------------------------- propose

def apply_corrections(source: bytes, spec: Spec = UNIT) -> tuple[bytes, list[str]]:
    """(corrected bytes, one line per correction).  Refuses anything but the known old/new values."""
    ID = spec.id
    text = source.decode('utf-8')
    rows = rows_of(source, spec)
    notes, applied = [], []
    for c in spec.corrections:
        hits = [r for r in rows if r.get(ID) == c.unit]
        if len(hits) != 1:
            raise Refused(f'unit {c.unit}: {len(hits)} rows, expected exactly one')
        current = hits[0].get(c.key)
        if current == c.new:
            notes.append(f'{c.unit} {c.key} already {c.new} (unchanged)')
            continue
        if current != c.old:
            raise Refused(f'unit {c.unit} {c.key} is {current!r}, neither the known old {c.old!r} nor the new '
                          f'{c.new!r}: an unknown source, refusing to guess')
        start = text.find(f'"{ID}": "{c.unit}"')
        if start < 0 or text.find(f'"{ID}": "{c.unit}"', start + 1) >= 0:
            raise Refused(f'unit {c.unit}: its id must appear exactly once as "{ID}": "{c.unit}"')
        end = text.find(f'"{ID}": "', start + 1)
        end = len(text) if end < 0 else end
        field = f'"{c.key}": "{c.old}"'
        at = text.find(field, start, end)
        if at < 0 or text.find(field, at + 1, end) >= 0:
            raise Refused(f'unit {c.unit}: {field} must appear exactly once in its row')
        text = text[:at] + f'"{c.key}": "{c.new}"' + text[at + len(field):]
        applied.append((c.unit, c.key))
        notes.append(f'{c.unit} {c.key} {c.old} -> {c.new}')
    out = text.encode('utf-8')
    after = rows_of(out, spec)
    if len(after) != len(rows):
        raise Refused('row count changed')
    diff = [(a.get(ID), k) for a, b in zip(rows, after) for k in sorted(set(a) | set(b)) if a.get(k) != b.get(k)]
    if sorted(diff) != sorted(applied):
        raise Refused(f'parsed difference {diff} is not exactly the applied corrections {applied}')
    return out, notes


def propose(source: Path, out: Path, spec: Spec = UNIT) -> str:
    given = Path(os.path.abspath(out))
    if os.path.islink(given) or (os.path.lexists(given) and not given.is_file()):
        raise Refused(f'{given} is a link or not a regular file: a proposal is written only as a new plain file')
    source, out = Path(source).resolve(), given.parent.resolve() / given.name
    if out == source:
        raise Refused('--out is the source: a proposal never overwrites its input')
    if inside_deploy(out):
        raise Refused(f'{out} is inside deploy/: proposals go to a disposable folder')
    data = source.read_bytes()
    before = sha(data)
    result, notes = apply_corrections(data, spec)
    if sha(source.read_bytes()) != before:
        raise Refused(f'{source} changed while it was read')
    if out.exists():
        if out.read_bytes() != result:
            raise Refused(f'{out} exists with different bytes; refusing to overwrite it')
        state = 'unchanged (identical bytes already there)'
    else:
        out.parent.mkdir(parents=True, exist_ok=True)
        replace_into(out, result)
        state = 'written'
    known = {spec.pristine_sha: 'the pristine source', spec.corrected_sha: 'already corrected'}
    lines = [f'source {source} sha256 {before} ({known.get(before, "not a known source hash; values checked")})',
             *notes,
             f'{out}: {state}, sha256 {sha(result)}'
             + (' = the accepted proposal' if sha(result) == spec.corrected_sha else '')]
    return '\n'.join(lines)


# ---------------------------------------------------------------- stage

def chunk_rows(rows: list) -> list[list]:
    """Complete JSON arrays of equal row counts (tools/gen_mst_download.py's partition, kept identical)."""
    if not rows:
        return [[]]
    size = len(json.dumps(rows, ensure_ascii=False, separators=(',', ':')).encode('utf-8'))
    count = max(1, -(-size // CHUNK_BYTES))
    per = -(-len(rows) // count)
    return [rows[i:i + per] for i in range(0, len(rows), per)]


def encode_parts(rows: list, version: int, spec: Spec = UNIT) -> dict[str, bytes]:
    FILE_KEY = spec.file_key
    key = spec.enc_key.encode()[:16].ljust(16, b'\x00')
    chunks = chunk_rows(rows)
    out = {}
    for i, chunk in enumerate(chunks, start=1):
        name = f'Ver{version}_{FILE_KEY}.dat' if len(chunks) == 1 else f'Ver{version}_{FILE_KEY}_{i}.dat'
        plain = json.dumps(chunk, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
        out[name] = base64.b64encode(AES.new(key, AES.MODE_ECB).encrypt(pad(plain, 16)))
    return out


def decode_part(blob: bytes, spec: Spec = UNIT) -> list:
    key = spec.enc_key.encode()[:16].ljust(16, b'\x00')
    return json.loads(unpad(AES.new(key, AES.MODE_ECB).decrypt(base64.b64decode(blob)), 16).decode('utf-8'))


def manifest_bytes(version: int, parts: int, records: int, spec: Spec = UNIT) -> bytes:
    # The generator writes this with Path.write_text, which gives CRLF on Windows; kept so the accepted
    # candidate's manifest regenerates byte for byte (the server parses either).
    doc = {'table': spec.table, 'file_key': spec.file_key, 'version': version, 'file_count': parts, 'record_num': records}
    return json.dumps(doc, indent=1).replace('\n', '\r\n').encode('utf-8')


def tree_hashes(folder: Path) -> dict[str, str]:
    return {str(p.relative_to(folder)): sha(p.read_bytes()) for p in sorted(folder.rglob('*')) if p.is_file()} \
        if folder.is_dir() else {}


def replace_into(path: Path, data: bytes) -> None:
    """Write `data` to `path` without ever writing through an existing file or link.

    The bytes go to a temporary file this call creates EXCLUSIVELY in the destination folder (mkstemp:
    O_CREAT|O_EXCL under a random name, so no earlier attempt's file and no link -- hard, symbolic or
    dangling -- is ever opened for writing), which is closed and then renamed over `path`.  The rename
    replaces the directory entry, so a hard-linked or symlinked `path` is swapped and its other names keep
    their bytes.  On any failure only the temporary file this call created is removed.  (Round-4 QC P2: the
    old predictable `<name>.tmp`, opened with write_bytes, wrote through a pre-existing hard link.)"""
    fd, name = tempfile.mkstemp(prefix=f'.{path.name}.', suffix='.tmp', dir=path.parent)
    tmp = Path(name)
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(data)
        os.replace(tmp, path)
    except BaseException:
        try:
            tmp.unlink()
        except FileNotFoundError:
            pass
        raise


def mirror(source: Path, dest: Path) -> int:
    if dest.exists():
        raise Refused(f'{dest} exists; a mirror is made only into a new folder')
    if source.drive.lower() != dest.drive.lower():
        raise Refused('hard links need one volume')
    n = 0
    for folder, _, files in os.walk(source):
        rel = Path(folder).relative_to(source)
        (dest / rel).mkdir(parents=True, exist_ok=True)
        for name in files:
            os.link(Path(folder) / name, dest / rel / name)
            n += 1
    return n


def stage(rows_path: Path, content_root: Path, version: int, mirror_from: Path | None = None,
          spec: Spec = UNIT) -> str:
    TABLE, FILE_KEY, DEPLOY_STAGE = spec.table, spec.file_key, spec.deploy_stage
    root = Path(content_root).resolve()
    if inside_deploy(root):
        raise Refused(f'{root} is inside deploy/: staging goes to a disposable content root')
    if version < 1:
        raise Refused('the version must be a positive integer')
    rows = rows_of(Path(rows_path).read_bytes(), spec)
    by_id = {r.get(spec.id): r for r in rows}
    unapplied = [c.unit for c in spec.corrections if by_id.get(c.unit, {}).get(c.key) != c.new]
    if unapplied:
        raise Refused(f'rows lack the corrections for {unapplied}: run propose first')
    deploy_before = tree_hashes(DEPLOY_STAGE)
    lines = []
    source_before = {}
    if mirror_from:
        src = Path(mirror_from).resolve()
        source_before = tree_hashes(src / 'content' / TABLE)
        lines.append(f'{mirror(src, root)} hard links {src} -> {root}')
    folder = root / 'content' / TABLE
    folder.mkdir(parents=True, exist_ok=True)
    existing = tree_hashes(folder)
    manifest = folder / 'manifest.json'
    staged = None
    if manifest.is_file():
        staged = json.loads(manifest.read_text(encoding='utf-8')).get('version')
        if isinstance(staged, int) and version < staged:
            raise Refused(f'version {version} is below the staged {staged}')
    parts = encode_parts(rows, version, spec)
    collisions = [n for n, b in parts.items() if n in existing and existing[n] != sha(b)]
    if collisions:
        raise Refused(f'version collision: {collisions} already exist with different bytes')
    part_name = re.compile(rf'Ver{version}_{FILE_KEY}(_\d+)?\.dat')       # only real parts, not e.g. a .tmp
    stale = [n for n in existing if part_name.fullmatch(n) and n not in parts]
    if stale:
        raise Refused(f'version collision: {stale} belong to another split of version {version}')
    for name, blob in parts.items():
        if name in existing:
            lines.append(f'{name}: identical bytes already staged (kept)')
        else:
            replace_into(folder / name, blob)
            lines.append(f'{name}: written, sha256 {sha(blob)}')
    mbytes = manifest_bytes(version, len(parts), len(rows), spec)
    if manifest.is_file() and manifest.read_bytes() == mbytes:
        lines.append('manifest.json: identical (kept)')
    else:
        replace_into(manifest, mbytes)
        lines.append(f'manifest.json: version {version} (replaced by rename), sha256 {sha(mbytes)}')
    back = []
    for name in parts:
        back.extend(decode_part((folder / name).read_bytes(), spec))
    if back != rows:
        raise Refused('round trip failed: the staged parts do not rejoin to the rows')
    if tree_hashes(DEPLOY_STAGE) != deploy_before:
        raise Refused('deploy staging changed during the run')
    if mirror_from and tree_hashes(Path(mirror_from).resolve() / 'content' / TABLE) != source_before:
        raise Refused('the mirror source changed during the run')
    unchanged = {n: h for n, h in existing.items() if n not in parts and n != 'manifest.json'}
    if any(sha((folder / n).read_bytes()) != h for n, h in unchanged.items()):
        raise Refused('an existing staged file changed')
    accepted = (spec.accepted_parts or {}).get(version)
    if accepted:
        same = [sha(b) for b in parts.values()] == accepted
        lines.append(f'version {version}: {"matches" if same else "DIFFERS FROM"} the client-accepted candidate')
    lines.append(f'round trip: {len(parts)} part(s) rejoin to {len(rows)} rows; deploy staging unchanged')
    return '\n'.join(lines)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    ap.add_argument('--table', choices=sorted(SPECS), default=UNIT.table)
    p = sub.add_parser('propose')
    p.add_argument('--source', type=Path, help='default: the table\'s deploy/mst source')
    p.add_argument('--out', type=Path, required=True)
    s = sub.add_parser('stage')
    s.add_argument('--rows', type=Path, required=True)
    s.add_argument('--content-root', type=Path, required=True)
    s.add_argument('--version', type=int, required=True)
    s.add_argument('--mirror-from', type=Path)
    a = ap.parse_args(argv)
    spec = SPECS[a.table]
    try:
        print(propose(a.source or REPO / 'deploy' / 'mst' / spec.source, a.out, spec) if a.cmd == 'propose'
              else stage(a.rows, a.content_root, a.version, a.mirror_from, spec))
        return 0
    except Refused as ex:
        print(f'refused: {ex}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
