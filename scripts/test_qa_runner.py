"""How run_bugfix_regressions.py judges a suite, on synthetic logs (no server).

    python scripts/test_qa_runner.py

The runner used to accept ANY nonzero exit from a suite marked known-open, so a
traceback, a server that never started or a new failed assertion in that suite
was reported as the documented reproducer.  classify() now needs positive
evidence of completion and accepts failures only case by case; these checks pin
that down, branch by branch.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import Checker  # noqa: E402
from run_bugfix_regressions import classify, classify_validator, validator_report_identity  # noqa: E402

TRACEBACK = ('Traceback (most recent call last):\n  File "scripts/test_x.py", line 9, in <module>\n'
             'RuntimeError: server did not start; see out/qa/current/x/server.log\n')


def checker_log(passed, failed, name='suite x'):
    lines = [f'PASS  {p}' for p in passed] + [f'FAIL  {f}  -- detail for {f}' for f in failed]
    return '\n'.join(lines + [f'{name}: {len(passed)} passed, {len(failed)} failed']) + '\n'


def main():
    check = Checker()
    expected = {'reason': 'documented reproducer', 'failures': ['delayed retry charges again'], 'passed': 2}

    r = classify('s.py', 0, checker_log(['a', 'b', 'c'], []))
    check('normal success: exit 0, summary agrees, no failures', r['ok'] and r['status'] == 'passed'
          and (r['passed'], r['failed']) == (3, 0), r)

    r = classify('s.py', 1, checker_log(['a', 'b'], ['delayed retry charges again']), expected=expected)
    check('expected failure: exactly the listed case, the listed passes', r['ok']
          and r['status'] == 'expected failure', r)

    r = classify('s.py', 1, checker_log(['a', 'b'], ['delayed retry charges again', 'a new regression']),
                 expected=expected)
    check('extra failure beside the expected one fails the run', not r['ok']
          and any('unexpected failures' in p for p in r['problems']), r)

    r = classify('s.py', 1, checker_log(['a', 'b'], ['a different failure']), expected=expected)
    check('a different failure in an expected-failure suite fails the run', not r['ok'], r)

    r = classify('s.py', 1, checker_log(['a'], ['delayed retry charges again']), expected=expected)
    check('expected failure with a missing pass (fewer checks ran) fails the run', not r['ok']
          and any('checks passed' in p for p in r['problems']), r)

    r = classify('s.py', 0, checker_log(['a', 'b', 'c'], []), expected=expected)
    check('an expected failure that stopped happening fails the run (remove the entry)', not r['ok']
          and any('did not happen' in p for p in r['problems']), r)

    r = classify('s.py', 1, TRACEBACK, expected=expected)
    check('startup failure (traceback, no summary) in an expected-failure suite fails the run', not r['ok']
          and any('traceback' in p for p in r['problems']) and any('summary' in p for p in r['problems']), r)

    r = classify('s.py', 1, checker_log(['a', 'b'], ['delayed retry charges again']) + TRACEBACK, expected=expected)
    check('a traceback after the expected failures still fails the run', not r['ok'], r)

    r = classify('s.py', 0, 'PASS  a\nPASS  b\n')
    check('missing summary on exit 0 fails (the suite did not finish)', not r['ok']
          and any('summary' in p for p in r['problems']), r)

    r = classify('s.py', 0, '')
    check('empty output on exit 0 fails (no results)', not r['ok'], r)

    r = classify('s.py', 0, 'suite x: 0 passed, 0 failed\n')
    check('a summary of zero checks fails (nothing ran)', not r['ok']
          and any('no checks ran' in p for p in r['problems']), r)

    r = classify('s.py', 0, 'PASS  a\nsuite x: 3 passed, 0 failed\n')
    check('a summary that disagrees with the PASS/FAIL lines fails', not r['ok']
          and any('summary says' in p for p in r['problems']), r)

    r = classify('s.py', 0, checker_log(['a'], ['b']))
    check('exit 0 with a FAIL line fails', not r['ok'], r)

    r = classify('s.py', 1, checker_log(['a'], ['b']))
    check('an ordinary failing suite fails', not r['ok'] and r['failures'] == ['b'], r)

    r = classify('s.py', 'timeout', checker_log(['a'], []))
    check('a timeout fails', not r['ok'] and 'timed out' in r['problems'], r)

    r = classify('s.py', 0, 'PASS  a\n\n0 failure(s)\n', completion='failures')
    check('"N failure(s)" suites: trailer agreeing with the lines passes', r['ok'], r)
    r = classify('s.py', 0, 'PASS  a\nFAIL  b  detail\n\n0 failure(s)\n', completion='failures')
    check('"N failure(s)" suites: a trailer that undercounts fails', not r['ok'], r)
    r = classify('s.py', 0, 'PASS  a\n', completion='failures')
    check('"N failure(s)" suites: no trailer fails', not r['ok'], r)

    r = classify('s.py', 0, 'PASS: one\nPASS: two\n', completion=('pass', 2))
    check('PASS-line suites: the expected number of phases passes', r['ok'], r)
    r = classify('s.py', 0, 'PASS: one\n', completion=('pass', 2))
    check('PASS-line suites: a missing phase fails', not r['ok'], r)

    r = classify('s.py', 0, '153 registrations; 0 structural errors\n',
                 completion=('marker', r'^\d+ registrations; 0 structural errors$'))
    check('marker tools: the success line passes', r['ok'], r)
    r = classify('s.py', 0, '153 registrations; 2 structural errors\n',
                 completion=('marker', r'^\d+ registrations; 0 structural errors$'))
    check('marker tools: anything else fails', not r['ok'], r)

    report = {'H15': {'severity': 'HIGH', 'items': [{'subject': '12', 'note': 'x'}, {'subject': '52', 'note': 'y'}]},
              'M4': {'severity': 'MEDIUM', 'items': [{'subject': '1', 'note': 'z'}]},
              'C1': {'severity': 'CRITICAL', 'items': []}}
    baseline = validator_report_identity(report)
    check('validator identity: per-check counts and total', baseline['by_code'] == {'H15': 2, 'M4': 1}
          and baseline['total'] == 3, baseline)
    shuffled = {'M4': report['M4'], 'C1': report['C1'],
                'H15': {'severity': 'HIGH', 'items': list(reversed(report['H15']['items']))}}
    check('validator identity: independent of item order', validator_report_identity(shuffled) == baseline)
    v = classify_validator(1, report, baseline)
    check('validator: the recorded baseline is accepted, labelled baseline', v['ok'] and v['status'] == 'baseline', v)
    worse = dict(report, M4={'severity': 'MEDIUM', 'items': report['M4']['items'] + [{'subject': '2', 'note': 'q'}]})
    v = classify_validator(1, worse, baseline)
    check('validator: a new finding fails the run', not v['ok'] and 'M4 1->2' in v['problems'][0], v)
    changed = dict(report, M4={'severity': 'MEDIUM', 'items': [{'subject': '7', 'note': 'z'}]})
    v = classify_validator(1, changed, baseline)
    check('validator: same counts, different findings fails (content digest)', not v['ok'], v)
    v = classify_validator(0, report, baseline)
    check('validator: exit 0 with findings fails', not v['ok'], v)
    v = classify_validator(1, None, baseline)
    check('validator: no report fails', not v['ok'], v)
    v = classify_validator(1, report, None)
    check('validator: no recorded baseline fails', not v['ok'], v)
    return check.summary('QA runner classification')


if __name__ == '__main__':
    sys.exit(main())
