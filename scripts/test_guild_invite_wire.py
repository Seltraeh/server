"""Guild invite regressions.  ONLY run against the isolated server on 19960.

    python scripts/test_guild_invite_wire.py out/guild-invite-tests-2026-09-26/gme.sqlite

The save must be a FRESH COPY under out/ (sqlite3's backup API) whose player
already leads a guild and has at least two friends; the test empties that
guild down to its founder first, then invites one of them.

The 2026-09-26 crash: tapping a candidate in the Guild Hall makes
GuildHallScene::touchEnded look the candidate up in the list 8lAroepR fills
and clone() the result with no null check.  The server sent only the cards
(fRaBu6et), so the lookup returned null.  The invite button then sends
GuildJoin (bfa2D1bp) with request type 2, which was a `{}` stub.

The 2026-09-27 crash: promoting a member sends GuildMemberUpdate (ad81b8at)
with the member's type - 1; the reply carried the roster, which clears the
client's member list, and the member screen then updated a freed member.
Ranks are GUILD_RANK_NAME_1..4 (Guild Master, Vice, Officer, Member); the
server used to send 0 for every friend.  The last section dissolves the
copied save's guild, so it runs last.
"""
import base64
import gzip
import json
import sqlite3
import sys
import urllib.request
from pathlib import Path

from Crypto.Cipher import AES

ROOT = Path(__file__).resolve().parents[1]
DB = Path(sys.argv[1]).resolve()
assert DB.is_relative_to(ROOT / 'out'), 'Refusing to modify a live save'
HOST = 'http://127.0.0.1:19960/bf/gme/action.php'
db = sqlite3.connect(DB, timeout=15)
user, login = db.execute('SELECT id, gumi_user_id FROM user_info LIMIT 1').fetchone()

# Every key GuildRecommendFriendInfoResponse::readParam @0x1C64DF4 accepts,
# from tools/readparam_map.py.
PROFILE_KEYS = {
    'h7eY3sAK', 'B5JQyV8j', '2Fh3J7ng', '96Nxs2WQ', '0CAQ6wUe', 'pn16CNah', '4A6LzBxr', 'e7DK0FQT',
    '67CApcti', 'q08xLEsy', 'PWXu25cg', 'cuIWp89g', 'TokWs1B3', 'RT4CtH5d', 't4m1RH6Y', 'GcMD0hy6',
    'e6mY8Z0k', 'C1HZr3pb', 'X6jf8DUw', 'a1Jp3TVb', 's2WnRw9N', '5JbjC3Pp', 'Ge8Yo32T', 'mZA7fH2v',
    '98WfKiyA', 'bM7RLu5K', '8DtoZdXE', 'JmFn3g9t', 'U4pMNjy0', 'nBTx56W9', 'nj9Lw7mV', '3NbeC8AB',
    'iEFZ6H19', 'RQ5GnFE2', '3InKeya4', 'paND1zM8', '7x3pPB2C', 'Sv80kL5r', 'cP83zNsv', 'LjY4DfRg',
    '2pAyFjmZ', 'sD73jd20', 'Fnxab5CN', 'zsiAn9P1', 'yu18xScw', '49sa3sld',
}
assert len(PROFILE_KEYS) == 46
failures = 0


def check(label, cond, detail=''):
    global failures
    print(('PASS  ' if cond else 'FAIL  ') + label + ('' if cond else f'  {detail}'))
    failures += 0 if cond else 1


def call(group, key, body):
    body['IKqx1Cn9'] = [{'iN7buP2h': login, 'h7eY3sAK': user}]
    k = key.encode().ljust(16, b'\0')[:16]
    raw = json.dumps(body).encode()
    pad = 16 - len(raw) % 16
    cipher = AES.new(k, AES.MODE_ECB)
    envelope = {'F4q6i9xe': {'Hhgi79M1': group, 'aV6cLn3v': 'guildinvite'},
                'a3vSYuq2': {'Kn51uR4Y': base64.b64encode(cipher.encrypt(raw + bytes([pad]) * pad)).decode()}}
    req = urllib.request.Request(HOST, json.dumps(envelope).encode(), {'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=60) as r:
        data = r.read()
        if r.headers.get('Content-Encoding') == 'gzip':
            data = gzip.decompress(data)
    outer = json.loads(data)
    if 'b5PH6mZa' in outer:
        return {'error': outer['b5PH6mZa']}
    plain = cipher.decrypt(base64.b64decode(outer['a3vSYuq2']['Kn51uR4Y']))
    return json.loads(plain[:-plain[-1]])


def candidates():
    return call('ja5Enusw', '8upheqaC', {})


def guild():
    return call('138ba8d4', '23gD81ia', {})


def invite(guild_id, friend_id, request_type='2'):
    return call('bfa2D1bp', '9b3abdk1', {'aj38Jk10': [
        {'sD73jd20': str(guild_id), 'h7eY3sAK': friend_id, 'xvkLFco0': request_type}]})


def members():
    return {r[0] for r in db.execute(
        'SELECT m.member_id FROM user_guild_members m JOIN user_guilds g ON g.guild_id = m.guild_id'
        ' WHERE g.owner_user_id = ?', (user,))}


# Fixture (the copy only): leave the founder alone in their guild, so every
# friend is invitable whatever the live save looked like when it was copied.
db.execute('DELETE FROM user_guild_members WHERE member_id != ? AND guild_id IN'
           ' (SELECT guild_id FROM user_guilds WHERE owner_user_id = ?)', (user, user))
db.commit()

# --- the candidate list and the profiles it opens -------------------------------
g = guild()
check('the copied save leads a guild', 'error' not in g and len(g.get('IkdSufj5', [])) == 1, str(g)[:300])
gid = int(g['IkdSufj5'][0]['sD73jd20'])
count0 = int(g['IkdSufj5'][0]['SivJ9sL9'])

r = candidates()
cards, profiles = r.get('fRaBu6et', []), r.get('8lAroepR', [])
check('GuildRecomendedMember answers without an error', 'error' not in r, str(r.get('error')))
check('at least two invitable friends (the copy has to allow one invite and a leftover)', len(cards) >= 2, len(cards))
check('one profile per card, same user ids, same order (the tap looks the card up by id)',
      [c['h7eY3sAK'] for c in cards] == [p['h7eY3sAK'] for p in profiles],
      f"{[c['h7eY3sAK'] for c in cards]} vs {[p['h7eY3sAK'] for p in profiles]}")
check('every profile carries exactly the 46 keys the client reads',
      all(set(p) == PROFILE_KEYS for p in profiles), str([sorted(set(p) ^ PROFILE_KEYS) for p in profiles][:1]))
check('profile and card agree on name, unit and level',
      all(p['B5JQyV8j'] == c['B5JQyV8j'] and p['pn16CNah'] == c['1ctR6GHC'] and p['4A6LzBxr'] == c['8Ix0Soup']
          for c, p in zip(cards, profiles)))
check('profiles: base artwork, no guild, friend type 1, a real unit',
      all(p['2pAyFjmZ'] == '1' and p['sD73jd20'] == '0' and p['96Nxs2WQ'] == '1' and int(p['pn16CNah']) > 0
          for p in profiles), str(profiles[:1])[:400])
check('no current member is offered', not ({c['h7eY3sAK'] for c in cards} & members()))

# --- the invite -----------------------------------------------------------------
target = cards[0]['h7eY3sAK']
r = invite(gid, target)
check('GuildJoin type 2 answers without an error (the session stays up)', 'error' not in r, str(r.get('error')))
check('the invited friend is now a member (database)', target in members())
roster = [m['h7eY3sAK'] for m in r.get('csIuech30', [])]
check('the reply carries the refreshed roster with the new member', target in roster and user in roster, str(roster))
check('the reply carries the refreshed member count',
      int(r.get('IkdSufj5', [{}])[0].get('SivJ9sL9', -1)) == count0 + 1, str(r.get('IkdSufj5'))[:200])

r = candidates()
cards2, profiles2 = r.get('fRaBu6et', []), r.get('8lAroepR', [])
check('the new member leaves both candidate lists',
      target not in {c['h7eY3sAK'] for c in cards2} | {p['h7eY3sAK'] for p in profiles2})
check('... and the lists still pair up', [c['h7eY3sAK'] for c in cards2] == [p['h7eY3sAK'] for p in profiles2])

# --- refusals change nothing and never close the session ----------------------------
other = cards2[0]['h7eY3sAK']
before = members()
for label, args in (('an application (type 1)', (gid, other, '1')),
                    ('someone else\'s guild id', (gid + 1000, other, '2')),
                    ('an id that is not on the roster', (gid, 'NOT_A_FRIEND', '2')),
                    ('a current member again', (gid, target, '2'))):
    r = invite(*args)
    check(f'refused, no error: {label}', 'error' not in r and members() == before, str(r.get('error')))
check('the member count did not move on refusals',
      int(guild()['IkdSufj5'][0]['SivJ9sL9']) == count0 + 1)

# --- ranks and member management (GuildMemberUpdate) ---------------------------------
def roster():
    return {m['h7eY3sAK']: m for m in guild().get('csIuech30', [])}


def update(member_id, member_type):
    return call('ad81b8at', '2b1bDo2m', {'IkdSufj5': [{'sD73jd20': str(gid)}],
                                          'csIuech30': [{'h7eY3sAK': member_id, 'gr48vsdJ': str(member_type)}]})


def rank_of(member_id):
    return roster().get(member_id, {}).get('gr48vsdJ')


ros = roster()
check('ranks: the owner is Guild Master (1) and every friend a Member (4) -- 0 has no name',
      ros[user]['gr48vsdJ'] == '1' and all(m['gr48vsdJ'] == '4' for k, m in ros.items() if k != user),
      str({k: m['gr48vsdJ'] for k, m in ros.items()}))

r = update(target, 3)
check('promote (4 -> 3) answers {} -- no roster, the client updates its own list', r == {}, str(r)[:200])
check('... and the new rank is stored and served', rank_of(target) == '3', rank_of(target))
update(target, 2)
check('promote again (3 -> 2): Vice Guild Master', rank_of(target) == '2', rank_of(target))
r = update(target, 1)
check('promote to Guild Master (a hand-over) is refused without an error', r == {} and rank_of(target) == '2',
      f'{r} {rank_of(target)}')
update(target, 3)
check('demote (2 -> 3): Officer', rank_of(target) == '3', rank_of(target))
for bad in (5, -1, 'abc'):
    r = update(target, bad)
    check(f'rank {bad!r} is refused without an error', r == {} and rank_of(target) == '3', f'{r} {rank_of(target)}')
r = update(user, 2)
check('the owner cannot demote themselves', r == {} and rank_of(user) == '1', f'{r} {rank_of(user)}')
r = update('NOT_A_MEMBER', 3)
check('a rank for a non-member is refused without an error', r == {})

count_before = int(guild()['IkdSufj5'][0]['SivJ9sL9'])
r = update(target, 0)
check('dismiss (0) answers {}', r == {}, str(r)[:200])
check('... removes the membership (database and roster)', target not in members() and target not in roster())
check('... lowers the member count', int(guild()['IkdSufj5'][0]['SivJ9sL9']) == count_before - 1)
check('... and makes the friend invitable again',
      target in {c['h7eY3sAK'] for c in candidates().get('fRaBu6et', [])})
r = invite(gid, target)
check('re-invited after a dismissal, back as a Member', 'error' not in r and rank_of(target) == '4',
      str(rank_of(target)))

r = update(user, 0)
check('leaving your own guild answers {}', r == {}, str(r)[:200])
g = guild()
check('... dissolves it: no guild block, the founding fee instead',
      not g.get('IkdSufj5') and g.get('jKeiqDbl'), str(g)[:300])
check('... and no membership or guild row is left',
      not members() and not db.execute('SELECT 1 FROM user_guilds WHERE owner_user_id = ?', (user,)).fetchone())

print(f'\n{failures} failure(s)')
sys.exit(1 if failures else 0)
