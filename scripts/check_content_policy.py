"""Dry-run the shipped content provider policy against a real content tree.

Starts an isolated server (fresh save, port 19960) whose system_root holds a
copy of deploy/system with content_provider.json switched on, and the
document_root under test (default deploy/game_content, read-only).  Reports
what the broker would serve -- owned names, collisions, unavailable bundle
members -- and fails when the policy does not own exactly the names its
bundles declare.  Nothing in the content tree or deploy/ is modified.

    python scripts/check_content_policy.py out/qa/current/bin/gimuserverw.exe [document_root]
                                           [--policy GENERATED.json]

--policy checks a generated family policy (scripts/gen_unit_family_policy.py)
instead of the shipped one, with the same judgements; its owned names are
summarised by family rather than listed one per line.
"""
from __future__ import annotations

import hashlib
import json
import re
import shutil
import sys
import time
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Fixture, IsolatedServer, qa_dir  # noqa: E402

PORT = 19960


def load_policy(path: Path) -> dict:
    # jsoncpp accepts // comments; strip whole-line comments for Python.
    text = '\n'.join(line for line in path.read_text(encoding='utf-8').splitlines()
                     if not line.lstrip().startswith('//'))
    return json.loads(text)


def get(path: str) -> str:
    with urllib.request.urlopen(f'http://127.0.0.1:{PORT}/offline_mod/content/v1{path}', timeout=30) as r:
        return r.read().decode()


def main() -> int:
    argv = sys.argv[1:]
    generated = None
    if '--policy' in argv:
        i = argv.index('--policy')
        generated = Path(argv[i + 1])
        del argv[i:i + 2]
    exe = Path(argv[0] if argv else ROOT / 'out/qa/current/bin/gimuserverw.exe')
    document_root = Path(argv[1]) if len(argv) > 1 else ROOT / 'deploy' / 'game_content'
    check = Checker()
    work = qa_dir('content_policy' if generated is None else 'content_policy_generated')
    system = work / 'system'
    if system.exists():
        shutil.rmtree(system)
    shutil.copytree(ROOT / 'deploy' / 'system', system)
    if generated is None:
        policy = load_policy(ROOT / 'deploy' / 'system' / 'content_provider.json')
        # The rollout switch is reported, not judged: the tracked file ships it off,
        # and an activated installation legitimately has it on.
        print(f"rollout switch in deploy/system/content_provider.json: enabled={policy.get('enabled')}")
    else:
        policy = load_policy(generated)
        print(f'generated policy {generated}: {len(policy["scope"])} scope entries, {len(policy["bundles"])} bundles')
    policy['enabled'] = True
    (system / 'content_provider.json').write_text(json.dumps(policy, indent=1), encoding='utf-8')

    fx = Fixture.create(work / 'server', port=PORT, fresh=True, document_root=document_root, system_root=system)
    with IsolatedServer(exe, fx, log_name='server.log'):
        deadline = time.time() + 120
        status = {}
        while time.time() < deadline:
            status = dict(line.split(' ', 1) for line in get('/status').splitlines() if ' ' in line)
            if status.get('ready') == '1':
                break
            time.sleep(0.5)
        check('broker ready on the real tree', status.get('ready') == '1', status)
        index = get('/index?since=0&epoch=').splitlines()
    owned = {}
    identities = {}
    collisions, unavailable = {}, {}
    for line in index:
        parts = line.split('\t')
        if parts[0] == 'F':
            owned[parts[3].lower()] = parts[4]
            identities[parts[3].lower()] = parts[5] if len(parts) > 5 else ''
        elif parts[0] == 'C':
            collisions[parts[1]] = parts[2:]
        elif parts[0] == 'X':
            unavailable[parts[2]] = parts[3]
    declared = {m.lower() for members in policy.get('bundles', {}).values() for m in members}
    print(f'files {status.get("files")}, owned {len(owned)}, collisions {len(collisions)}, unavailable {len(unavailable)}')
    if generated is None:
        for name in sorted(owned):
            print(f'  F {name:32s} <- {owned[name]}')
    else:
        by_folder = {}
        for rel in owned.values():
            folder = rel.rsplit('/', 1)[0] if '/' in rel else '.'
            folder = folder.split('/')[0] if folder.startswith('sam/') else folder
            by_folder[folder] = by_folder.get(folder, 0) + 1
        print('  owned by folder: ' + ', '.join(f'{k} {v}' for k, v in sorted(by_folder.items())))
    for name, why in sorted(unavailable.items()):
        print(f'  X {name:32s} {why}')
    for name, rels in sorted(collisions.items()):
        print(f'  C {name:32s} {" | ".join(rels)}')
    check('every declared bundle member is served', declared <= set(owned), sorted(declared - set(owned)))
    check('nothing outside the bundles is served', set(owned) <= declared, sorted(set(owned) - declared))
    check('no bundle is incomplete', not unavailable, unavailable)
    check('no collision is left unresolved in scope', not collisions, sorted(collisions))
    check('aliases select the copies the player-confirmed Town used',
          owned.get('mapvillage.sam') == 'sam/MapVillage/MapVillage.sam'
          and owned.get('mapvillage_320x289.jpg') == '_dlcbundle/MapVillage/MapVillage_320x289.jpg'
          and owned.get('mapvillage_640x208.png') == 'sam/MapVillage/MapVillage_640x208.png', owned)
    check('no client state name is served', not any(re.search(r'\.(dat|xml|ini|tmp|json|zip)$', n) for n in owned))
    check('every served name is published with the SHA-256 of the copy it serves',
          owned and all(identities[n] == 'sha256:' + hashlib.sha256((document_root / 'content' / rel).read_bytes()).hexdigest()
                        for n, rel in owned.items()),
          {n: identities[n][:15] for n in owned})
    return check.summary('check_content_policy')


if __name__ == '__main__':
    raise SystemExit(main())
