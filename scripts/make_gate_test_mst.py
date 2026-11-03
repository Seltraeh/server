"""A test-only MST root that makes every summon's gate deterministic per gate.

    python scripts/make_gate_test_mst.py --out out/qa/current/session14/mst_gate_test

Gacha.cpp picks the gate effect by the summoned unit's rarity alone, weighted
over gacha_effect_mst rows of that rarity (900 double / 901 single for Omni,
50/50).  To reproduce #24 without summoning until a double gate happens, this
writes a COPY of deploy/mst -- hard links, except gacha_effect_mst.json, whose
weights it edits -- for the harness's `server --mst-root`:

  rarity 8 (Test Gate: Omni, 1 gem)    -> always 900: Rainbow -> Black double gate
                                           (gWaitRainbow, gChange(Rainbow-black),
                                           gOpenGold(Rare7) -- the Session 13 report)
  rarity 7 (Test Gate: 7 Star, 1 gem)  -> always 229: the Black single gate
  rarity 5 / 6 (Test Gates 5 / 6 Star) -> only their double gates (13/15, 19/21)
  rarity 4                              -> only the Gold -> Red double (10)
  rarity 1-3                            -> unchanged (they have single gates only)

Only weights change (the client never reads them; it maps the effect id the
server sends to scripts through its own copy of the table), so rows, ids and
scripts are exactly the shipped ones.  deploy/mst is never written.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MST = ROOT / 'deploy' / 'mst'
TABLE = 'gacha_effect_mst.json'
# effect id -> weight forced to 0 (every other row keeps its shipped weight)
SILENCED = {11: 'rarity 4 single', 5: 'rarity 5 single (gacha 1000 row)', 17: 'rarity 6 single (gacha 1000 row)',
            228: 'rarity 7 double', 901: 'rarity 8 single'}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', required=True, type=Path)
    args = ap.parse_args()
    out = args.out.resolve()
    if (ROOT / 'deploy').resolve() in [out, *out.parents] or (ROOT / 'out').resolve() not in out.parents:
        raise SystemExit('refusing: the copy must live under out/, never in deploy/')
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    linked = 0
    for p in sorted(MST.iterdir()):
        if p.is_file() and p.name != TABLE:
            os.link(p, out / p.name)
            linked += 1
    data = json.loads((MST / TABLE).read_text(encoding='utf-8'))
    key = next(iter(data))
    by_rarity = {}
    for row in data[key]:
        eid = int(row['u0vkt9yH'])
        if eid in SILENCED:
            row['ug9xV4Fz'] = '0'
        if int(row['ug9xV4Fz']) > 0:
            by_rarity.setdefault(int(row['7ofj5xa1']), []).append(
                f"{eid} ({'double' if row['YTx3c1jQ'] else 'single'}: {row['7ZNcmYS2']}"
                f"{' > ' + row['YTx3c1jQ'] if row['YTx3c1jQ'] else ''} > {row['tj0i9JhC']})")
    (out / TABLE).write_text(json.dumps(data, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    for rarity in sorted(by_rarity):
        print(f'rarity {rarity}: ' + '; '.join(by_rarity[rarity]))
    digest = hashlib.sha256((out / TABLE).read_bytes()).hexdigest()
    print(f'{out}: {linked} tables hard-linked to deploy/mst, {TABLE} edited (sha256 {digest})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
