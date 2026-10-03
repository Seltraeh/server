"""Merit Exchange (AchievementTrade m9LiF6P2): quantity, stock, payment and refusals.

    python scripts/test_merit_exchange_wire.py PATH_TO_DEBUG_EXE [--port 19987] [--out DIR]

Isolated server on a copy of the live save under out/qa/current/merit_exchange.

The shop's own rules (libgame.so, RandallAchievementShopDetailScene):
  * the quantity slider runs 1 .. AchievementTradeMst.limit_count (S8rdp9zk) minus
    what the player has bought (UserAchievementTradeInfo H6k1LIxC) -- sliderSet
    @0x1A62FBC;
  * the price is quantity x price (getBeedPointStr), and Buy is refused client-side
    when the stock is used up or the points are short (touchBegan @0x1A60890);
  * a purchase pays quantity x the reward's own count.
The server read 9Hau45Jj -- 1 on every row -- as the limit, so the reported buy
(offer 91000059, 400 Ignis Shards at 50 = 20,000 points) was refused, and every
offer could be bought only once.  Refusals must change nothing and must be the
recoverable kind (GmeErrorCommand 6, back Home), not an app exit (4).
"""
import argparse
import json
import sys
import threading
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import ROOT, Checker, Client, Fixture, IsolatedServer, qa_dir  # noqa: E402

IGNIS = '91000059'      # Ignis Shard 4:800901:1, 50 points, limit 400
LEGEND = '80000059'     # Legend Stone 4:110100:1, 4000 points, limit 99

OFFERS = {r['Mdgsh04u']: r for r in next(iter(json.loads(
    (ROOT / 'deploy/mst/achievement_trade_mst.json').read_text(encoding='utf-8')).values()))}
UNITS = {r['id'] for r in json.loads((ROOT / 'deploy/archive/unit.json').read_text(encoding='utf-8'))}


def reward(offer_id):
    parts = OFFERS[offer_id]['qBAb07rh'].split(':')
    return int(parts[0]), parts[1], int(parts[2])


def first_offer(kind, limit=None, count=None, minimum_limit=None):
    for oid, row in OFFERS.items():
        t, target, n = reward(oid)
        lim = int(row['S8rdp9zk'])
        if t != kind or (limit is not None and lim != limit) or (count is not None and n != count):
            continue
        if minimum_limit is not None and lim < minimum_limit:
            continue
        if kind == 6 and int(target) not in UNITS:
            continue
        return oid
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--port', type=int, default=19987)
    ap.add_argument('--out', default=str(qa_dir('merit_exchange')))
    args = ap.parse_args()
    check = Checker()

    check('data: offer 91000059 is Ignis Shard x1 at 50 points, 400 per player',
          reward(IGNIS) == (4, '800901', 1) and OFFERS[IGNIS]['3EWLm0sA'] == '50'
          and OFFERS[IGNIS]['S8rdp9zk'] == '400', OFFERS[IGNIS])
    check('data: 9Hau45Jj is 1 on every offer (it is not the limit)',
          {r['9Hau45Jj'] for r in OFFERS.values()} == {'1'})
    unit_offer = first_offer(6, minimum_limit=3)
    sphere_offer = first_offer(7, limit=1)
    material_offer = first_offer(5, count=10, minimum_limit=2)
    art_offer = first_offer(15, limit=1)
    check('data: a unit, a sphere, a 10-per-buy material and an alternate-art offer exist',
          all((unit_offer, sphere_offer, material_offer, art_offer)),
          (unit_offer, sphere_offer, material_offer, art_offer))

    fx = Fixture.create(args.out, port=args.port)
    db = fx.db()

    def execute(sql, params=()):
        r = db.execute(sql, params)
        db.commit()
        return r

    user = execute('SELECT id FROM user_info LIMIT 1').fetchone()[0]

    def points():
        return execute('SELECT achieve_point FROM user_info WHERE id=?', (user,)).fetchone()[0]

    def set_points(value):
        execute('UPDATE user_info SET achieve_point=? WHERE id=?', (value, user))

    def bought(oid):
        row = execute('SELECT count FROM user_achievement_trades WHERE user_id=? AND trade_id=?',
                      (user, oid)).fetchone()
        return row[0] if row else 0

    def reset(oid):
        execute('DELETE FROM user_achievement_trades WHERE user_id=? AND trade_id=?', (user, oid))

    def items(item_id):
        return execute('SELECT COALESCE(SUM(item_num),0) FROM user_items WHERE user_id=? AND item_id=?',
                       (user, int(item_id))).fetchone()[0]

    def units(unit_id):
        return execute('SELECT COUNT(*) FROM user_units WHERE user_id=? AND unit_id=?',
                       (user, str(unit_id))).fetchone()[0]

    def state():
        return (points(), tuple(execute('SELECT trade_id,count FROM user_achievement_trades WHERE user_id=?'
                                        ' ORDER BY trade_id', (user,)).fetchall()),
                execute('SELECT COALESCE(SUM(item_num),0) FROM user_items WHERE user_id=?', (user,)).fetchone()[0],
                execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (user,)).fetchone()[0],
                execute('SELECT COUNT(*) FROM user_unit_alt_art WHERE user_id=?', (user,)).fetchone()[0])

    with IsolatedServer(args.exe, fx):
        c = Client(fx)

        def buy(oid, quantity, raw=None):
            node = {'Mdgsh04u': oid, 'H6k1LIxC': quantity if raw else str(quantity)}
            return c.call('AchievementTrade', body={'rX74GNsm': [node]} if raw is None else raw)

        def refused(label, reply, before):
            err = reply.get('error') or {}
            check(f'{label}: refused', bool(err), reply)
            check(f'{label}: recoverable (back Home, cmd 6), not an app exit',
                  str(err.get('iPD12YCr')) == '6', err)
            check(f'{label}: nothing debited, granted or counted', state() == before, (state(), before))

        def trade_row(reply, oid):
            return next((r for r in reply.get('9j3ALx8I') or [] if r.get('Mdgsh04u') == oid), None)

        # ---- quantity 1 ------------------------------------------------------------
        reset(IGNIS)
        set_points(100000)
        shards = items(800901)
        reply = buy(IGNIS, 1)
        check('x1: accepted', 'error' not in reply, reply.get('error'))
        check('x1: 50 points, one shard, bought 1',
              points() == 100000 - 50 and items(800901) == shards + 1 and bought(IGNIS) == 1,
              (points(), items(800901) - shards, bought(IGNIS)))
        row = trade_row(reply, IGNIS)
        check('x1: the reply refreshes the balance and this offer\'s count',
              str((reply.get('Bnc4LpM8') or {}).get('idfCDG70')) == str(points())
              and row is not None and row.get('H6k1LIxC') == '1', (reply.get('Bnc4LpM8'), row))
        check('x1: the reply carries the warehouse with the shards',
              sum(int(r['wgV86x1q']) for r in reply.get('9wjrh74P') or [] if r.get('kixHbe54') == '800901')
              == items(800901))
        check('x1: the offer keeps no expiry countdown (VDKB0Y5h -1)', row is not None and row.get('VDKB0Y5h') == '-1',
              row)

        # ---- the remaining stock, then nothing more --------------------------------
        before_pts = points()
        reply = buy(IGNIS, 399)
        check('x399: the rest of the stock in one purchase', 'error' not in reply and bought(IGNIS) == 400
              and points() == before_pts - 399 * 50 and items(800901) == shards + 400,
              (reply.get('error'), bought(IGNIS), before_pts - points()))
        before = state()
        refused('sold out (401st)', buy(IGNIS, 1), before)

        # ---- the reported purchase: 400 at once costs exactly 20,000 ---------------
        reset(IGNIS)
        set_points(20000)
        shards = items(800901)
        reply = buy(IGNIS, 400)
        check('x400: accepted with exactly 20,000 points', 'error' not in reply, reply.get('error'))
        check('x400: costs 20,000 and grants exactly 400', points() == 0 and items(800901) == shards + 400
              and bought(IGNIS) == 400, (points(), items(800901) - shards, bought(IGNIS)))

        # ---- partial stock then the remainder ---------------------------------------
        reset(IGNIS)
        set_points(100000)
        a = buy(IGNIS, 150)
        b = buy(IGNIS, 250)
        check('150 then 250: both accepted and the stock is spent',
              'error' not in a and 'error' not in b and bought(IGNIS) == 400 and points() == 100000 - 400 * 50,
              (a.get('error'), b.get('error'), bought(IGNIS)))
        reset(IGNIS)
        execute('UPDATE user_info SET achieve_point=100000 WHERE id=?', (user,))
        buy(IGNIS, 100)
        before = state()
        refused('above the remaining stock (301 of 300 left)', buy(IGNIS, 301), before)

        # ---- invalid quantities and malformed bodies -----------------------------------
        reset(IGNIS)
        set_points(100000)
        before = state()
        refused('quantity 0', buy(IGNIS, 0), before)
        refused('quantity -5', buy(IGNIS, -5), before)
        refused('quantity above the per-player total (401)', buy(IGNIS, 401), before)
        refused('quantity overflowing int32', buy(IGNIS, '99999999999', raw={'rX74GNsm': [
            {'Mdgsh04u': IGNIS, 'H6k1LIxC': '99999999999'}]}), before)
        refused('non-numeric quantity', buy(IGNIS, 'abc', raw={'rX74GNsm': [
            {'Mdgsh04u': IGNIS, 'H6k1LIxC': 'abc'}]}), before)
        refused('unquoted quantity (the client always quotes)', buy(IGNIS, 400, raw={'rX74GNsm': [
            {'Mdgsh04u': IGNIS, 'H6k1LIxC': 400}]}), before)
        refused('no offer named', buy(IGNIS, 1, raw={'rX74GNsm': []}), before)
        refused('two offers in one request', buy(IGNIS, 1, raw={'rX74GNsm': [
            {'Mdgsh04u': IGNIS, 'H6k1LIxC': '1'}, {'Mdgsh04u': LEGEND, 'H6k1LIxC': '1'}]}), before)
        refused('unknown offer', buy('12345678', 1), before)
        set_points(49)
        before = state()
        refused('49 points for a 50-point shard', buy(IGNIS, 1), before)
        set_points(19950)
        before = state()
        refused('19,950 points for 400 shards (20,000)', buy(IGNIS, 400), before)

        # ---- two taps racing for the last of the stock ----------------------------------
        reset(IGNIS)
        set_points(1000000)
        buy(IGNIS, 399)
        results = []
        threads = [threading.Thread(target=lambda: results.append(buy(IGNIS, 1))) for _ in range(2)]
        [t.start() for t in threads]
        [t.join() for t in threads]
        check('race: only one of two simultaneous buys of the last shard succeeds',
              sorted('error' in r for r in results) == [False, True] and bought(IGNIS) == 400,
              ([r.get('error') for r in results], bought(IGNIS)))

        # ---- the other kinds of offer, so the corrected limit regresses none ------------
        set_points(1000000)
        reset(LEGEND)
        stones = items(110100)
        reply = buy(LEGEND, 2)
        check('Legend Stone x2: 8,000 points, two stones, 97 left',
              'error' not in reply and items(110100) == stones + 2 and points() == 1000000 - 8000
              and trade_row(reply, LEGEND) is not None and trade_row(reply, LEGEND).get('H6k1LIxC') == '2',
              (reply.get('error'), items(110100) - stones))

        t, unit_id, per = reward(unit_offer)
        reset(unit_offer)
        have = units(unit_id)
        reply = buy(unit_offer, 3)
        check(f'unit offer {unit_offer} x3: three units, listed in the reply',
              'error' not in reply and units(unit_id) == have + 3 * per
              and len(reply.get('qC2tJs4E') or []) == 3 * per and reply.get('GV81ctzR'),
              (reply.get('error'), units(unit_id) - have, len(reply.get('qC2tJs4E') or [])))

        t, sphere_id, per = reward(sphere_offer)
        reset(sphere_offer)
        have = items(sphere_id)
        reply = buy(sphere_offer, 1)
        check(f'sphere offer {sphere_offer}: one sphere', 'error' not in reply and items(sphere_id) == have + per,
              reply.get('error'))
        before = state()
        refused(f'sphere offer {sphere_offer}: a second one (limit 1)', buy(sphere_offer, 1), before)

        t, material_id, per = reward(material_offer)
        reset(material_offer)
        have = items(material_id)
        reply = buy(material_offer, 2)
        check(f'material offer {material_offer} x2: 2 x {per} materials',
              'error' not in reply and items(material_id) == have + 2 * per, (reply.get('error'),
                                                                              items(material_id) - have))

        reset(art_offer)
        execute('DELETE FROM user_unit_alt_art WHERE user_id=? AND unit_id=?', (user, int(reward(art_offer)[1])))
        reply = buy(art_offer, 1)
        check(f'alternate art {art_offer}: unlocked', 'error' not in reply, reply.get('error'))
        before = state()
        refused(f'alternate art {art_offer}: cannot be paid for twice', buy(art_offer, 1), before)

        balance = points()
        counts = {oid: bought(oid) for oid in (IGNIS, LEGEND, unit_offer)}

    # ---- reconnect: what was bought is still bought, the balance still spent ---------
    with IsolatedServer(args.exe, fx, log_name='server_restart.log'):
        c = Client(fx)
        info = c.call('GetAchievementInfo')
        rows = {r.get('Mdgsh04u'): r for r in info.get('9j3ALx8I') or []}
        check('restart: GetAchievementInfo answers', 'error' not in info, info.get('error'))
        check('restart: every purchase count survives', all(rows.get(oid, {}).get('H6k1LIxC') == str(n)
                                                            for oid, n in counts.items()),
              {oid: rows.get(oid, {}).get('H6k1LIxC') for oid in counts})
        check('restart: the balance survives', str((info.get('Bnc4LpM8') or {}).get('idfCDG70')) == str(balance),
              (info.get('Bnc4LpM8'), balance))
    db.close()
    return check.summary('merit exchange')


if __name__ == '__main__':
    sys.exit(main())
