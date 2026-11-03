"""#12: the Vortex Trial skill lists, per section, as the wiki states them -> scripts/data/vortex_trials_skills.json.

    python scripts/extract_trial_skills.py WIKI_CACHE_DIR [--out scripts/data/vortex_trials_skills.json]

WIKI_CACHE_DIR holds the MediaWiki API answers the round-8 scrape saved (out/qa/current/fresh_round8_20261031/
wiki/trials/*.json: {title, revisions: [{revid, timestamp, text}]}; latest revisions, the end-of-service versions).
For every mission of scripts/data/vortex_trials_wiki.json, its {{Quest}} block's notes are read as:

  '''Section'''             a section header: "General skillset", "Phase 1", a boss's name, "Enrage", ...
  *''Skill name'' - text    a skill (its effect text, wiki markup stripped)
  **...                     a note on the last skill ("Used on turn 3 and every 4 turns afterwards", ...)
  *Has N HP / N% ...        a section fact (HP, passive mitigation)

Nothing is interpreted here; scripts/gen_vortex_trials.py turns the notes into AI rows.  Each record keeps the
page and revid it came from.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DATASET = HERE / 'data' / 'vortex_trials_wiki.json'
OUT = HERE / 'data' / 'vortex_trials_skills.json'
MARKUP = re.compile(r"\{\{[Cc]olor\|[^|}]*\|([^}]*)\}\}|\{\{[^}]*\}\}|\[\[(?:[^|\]]*\|)?([^\]]*)\]\]|'''|''|<[^>]+>")


def clean(s: str) -> str:
    return re.sub(r'\s+', ' ', MARKUP.sub(lambda m: m.group(1) or m.group(2) or '', s)).strip()


def quests(text: str):
    for m in re.finditer(r'\{\{Quest\s*\n(.*?)\n\}\}', text, re.S):
        body = m.group(1)
        q = re.search(r'\|quest\s*=\s*(.*)', body)
        yield (q.group(1).strip() if q else ''), body


def parse_notes(body: str) -> tuple[list, list, list]:
    """(rules, sections, facts): rules = the notes before the first section; sections = [{section, skills}]."""
    notes = re.search(r'\|notes\s*=(.*?)(?=\n\|[a-z0-9]+\s*=|\Z)', body, re.S)
    text = notes.group(1) if notes else ''
    # Two layouts.  A: "*''Skill'' - effect" with "**Used on turn 3" notes under it.  B (the Gate of Memoirs pages):
    # "*Turn 3 & every 4 turns afterwards" with "**''Skill'' - effect" under it -- the bullet is the skills'
    # condition, recorded as their first note.
    rules, sections, current, cond = [], [], None, None
    skill = re.compile(r"^\*+\s*''(.+?)''\s*-\s*(.*)$")

    def add_skill(m, notes):
        nonlocal current
        if current is None:
            current = {'section': '', 'facts': [], 'skills': []}
            sections.append(current)
        current['skills'].append({'name': clean(m.group(1)), 'effect': clean(m.group(2)), 'notes': notes})

    for raw in text.splitlines():
        s = raw.strip()
        if not s or s.startswith('----'):
            continue
        h = re.match(r"^'''(.+?)'''\s*$", s)
        if h:
            current, cond = {'section': clean(h.group(1)), 'facts': [], 'skills': []}, None
            sections.append(current)
            continue
        depth = len(s) - len(s.lstrip('*'))
        m = skill.match(s)
        if depth == 1:
            if m:
                cond = None
                add_skill(m, [])
            elif current is None:
                # a page without section headers: the bullet is a rule, and also the condition of any skills under it
                rules.append(clean(s.lstrip('*')))
                cond = clean(s.lstrip('*'))
            elif re.match(r'^\*\s*(Has |Passively )', s):
                current['facts'].append(clean(s.lstrip('*')))
                cond = None
            else:
                cond = clean(s.lstrip('*'))
        elif depth == 2 and m and cond is not None:
            add_skill(m, [cond])
        elif depth >= 2:
            if current and current['skills']:
                current['skills'][-1]['notes'].append(clean(s.lstrip('*')))
            elif current is not None:
                current['facts'].append(clean(s.lstrip('*')))
    return rules, sections


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('cache', type=Path)
    ap.add_argument('--out', type=Path, default=OUT)
    a = ap.parse_args(argv)
    pages = {}
    for p in sorted(a.cache.glob('*.json')):
        d = json.loads(p.read_text(encoding='utf-8'))
        for rev in d.get('revisions') or []:
            for name, body in quests(rev['text']):
                pages.setdefault(name.lower(), (d['title'], rev['revid'], body))
    out, missing = [], []
    for m in json.loads(DATASET.read_text(encoding='utf-8'))['missions']:
        hit = pages.get(m['name'].lower())
        if hit is None:
            missing.append(m['mission'])
            continue
        title, revid, body = hit
        rules, sections = parse_notes(body)
        out.append({'mission': m['mission'], 'name': m['name'], 'page': title, 'revid': revid,
                    'rules': rules, 'sections': sections})
    a.out.write_text(json.dumps({'source': 'bravefrontierglobal.fandom.com, latest revisions (round-8 cache)',
                                 'missions': out}, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')
    print(f'{len(out)} mission(s) -> {a.out}; no quest block: {missing}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
