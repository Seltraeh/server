"""Wire tests for the read-only content broker (/offline_mod/content/v1/*).

The broker is what the offline-proxy content provider reads assets from (see
gimuserver/content/ContentBroker.cpp and docs/OFFLINE_CONTENT_LOADING_PLAN.md).
Every server here runs on a fresh isolated save with a small temporary content
root under out/qa/current/content_broker -- never the live save, never the
full content tree.

Covers: exact relative paths only; flat-basename ownership with aliases,
byte-identical duplicates and reported collisions; every owned name published
with its identity (SHA-256); extension/exclude policy (client state names are
never served); confinement against traversal strings, junctions and symlinks;
same-size edits with and without a preserved mtime; a touch that changes
nothing; deletions and new collisions as tombstones; watcher overflow
recovered by a rescan; no torn reads under concurrent writers; a busy file;
a file met in the replace window (delete-pending) is served once its new
copy lands, or answered busy -- never missing -- while a really removed file
is still a 404; dormant and rejected policies.  Reads racing the watcher and
version-bound reads are in test_content_generation_qc.py.

    python scripts/test_content_broker_wire.py out/qa/current/bin/gimuserverw.exe
"""
from __future__ import annotations

import ctypes
import hashlib
import json
import os
import shutil
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Fixture, IsolatedServer, qa_dir  # noqa: E402

REAL = ROOT / 'deploy' / 'game_content' / 'content'
PORT = 19960


def sha(b: bytes) -> str:
    return 'sha256:' + hashlib.sha256(b).hexdigest()


class Broker:
    def __init__(self, port: int):
        self.base = f'http://127.0.0.1:{port}/offline_mod/content/v1'

    def get(self, path: str, params: dict | None = None):
        url = self.base + path + ('?' + urllib.parse.urlencode(params) if params else '')
        try:
            with urllib.request.urlopen(url, timeout=10) as r:
                return r.status, {k.lower(): v for k, v in r.headers.items()}, r.read()
        except urllib.error.HTTPError as e:
            return e.code, {k.lower(): v for k, v in e.headers.items()}, e.read()

    def get_raw(self, raw_query: str):
        """A query string sent exactly as written (no re-encoding)."""
        try:
            with urllib.request.urlopen(self.base + '/file?' + raw_query, timeout=10) as r:
                return r.status, {k.lower(): v for k, v in r.headers.items()}, r.read()
        except urllib.error.HTTPError as e:
            return e.code, {k.lower(): v for k, v in e.headers.items()}, e.read()

    def status(self) -> dict:
        code, _, body = self.get('/status')
        assert code == 200, code
        out = {}
        for line in body.decode().splitlines():
            if ' ' in line:
                k, v = line.split(' ', 1)
                out[k] = v
        return out

    def index(self, since: int = 0, epoch: str = '') -> dict:
        code, _, body = self.get('/index', {'since': since, 'epoch': epoch})
        lines = body.decode().splitlines()
        out = {'code': code, 'F': {}, 'D': {}, 'C': {}, 'head': {}, 'end': lines[-1:] == ['end']}
        for line in lines:
            parts = line.split('\t')
            if parts[0] == 'F':
                out['F'][parts[3].lower()] = {'gen': int(parts[1]), 'size': int(parts[2]),
                                              'name': parts[3], 'rel': parts[4],
                                              'digest': parts[5] if len(parts) > 5 else ''}
            elif parts[0] == 'D':
                out['D'][parts[2]] = int(parts[1])
            elif parts[0] == 'C':
                out['C'][parts[1]] = parts[2:]
            elif ' ' in line:
                k, v = line.split(' ', 1)
                out['head'][k] = v
        return out

    def file(self, rel: str):
        return self.get('/file', {'p': rel})

    def wait_ready(self, timeout=60):
        deadline = time.time() + timeout
        while time.time() < deadline:
            st = self.status()
            if st.get('ready') == '1' or st.get('enabled') == '0':
                return st
            time.sleep(0.2)
        raise RuntimeError('broker never became ready')

    def wait_generation_above(self, gen: int, timeout=15) -> int:
        deadline = time.time() + timeout
        while time.time() < deadline:
            g = int(self.status()['generation'])
            if g > gen:
                return g
            time.sleep(0.1)
        return int(self.status()['generation'])


_k32 = ctypes.WinDLL('kernel32', use_last_error=True)
_k32.CreateFileW.restype = wintypes.HANDLE
_k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                             wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
_k32.SetFileInformationByHandle.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
_k32.GetFileTime.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
_k32.SetFileTime.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
_k32.CloseHandle.argtypes = [wintypes.HANDLE]
FILE_DISPOSITION_INFO_CLASS = 4


def delete_pending(path: Path):
    """A held handle that has marked `path` for deletion: until it closes, the name
    stays but every open of it fails with ERROR_ACCESS_DENIED -- the state of the
    old file during an atomic replace.  cancel_delete() takes the mark back."""
    generic_read, delete, write_attributes, share_all, open_existing = 0x80000000, 0x10000, 0x100, 7, 3
    h = _k32.CreateFileW(str(path), generic_read | delete | write_attributes, share_all, None, open_existing, 0, None)
    assert h not in (None, wintypes.HANDLE(-1).value), ctypes.get_last_error()
    flag = ctypes.c_ubyte(1)                   # FILE_DISPOSITION_INFO.DeleteFile = TRUE
    assert _k32.SetFileInformationByHandle(h, FILE_DISPOSITION_INFO_CLASS, ctypes.byref(flag), 1), \
        ctypes.get_last_error()
    return h


def cancel_delete(h) -> None:
    flag = ctypes.c_ubyte(0)
    assert _k32.SetFileInformationByHandle(h, FILE_DISPOSITION_INFO_CLASS, ctypes.byref(flag), 1), \
        ctypes.get_last_error()


def nudge_write_time(h, delta: int) -> None:
    """Moves the last-write time of an open handle by `delta` 100-ns ticks: the
    directory watcher is notified about that name, delete-pending or not."""
    t = ctypes.c_ulonglong()
    assert _k32.GetFileTime(h, None, None, ctypes.byref(t)), ctypes.get_last_error()
    t.value += delta
    assert _k32.SetFileTime(h, None, None, ctypes.byref(t)), ctypes.get_last_error()


def close_handle(h) -> None:
    _k32.CloseHandle(h)


def copy_real(rel_src: str, dest_root: Path, rel_dest: str | None = None) -> bytes:
    src = REAL / rel_src
    dst = dest_root / (rel_dest or rel_src)
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dst)
    return dst.read_bytes()


POLICY = {
    'enabled': True,
    'scope': ['sam/MapVillage/*', '_dlcbundle/MapVillage/*', 'unit/**', 'sam/Junk/**'],
    'aliases': {'MapVillage.sam': 'sam/MapVillage/MapVillage.sam',
                'Other.sam': 'sam/Other/Other.sam'},
    'extensions': ['.sam', '.png', '.jpg', '.csv'],
    'exclude': ['UserDefault.xml', 'XMLFileProcessor.xml', 'Ver*.dat', '*.dat', '*.xml', '*.ini',
                '*.tmp', '*.json', '*.zip', 'Thumbs.db', '.DS_*'],
    # Starts incomplete: unit_ills_battle_99999.png does not exist yet.
    'bundles': {'unit_99999': ['unit_anime_99999.png', 'unit_cgg_99999.csv', 'unit_ills_battle_99999.png']},
}


def build_fixture(base: Path) -> dict:
    if base.exists():
        shutil.rmtree(base, ignore_errors=True)
    content = base / 'game_content' / 'content'
    outside = base / 'outside'
    system = base / 'system'
    for d in (content, outside, system):
        d.mkdir(parents=True, exist_ok=True)
    files = {}
    files['sam_sam'] = copy_real('sam/MapVillage/MapVillage.sam', content)
    files['dlc_sam'] = copy_real('_dlcbundle/MapVillage/MapVillage.sam', content)
    copy_real('_dlcbundle/MapVillage.sam', content)                       # identical, out of scope
    files['p111_sam'] = copy_real('sam/MapVillage/MapVillage_111x55.png', content)
    files['p111_dlc'] = copy_real('_dlcbundle/MapVillage/MapVillage_111x55.png', content)
    files['p208'] = copy_real('sam/MapVillage/MapVillage_640x208.png', content)
    files['p809'] = copy_real('sam/MapVillage/MapVillage_640x809.png', content)
    copy_real('_dlcbundle/MapVillage/MapVillage_640x809.png', content)     # identical duplicate
    files['p200'] = copy_real('sam/MapVillage/MapVillage_320x200.png', content)
    files['p289'] = copy_real('_dlcbundle/MapVillage/MapVillage_320x289.jpg', content)
    files['anime'] = copy_real('unit/img/unit_anime_60956.png', content)
    files['cgg'] = copy_real('unit/cgg/unit_cgg_60956.csv', content)
    files['idle'] = copy_real('unit/cgs/unit_idle_cgs_60956.csv', content)
    # Client state / MST names inside the scope must never be offered.
    (content / 'sam' / 'MapVillage' / 'UserDefault.xml').write_text('<state/>')
    (content / 'sam' / 'MapVillage' / 'Ver1_test.dat').write_bytes(b'mst')
    (content / 'sam' / 'MapVillage' / 'notes.txt').write_text('not an asset family')
    (content / 'sam' / 'Other').mkdir(parents=True)
    (content / 'sam' / 'Other' / 'Other.sam').write_bytes(b'MAS.other')
    (content / 'sam' / 'Junk').mkdir(parents=True)
    files['ok'] = b'\x89PNG\r\n\x1a\nok'
    # Two of a three-file bundle: frame layout and sprite sheet, no portrait.
    (content / 'unit' / 'img' / 'unit_anime_99999.png').write_bytes(b'\x89PNG\r\n\x1a\nsprite99999')
    (content / 'unit' / 'cgg' / 'unit_cgg_99999.csv').write_bytes(b'1,2,3\r\n')
    (content / 'sam' / 'Junk' / 'ok.png').write_bytes(files['ok'])
    # Escape attempts: a junction and a file symlink into a folder outside the root.
    (outside / 'leak.png').write_bytes(b'\x89PNG\r\n\x1a\nLEAK')
    (base / 'secret.txt').write_text('outside the content root')
    junction = content / 'sam' / 'Junk' / 'link'
    subprocess.run(['cmd', '/c', 'mklink', '/J', str(junction), str(outside)], capture_output=True)
    files['junction'] = junction.exists()
    try:
        os.symlink(outside / 'leak.png', content / 'sam' / 'Junk' / 'leak2.png')
        files['symlink'] = True
    except OSError:
        files['symlink'] = False
    # The server loads its other system files from the same root.
    for real in (ROOT / 'deploy' / 'system').glob('*.json'):
        if real.name != 'content_provider.json':
            shutil.copyfile(real, system / real.name)
    (system / 'content_provider.json').write_text(json.dumps(POLICY, indent=1))
    return {'content': content, 'system': system, 'files': files, 'base': base}


def _unlocked(path: Path) -> bool:
    try:
        os.rename(path, str(path) + '.probe')
        os.rename(str(path) + '.probe', path)
        return True
    except OSError:
        return False


def write_same_size(path: Path, mutate) -> bytes:
    data = bytearray(path.read_bytes())
    mutate(data)
    with open(path, 'r+b') as fh:
        fh.write(data)
    return bytes(data)


def main() -> int:
    exe = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / 'out/qa/current/bin/gimuserverw.exe')
    check = Checker()
    work = qa_dir('content_broker')
    # A zombie from an aborted run can keep its save locked; never reuse a locked folder.
    for attempt in range(1, 10):
        probe = work / ('server' if attempt == 1 else f'server{attempt}')
        if not (probe / 'gme.sqlite').exists() or _unlocked(probe / 'gme.sqlite'):
            break
    server_dir = probe
    fx_info = build_fixture(work / 'tree')
    content, files = fx_info['content'], fx_info['files']
    fx = Fixture.create(server_dir, port=PORT, fresh=True,
                        document_root=work / 'tree' / 'game_content', system_root=fx_info['system'])
    broker = Broker(PORT)

    with IsolatedServer(exe, fx, log_name='server.log'):
        st = broker.wait_ready()
        check('status: enabled and ready', st.get('enabled') == '1' and st.get('ready') == '1', st)
        check('status: escape links were seen and skipped',
              int(st.get('reparse_skipped', 0)) >= (1 if files['junction'] else 0) + (1 if files['symlink'] else 0), st)
        epoch = st['epoch']
        idx = broker.index()
        check('index: well formed and terminated', idx['code'] == 200 and idx['end'] and idx['head'].get('full') == '1', idx['head'])
        owned = idx['F']
        expected = {
            'mapvillage.sam': 'sam/MapVillage/MapVillage.sam',
            'mapvillage_640x208.png': 'sam/MapVillage/MapVillage_640x208.png',
            'mapvillage_640x809.png': '_dlcbundle/MapVillage/MapVillage_640x809.png',
            'mapvillage_320x200.png': 'sam/MapVillage/MapVillage_320x200.png',
            'mapvillage_320x289.jpg': '_dlcbundle/MapVillage/MapVillage_320x289.jpg',
            'unit_anime_60956.png': 'unit/img/unit_anime_60956.png',
            'unit_cgg_60956.csv': 'unit/cgg/unit_cgg_60956.csv',
            'unit_idle_cgs_60956.csv': 'unit/cgs/unit_idle_cgs_60956.csv',
            'ok.png': 'sam/Junk/ok.png',
        }
        check('index: owned set is exactly the scoped, unambiguous names',
              {k: v['rel'] for k, v in owned.items()} == expected, sorted(owned))
        check('protocol: bfcp 2 (identities in the index, version-bound reads)',
              broker.get('/status')[2].decode().splitlines()[0] == 'bfcp 2'
              and broker.get('/index')[2].decode().splitlines()[0] == 'bfcp 2')
        check('index: every owned name is published with the SHA-256 of the copy it serves',
              owned and all(v['digest'] == sha((content / v['rel']).read_bytes()) for v in owned.values()),
              {k: v['digest'][:15] for k, v in owned.items()})
        check('index: alias picks the framed SAM, not the iph5 copy under the same name',
              owned.get('mapvillage.sam', {}).get('size') == len(files['sam_sam']))
        check('index: differing copies without an alias are a reported collision',
              sorted(idx['C'].get('mapvillage_111x55.png', [])) ==
              ['_dlcbundle/MapVillage/MapVillage_111x55.png', 'sam/MapVillage/MapVillage_111x55.png'], idx['C'])
        check('index: an alias that leaves the scope is reported, not served',
              'other.sam' in idx['C'] and 'other.sam' not in owned, idx['C'])
        for name in ('userdefault.xml', 'ver1_test.dat', 'notes.txt', 'leak.png', 'leak2.png'):
            check(f'index: {name} is never offered', name not in owned)

        # --- reads ---------------------------------------------------------
        code, hdr, body = broker.file('sam/MapVillage/MapVillage.sam')
        check('file: owned path served byte-exact', code == 200 and body == files['sam_sam'], code)
        check('file: digest header is the SHA-256 of the body', hdr.get('x-bfcp-digest') == sha(body), hdr.get('x-bfcp-digest'))
        check('file: epoch and generation headers present',
              hdr.get('x-bfcp-epoch') == epoch and hdr.get('x-bfcp-gen', '').isdigit(), hdr)
        code, _, body = broker.file('unit/cgg/unit_cgg_60956.csv')
        check('file: CSV served raw (no newline translation server-side)', code == 200 and body == files['cgg'])
        code, _, body = broker.file('_dlcbundle/MapVillage/MapVillage.sam')
        check('file: the non-selected copy of an aliased name is not served', code == 404, code)
        code, _, _ = broker.file('sam/MapVillage/MapVillage_111x55.png')
        check('file: a collided name is not served', code == 404, code)
        code, _, _ = broker.file('SAM/MapVillage/MapVillage.sam')
        check('file: exact path only (no case folding of the path)', code == 404, code)
        for attempt in ('../secret.txt', '..\\secret.txt', 'sam/MapVillage/../../../secret.txt',
                        '/sam/MapVillage/MapVillage.sam', 'sam\\MapVillage\\MapVillage.sam',
                        'sam/Junk/link/leak.png', 'sam/Junk/leak2.png', 'sam/MapVillage/UserDefault.xml',
                        'sam/MapVillage/Ver1_test.dat', 'C:/Windows/win.ini', '', 'sam/MapVillage/MapVillage.sam\x00'):
            code, _, body = broker.file(attempt)
            check(f'confinement: {attempt!r} refused', code in (403, 404) and b'LEAK' not in body and b'outside' not in body, code)
        for raw in ('p=%2e%2e%2fsecret.txt', 'p=%252e%252e%252fsecret.txt', 'p=sam%2fJunk%2flink%2fleak.png',
                    'p=..%5csecret.txt'):
            code, _, body = broker.get_raw(raw)
            check(f'confinement: encoded {raw} refused', code in (403, 404) and b'LEAK' not in body, code)

        # --- same-size edit, preserved mtime --------------------------------
        g0 = int(broker.status()['generation'])
        target = content / 'sam' / 'MapVillage' / 'MapVillage_640x208.png'
        st0 = target.stat()
        def flip(d):
            d[-20] ^= 0xFF
        new_bytes = write_same_size(target, flip)
        os.utime(target, ns=(st0.st_atime_ns, st0.st_mtime_ns))
        g1 = broker.wait_generation_above(g0)
        check('same-size edit with a restored mtime bumps the generation', g1 > g0, (g0, g1))
        delta = broker.index(since=g0, epoch=epoch)
        check('delta: only the edited name is listed',
              delta['head'].get('full') == '0' and set(delta['F']) == {'mapvillage_640x208.png'}, sorted(delta['F']))
        check('delta: the edited entry carries the new generation',
              delta['F'].get('mapvillage_640x208.png', {}).get('gen') == g1, delta['F'])
        check('delta: and the new identity',
              delta['F'].get('mapvillage_640x208.png', {}).get('digest') == sha(new_bytes), delta['F'])
        code, hdr, body = broker.file('sam/MapVillage/MapVillage_640x208.png')
        check('file: edited bytes served with a new digest', code == 200 and body == new_bytes and hdr.get('x-bfcp-digest') == sha(new_bytes))
        check('file: edit really was same-size', len(new_bytes) == len(files['p208']) and new_bytes != files['p208'])

        # --- touch without content change -----------------------------------
        broker.file('unit/img/unit_anime_60956.png')                       # digest now known
        g2 = int(broker.status()['generation'])
        anime = content / 'unit' / 'img' / 'unit_anime_60956.png'
        os.utime(anime, None)
        with open(anime, 'r+b') as fh:                                     # rewrite identical bytes
            fh.write(files['anime'])
        time.sleep(1.5)
        g3 = int(broker.status()['generation'])
        check('touch / identical rewrite does not count as a change', g3 == g2, (g2, g3))

        # --- deletion and a new collision become tombstones -----------------
        g4 = int(broker.status()['generation'])
        (content / 'unit' / 'cgs' / 'unit_idle_cgs_60956.csv').unlink()
        g5 = broker.wait_generation_above(g4)
        d = broker.index(since=g4, epoch=epoch)
        check('delete: tombstone listed for the removed owned name', 'unit_idle_cgs_60956.csv' in d['D'], d['D'])
        code, _, _ = broker.file('unit/cgs/unit_idle_cgs_60956.csv')
        check('delete: file no longer served', code == 404, code)
        (content / 'unit' / 'other').mkdir(parents=True, exist_ok=True)
        (content / 'unit' / 'other' / 'unit_cgg_60956.csv').write_bytes(b'different bytes')
        g6 = broker.wait_generation_above(g5)
        d = broker.index(since=g5, epoch=epoch)
        check('new differing copy: owned name becomes a collision',
              'unit_cgg_60956.csv' in d['D'] and 'unit_cgg_60956.csv' in d['C'], (d['D'], d['C']))
        code, _, _ = broker.file('unit/cgg/unit_cgg_60956.csv')
        check('new differing copy: neither copy is served', code == 404, code)

        # --- epoch mismatch forces a full listing ---------------------------
        f = broker.index(since=g6, epoch='not-this-server')
        check('epoch mismatch returns a full listing', f['head'].get('full') == '1' and 'mapvillage.sam' in f['F'])

        # --- watcher overflow -------------------------------------------------
        st_before = broker.status()
        burst = content / 'zz_burst_out_of_scope'
        burst.mkdir()
        sam_path = content / 'sam' / 'MapVillage' / 'MapVillage.sam'
        def burst_writer():
            for i in range(4000):
                (burst / (f'{i:05d}_' + 'x' * 120 + '.bin')).write_bytes(b'x')
        t = threading.Thread(target=burst_writer)
        t.start()
        time.sleep(0.05)
        sam_new = write_same_size(sam_path, lambda b: b.__setitem__(len(b) - 7, b[len(b) - 7] ^ 0x5A))
        t.join()
        deadline = time.time() + 30
        while time.time() < deadline:
            st_after = broker.status()
            if int(st_after['overflows']) > int(st_before['overflows']) or int(st_after['rescans']) > int(st_before['rescans']):
                break
            time.sleep(0.2)
        check('overflow: a burst overflows the watcher and triggers a rescan',
              int(st_after['overflows']) > int(st_before['overflows']) or int(st_after['rescans']) > int(st_before['rescans']),
              (st_before, st_after))
        time.sleep(1.0)
        code, hdr, body = broker.file('sam/MapVillage/MapVillage.sam')
        check('overflow: a same-size edit made during the burst is not lost',
              code == 200 and body == sam_new and hdr.get('x-bfcp-digest') == sha(sam_new), code)
        d = broker.index(since=int(st_before['generation']), epoch=epoch)
        check('overflow: the edited SAM is in the delta after the rescan', 'mapvillage.sam' in d['F'], sorted(d['F']))
        check('overflow: burst files are not offered (out of scope)',
              not any(k.endswith('.bin') for k in broker.index()['F']))

        # --- no torn reads under concurrent writers ---------------------------
        p200 = content / 'sam' / 'MapVillage' / 'MapVillage_320x200.png'
        versions = [files['p200']]
        for k in range(3):
            v = bytearray(files['p200'])
            v[-50] = (v[-50] + 1 + k) & 0xFF
            versions.append(bytes(v))
        valid = {sha(v) for v in versions}
        stop = threading.Event()
        def rewriter():
            i = 0
            while not stop.is_set():
                tmp = p200.with_suffix('.tmp_write')
                tmp.write_bytes(versions[i % len(versions)])
                for _ in range(50):
                    try:
                        os.replace(tmp, p200)
                        break
                    except PermissionError:
                        time.sleep(0.005)
                i += 1
        results = []
        def reader():
            for _ in range(60):
                results.append(broker.file('sam/MapVillage/MapVillage_320x200.png'))
        wt = threading.Thread(target=rewriter)
        wt.start()
        readers = [threading.Thread(target=reader) for _ in range(4)]
        for r in readers:
            r.start()
        for r in readers:
            r.join()
        stop.set()
        wt.join()
        ok200 = [(h, b) for c, h, b in results if c == 200]
        torn = [1 for h, b in ok200 if sha(b) != h.get('x-bfcp-digest') or sha(b) not in valid]
        other = [c for c, h, b in results if c not in (200, 503)]
        check('concurrency: every served body matches its digest and is a complete version',
              ok200 and not torn, (len(ok200), len(torn)))
        check('concurrency: failures are only bounded 503s', not other, other[:5])
        for leftover in p200.parent.glob('*.tmp_write'):
            leftover.unlink()

        # --- a writer holding the file open ---------------------------------
        with open(content / 'unit' / 'img' / 'unit_anime_60956.png', 'r+b'):
            t0 = time.time()
            code, hdr, _ = broker.file('unit/img/unit_anime_60956.png')
            elapsed = time.time() - t0
        check('busy: a file held open for writing answers 503 busy',
              code == 503 and hdr.get('x-bfcp-error') == 'busy', (code, hdr.get('x-bfcp-error')))
        check('busy: the refusal is bounded in time', elapsed < 2.0, elapsed)

        # --- the replace window: a delete-pending file ------------------------
        # While a file is being replaced the old one is DELETE-PENDING for a
        # moment, and opening it then fails with ERROR_ACCESS_DENIED -- which the
        # broker answered as 404 "missing", the intermittent failure of the
        # concurrency case above.  Reproduced deterministically here: a handle
        # marks the file for deletion (classic disposition) and is held.
        ok_path, ok_rel = content / 'sam' / 'Junk' / 'ok.png', 'sam/Junk/ok.png'

        def put_back():
            tmp = ok_path.with_suffix('.png.tmp')
            tmp.write_bytes(files['ok'])
            os.replace(tmp, ok_path)

        h = delete_pending(ok_path)
        result = {}
        reading = threading.Thread(target=lambda: result.update(r=broker.file(ok_rel)))
        reading.start()
        time.sleep(0.12)
        close_handle(h)                # the old file goes ...
        put_back()                     # ... and the new one arrives, as a replace does
        reading.join(10)
        code, hdr, body = result.get('r', (None, {}, b''))
        check('replace window: a read that meets a delete-pending file is served the new one',
              code == 200 and body == files['ok'], (code, hdr.get('x-bfcp-error')))

        h = delete_pending(ok_path)
        t0 = time.time()
        code, hdr, _ = broker.file(ok_rel)
        elapsed = time.time() - t0
        close_handle(h)
        put_back()
        check('replace window: a file still delete-pending after the retry window is busy (503), not missing',
              code == 503 and hdr.get('x-bfcp-error') == 'busy', (code, hdr.get('x-bfcp-error')))
        check('replace window: that refusal is bounded in time', elapsed < 2.0, elapsed)

        os.remove(ok_path)
        code, hdr, _ = broker.file(ok_rel)
        put_back()
        check('replace window: a file really removed is still a 404 (missing, or not-owned once the watcher saw it)',
              code == 404 and hdr.get('x-bfcp-error') in ('missing', 'not-owned'), (code, hdr.get('x-bfcp-error')))
        deadline = time.time() + 15
        while (back := broker.file(ok_rel))[0] != 200 and time.time() < deadline:
            time.sleep(0.1)
        check('replace window: once put back, the file is served again', back[0] == 200 and back[2] == files['ok'],
              (back[0], back[1].get('x-bfcp-error')))

        # The watcher meets the same window: examining a delete-pending name
        # failed its existence check, which withdrew the name (404 not-owned)
        # until the next quiet period -- under a writer that keeps replacing,
        # for the rest of the burst (the concurrency case above, again
        # intermittently).  A notification is raised for the held file through
        # its own handle, the watcher is given time to examine it, and the
        # deletion is then taken back.
        offered = broker.index()['F'].get('ok.png')
        h = delete_pending(ok_path)
        nudge_write_time(h, 10_000_000)
        time.sleep(1.0)                                # several debounce periods
        during = broker.index()['F'].get('ok.png')
        code, hdr, _ = broker.file(ok_rel)
        nudge_write_time(h, -10_000_000)
        cancel_delete(h)
        close_handle(h)
        check('watcher: a name examined while delete-pending stays offered, its identity unchanged',
              offered is not None and during is not None and during['digest'] == offered['digest'],
              (offered and offered['digest'][:16], during))
        check('watcher: meanwhile a read of it is busy (503), not missing or not-owned',
              code == 503 and hdr.get('x-bfcp-error') == 'busy', (code, hdr.get('x-bfcp-error')))
        deadline = time.time() + 15
        while (back := broker.file(ok_rel))[0] != 200 and time.time() < deadline:
            time.sleep(0.1)
        check('watcher: once the deletion is taken back, the file is served again',
              back[0] == 200 and back[2] == files['ok'], (back[0], back[1].get('x-bfcp-error')))

        # --- bundles are all or nothing ---------------------------------------
        idx = broker.index()
        members = {'unit_anime_99999.png', 'unit_cgg_99999.csv', 'unit_ills_battle_99999.png'}
        check('bundle: an incomplete bundle is not offered at all', not (members & set(idx['F'])), sorted(idx['F']))
        st = broker.status()
        check('bundle: its members are reported unavailable', int(st.get('unavailable', 0)) == 3, st)
        code, _, _ = broker.file('unit/img/unit_anime_99999.png')
        check('bundle: a present member of an incomplete bundle is not served', code == 404, code)
        gb = int(st['generation'])
        portrait = content / 'unit' / 'img' / 'unit_ills_battle_99999.png'
        portrait.write_bytes(b'\x89PNG\r\n\x1a\nportrait99999')

        def wait_index(pred, since, timeout=15):
            deadline = time.time() + timeout
            while True:
                d = broker.index(since=since, epoch=epoch)
                if pred(d) or time.time() > deadline:
                    return d
                time.sleep(0.1)

        d = wait_index(lambda d: members <= set(d['F']), gb)
        gens = {d['F'][m]['gen'] for m in members if m in d['F']}
        check('bundle: completing it offers every member in the delta, in one generation',
              members <= set(d['F']) and len(gens) == 1 and gens.pop() > gb, sorted(d['F']))
        check('bundle: nothing is reported unavailable once complete', int(broker.status().get('unavailable', -1)) == 0)
        code, _, body = broker.file('unit/img/unit_anime_99999.png')
        check('bundle: members are served once complete', code == 200 and body.endswith(b'sprite99999'), code)
        gc = int(d['head']['generation'])
        portrait.unlink()
        d = wait_index(lambda d: members <= set(d['D']), gc)
        check('bundle: losing a member withdraws the whole bundle (tombstones)',
              members <= set(d['D']) and not (members & set(d['F'])), (d['D'], sorted(d['F'])))
        code, _, _ = broker.file('unit/cgg/unit_cgg_99999.csv')
        check('bundle: the frame layout is not served without its portrait', code == 404, code)

    # --- dormant and rejected policies --------------------------------------
    def run_with_policy(policy_text: str, label: str):
        (fx_info['system'] / 'content_provider.json').write_text(policy_text)
        fx2 = Fixture.create(work / ('server_' + label + server_dir.name[6:]), port=PORT, fresh=True,
                             document_root=work / 'tree' / 'game_content', system_root=fx_info['system'])
        with IsolatedServer(exe, fx2, log_name='server.log'):
            st = broker.status()
            code_i, _, _ = broker.get('/index')
            code_f, _, _ = broker.file('sam/MapVillage/MapVillage.sam')
            return st, code_i, code_f

    disabled = dict(POLICY, enabled=False)
    st, ci, cf = run_with_policy(json.dumps(disabled), 'disabled')
    check('dormant: enabled false keeps the broker off', st.get('enabled') == '0' and ci == 503 and cf == 503, (st, ci, cf))
    st, ci, cf = run_with_policy(json.dumps(dict(POLICY, typo=1)), 'unknown_key')
    check('rejected: an unknown policy key is an error, not ignored',
          st.get('enabled') == '0' and 'unknown key' in st.get('reason', '') and cf == 503, st)
    bad = dict(POLICY, aliases={'MapVillage.sam': 'sam/Other/Other.sam'})
    st, ci, cf = run_with_policy(json.dumps(bad), 'bad_alias')
    check('rejected: an alias that renames a file is refused',
          st.get('enabled') == '0' and 'same basename' in st.get('reason', ''), st)
    # A separate broker root (a temporary fixture root beside the real document root).
    alt = work / 'alt_root'
    shutil.rmtree(alt, ignore_errors=True)
    (alt / 'sam' / 'MapVillage').mkdir(parents=True)
    alt_sam = b'MAS.' + b'\x00' * 12 + b'fixture root copy'
    (alt / 'sam' / 'MapVillage' / 'MapVillage.sam').write_bytes(alt_sam)
    rooted = dict(POLICY, root=str(alt), aliases={})
    (fx_info['system'] / 'content_provider.json').write_text(json.dumps(rooted))
    fx4 = Fixture.create(work / ('server_root' + server_dir.name[6:]), port=PORT, fresh=True,
                         document_root=work / 'tree' / 'game_content', system_root=fx_info['system'])
    with IsolatedServer(exe, fx4, log_name='server.log'):
        broker.wait_ready()
        idx = broker.index()
        code, _, body = broker.file('sam/MapVillage/MapVillage.sam')
    check('root: a configured fixture root replaces <document_root>/content',
          set(idx['F']) == {'mapvillage.sam'} and code == 200 and body == alt_sam, (sorted(idx['F']), code))

    # --- bundles that share a member (Session 13) ---------------------------------
    # Generated unit families share frame layouts between units (58 names of the
    # C1 policy are in several bundles: elemental variants drawn with one CGG).
    # When the bundle that sorts LATER is incomplete, the shared layout is
    # withdrawn with it, so the bundle that sorts earlier has lost a member too
    # and must be withdrawn as well -- not served as a sprite without its layout.
    shared = work / 'shared_root'
    shutil.rmtree(shared, ignore_errors=True)
    (shared / 'unit' / 'img').mkdir(parents=True)
    (shared / 'unit' / 'cgg').mkdir(parents=True)
    (shared / 'unit' / 'img' / 'unit_anime_10202.png').write_bytes(b'\x89PNG\r\n\x1a\nsprite10202')
    (shared / 'unit' / 'img' / 'unit_anime_60132.png').write_bytes(b'\x89PNG\r\n\x1a\nsprite60132')
    (shared / 'unit' / 'cgg' / 'unit_cgg_60132.csv').write_bytes(b'shared,layout\r\n')
    early = ['unit_anime_10202.png', 'unit_cgg_60132.csv']
    late = ['unit_anime_60132.png', 'unit_cgg_60132.csv', 'unit_ills_battle_60132.png']   # no portrait yet
    shared_policy = dict(POLICY, root=str(shared), scope=['unit/**'], aliases={},
                         bundles={'unit_10202': early, 'unit_60132': late})
    (fx_info['system'] / 'content_provider.json').write_text(json.dumps(shared_policy))
    fx5 = Fixture.create(work / ('server_shared' + server_dir.name[6:]), port=PORT, fresh=True,
                         document_root=work / 'tree' / 'game_content', system_root=fx_info['system'])
    with IsolatedServer(exe, fx5, log_name='server.log'):
        broker.wait_ready()
        idx = broker.index()
        st = broker.status()
        code_early, _, _ = broker.file('unit/img/unit_anime_10202.png')
        check('shared bundles: the bundle sorting after its partner is incomplete -> neither is offered',
              not ({m.lower() for m in early + late} & set(idx['F'])), sorted(idx['F']))
        check('shared bundles: the earlier bundle\'s sprite is not served without the shared layout',
              code_early == 404, code_early)
        check('shared bundles: every member of both is reported unavailable (the missing portrait included)',
              int(st.get('unavailable', 0)) == len({m.lower() for m in early + late}), st)
        g0 = int(st['generation'])
        (shared / 'unit' / 'img' / 'unit_ills_battle_60132.png').write_bytes(b'\x89PNG\r\n\x1a\nportrait60132')
        deadline = time.time() + 15
        while True:
            d = broker.index(since=g0, epoch=st['epoch'])
            if {m.lower() for m in early + late} <= set(d['F']) or time.time() > deadline:
                break
            time.sleep(0.1)
        check('shared bundles: completing the later one offers both bundles again',
              {m.lower() for m in early + late} <= set(d['F']) and int(broker.status().get('unavailable', -1)) == 0,
              sorted(d['F']))

    (fx_info['system'] / 'content_provider.json').unlink()
    fx3 = Fixture.create(work / ('server_absent' + server_dir.name[6:]), port=PORT, fresh=True,
                         document_root=work / 'tree' / 'game_content', system_root=fx_info['system'])
    with IsolatedServer(exe, fx3, log_name='server.log'):
        st = broker.status()
    check('dormant: no policy file means no broker', st.get('enabled') == '0' and 'absent' in st.get('reason', ''), st)
    return check.summary('test_content_broker_wire')


if __name__ == '__main__':
    raise SystemExit(main())
