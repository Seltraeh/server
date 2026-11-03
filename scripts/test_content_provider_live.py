"""Proxy content provider against a REAL broker: C++ client <-> C++ server.

Starts an isolated server (fresh save, port 19960) whose broker reads a
temporary fixture root holding copies of the 21 owned files (shipped policy,
switched on, root redirected), then runs the offline-proxy's
content_provider_tests.exe with BF_BROKER_PORT / BF_BROKER_ROOT so its live
case uses the DLL's own WinINet client and provider code over loopback:
byte-exact reads, the CRT text-mode CSV path, a same-size SAM edit adopted only
at the safe point, and a Zeal bundle withdrawn and restored.  The fixture is
edited, never deploy/game_content.

    python scripts/test_content_provider_live.py SERVER_EXE PROXY_TEST_EXE
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Fixture, IsolatedServer, qa_dir  # noqa: E402
from content_provider_client_test import CONTENT, load_policy, owned_targets  # noqa: E402

PORT = 19960


def main() -> int:
    server_exe, proxy_tests = Path(sys.argv[1]), Path(sys.argv[2])
    check = Checker()
    work = qa_dir('content_provider_live')
    root = work / 'broker_root'
    if root.exists():
        shutil.rmtree(root)
    for name, rel in owned_targets().items():
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(CONTENT / rel, root / rel)
    system = work / 'system'
    if system.exists():
        shutil.rmtree(system)
    shutil.copytree(ROOT / 'deploy' / 'system', system)
    policy = load_policy()
    policy['enabled'] = True
    policy['root'] = str(root)
    (system / 'content_provider.json').write_text(json.dumps(policy, indent=1), encoding='utf-8')
    fx = Fixture.create(work / 'server', port=PORT, fresh=True,
                        document_root=ROOT / 'deploy' / 'game_content', system_root=system)
    env = dict(os.environ, BF_BROKER_PORT=str(PORT), BF_BROKER_ROOT=str(root),
               BF_APPX_DIR=str(ROOT.parent / 'BraveFrontierAppxClient'))
    with IsolatedServer(server_exe, fx, log_name='server.log'):
        run = subprocess.run([str(proxy_tests)], env=env, capture_output=True, text=True, timeout=600)
    out = run.stdout + run.stderr
    (work / 'content_provider_tests.log').write_text(out, encoding='utf-8')
    print(out.strip())
    m = re.search(r'content_provider_tests: (\d+) passed, (\d+) failed, (\d+) skipped', out)
    check('proxy tests completed', m is not None, out[-300:])
    check('proxy tests: no failures', m and m.group(2) == '0' and run.returncode == 0, m and m.group(0))
    check('live broker case ran (not skipped)', 'SKIP  live broker' not in out)
    log = (fx.dir / 'server.log').read_text(encoding='utf-8', errors='replace')
    served = re.findall(r'content broker: served (\S+)', log)
    check('the server log shows the provider fetching owned files', len(served) >= 7, served[:10])
    check('the same-size edit reached the broker as a generation bump', 'content broker: generation' in log)
    return check.summary('test_content_provider_live')


if __name__ == '__main__':
    raise SystemExit(main())
