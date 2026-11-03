"""Tutorial chapter checkpoints written by MissionEnd (Mission.cpp kFirst/kSecondTutorialCheckpoint).

    python scripts/test_tutorial_checkpoint_wire.py PATH_TO_DEBUG_EXE [--port 19974] [--out DIR]

Isolated server on a copy of the live save under out/qa/current/tutorial_checkpoint.

tutorial_status (9sQM2XcN) is a CHAPTER id.  The client's opening runs 1..9, then
10 (tuto15, the free summon), 11 (tuto16, the farewell) and 12 (done):
TutorialStepScene::checkChapter (arm64 0x1B9275C) ends the tutorial for any chapter
above 11.  Clearing mission 2 therefore has to write 10.  A checkpoint of 50 -- the
triggerable band, which GameScene::checkTouchFooter enters from chapter 12 --
finished the tutorial on the spot and skipped the summon (S19 Fresh client,
Session 20).  The same checkpoint pays tuto15's 5 gems, so it fires only for a
player still inside the opening band (1..9): never on a loss, and never for a
replay at 10, by a finished player (12) or in the triggerable band (50).
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (Checker, Client, Fixture, IsolatedServer, mission_end_body,  # noqa: E402
                        mission_start_body, qa_dir, started_serial)


def find(obj, key):
    """The first value of `key` anywhere in a reply."""
    if isinstance(obj, dict):
        if key in obj:
            return obj[key]
        obj = list(obj.values())
    if isinstance(obj, list):
        for v in obj:
            hit = find(v, key)
            if hit is not None:
                return hit
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19974)
    ap.add_argument('--out', default=str(qa_dir('tutorial_checkpoint')))
    args = ap.parse_args()
    check = Checker()
    fx = Fixture.create(args.out, port=args.port)
    db = fx.db()

    def execute(sql, params=()):
        r = db.execute(sql, params)
        db.commit()
        return r

    user = execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]

    def state():
        return tuple(execute('SELECT tutorial_status, tutorial_end_flag, gems FROM user_info WHERE id=?',
                             (user,)).fetchone())

    def set_tutorial(status, end_flag):
        execute('UPDATE user_info SET tutorial_status=?, tutorial_end_flag=? WHERE id=?', (status, end_flag, user))

    with IsolatedServer(args.exe, fx):
        c = Client(fx)

        def play(mid, status=2):
            execute('UPDATE user_info SET energy=20000,energy_full_ts=0 WHERE id=?', (user,))
            start = c.call('MissionStart', body=mission_start_body(mid))
            assert 'error' not in start, (mid, start.get('error'))
            return c.call('MissionEnd', body=mission_end_body(mid, serial=started_serial(start), status=status))

        # ---- mission 2 from inside the opening band: chapter 10 and the summon's gems ----
        for opening in (2, 9):
            set_tutorial(opening, 0)
            before = state()
            reply = play(2)
            after = state()
            label = f'mission 2 cleared at chapter {opening}'
            check(f'{label}: MissionEnd answers', 'error' not in reply, reply.get('error'))
            check(f'{label}: chapter 10 (tuto15, the free summon), not 50', after[0] == 10, after)
            check(f'{label}: the tutorial is not marked finished', after[1] == 0, after)
            check(f"{label}: tuto15's 5 gems paid", after[2] - before[2] == 5, (before, after))
        chapter = find(c.call('UserInfo'), '9sQM2XcN')
        check('UserInfo reports chapter 10 afterwards', str(chapter) == '10', chapter)

        # ---- a loss moves nothing --------------------------------------------------------
        set_tutorial(2, 0)
        before = state()
        play(2, status=3)
        check('mission 2 lost at chapter 2: chapter, flag and gems unchanged', state() == before, (before, state()))

        # ---- everyone else is left alone ------------------------------------------------
        for status, end_flag, label in ((10, 0, 'a replay at chapter 10 (summon not yet done)'),
                                        (12, 1, 'a replay by a finished player (12)'),
                                        (50, 1, 'a replay in the triggerable band (50)')):
            set_tutorial(status, end_flag)
            before = state()
            reply = play(2)
            after = state()
            check(f'{label}: MissionEnd answers', 'error' not in reply, reply.get('error'))
            check(f'{label}: chapter and flag unchanged', after[:2] == before[:2], (before, after))
            check(f'{label}: no gems paid again', after[2] == before[2], (before, after))

        # ---- mission 1: the first checkpoint only moves chapter 1 forward -------------------
        set_tutorial(1, 0)
        play(1)
        check('mission 1 cleared at chapter 1: chapter 2', state()[0] == 2, state())
        set_tutorial(10, 0)
        play(1)
        check('mission 1 cleared at chapter 10: chapter stays 10', state()[0] == 10, state())

    return check.summary('tutorial checkpoint')


if __name__ == '__main__':
    sys.exit(main())
