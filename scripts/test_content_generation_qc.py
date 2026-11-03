"""QC regression: a read racing the watcher must not swallow invalidation.

Uses a fresh isolated save and one tiny fixture, never the live content/save.
Run: python scripts/test_content_generation_qc.py SERVER_EXE

Added with the repair: the generation sent with the racing read, no second
bump when the watcher flushes, the identity published in the index,
version-bound reads (409 / 200 / 400), an identical rewrite with a restored
timestamp, and concurrent readers while same-size edits with restored
timestamps land (one identity per generation, no stale metadata left).
"""
from pathlib import Path
import json
import os
import shutil
import sys
import threading
import time

from bf_testkit import ROOT, Checker, Fixture, IsolatedServer, qa_dir
from test_content_broker_wire import Broker, sha


def replace_keeping_mtime(path: Path, data: bytes, stamp) -> None:
    """Same-size edit with the old timestamp restored, via an atomic replace
    (retried while the broker holds the file open for a read)."""
    tmp = path.with_suffix('.tmp_write')
    tmp.write_bytes(data)
    os.utime(tmp, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
    for _ in range(200):
        try:
            os.replace(tmp, path)
            return
        except PermissionError:
            time.sleep(0.005)
    raise RuntimeError('could not replace ' + str(path))


def main():
    work = qa_dir('session7') / 'qc_generation'
    root = work / 'content'
    root.mkdir(parents=True, exist_ok=True)
    asset = root / 'test.png'
    asset.write_bytes(b'old-bytes')
    for leftover in root.glob('*.tmp_write'):
        leftover.unlink()
    system = work / 'system'
    shutil.copytree(ROOT / 'deploy/system', system, dirs_exist_ok=True)
    policy = {'enabled': True, 'root': str(root), 'scope': ['test.png'],
              'extensions': ['.png'], 'exclude': [], 'aliases': {}, 'bundles': {}}
    (system / 'content_provider.json').write_text(json.dumps(policy), encoding='utf-8')
    fx = Fixture.create(work / 'server', port=19960, fresh=True, system_root=system)
    check = Checker()
    with IsolatedServer(Path(sys.argv[1]), fx):
        b = Broker(19960)
        check('broker ready', b.wait_ready().get('ready') == '1')
        check('initial bytes served', b.file('test.png')[2] == b'old-bytes')
        time.sleep(0.4)
        generation = int(b.status()['generation'])
        stamp = asset.stat()
        asset.write_bytes(b'new-bytes')
        os.utime(asset, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        code, headers, data = b.file('test.png')
        check('immediate read gets complete new bytes', code == 200 and data == b'new-bytes')
        after = b.wait_generation_above(generation, timeout=2)
        check('read before watcher does not swallow the generation bump', after > generation,
              f'before={generation}, response={headers.get("x-bfcp-gen")}, after={after}')

        # --- added with the repair ------------------------------------------
        old_id, new_id = sha(b'old-bytes'), sha(b'new-bytes')
        check('the racing read carries the generation it published, with its identity',
              headers.get('x-bfcp-gen') == str(after) and headers.get('x-bfcp-digest') == new_id, headers)
        time.sleep(0.8)   # past the watcher's 200 ms debounce
        settled = int(b.status()['generation'])
        check('the watcher flush that follows does not bump it a second time', settled == after, (after, settled))
        idx = b.index()
        check('the index publishes the new identity at that generation',
              idx['F'].get('test.png', {}).get('digest') == new_id and idx['F'].get('test.png', {}).get('gen') == after,
              idx['F'])

        code, h, body = b.get('/file', {'p': 'test.png', 'd': old_id})
        check('a read bound to the replaced version is refused, not answered with other bytes',
              code == 409 and h.get('x-bfcp-error') == 'version-unavailable' and b'new-bytes' not in body,
              (code, h.get('x-bfcp-error')))
        check('the refusal names the version the file has now',
              h.get('x-bfcp-digest') == new_id and h.get('x-bfcp-gen') == str(after), h)
        code, h, body = b.get('/file', {'p': 'test.png', 'd': new_id})
        check('a read bound to the current version is served',
              code == 200 and body == b'new-bytes' and h.get('x-bfcp-digest') == new_id, code)
        code, h, _ = b.get('/file', {'p': 'test.png', 'd': 'sha256:NOT-HEX'})
        check('a malformed identity is a bad request', code == 400 and h.get('x-bfcp-error') == 'bad-digest',
              (code, h.get('x-bfcp-error')))

        # Identical rewrite, timestamp restored, read before the watcher: no change.
        g = int(b.status()['generation'])
        stamp = asset.stat()
        asset.write_bytes(b'new-bytes')
        os.utime(asset, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        code, h, data = b.file('test.png')
        time.sleep(0.8)
        check('an identical rewrite read before the watcher is not a change',
              code == 200 and data == b'new-bytes' and h.get('x-bfcp-gen') == str(g)
              and int(b.status()['generation']) == g, (code, h.get('x-bfcp-gen'), g, b.status()['generation']))

        # Concurrent readers while same-size edits with restored timestamps land.
        versions = [b'version-A', b'version-B', b'version-C']   # 9 bytes, like the rest
        stop = threading.Event()
        written = []

        def writer():
            i = 0
            while not stop.is_set():
                stamp = asset.stat()
                data = versions[i % len(versions)]
                replace_keeping_mtime(asset, data, stamp)
                written.append(data)
                i += 1
                time.sleep(0.01)

        results = []
        lock = threading.Lock()

        def reader():
            for _ in range(40):
                r = b.file('test.png')
                with lock:
                    results.append(r)

        wt = threading.Thread(target=writer)
        wt.start()
        readers = [threading.Thread(target=reader) for _ in range(4)]
        for r in readers:
            r.start()
        for r in readers:
            r.join()
        stop.set()
        wt.join()
        ok = [(h, body) for c, h, body in results if c == 200]
        valid = {sha(v) for v in versions} | {new_id}
        check('concurrency: every served body matches its identity and is a complete version',
              ok and all(sha(body) == h.get('x-bfcp-digest') and sha(body) in valid for h, body in ok),
              (len(ok), len(results)))
        by_gen = {}
        for h, body in ok:
            by_gen.setdefault(h.get('x-bfcp-gen'), set()).add(h.get('x-bfcp-digest'))
        check('concurrency: one identity per generation (no response carries the wrong generation)',
              all(len(ids) == 1 for ids in by_gen.values()), {g: len(ids) for g, ids in by_gen.items()})
        check('concurrency: failures are only bounded 503s', all(c in (200, 503) for c, _, _ in results),
              sorted({c for c, _, _ in results}))
        final = asset.read_bytes()
        deadline = time.time() + 5
        while time.time() < deadline and b.index()['F'].get('test.png', {}).get('digest') != sha(final):
            time.sleep(0.1)
        idx = b.index()
        check('concurrency: the published identity ends at the file\'s final bytes (nothing stale left)',
              idx['F'].get('test.png', {}).get('digest') == sha(final), (idx['F'].get('test.png'), final))
        code, h, body = b.file('test.png')
        check('concurrency: and a read afterwards serves exactly that version',
              code == 200 and body == final and h.get('x-bfcp-gen') == str(idx['F'].get('test.png', {}).get('gen')),
              (code, h.get('x-bfcp-gen')))
    return check.summary('test_content_generation_qc')


if __name__ == '__main__':
    raise SystemExit(main())
