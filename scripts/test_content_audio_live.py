"""The audio reader against a REAL broker: owned sounds through the proxy's hooks, byte-exact.

    python scripts/test_content_audio_live.py SERVER_EXE PROXY_TESTS AUDIO_POLICY

AUDIO_POLICY: a sounds-only policy from scripts/gen_audio_policy.py (no --base).
Starts an isolated server (fresh save, port 19964) whose broker serves a
DISPOSABLE root -- a hard-link mirror of exactly the policy's sounds under
out/qa/current/content_audio_live/root, two of them withdrawn (their links
moved out, so their bundles are incomplete) -- and runs content_provider_tests
with BF_AUDIO_PORT / BF_AUDIO_ROOT / BF_AUDIO_EXPECT / BF_AUDIO_LS.  Its
TestLiveAudio drives the real hook functions (Hook_FopenS / Hook_Fread /
Hook_Fclose) with the real CRT behind them and the real WinINet broker client,
exactly as the audio reader x86 0xEC0030 calls them, against BF_AUDIO_LS: an
empty disposable LocalState that holds only planted stale copies.

  active   every present sound is served byte-exact (cold), five warm repeats
           come from RAM, the two withdrawn sounds fail with ENOENT while their
           stale copies are never opened, and nothing is written;
  dormant  (the same server restarted with the policy switched off) every
           planted stale copy is what the CRT reads, an uncached sound is
           missing -- the game without the proxy.

deploy/ is only read: the mirror's links are moved, never the originals; the
save is fresh.  The disposable LocalState must end exactly as planted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Fixture, IsolatedServer, qa_dir  # noqa: E402

PORT = 19964
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'


def snapshot(folder: Path) -> dict:
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(folder.iterdir()) if p.is_file()}


def run_tests(proxy_tests: Path, server_exe: Path, fx: Fixture, env: dict, work: Path, label: str):
    with IsolatedServer(server_exe, fx, log_name=f'server_{label}.log'):
        run = subprocess.run([str(proxy_tests)], env=env, capture_output=True, text=True, timeout=1800)
    out = run.stdout + run.stderr
    (work / f'content_provider_tests_{label}.log').write_text(out, encoding='utf-8')
    return run.returncode, out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('server_exe', type=Path)
    ap.add_argument('proxy_tests', type=Path)
    ap.add_argument('policy', type=Path)
    args = ap.parse_args()
    check = Checker()
    work = qa_dir('content_audio_live')
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    policy = json.loads(args.policy.read_text(encoding='utf-8'))
    bundles = {b: ms for b, ms in policy['bundles'].items() if b.startswith('snd_')}
    check('the policy is sounds only (gen_audio_policy.py without --base)', bool(bundles)
          and len(bundles) == len(policy['bundles']) and policy.get('root') in (None, ''), len(policy['bundles']))
    rels = {Path(p).name.lower(): p for p in policy['scope']}
    names = sorted(m for ms in bundles.values() for m in ms)

    # The disposable root: hard links to game_content's sounds; two withdrawn.
    root = work / 'root' / 'content'
    parked = work / 'parked'
    for n in names:
        dst = root / rels[n.lower()]
        dst.parent.mkdir(parents=True, exist_ok=True)
        os.link(CONTENT / rels[n.lower()], dst)
    gate_sounds = ['bf253_se_gousei02.mp3', 'bf1126_zin_act_union.mp3', 'bf210_se_water2.mp3',
                   'bf236_se_debility.mp3', 'bf110_zin_act_gatya021.mp3']
    withdrawn = ['bf236_se_debility.mp3', 'bf001_title.mp3']
    parked.mkdir()
    for n in withdrawn:
        os.rename(root / rels[n.lower()], parked / n)
    originals = {n: hashlib.sha256((CONTENT / rels[n.lower()]).read_bytes()).hexdigest() for n in gate_sounds + withdrawn}
    present = [n for n in names if n not in withdrawn]
    # present[0] must stay uncached (the dormant case reads it as missing): gate sounds first, uncached.
    order = [n for n in gate_sounds if n in present] + [n for n in present if n not in gate_sounds]

    # The disposable LocalState: empty but for planted stale copies.
    ls = work / 'localstate'
    ls.mkdir()
    stale = withdrawn + ['bf253_se_gousei02.mp3'.replace('bf253', 'bf253'), 'bf110_zin_act_gatya021.mp3']
    stale = [n for n in dict.fromkeys(stale) if n != order[0]]
    for n in stale:
        (ls / n).write_bytes(f'STALE CACHED COPY {n}'.encode())
    planted = snapshot(ls)

    def expect(mode: str) -> Path:
        path = work / f'expect_{mode}.txt'
        path.write_text(f'mode {mode}\nowned {len(names)}\n'
                        + ''.join(f'present {n} {rels[n.lower()].replace("/", chr(92))}\n' for n in order)
                        + ''.join(f'absent {n}\n' for n in withdrawn)
                        + ''.join(f'stale {n}\n' for n in stale), encoding='utf-8')
        return path

    system = work / 'system'
    shutil.copytree(ROOT / 'deploy' / 'system', system)
    base_env = dict(os.environ, BF_AUDIO_PORT=str(PORT), BF_AUDIO_ROOT=str(root), BF_AUDIO_LS=str(ls),
                    BF_APPX_DIR=str(ROOT.parent / 'BraveFrontierAppxClient'))
    for k in ('BF_BROKER_PORT', 'BF_BROKER_ROOT', 'BF_FAMILY_PORT', 'BF_SWAP_PORT'):
        base_env.pop(k, None)
    results = {}
    for mode, enabled in (('active', True), ('dormant', False)):
        (system / 'content_provider.json').write_text(json.dumps(dict(policy, enabled=enabled, root=str(root)), indent=1),
                                                      encoding='utf-8')
        fx = Fixture.create(work / f'server_{mode}', port=PORT, fresh=True,
                            document_root=ROOT / 'deploy' / 'game_content', system_root=system)
        code, out = run_tests(args.proxy_tests, args.server_exe, fx, dict(base_env, BF_AUDIO_EXPECT=str(expect(mode))),
                              work, mode)
        results[mode] = (code, out, fx)
        print('\n'.join(l for l in out.splitlines() if 'audio' in l.lower() or 'passed' in l or l.startswith('FAIL'))[-2500:])
        m = re.search(r'content_provider_tests: (\d+) passed, (\d+) failed, (\d+) skipped', out)
        check(f'{mode}: proxy tests completed without failures', m is not None and m.group(2) == '0' and code == 0,
              m.group(0) if m else out[-300:])
        check(f'{mode}: the audio case ran (not skipped)', 'SKIP  audio reader integration' not in out)
    out = results['active'][1]
    check(f'active: all {len(present)} present sounds served byte-exact to the reader contract',
          f'live audio: {len(present)} sound(s) served byte-exact' in out, re.findall(r'live audio: \d+ sound[^\n]*', out))
    check('active: warm repeats from RAM', 'live audio: 5 warm repeat(s) from RAM' in out)
    check(f'active: the {len(withdrawn)} withdrawn sounds reported missing, stale copies untouched',
          f'live audio: {len(withdrawn)} withdrawn sound(s) reported missing' in out)
    out = results['dormant'][1]
    check(f'dormant: the {len(stale)} planted cached copies read by the CRT, uncached sounds missing',
          f'live audio: dormant broker -> {len(stale)} cached cop(ies) read by the CRT' in out,
          re.findall(r'live audio: dormant[^\n]*', out))
    log = (results['active'][2].dir / 'server_active.log').read_text(encoding='utf-8', errors='replace')
    served = {p.rsplit('/', 1)[-1].lower() for p in re.findall(r'content broker: served (\S+)', log)}
    check('active: the server log shows the broker serving the present sounds',
          {n.lower() for n in present} <= served, sorted({n.lower() for n in present} - served)[:5])
    check('the disposable LocalState is exactly as planted (nothing written, nothing removed)', snapshot(ls) == planted,
          sorted(set(snapshot(ls)) ^ set(planted)))
    check('game_content is untouched (the parked links were moved, not the originals)',
          all(hashlib.sha256((CONTENT / rels[n.lower()]).read_bytes()).hexdigest() == d for n, d in originals.items()))
    return check.summary('test_content_audio_live')


if __name__ == '__main__':
    raise SystemExit(main())
