"""Fresh-install master-table bootstrap: what a client with an empty LocalState receives (Session 19).

    python scripts/check_mst_bootstrap.py [--localstate DIR] [--server PORT] [--json OUT]
                                          [--mst-root DIR] [--document-root DIR] [--system-root DIR]

The client side is scripts/qa_baselines/mst_bootstrap_x86.json, re-read from the supported x86 EXE by
tools/bin/x86_mst_bootstrap_check.py: the 154 tables VersionInfoResponse::readParam registers (file key,
encode key), the 52 response tags that save a LocalState file, and the 79 lists checkCorruptionsOfMsts
tests at login.  The server side is what this server announces and serves:

  announcement  Initialize KeC10fuL: deploy/mst/version_info_mst.json (the baseline) with each staged table
                (game_content/content/<TABLE>/manifest.json, all parts present, folder = table) replacing
                or appending its row -- Initialize.cpp + gme::mstVersions, mirrored offline, or read from a
                running server's own Initialize reply (--server, a guest login on an empty save).
  parts         the client asks for /mst/Ver<v>_<key>.dat (one part) or /mst/Ver<v>_<key>_<n>.dat, n from 1;
                BfWebController::HandleWebPage answers from <document root>/mst, then findMstPart
                (content/, then one table folder deep), then findContentByName (any depth).  --server GETs
                them instead.
  contents      base64(AES-128-ECB(encode key, zero-padded to 16), PKCS#7) of a JSON array of row objects.

For each registered table, for the given client state (default: an empty LocalState -- never the installed
player's), the table is:
  available / empty  every part held or served and decodable, rows = the announced record count (or that
                     count is 0); empty when there are no rows
  missing            not announced (a fresh client never asks for it), or announced with no part anywhere
  wrong-version      announced parts absent, but the server holds the table's parts at another version
  incomplete         some parts absent (a partial delivery), or the rows differ from a non-zero record
                     count: loadFile 0x543030 deletes every part and re-queues the table at each login
  malformed          a part that is not that encoding of a JSON array of objects
and its requirement, from the login check: required (deleteCorruptFile: an empty list shows
"REDOWNLOAD_MST_FILES" and closes, every launch), required-conditional (F_RESOURCE_BASE_MST, under a gate
the binary reads at runtime), tolerated (resetFileVersion: F_NPC_MESSAGE_MST, F_RELOAD_FILE_MST) or
unchecked (no login check reads it; its own consumers are not traced here).  The response channel (the 29
checked Initialize tags), the initial-unit rule of setInitialUnits and the corrupted_mst_check feature are
checked too -- the tags only against a running server (--server) -- and so are the text-dictionary ZIPs
the delivered F_RESOURCE_MST names (createDownloadCheckList queues them; LoginScene extracts them).

Exit 0 when nothing required is blocked, 1 otherwise.  Read only: it writes nothing but --json.
"""
from __future__ import annotations

import argparse
import base64
import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Iterator

from Crypto.Cipher import AES

sys.path.insert(0, str(Path(__file__).resolve().parent))
ROOT = Path(__file__).resolve().parents[1]
# Beside the scripts, not under tools/ (local tooling, git-ignored): the audit must run from a checkout.
FACTS = ROOT / 'scripts' / 'qa_baselines' / 'mst_bootstrap_x86.json'
FORMAT = 'bfcp-mst-bootstrap-x86 1'
BUILD = '6253F827-01756000-00ED273D'
MST_ROOT = ROOT / 'deploy' / 'mst'
DOCUMENT_ROOT = ROOT / 'deploy' / 'game_content'
SYSTEM_ROOT = ROOT / 'deploy' / 'system'
PART = re.compile(r'ver(\d+)_([0-9a-z]{8})(?:_(\d+))?\.dat', re.I)
# GmeControllerHandlers.cpp: REGISTER("MfZyu1q9", Initialize, ...)
INITIALIZE = ('MfZyu1q9', 'EmcshnQoDr20TZz1')
INITIAL_UNIT_COLUMN = '21ovwqYT'          # DefineMst initial_unit (packet-generator assets/mst/define.kdl)
FIELDS = ('moWQ30GH', 'd2RFtP8T', 'H6k1LIxC', '5kbnkTp0')
# The text dictionaries are F_RESOURCE_MST paths (ZwstRU92) ending in .zip: createDownloadCheckList 0x8DF880
# queues them (0x3918E0 at 0x8E06DB) and LoginScene::updateEvent extracts them (0x392DA0 at 0x8E5069).
DICTIONARY_TABLE, DICTIONARY_PATH = 'F_RESOURCE_MST', 'ZwstRU92'


def load_facts(path: Path = FACTS) -> dict:
    facts = json.loads(path.read_text(encoding='utf-8'))
    if facts.get('format') != FORMAT or facts.get('build') != BUILD:
        raise SystemExit(f'{path}: not {FORMAT} for {BUILD} (run tools/bin/x86_mst_bootstrap_check.py)')
    return facts


def number(value) -> int | None:
    """A version-row number as Initialize sends it: quoted (glz::quoted_num) or plain."""
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, str) and re.fullmatch(r'-?\d+', value.strip()):
        return int(value)
    return None


def row(table: str, version: int, file_count: int, record_num: int) -> dict:
    return {'table': table, 'version': version, 'file_count': file_count, 'record_num': record_num}


def baseline_rows(mst_root: Path) -> list[dict]:
    """deploy/mst/version_info_mst.json, the rows Initialize's cached response carries."""
    data = json.loads((mst_root / 'version_info_mst.json').read_text(encoding='utf-8'))
    return [row(r['moWQ30GH'], number(r.get('d2RFtP8T')), number(r.get('H6k1LIxC')), number(r.get('5kbnkTp0')))
            for r in data['KeC10fuL']]


def part_names(version: int, file_key: str, file_count: int) -> Iterator[str]:
    """MstVersions.hpp and getMstFileName 0x554C80: no part suffix for one part, _1.._n otherwise.  Lazy, so
    a check can stop at the first absent part as mstVersions does, whatever count a manifest claims."""
    if file_count == 1:
        yield f'Ver{version}_{file_key}.dat'
        return
    for n in range(1, file_count + 1):
        yield f'Ver{version}_{file_key}_{n}.dat'


MANIFEST_FIELDS = {'table': str, 'file_key': str, 'version': int, 'file_count': int, 'record_num': int}


def read_manifest(text: str) -> dict | None:
    """MstVersions.hpp's glz::read into MstDownloadManifest: the document must be a JSON object; a missing
    field keeps its default ('' or 0), unknown keys are ignored, and any field present with the wrong type
    (a quoted number, null, a bool, a fraction, a number outside int32) fails the whole read, so the
    manifest is skipped.  None means skipped."""
    try:
        m = json.loads(text)
    except ValueError:
        return None
    if not isinstance(m, dict):
        return None
    out = {k: ('' if t is str else 0) for k, t in MANIFEST_FIELDS.items()}
    for key, kind in MANIFEST_FIELDS.items():
        if key not in m:
            continue
        v = m[key]
        if kind is int and (isinstance(v, bool) or not isinstance(v, int) or not -2**31 <= v < 2**31):
            return None
        if kind is str and not isinstance(v, str):
            return None
        out[key] = v
    return out


def staged_rows(document_root: Path) -> list[dict]:
    """gme::mstVersions: content/<TABLE>/manifest.json, announced only when readable as a manifest
    (read_manifest), complete (table, file_key, version > 0, file_count > 0), every part present in that
    folder and the folder named the table."""
    content = document_root / 'content'
    out = []
    if not content.is_dir():
        return out
    for folder in sorted(p for p in content.iterdir() if p.is_dir()):
        manifest = folder / 'manifest.json'
        if not manifest.is_file():
            continue
        try:
            m = read_manifest(manifest.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            continue
        if m is None:
            continue
        if not m['table'] or not m['file_key'] or m['version'] <= 0 or m['file_count'] <= 0:
            continue
        if not all((folder / n).is_file() for n in part_names(m['version'], m['file_key'], m['file_count'])):
            continue
        if folder.name != m['table']:
            continue
        out.append(row(m['table'], m['version'], m['file_count'], m['record_num']))
    return out


def announce(mst_root: Path, document_root: Path) -> list[dict]:
    """Initialize.cpp: the baseline, with each staged row replacing the first baseline row of its table or
    appended after them."""
    rows = baseline_rows(mst_root)
    for staged in staged_rows(document_root):
        at = next((i for i, r in enumerate(rows) if r['table'] == staged['table']), None)
        if at is None:
            rows.append(staged)
        else:
            rows[at] = staged
    return rows


def wire_rows(reply: dict) -> list[dict]:
    return [row(r.get('moWQ30GH'), number(r.get('d2RFtP8T')), number(r.get('H6k1LIxC')), number(r.get('5kbnkTp0')))
            for r in reply.get('KeC10fuL') or []]


class ServerFiles:
    """What BfWebController::HandleWebPage serves for GET /mst/<name>, read from the document root."""

    def __init__(self, document_root: Path):
        self.root = Path(document_root)
        self.content = self.root / 'content'
        self._index = None

    def _walk(self, folder: Path):
        """recursive_directory_iterator: depth-first, each directory's entries in name order (NTFS)."""
        try:
            entries = sorted(folder.iterdir(), key=lambda p: p.name)
        except OSError:
            return
        for p in entries:
            if p.is_dir():
                yield from self._walk(p)
            elif p.is_file():
                yield p

    def index(self) -> dict[str, Path]:
        """findContentByName's index: first file wins per basename."""
        if self._index is None:
            self._index = {}
            if self.content.is_dir():
                for p in self._walk(self.content):
                    self._index.setdefault(p.name, p)
        return self._index

    def resolve(self, name: str) -> Path | None:
        direct = self.root / 'mst' / name
        if direct.is_file():
            return direct
        if (len(name) >= 8 and name.startswith('Ver') and name.endswith('.dat')
                and all(c.isalnum() or c == '_' for c in name[:-4]) and self.content.is_dir()):
            if (self.content / name).is_file():
                return self.content / name
            for folder in sorted(p for p in self.content.iterdir() if p.is_dir()):
                if (folder / name).is_file():
                    return folder / name
        if name and name != '.' and '..' not in name:
            return self.index().get(name)
        return None

    def versions_of(self, file_key: str) -> dict[int, list[str]]:
        """Every Ver<v>_<file_key>[_<n>].dat this server could serve, by version."""
        out = {}
        names = set(self.index())
        if (self.root / 'mst').is_dir():
            names |= {p.name for p in (self.root / 'mst').iterdir() if p.is_file()}
        for n in names:
            m = PART.fullmatch(n)
            if m and m.group(2).lower() == file_key.lower():
                out.setdefault(int(m.group(1)), []).append(n)
        return {v: sorted(ns) for v, ns in sorted(out.items())}


def decode(data: bytes, encode_key: str) -> tuple[list | None, str | None]:
    """base64(AES-ECB(json array, PKCS#7)) with the key zero-padded to 16 bytes (tools/gen_mst_download.py)."""
    try:
        raw = base64.b64decode(data, validate=False)
        if not raw or len(raw) % 16:
            return None, 'not a whole number of AES blocks after base64'
        plain = AES.new(encode_key.encode()[:16].ljust(16, b'\0'), AES.MODE_ECB).decrypt(raw)
        pad = plain[-1]
        if not 1 <= pad <= 16 or plain[-pad:] != bytes([pad]) * pad:
            return None, 'bad PKCS#7 padding (wrong key, or not this table\'s file)'
        rows = json.loads(plain[:-pad].decode('utf-8'))
    except (ValueError, UnicodeDecodeError) as e:
        return None, f'not decodable: {type(e).__name__}'
    if not isinstance(rows, list) or not all(isinstance(r, dict) for r in rows):
        return None, 'not a JSON array of row objects'
    return rows, None


def requirements(facts: dict) -> dict[str, tuple[str, str]]:
    """table -> (requirement, consumer) from the login check."""
    out = {}
    for c in facts['corruption_checks']:
        if c['channel'] == 'version':
            kind = ('tolerated' if c['handler'] == 'resetFileVersion' else
                    'required-conditional' if c['table'] == 'F_RESOURCE_BASE_MST' else 'required')
            out[c['table']] = (kind, f'checkCorruptionsOfMsts {c["handler"]} at {c["site"]} (list {c["list"]})')
        else:
            for filler in c['filled_by'].get('login', []):
                if filler.startswith('F_') and filler not in out:
                    out[filler] = ('required', f'checkCorruptionsOfMsts {c["table"]} at {c["site"]}: its list '
                                               f'{c["list"]} is filled at login by loading {filler}')
    return out


class Source:
    """Where the parts come from: the document root (offline) or a running server (GETs)."""

    def __init__(self, document_root: Path | None = None, port: int | None = None):
        self.files = ServerFiles(document_root) if document_root is not None else None
        self.port = port

    def has_content(self, path: str) -> tuple[bool, str]:
        """A content file at its F_RESOURCE_MST path: <document root>/content/<path>, else by name; or GET."""
        if self.port is not None:
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{self.port}/content{path}', timeout=60) as r:
                    r.read()
                    return True, f'GET /content{path} {r.status}'
            except urllib.error.HTTPError as e:
                return False, f'GET /content{path} {e.code}'
        direct = self.files.content / path.lstrip('/')
        if direct.is_file():
            return True, str(direct)
        hit = self.files.index().get(path.rsplit('/', 1)[-1])
        return (hit is not None, str(hit) if hit else 'no file by that name under the content root')

    def fetch(self, name: str) -> tuple[bytes | None, str]:
        if self.port is not None:
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{self.port}/mst/{name}', timeout=60) as r:
                    return r.read(), f'GET /mst/{name} {r.status}'
            except urllib.error.HTTPError as e:
                return None, f'GET /mst/{name} {e.code}'
        path = self.files.resolve(name)
        if path is None:
            return None, 'no file by that name under the document root'
        return path.read_bytes(), str(path)


def classify(facts: dict, announced: list[dict], source: Source, localstate: Path | None,
             server_files: ServerFiles | None = None, dictionary_rows: list | None = None) -> list[dict]:
    """One entry per registered table plus one per announced row readParam does not register.  The rows of
    the F_RESOURCE_MST the client would load are appended to dictionary_rows."""
    dictionary_rows = [] if dictionary_rows is None else dictionary_rows
    registry = {t['table']: t for t in facts['version_channel']['tables']}
    needs = requirements(facts)
    rows = {}
    for r in announced:
        rows.setdefault(r['table'], r)          # readParam registers the first row per table it reaches
    held = {p.name.lower(): p for p in localstate.iterdir() if p.is_file()} if localstate and localstate.is_dir() else {}
    out = []
    for table, reg in registry.items():
        need, consumer = needs.get(table, ('unchecked', 'no login check reads it'))
        entry = {'table': table, 'file_key': reg['file_key'], 'encode_key': reg['encode_key'],
                 'requirement': need, 'consumer': consumer, 'announced': None, 'parts': [], 'rows': None,
                 'local_versions': sorted({int(m.group(1)) for n in held if (m := PART.fullmatch(n))
                                           and m.group(2).lower() == reg['file_key'].lower()})}
        r = rows.get(table)
        if r is None:
            entry.update(status='missing', why='not announced: readParam never adds it, so a fresh client never '
                                               'asks for it and its list stays empty')
            out.append(entry)
            continue
        entry['announced'] = {k: r[k] for k in ('version', 'file_count', 'record_num')}
        if not r['version'] or not r['file_count'] or r['file_count'] < 1:
            entry.update(status='malformed', why=f'announced row is not usable: {entry["announced"]}')
            out.append(entry)
            continue
        total, bad, absent = 0, [], []
        for name in part_names(r['version'], reg['file_key'], r['file_count']):
            part = {'name': name}
            local = held.get(name.lower())
            if local is not None:
                data, where = local.read_bytes(), 'held in LocalState (existsLocalFile: not downloaded)'
            else:
                data, where = source.fetch(name)
            part['source'] = where
            if data is None:
                absent.append(name)
                part['rows'] = None
            else:
                got, error = decode(data, reg['encode_key'])
                part['rows'] = None if got is None else len(got)
                if error:
                    part['error'] = error
                    bad.append(name)
                else:
                    total += len(got)
                    if table == DICTIONARY_TABLE:
                        dictionary_rows.extend(got)
            entry['parts'].append(part)
        if absent and len(absent) == len(entry['parts']):
            other = {v: n for v, n in (server_files.versions_of(reg['file_key']).items() if server_files else [])
                     if v != r['version']}
            if other:
                entry.update(status='wrong-version', why=f'announced v{r["version"]} is not served; the server holds '
                                                         f'v{", v".join(str(v) for v in other)}', other_versions=other)
            else:
                entry.update(status='missing', why=f'announced v{r["version"]}, but no part is served')
        elif absent:
            entry.update(status='incomplete', why=f'partial delivery: {len(absent)} of {len(entry["parts"])} part(s) '
                                                  f'not served ({absent[0]}...)')
        elif bad:
            entry.update(status='malformed', why=f'{bad[0]}: {next(p["error"] for p in entry["parts"] if p.get("error"))}')
        elif r['record_num'] and total != r['record_num']:
            entry.update(status='incomplete', rows=total,
                         why=f'{total} row(s), announced {r["record_num"]}: loadFile 0x543030 deletes every part and '
                             f're-queues the table at each login')
        else:
            entry.update(status='available' if total else 'empty', rows=total, why='')
        stale = [v for v in entry['local_versions'] if v != r['version']]
        if stale:
            entry['stale_local'] = f'LocalState also holds v{", v".join(map(str, stale))}: replaced by v{r["version"]}; ' \
                                   'saveVersion 0x558930 deletes lower versions after a successful login check'
        out.append(entry)
    for table, r in rows.items():
        if table not in registry:
            out.append({'table': table, 'announced': {k: r[k] for k in ('version', 'file_count', 'record_num')},
                        'status': 'not-registered', 'requirement': 'none',
                        'why': ('a response table\'s version row (no download)' if table.startswith('M_') else
                                'readParam does not register it: an announced row the client ignores')})
    return out


def blocked(entry: dict) -> bool:
    if entry['requirement'] in ('required', 'required-conditional'):
        return entry['status'] != 'available'
    if entry['requirement'] == 'tolerated':
        return entry['status'] not in ('available', 'empty', 'missing')
    return False


def initial_units(define: dict | None) -> tuple[bool, str]:
    """setInitialUnits 0x8DF020 over DefineMst 21ovwqYT: entries split on ',', each '<element>-<members>',
    members on ':', each on '@' (the last token the unit id); true only when each element 1..4 has a member
    with a non-empty unit id."""
    if not isinstance(define, dict) or not isinstance(define.get(INITIAL_UNIT_COLUMN), str):
        return False, f'no DefineMst {INITIAL_UNIT_COLUMN}'
    units = {}
    for entry in define[INITIAL_UNIT_COLUMN].split(','):
        element, _, members = entry.partition('-')
        if not element.strip().lstrip('-').isdigit():
            continue
        for member in members.split(':'):
            unit = member.split('@')[-1] if member else ''
            if unit:
                units.setdefault(int(element), []).append(unit)
    missing = [e for e in (1, 2, 3, 4) if not units.get(e)]
    return (not missing,
            'elements 1-4 each have an initial unit' if not missing else f'no initial unit for element(s) {missing}')


def feature_flag(value) -> tuple[bool, str]:
    return (value == 1 or value is True,
            f'corrupted_mst_check = {value!r}' + ('' if value == 1 or value is True else
                                                   ': the login corruption check would be switched off'))


def dictionaries(rows: list, source: Source) -> list[dict]:
    """Every text-dictionary ZIP the delivered F_RESOURCE_MST names, and whether this server serves it."""
    out = []
    paths = {r.get(DICTIONARY_PATH) for r in rows if str(r.get(DICTIONARY_PATH, '')).lower().endswith('.zip')}
    for path in sorted(paths):
        ok, where = source.has_content(path)
        out.append({'path': path, 'status': 'available' if ok else 'missing', 'source': where})
    return out


def response_channel(facts: dict, reply: dict) -> list[dict]:
    """The checked response tables: every tag must carry at least one row in the Initialize reply."""
    out = []
    for c in facts['corruption_checks']:
        if c['channel'] != 'response' or not c.get('tag'):
            continue
        value = reply.get(c['tag'])
        rows = len(value) if isinstance(value, list) else (1 if isinstance(value, dict) and value else 0)
        out.append({'table': c['table'], 'tag': c['tag'], 'file': c['file'], 'rows': rows,
                    'status': 'available' if rows else 'missing'})
    return out


# ---------------------------------------------------------------------------------------------------
# a running server

def http_json(port: int, path: str):
    with urllib.request.urlopen(f'http://127.0.0.1:{port}{path}', timeout=60) as r:
        return json.loads(r.read())


def initialize(port: int) -> dict:
    """A fresh install's first two requests: the guest login (it creates the Gumi Live user on an empty
    save) and Initialize with no game user id yet."""
    import bf_testkit
    login = http_json(port, '/accounts/guest/login/')
    client = bf_testkit.Client.__new__(bf_testkit.Client)
    client.user, client.login = '', login['game_user_id']
    client.url = f'http://127.0.0.1:{port}/bf/gme/action.php'
    reply = client.call(INITIALIZE[0], INITIALIZE[1], {'KeC10fuL': []}, tag='bootstrap')
    if 'error' in reply:
        raise RuntimeError(f'Initialize answered an error: {reply["error"]}')
    return reply


def audit(facts: dict, *, localstate: Path | None = None, port: int | None = None, mst_root: Path = MST_ROOT,
          document_root: Path = DOCUMENT_ROOT, system_root: Path = SYSTEM_ROOT) -> dict:
    """The inventory: offline over the roots, or (port) over a running server's own replies."""
    files = ServerFiles(document_root)
    if port is not None:
        reply = initialize(port)
        announced = wire_rows(reply)
        source = Source(port=port)
        responses = response_channel(facts, reply)
        units = initial_units(reply.get('VkoZ5t3K') if isinstance(reply.get('VkoZ5t3K'), dict) else None)
        try:
            flag = feature_flag(http_json(port, '/bf/gme/featureCheck.php').get('corrupted_mst_check'))
        except (OSError, ValueError) as e:
            flag = (False, f'featureCheck.php unreadable: {e}')
    else:
        announced = announce(mst_root, document_root)
        source = Source(document_root=document_root)
        responses = None
        define = json.loads((mst_root / 'defines_mst.json').read_text(encoding='utf-8'))
        units = initial_units(define if isinstance(define, dict) else None)
        features = json.loads((system_root / 'features.json').read_text(encoding='utf-8'))
        flag = feature_flag(features.get('corrupted_mst_check'))
    dictionary_rows = []
    tables = classify(facts, announced, source, localstate, files, dictionary_rows)
    zips = dictionaries(dictionary_rows, source)
    content_versions = {r['table']: r['version'] for r in announced if r['table'] in facts['resource_versions']}
    blockers = [t['table'] for t in tables if blocked(t)]
    blockers += [f'dictionary {z["path"]}' for z in zips if z['status'] != 'available']
    if responses is not None:
        blockers += [f'{r["table"]} ({r["tag"]})' for r in responses if r['status'] != 'available']
    if not units[0]:
        blockers.append(f'setInitialUnits: {units[1]}')
    if not flag[0]:
        blockers.append(f'features: {flag[1]}')
    return {'mode': 'server' if port is not None else 'offline', 'announced': len(announced), 'tables': tables,
            'responses': responses, 'dictionaries': zips, 'content_versions': content_versions,
            'initial_units': units[1], 'feature': flag[1], 'blockers': blockers}


def summary_lines(result: dict) -> list[str]:
    tables = result['tables']
    lines = []
    for t in sorted(tables, key=lambda t: (t['requirement'] == 'none', t['requirement'] == 'unchecked', t['table'])):
        if t['status'] == 'not-registered':
            continue
        a = t.get('announced')
        lines.append(f'{"BLOCK" if blocked(t) else "ok   "} {t["table"]:40} {t["requirement"]:20} {t["status"]:13} '
                     + (f'v{a["version"]} parts {a["file_count"]} records {a["record_num"]}' if a else 'not announced')
                     + (f' rows {t["rows"]}' if t.get('rows') is not None else '')
                     + (f' -- {t["why"]}' if t.get('why') else '') + (f' ({t["stale_local"]})' if t.get('stale_local') else ''))
    ignored = [t['table'] for t in tables if t['status'] == 'not-registered']
    lines.append(f'note: {len(ignored)} announced row(s) readParam does not register: {", ".join(ignored[:8])}'
                 + ('...' if len(ignored) > 8 else ''))
    if result['responses'] is None:
        lines.append('note: the response channel (the 29 checked Initialize tags) needs --server; not checked')
    else:
        for r in result['responses']:
            lines.append(f'{"ok   " if r["status"] == "available" else "BLOCK"} response {r["table"]:30} {r["tag"]} '
                         f'-> {r["file"]}: {r["rows"]} row(s)')
    zips = result['dictionaries']
    missing = [z['path'] for z in zips if z['status'] != 'available']
    lines.append(f'{"ok   " if zips and not missing else "BLOCK"} text dictionaries: {len(zips) - len(missing)} of '
                 f'{len(zips)} ZIP(s) the delivered F_RESOURCE_MST names are served'
                 + (f'; missing: {missing[:6]}' if missing else '') + ('' if zips else ' (no F_RESOURCE_MST rows)'))
    lines.append('note: content-kind version rows (0x531A00 purges cached files on a version change; a fresh '
                 f'install has nothing cached): {result["content_versions"]}')
    lines.append(f'{"ok   " if not result["initial_units"].startswith("no") else "BLOCK"} setInitialUnits: '
                 f'{result["initial_units"]}')
    lines.append(f'{"ok   " if result["feature"].endswith(" 1") else "BLOCK"} features: {result["feature"]}')
    counts = {}
    for t in tables:
        counts.setdefault((t['requirement'], t['status']), 0)
        counts[(t['requirement'], t['status'])] += 1
    lines.append('counts: ' + ', '.join(f'{req}/{st} {n}' for (req, st), n in sorted(counts.items())))
    return lines


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--localstate', type=Path, help='the client state to audit (default: an empty LocalState)')
    ap.add_argument('--server', type=int, metavar='PORT', help='a running isolated server on 127.0.0.1')
    ap.add_argument('--mst-root', type=Path, default=MST_ROOT)
    ap.add_argument('--document-root', type=Path, default=DOCUMENT_ROOT)
    ap.add_argument('--system-root', type=Path, default=SYSTEM_ROOT)
    ap.add_argument('--facts', type=Path, default=FACTS)
    ap.add_argument('--json', type=Path, help='write the whole inventory here')
    args = ap.parse_args()
    result = audit(load_facts(args.facts), localstate=args.localstate, port=args.server, mst_root=args.mst_root,
                   document_root=args.document_root, system_root=args.system_root)
    for line in summary_lines(result):
        print(line)
    if args.json:
        args.json.write_text(json.dumps(result, indent=1), encoding='utf-8')
    print(f'check_mst_bootstrap ({result["mode"]}, {result["announced"]} announced row(s)): '
          + (f'{len(result["blockers"])} required delivery(ies) blocked: {", ".join(result["blockers"][:10])}'
             + ('...' if len(result['blockers']) > 10 else '') if result['blockers']
             else 'every required delivery is available'))
    return 1 if result['blockers'] else 0


if __name__ == '__main__':
    sys.exit(main())
