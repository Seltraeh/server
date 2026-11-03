"""#39 round 4: MissionStart sends at most one capture per wave -- the one the client keeps.

python scripts/test_mission_capture_wire.py PATH_TO_DEBUG_EXE [--port 19986]

Client evidence (round 4, supported x86 build fb4f3c25..., arm64 libgame.so corroborating): drop_info
(LE6JkUp7) records are split on '@' and fields on '|' (MissionDropInfoList::parse, x86 0x57FCB0); field [1]
goes ONCE to parseUnitDropInfo (x86 0x57F340), which splits on ':' only and builds one drop
(slot:_:unit:level:type[:extraSkill], type read by a stream StrToInt that stops at '-'); getObject (x86
0x20D090) returns the first record per battle order.  The client therefore keeps one capture per wave: with
three pre-rolled captures it showed and settled only the first, claiming "20202:1:2:0" -- type from "2-1",
extra skill "0" from the second entry (round-4 client run w1_base).

A disposable archive built here (hard links to deploy/archive; mission.json with Metal Parade! 100600's
waves capturing 0 / 1 (slot 2) / 3 / 1 (slot 0) / 2 (from slot 1) monsters at 0 or 100 %, all flagged
authored_capture so the 40 % floor leaves them) makes WHICH monsters capture deterministic; only types are
rolled.  Checked: every wave sends no capture or exactly the first rolled one, as one complete entry with
no extra-skill field; a client-side model of the parse keeps exactly what was sent; the client's claim
settles with exact units and types; a repeated settlement adds nothing; the legacy claim with ":0" still
settles.  Production missions (100601/100614 here) send at most one capture per wave too.
"""
import argparse
import json
import os
import shutil
import sys
from pathlib import Path

from bf_testkit import (ROOT, Fixture, IsolatedServer, Client, Checker, mission_start_body, mission_end_body,
                        started_serial)
from bf_testkit import qa_dir  # noqa: E402

MISSION = 100600
PATTERN = [[0, 0, 0], [0, 0, 100, 0], [100, 100, 100], [100, 0, 0], [0, 100, 100]]
# Per wave: (slot, unit) the client keeps -- the first monster rolled, in monster order.
EXPECT = [None, (2, 60132), (0, 20202), (0, 50203), (1, 20202)]


def build_archive(folder: Path) -> Path:
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    src = ROOT / 'deploy' / 'archive'
    rows = json.loads((src / 'mission.json').read_text(encoding='utf-8'))
    stages = next(r for r in rows if r['id'] == MISSION)['stages']
    assert [len(s['battle_monsters']) for s in stages] == [len(p) for p in PATTERN], 'wave sizes changed'
    for stage, chances in zip(stages, PATTERN):
        for monster, chance in zip(stage['battle_monsters'], chances):
            monster['unit_drop_chance'] = chance
            monster['authored_capture'] = True
    for p in src.iterdir():
        if p.is_file() and p.name != 'mission.json':
            os.link(p, folder / p.name)
    (folder / 'mission.json').write_text(json.dumps(rows, indent=1, ensure_ascii=False), encoding='utf-8')
    return folder


def waves(drop_info: str) -> list[tuple[int, str]]:
    """(battle order, unit-drop field) per '@' record, as MissionDropInfoList::parse reads them."""
    out = []
    for record in drop_info.split('@'):
        fields = record.split('|')
        out.append((int(fields[0]), fields[1] if len(fields) > 1 else ''))
    return out


def stream_int(token: str) -> int:
    """CommonUtils::StrToInt (x86 0x65E760): a stream read that stops at the first non-digit."""
    digits = ''
    for ch in token.lstrip():
        if not ch.isdigit():
            break
        digits += ch
    return int(digits or 0)


def client_keeps(field: str) -> dict | None:
    """parseUnitDropInfo's one drop (None for an empty or ' ' field)."""
    if field in ('', ' '):
        return None
    t = field.split(':')
    return {'slot': stream_int(t[0]), 'unit': t[2], 'level': stream_int(t[3]), 'type': stream_int(t[4]),
            'extra': t[5] if len(t) >= 6 else None}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19986)
    args = ap.parse_args()
    archive = build_archive(qa_dir('mission_capture_archive'))
    fx = Fixture.create(qa_dir('mission_capture'), port=args.port, archive_root=archive)
    record = next(r for r in json.loads((archive / 'mission.json').read_text(encoding='utf-8')) if r['id'] == MISSION)
    check = Checker()
    with IsolatedServer(args.exe, fx):
        c = Client(fx)
        db = fx.db()
        db.execute('UPDATE user_info SET level=50, exp=0, energy=20000, energy_full_ts=0 WHERE id=?', (c.user,))
        db.commit()

        def units() -> list[tuple]:
            return [tuple(r) for r in db.execute(
                'SELECT unit_id, unit_type_id, unit_lvl, extra_skill_id FROM user_units WHERE user_id=? '
                'ORDER BY user_unit_id', (c.user,))]

        # ---- the wire: one capture per wave, the first rolled ------------------------------------------
        sent, kept, problems = [], [], []
        for _ in range(12):
            reply = c.call('MissionStart', body=mission_start_body(MISSION))
            if 'error' in reply:
                problems.append(reply['error'])
                break
            info = reply['Kz7qfSs5'][0]['LE6JkUp7']
            sent.append(info)
            per_wave = waves(info)
            if [w for w, _ in per_wave] != [1, 2, 3, 4, 5]:
                problems.append(f'battle orders {per_wave}')
            for (wave, field), expect in zip(per_wave, EXPECT):
                drop = client_keeps(field)
                if expect is None:
                    if drop is not None:
                        problems.append(f'wave {wave}: a capture where none was rolled: {field!r}')
                    continue
                if '-' in field or drop is None or (drop['slot'], int(drop['unit'])) != expect:
                    problems.append(f'wave {wave}: sent {field!r}, expected one entry for slot/unit {expect}')
                elif field.count(':') != 4 or drop['extra'] is not None or drop['level'] != 1 \
                        or not 1 <= drop['type'] <= 5:
                    problems.append(f'wave {wave}: entry {field!r} is not slot:0:unit:level:type')
            kept.append([client_keeps(f) for _, f in per_wave])
            c.call('MissionEnd', body=mission_end_body(MISSION, serial=started_serial(reply)))
        check('12 starts: wave 1 sends no capture; each other wave sends exactly the FIRST rolled monster '
              '(slot 2 alone, slot 0 of three, slot 0 alone, slot 1 of two) as one slot:0:unit:1:type entry',
              not problems and len(sent) == 12, problems[:4] or sent[:1])
        check('no unit-drop field carries "-" (it has no consumer in the client parse)',
              all('-' not in f for info in sent for _, f in waves(info)), sent[:1])
        types = sorted({d['type'] for k in kept for d in k if d})
        check('types are rolled per start, within the drop range 1..5', types and set(types) <= {1, 2, 3, 4, 5}, types)

        # ---- settlement: the client's claim, exactly; a repeat adds nothing ----------------------------
        reply = c.call('MissionStart', body=mission_start_body(MISSION))
        serial = started_serial(reply)
        drops = [client_keeps(f) for _, f in waves(reply['Kz7qfSs5'][0]['LE6JkUp7'])]
        claim = ','.join(f'{d["unit"]}:{d["level"]}:{d["type"]}' for d in drops if d)
        before = units()
        end = c.call('MissionEnd', body=mission_end_body(MISSION, serial=serial, units=claim))
        added = units()[len(before):]
        check('settlement: the claim built from the sent entries adds exactly those units, types and level 1',
              'error' not in end and added == [(int(d['unit']), d['type'], 1, 0) for d in drops if d],
              (end.get('error'), claim, added))
        after = units()
        c.call('MissionEnd', body=mission_end_body(MISSION, serial=serial, units=claim))
        check('a repeated settlement of the same battle adds nothing', units() == after, len(units()) - len(after))
        reply = c.call('MissionStart', body=mission_start_body(MISSION))
        legacy = '60132:1:1,20202:1:2:0,50203:1:2,20202:1:5:0'     # what the old encoding made the client claim
        before = units()
        end = c.call('MissionEnd', body=mission_end_body(MISSION, serial=started_serial(reply), units=legacy))
        check('the legacy claim with an ":0" extra-skill field still settles, with the claimed types',
              'error' not in end and units()[len(before):] == [(60132, 1, 1, 0), (20202, 2, 1, 0), (50203, 2, 1, 0),
                                                               (20202, 5, 1, 0)], (end.get('error'), units()[len(before):]))

        # ---- production data: at most one capture per wave -----------------------------------------------
        prod = {}
        for mission in (100601, 100614):
            sizes = [len(s['battle_monsters']) for s in
                     next(r for r in json.loads((ROOT / 'deploy/archive/mission.json').read_text(encoding='utf-8'))
                          if r['id'] == mission)['stages']]
            counts = [0] * len(sizes)
            bad = []
            for _ in range(20):
                reply = c.call('MissionStart', body=mission_start_body(mission))
                for i, (_, field) in enumerate(waves(reply['Kz7qfSs5'][0]['LE6JkUp7'])):
                    drop = client_keeps(field)
                    if drop:
                        counts[i] += 1
                        if '-' in field or field.count(':') != 4 or not 0 <= drop['slot'] < sizes[i]:
                            bad.append(field)
                c.call('MissionEnd', body=mission_end_body(mission, serial=started_serial(reply)))
            prod[mission] = counts
            check(f'{mission} (production data): every wave sends at most one complete capture entry', not bad, bad[:3])
        print(f'production captures per wave over 20 starts (informational): {prod}')
        db.close()
    return check.summary('mission capture')


if __name__ == '__main__':
    sys.exit(main())
