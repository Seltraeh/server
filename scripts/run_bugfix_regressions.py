"""Run the regression suites, each with a fresh disposable save and its own server.

    python scripts/run_bugfix_regressions.py PATH_TO_DEBUG_EXE [--suite NAME ...] [--list]

Every suite runs sequentially against a SQLite-backup copy of deploy/gme.sqlite
(the live save is only ever opened read-only) on its own loopback port, and keeps
its fixture, config and server log in out/qa/current/<suite>/, recreated on each
run.  The suite's own output goes to out/qa/current/<suite>.log, and one compact
summary of the whole run -- source revisions, the executable's identity, every
command and every result -- replaces out/qa/current/SUMMARY.md and SUMMARY.json.

A suite PASSES only on positive evidence that it ran to the end (see classify):
its exit status is 0, its own completion line is there and agrees with the
PASS/FAIL lines it printed, it printed at least one check, and nothing in its
log is a traceback.  A failing suite is accepted only when it is listed in
EXPECTED_FAILURES and failed exactly the listed cases with the listed number of
passes -- an extra failure, a different one, a traceback, a startup error or a
missing summary all fail the run, as does an expected failure that has stopped
happening (remove its entry).  validate_missions.py is a data BASELINE, not a
clean result: its report must match scripts/qa_baselines/validate_missions.json
check for check and by content digest; any change fails the run until the
baseline is deliberately re-recorded (--record-validator-baseline).

Exit status is nonzero when any entry fails.
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import LIVE_SAVE, QA_ROOT, ROOT, Fixture, IsolatedServer, qa_dir  # noqa: E402

PY = sys.executable

# Failures accepted on purpose, by exact case.  Each entry names the reason,
# every FAIL label the suite is expected to print (its Checker label, the text
# before "  -- "), and how many checks pass beside them.  Anything else from
# that suite fails the run.  Empty: the delayed-Continue (QC_SESSION_3_REVIEW
# P2) and ShopUse type-9 reset reproducers both pass since Session 6.
#
#   'test_example_wire.py': {'reason': 'why it is still open, with a doc link',
#                            'failures': ['exact failing label'], 'passed': 12},
EXPECTED_FAILURES = {}

# Suites that start their own isolated server from the executable.
SERVER_SUITES = [
    'test_battle_simulator_wire.py', 'test_mission_progression_wire.py', 'test_merit_exchange_wire.py',
    'test_mission_settlement_wire.py', 'test_mission_run_edges_wire.py', 'test_mission_run_ownership_wire.py',
    'test_story_gates_wire.py', 'test_lizeria_qc_wire.py', 'test_bugfix_qc_wire.py',
    'test_continue_delayed_retry_wire.py', 'test_enhancement_sp_wire.py', 'test_leader_exp_boost_wire.py',
    'test_metal_parades_wire.py', 'test_evolution_qc_wire.py', 'test_fe_skill_purchase_wire.py',
    'test_fe_skill_qc_wire.py', 'test_sphere_qc_wire.py', 'test_unit_roster_wire.py',
    'test_warehouse_placeholder_wire.py', 'test_warehouse_integration_wire.py',
    'test_warehouse_consumer_model_wire.py', 'test_reward_lock_snapshots_wire.py',
    'test_summon_reveal_wire.py', 'test_brave_slots_wire.py',
    'test_town_unlock_wire.py', 'test_sphere_equip_format_wire.py', 'test_grant_bundle_wire.py',
]
# Suites that take a copied save and expect a server this runner provisions.
SAVE_SUITES = [('test_fusion_merit.py', 19960), ('test_exchange_ui_wire.py', 19960),
               ('test_synthesis_wire.py', 19962), ('test_feature_visits_wire.py', 19962),
               ('test_research_lab_wire.py', 19960), ('test_guild_invite_wire.py', 19960)]
# Checks that need no server (or start their own on a fresh save).
OTHER = ['test_qa_runner.py', 'test_research_lab_ai.py', 'test_unit_text_data.py', 'audit_handlers.py',
         'validate_missions.py', 'gen_battle_simulator.py --check', 'test_debug_startup.py', 'test_debug_cli.py']

# How each entry proves it finished.  The default, 'checker', is bf_testkit's
# Checker: "PASS  label" / "FAIL  label  -- detail" lines and a closing
# "<name>: N passed, M failed" that must agree with them.  'failures' is the
# older "PASS  label" / "FAIL  label  detail" with a closing "N failure(s)".
# ('pass', n) suites print "PASS: ..." per phase and assert -- a failure is a
# traceback -- so exactly n PASS lines is completion.  ('marker', regex) is a
# tool that prints that line only when it succeeded.
COMPLETION = {
    'test_research_lab_ai.py': 'failures', 'test_research_lab_wire.py': 'failures',
    'test_guild_invite_wire.py': 'failures',
    'test_fusion_merit.py': ('pass', 8), 'test_exchange_ui_wire.py': ('pass', 4),
    'test_synthesis_wire.py': ('pass', 5), 'test_feature_visits_wire.py': ('pass', 4),
    'test_debug_startup.py': ('pass', 1), 'test_debug_cli.py': ('pass', 2),
    'audit_handlers.py': ('marker', r'^\d+ registrations; 0 structural errors$'),
    'gen_battle_simulator.py --check': ('marker', r'^up to date$'),
}

VALIDATOR_BASELINE = ROOT / 'scripts' / 'qa_baselines' / 'validate_missions.json'

CHECKER_SUMMARY = re.compile(r'^(?P<name>.+): (?P<passed>\d+) passed, (?P<failed>\d+) failed$', re.M)
FAILURES_TRAILER = re.compile(r'^(?P<failed>\d+) failure\(s\)$', re.M)


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def git(*args, cwd=ROOT):
    # Bytes, decoded as UTF-8: diffs carry non-ASCII text, which the console code
    # page cannot decode.
    out = subprocess.run(['git', *args], cwd=cwd, capture_output=True).stdout or b''
    return out.decode('utf-8', 'replace')


def source_identity():
    out = {}
    for name, cwd in (('parent', ROOT), ('packet-generator', ROOT / 'packet-generator')):
        diff = git('diff', 'HEAD', cwd=cwd)
        out[name] = {'head': git('rev-parse', 'HEAD', cwd=cwd).strip(),
                     'branch': git('branch', '--show-current', cwd=cwd).strip(),
                     'modified_or_untracked': len([l for l in git('status', '--porcelain', cwd=cwd).splitlines() if l]),
                     'diff_sha256': hashlib.sha256(diff.encode('utf-8')).hexdigest()}
    return out


def check_lines(text):
    """(passed labels, failed labels) from 'PASS  x', 'PASS: x', 'FAIL  x  -- d', 'FAIL: x' lines."""
    passed, failed = [], []
    for line in text.splitlines():
        m = re.match(r'^(PASS|FAIL)(?::\s*|\s{2})(.*)$', line)
        if not m:
            continue
        label = m.group(2).split('  -- ')[0].rstrip()
        (passed if m.group(1) == 'PASS' else failed).append(label)
    return passed, failed


def classify(name, code, text, completion='checker', expected=None):
    """Judge one suite run from its exit status and its own output.

    Returns a dict: ok (bool), status ('passed' | 'expected failure' | 'FAILED'),
    passed / failed counts, failures (labels) and problems (why it is not ok).
    Pure, so scripts/test_qa_runner.py can exercise every branch.
    """
    passed, failed = check_lines(text)
    problems = []
    if code == 'timeout':
        problems.append('timed out')
    if 'Traceback (most recent call last)' in text:
        problems.append('a traceback in the output (unexpected exception, startup or tooling error)')

    if completion == 'checker':
        summaries = list(CHECKER_SUMMARY.finditer(text))
        if not summaries:
            problems.append('no "<name>: N passed, M failed" summary line (the suite did not finish)')
        else:
            p = sum(int(s['passed']) for s in summaries)
            f = sum(int(s['failed']) for s in summaries)
            if (p, f) != (len(passed), len(failed)):
                problems.append(f'summary says {p} passed / {f} failed but the log has '
                                f'{len(passed)} PASS / {len(failed)} FAIL lines')
    elif completion == 'failures':
        trailer = FAILURES_TRAILER.findall(text)
        if not trailer:
            problems.append('no "N failure(s)" trailer (the suite did not finish)')
        elif int(trailer[-1]) != len(failed):
            problems.append(f'trailer says {trailer[-1]} failure(s) but the log has {len(failed)} FAIL lines')
    elif completion[0] == 'pass':
        if len(passed) != completion[1]:
            problems.append(f'{len(passed)} PASS lines, {completion[1]} expected (a phase did not run)')
    elif completion[0] == 'marker':
        if not re.search(completion[1], text, re.M):
            problems.append(f'success marker /{completion[1]}/ missing')
    else:
        raise ValueError(f'unknown completion rule {completion!r}')

    if completion == 'checker' or completion == 'failures' or completion[0] == 'pass':
        if not passed and not failed:
            problems.append('no checks ran')

    status = 'passed'
    if expected is not None:
        want = sorted(expected['failures'])
        if sorted(failed) == want and len(passed) == expected['passed'] and code not in (0, 'timeout') \
                and not problems:
            status = 'expected failure'
        else:
            if sorted(failed) != want:
                extra = sorted(set(failed) - set(want))
                gone = sorted(set(want) - set(failed))
                if extra:
                    problems.append(f'unexpected failures: {extra}')
                if gone:
                    problems.append(f'expected failures did not happen (fixed? remove the entry): {gone}')
                if not extra and not gone:
                    problems.append(f'expected failures repeated a different number of times: {sorted(failed)}')
            if len(passed) != expected['passed']:
                problems.append(f'{len(passed)} checks passed, {expected["passed"]} expected')
            if code in (0, 'timeout') and failed:
                problems.append(f'exit status {code} with failures')
    else:
        if failed:
            problems.append(f'failed checks: {failed}')
        if code != 0:
            problems.append(f'exit status {code}')

    ok = not problems and (status == 'expected failure' or code == 0)
    if not ok:
        status = 'FAILED'
    return {'ok': ok, 'status': status, 'passed': len(passed), 'failed': len(failed), 'failures': failed,
            'problems': problems}


def validator_report_identity(report):
    """Per-check finding counts, the total, and a digest of every finding's content."""
    by_code = {code: len(f.get('items') or []) for code, f in sorted(report.items()) if f.get('items')}
    canonical = {code: sorted((i.get('subject', ''), i.get('note', '')) for i in report[code]['items'])
                 for code in by_code}
    digest = hashlib.sha256(json.dumps(canonical, sort_keys=True, ensure_ascii=False).encode('utf-8')).hexdigest()
    return {'total': sum(by_code.values()), 'by_code': by_code, 'sha256': digest}


def classify_validator(code, report, baseline):
    """validate_missions.py against its recorded baseline: same checks, counts and content."""
    problems = []
    if report is None:
        problems.append('no JSON report written (the validator did not finish)')
        identity = None
    else:
        identity = validator_report_identity(report)
        if baseline is None:
            problems.append(f'no recorded baseline at {VALIDATOR_BASELINE.relative_to(ROOT)}')
        else:
            if identity['by_code'] != baseline['by_code']:
                changed = sorted(set(identity['by_code']) | set(baseline['by_code']))
                problems.append('finding counts changed: ' + ', '.join(
                    f"{c} {baseline['by_code'].get(c, 0)}->{identity['by_code'].get(c, 0)}" for c in changed
                    if baseline['by_code'].get(c, 0) != identity['by_code'].get(c, 0)))
            elif identity['sha256'] != baseline['sha256']:
                problems.append('same counts but different findings (content digest changed)')
        expected_exit = 1 if identity['total'] else 0
        if code != expected_exit:
            problems.append(f'exit status {code}, {expected_exit} expected for {identity["total"]} findings')
    return {'ok': not problems, 'status': 'baseline' if not problems else 'FAILED', 'identity': identity,
            'problems': problems}


def run(cmd, log, timeout=1800, cwd=ROOT):
    started = time.time()
    # Suites print non-ASCII labels (unit rarity stars); a redirected child would
    # otherwise encode with the console code page and die mid-run.
    env = dict(os.environ, PYTHONIOENCODING='utf-8')
    with log.open('w', encoding='utf-8', errors='replace') as f:
        try:
            code = subprocess.run(cmd, cwd=cwd, stdout=f, stderr=subprocess.STDOUT, timeout=timeout,
                                  env=env).returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    return code, round(time.time() - started, 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--suite', action='append', help='run only these (file names); repeatable')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--record-validator-baseline', action='store_true',
                    help='write the current validate_missions report as the new baseline (review it first)')
    args = ap.parse_args()
    exe = Path(args.exe).resolve()
    assert exe.is_file(), exe
    every = SERVER_SUITES + [n for n, _ in SAVE_SUITES] + OTHER
    if args.list:
        print('\n'.join(every))
        return 0
    wanted = set(args.suite or every)
    unknown = wanted - set(every)
    assert not unknown, f'unknown suites: {sorted(unknown)}'
    QA_ROOT.mkdir(parents=True, exist_ok=True)

    started = datetime.datetime.now().astimezone()
    results = []

    def record(name, cmd, log, code, seconds):
        text = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
        verdict = classify(name, code, text, COMPLETION.get(name, 'checker'), EXPECTED_FAILURES.get(name))
        results.append({'suite': name, 'exit': code, 'seconds': seconds, 'log': str(log.relative_to(ROOT)),
                        'command': ' '.join(str(c) for c in cmd), 'baseline': False,
                        'expected': EXPECTED_FAILURES.get(name), **verdict})
        print(f"{name}: {verdict['status']} (exit {code}, {verdict['passed']} passed, {verdict['failed']} failed, "
              f"{seconds}s) -> {log.relative_to(ROOT)}" + (f"  !! {'; '.join(verdict['problems'])}"
                                                          if verdict['problems'] else ''), flush=True)

    for name in SERVER_SUITES:
        if name not in wanted:
            continue
        log = QA_ROOT / (name.removesuffix('.py') + '.log')
        cmd = [PY, f'scripts/{name}', str(exe)]
        code, seconds = run(cmd, log)
        record(name, cmd, log, code, seconds)

    for name, port in SAVE_SUITES:
        if name not in wanted:
            continue
        suite = name.removesuffix('.py')
        fx = Fixture.create(qa_dir(suite), port=port)
        if name == 'test_exchange_ui_wire.py':
            # It expects an invitable friend; the player may have invited them
            # all.  Empty only the fixture's guild.
            with fx.db() as db:
                user = db.execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]
                db.execute('DELETE FROM user_guild_members WHERE member_id != ? AND guild_id IN '
                           '(SELECT guild_id FROM user_guilds WHERE owner_user_id=?)', (user, user))
        if name == 'test_research_lab_wire.py':
            with fx.db() as db:
                db.execute("DELETE FROM user_campaign_missions WHERE mission_id IN "
                           "('2000000','2000001','2000002','2100004')")
        log = QA_ROOT / (suite + '.log')
        cmd = [PY, f'scripts/{name}', str(fx.db_path)]
        try:
            with IsolatedServer(exe, fx):
                code, seconds = run(cmd, log, timeout=600)
        except Exception as ex:  # a server that will not start is a failed suite, not a crashed runner
            log.write_text(f'Traceback (most recent call last):\n  runner: isolated server failed: {ex!r}\n',
                           encoding='utf-8')
            code, seconds = 'server', 0.0
        record(name, cmd, log, code, seconds)

    for entry in OTHER:
        if entry not in wanted:
            continue
        parts = entry.split()
        name = parts[0]
        log = QA_ROOT / (name.removesuffix('.py') + ('_check' if '--check' in parts else '') + '.log')
        if name in ('test_debug_startup.py',):
            cmd = [PY, f'scripts/{name}', str(exe)]
        elif name == 'test_debug_cli.py':
            cmd = [PY, f'scripts/{name}', str(LIVE_SAVE), str(exe)]
        elif name == 'validate_missions.py':
            report_path = QA_ROOT / 'validate_missions.json'
            if report_path.exists():
                report_path.unlink()
            cmd = [PY, f'scripts/{name}', '--json', str(report_path)]
        else:
            cmd = [PY, f'scripts/{name}', *parts[1:]]
        code, seconds = run(cmd, log)
        if name == 'validate_missions.py':
            report = json.loads(report_path.read_text(encoding='utf-8')) if report_path.exists() else None
            if args.record_validator_baseline and report is not None:
                VALIDATOR_BASELINE.parent.mkdir(parents=True, exist_ok=True)
                identity = validator_report_identity(report)
                VALIDATOR_BASELINE.write_text(json.dumps({
                    'recorded': datetime.datetime.now().astimezone().isoformat(timespec='seconds'),
                    'note': 'Known data baseline, NOT a clean validation: every finding here is open work.',
                    **identity}, indent=1), encoding='utf-8')
                print(f'Recorded validator baseline: {identity["total"]} findings, {identity["by_code"]}')
            baseline = json.loads(VALIDATOR_BASELINE.read_text(encoding='utf-8')) \
                if VALIDATOR_BASELINE.exists() else None
            verdict = classify_validator(code, report, baseline)
            ident = verdict['identity'] or {}
            results.append({'suite': entry, 'exit': code, 'passed': 0, 'failed': 0, 'failures': [],
                            'seconds': seconds, 'baseline': True, 'expected': None,
                            'findings': ident.get('total'), 'by_code': ident.get('by_code'),
                            'findings_sha256': ident.get('sha256'),
                            'log': str(log.relative_to(ROOT)), 'command': ' '.join(cmd),
                            'ok': verdict['ok'], 'status': verdict['status'], 'problems': verdict['problems']})
            print(f"{entry}: {verdict['status']} (exit {code}), findings {ident.get('total')} {ident.get('by_code')} "
                  f"-> {log.relative_to(ROOT)}" + (f"  !! {'; '.join(verdict['problems'])}"
                                                   if verdict['problems'] else ''), flush=True)
            continue
        record(entry, cmd, log, code, seconds)

    finished = datetime.datetime.now().astimezone()
    stat = exe.stat()
    summary = {
        'started': started.isoformat(timespec='seconds'), 'finished': finished.isoformat(timespec='seconds'),
        'runner': f'python scripts/run_bugfix_regressions.py {args.exe}'
                  + ''.join(f' --suite {s}' for s in (args.suite or [])),
        'executable': {'path': str(exe), 'bytes': stat.st_size,
                       'modified': datetime.datetime.fromtimestamp(stat.st_mtime).astimezone().isoformat(
                           timespec='seconds'),
                       'sha256': sha256(exe)},
        'source': source_identity(), 'python': sys.version.split()[0],
        'totals': {'suites': len(results), 'failed_suites': [r['suite'] for r in results if not r['ok']],
                   'expected_failures': [r['suite'] for r in results if r['status'] == 'expected failure'],
                   'passed_checks': sum(r['passed'] for r in results),
                   'failed_checks': sum(r['failed'] for r in results)},
        'results': results,
    }
    (QA_ROOT / 'SUMMARY.json').write_text(json.dumps(summary, indent=1), encoding='utf-8')
    lines = [f'# QA run {summary["started"]} -> {summary["finished"]}', '',
             f'Runner: `{summary["runner"]}`', '',
             f'Executable: `{stat.st_size}` bytes, modified {summary["executable"]["modified"]}, '
             f'sha256 `{summary["executable"]["sha256"]}`', '',
             'Source: ' + '; '.join(f'{k} `{v["head"][:10]}` on {v["branch"]}, {v["modified_or_untracked"]} '
                                    f'changed paths, diff sha256 `{v["diff_sha256"][:12]}`'
                                    for k, v in summary['source'].items()), '',
             '| Suite | Result | Exit | Passed | Failed | Seconds | Log |', '|---|---|---|---|---|---|---|']
    for r in results:
        extra = ''
        if r.get('baseline'):
            extra = f" ({r.get('findings')} findings {r.get('by_code')}, digest `{(r.get('findings_sha256') or '')[:12]}`)"
        if r.get('expected'):
            extra += f" -- EXPECTED FAILURE: {r['expected']['reason']}"
        if r.get('problems'):
            extra += ' -- ' + '; '.join(r['problems'])
        lines.append(f"| {r['suite']}{extra} | {r['status']} | {r['exit']} | {r['passed']} | {r['failed']} | "
                     f"{r['seconds']} | `{r['log']}` |")
    t = summary['totals']
    lines += ['', f"Totals: {t['suites']} entries, {t['passed_checks']} passed checks, {t['failed_checks']} failed; "
                  f"failed entries: {', '.join(t['failed_suites']) or 'none'}; "
                  f"expected failures: {', '.join(t['expected_failures']) or 'none'}. "
                  'The validator line is a recorded data baseline, not a clean validation.', '']
    (QA_ROOT / 'SUMMARY.md').write_text('\n'.join(lines), encoding='utf-8')
    print(f"Summary: {QA_ROOT.relative_to(ROOT)}/SUMMARY.md -- failed entries: {', '.join(t['failed_suites']) or 'none'}",
          flush=True)
    return 1 if t['failed_suites'] else 0


if __name__ == '__main__':
    sys.exit(main())
