"""#22B Omni Amadream header / #22C Omni Galtier leader-skill text: the data side.

python scripts/test_unit_text_data.py

Both screens draw CLIENT text: UnitMst::getUnitName and
LeaderSkillMst::getDescription build MST_UNIT_<id>_NAME /
MST_LEADERSKILLS_<id>_DESCRIPTION and read them through
TextManager::getMstText, i.e. the client's own sgtext bundle
(tools/sgtext/all.tsv is its English dump).  The server's MST rows only supply
the ids, rarity and element.  So this checks what the data CAN get wrong --
which unit and skill the screens resolve to, their rarity/element, and that no
copy of the text is shortened -- and prints the comparison set a client check
needs.  Whether the header overlaps or the description is cut is a client
layout question and is not claimed here.

Evidence recorded 2026-09-30: Galtier's skill text is 290 characters; the
report's cut ("...Dark types & 20% damage reductio") falls at character 252,
mid-word, 38 characters before the end.  32 leader-skill descriptions are
longer than 252, the longest Durumn's (840278, 349).
"""
import json
import re
import sys

from bf_testkit import ROOT, Checker

AMADREAM, GALTIER = 51337, 860428
GALTIER_LS = 801271
REPORTED_CUT = 'Dark types & 20% damage reductio'


def mst(name):
    return next(iter(json.loads((ROOT / 'deploy/mst' / f'{name}_mst.json').read_text(encoding='utf-8')).values()))


def main():
    check = Checker()
    sg = dict(line.split('\t', 1) for line in
              (ROOT / 'tools/sgtext/all.tsv').read_text(encoding='utf-8').splitlines() if '\t' in line)
    units = {int(r['pn16CNah']): r for r in mst('unit')}
    skills = {int(r['oS3kTZ2W']): r for r in mst('leader_skill')}

    # ---- #22B -----------------------------------------------------------------------
    name = sg.get(f'MST_UNIT_{AMADREAM}_NAME')
    row = units.get(AMADREAM)
    check('22B: 51337 is Intact Champion Amadream in the client text', name == 'Intact Champion Amadream', name)
    check('22B: 51337 is Omni (rarity 8) and Light (element 5)',
          row is not None and row['7ofj5xa1'] == '8' and row['iNy0ZU5M'] == '5', row and (row['7ofj5xa1'], row['iNy0ZU5M']))
    check('22B: the served name is the client name, whole', row is not None and row['utP1c0CD'] == name,
          row and row['utP1c0CD'])
    omni_names = sorted(((len(sg.get(f'MST_UNIT_{u}_NAME', '')), u, sg.get(f'MST_UNIT_{u}_NAME'))
                         for u, r in units.items() if r['7ofj5xa1'] == '8'), reverse=True)
    rank = next(i for i, (_, u, _) in enumerate(omni_names) if u == AMADREAM)
    print(f'22B: {len(omni_names)} Omni units; Amadream\'s name ({len(name or "")} chars) is #{rank + 1} longest. '
          f'As long or longer, for the client comparison: {[(u, n) for _, u, n in omni_names[:rank]][:6]}')

    # ---- #22C -----------------------------------------------------------------------
    text = sg.get(f'MST_LEADERSKILLS_{GALTIER_LS}_DESCRIPTION', '')
    check('22C: Ruinous Despoiler Galtier (860428) leads with skill 801271',
          units.get(GALTIER, {}).get('oS3kTZ2W') == str(GALTIER_LS))
    check('22C: the client text runs past the reported cut and ends the sentence',
          REPORTED_CUT in text and text.endswith('Dark, Light types in Guild Raid') and len(text) == 290, len(text))
    cut = text.index(REPORTED_CUT) + len(REPORTED_CUT)
    check('22C: the reported cut is at character 252 of 290', cut == 252, cut)
    served = skills.get(GALTIER_LS, {}).get('qp37xTDh')
    check('22C: the served leader-skill row carries the same complete text', served == text, served and len(served))
    long_ones = sorted(((len(v), int(k.split('_')[2])) for k, v in sg.items()
                        if re.fullmatch(r'MST_LEADERSKILLS_\d+_DESCRIPTION', k) and len(v) > 252), reverse=True)
    mismatched = [ls for _, ls in long_ones if ls in skills and skills[ls].get('qp37xTDh') != sg[f'MST_LEADERSKILLS_{ls}_DESCRIPTION']]
    check(f'22C: all {len(long_ones)} descriptions over 252 characters are served whole', not mismatched, mismatched[:5])
    owners = {int(r['oS3kTZ2W']): (int(u), sg.get(f'MST_UNIT_{u}_NAME')) for u, r in units.items()}
    print('22C: longest descriptions, for the client comparison:',
          [(n, ls, owners.get(ls)) for n, ls in long_ones[:5]])
    return check.summary('unit text data')


if __name__ == '__main__':
    sys.exit(main())
