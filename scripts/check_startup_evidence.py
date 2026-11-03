"""Client evidence against a startup manifest (Session 19).

    python scripts/check_startup_evidence.py [--manifest MANIFEST.json] [--evidence EVIDENCE.json]

Every name the client queued for its own downloader inside the LOGIN WINDOW of a recorded provider-off run
-- from its first LoginScene to the TransitionalDLCScene that downloads the priority list -- must be required
by the manifest (its login or background set) or excluded with its reason, and no excluded name may have
been queued at all.  Without --manifest the generator derives one now, for an isolated server (the layout
the evidence's server had), so a generator change that drops a requested name fails here.

The evidence is positive only: the logger caps the main list at 4,000 names, so a name missing from it
proves nothing and is never asked for.  One observed route is not a category's exhaustive requirement
either: derived lists are exact by their binary trail (tools/bin/x86_startup_rules_check.py), not by this.
Per derived list it prints how many of its names the client queued (informational).

Exit 0 when every login-window name is covered and no excluded name was queued, 1 otherwise.  Read only.
Default evidence: scripts/qa_baselines/c16_provider_off_queue_20261021.json (the C16 control, Session 17).
"""
from __future__ import annotations

import argparse
import collections
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import startup_coverage as sc  # noqa: E402

EVIDENCE = Path(__file__).resolve().parent / 'qa_baselines' / 'c16_provider_off_queue_20261021.json'


def login_window(evidence: dict) -> tuple[float, float]:
    """(first LoginScene, the first TransitionalDLCScene after it) on the evidence's log clock."""
    scenes = evidence['scenes']
    start = next(s['t'] for s in scenes if s['scene'] == 'LoginScene')
    end = next(s['t'] for s in scenes if s['scene'] == 'TransitionalDLCScene' and s['t'] > start)
    return start, end


def check(manifest: dict, evidence: dict, report: dict | None = None) -> tuple[bool, list[str]]:
    start, end = login_window(evidence)
    required = set(manifest['sets']['login']) | set(manifest['sets']['background'])
    excluded = set(manifest.get('excluded') or {})
    queued = evidence['queued']
    window = {n for n, lists in queued.items() if any(start <= t < end for t in lists.values())}
    uncovered = sorted(n for n in window if n not in required and n not in excluded)
    contradicted = sorted(n for n in queued if n in excluded)
    lines = [f'evidence {evidence["log"]["path"]} (sha256 {evidence["log"]["sha256"][:16]}..., pid '
             f'{evidence["log"]["pid"]}): {len(queued)} queued name(s), {len(window)} in the login window '
             f'{start:.1f}-{end:.1f} ms']
    lines.append(f'{"OK  " if not uncovered else "FAIL"} login window: {len(window) - len(uncovered)} of {len(window)} '
                 'queued names are required or excluded' + (f'; not: {uncovered[:12]}' if uncovered else ''))
    lines.append(f'{"OK  " if not contradicted else "FAIL"} no excluded name was queued'
                 + (f'; queued: {contradicted[:12]}' if contradicted else ''))
    outside = sorted(n for n in queued if n not in window and n not in required and n not in excluded)
    lines.append(f'note: {len(outside)} name(s) queued outside the login window (other scenes\' own downloads) are '
                 f'in no set: {outside[:6]}')
    if report:
        per = collections.defaultdict(set)
        for name, sources in report.get('provenance', {}).items():
            for s in sources:
                if s.startswith('category ') and ' (' in s and not s.startswith('category unit'):
                    per[s.split(' (')[0][len('category '):]].add(name)
        for key in sorted(per):
            names = per[key]
            lines.append(f'note: list {key}: {sum(1 for n in names if n in queued)} of {len(names)} name(s) queued')
    return not uncovered and not contradicted, lines


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--manifest', type=Path, help='a startup manifest (default: derive one now, isolated layout)')
    ap.add_argument('--report', type=Path, help="the manifest command's report, for the per-list notes")
    ap.add_argument('--evidence', type=Path, default=EVIDENCE)
    args = ap.parse_args()
    evidence = json.loads(args.evidence.read_text(encoding='utf-8'))
    if args.manifest:
        manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
        report = json.loads(args.report.read_text(encoding='utf-8')) if args.report else None
        label = str(args.manifest)
    else:
        manifest, report = sc.derive_manifest(sc.Content(), banner_dir=None)
        label = 'the manifest derived now (isolated layout)'
    ok, lines = check(manifest, evidence, report)
    for line in lines:
        print(line)
    print(f'check_startup_evidence: {label}: ' + ('consistent with the client evidence' if ok else 'CONTRADICTED by the client evidence'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
