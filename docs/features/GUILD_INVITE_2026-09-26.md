# Guild invites: the tap crash and the invite request

Dated 2026-09-26. Server-tested; **awaiting client confirmation**. The Guild
Hall's other crash that morning (founder thumbnail, 07:39) is covered in
[EXCHANGE_UI_2026-09-26.md](EXCHANGE_UI_2026-09-26.md).

## The crash (08:18 dump)

Tapping an invite candidate in the Guild Hall crashed with `read 0x230`.
`GuildHallScene::touchEnded` (arm64 0x1DCFD48–0x1DCFDB0) takes the tapped
`GuildRecomendedMemberInfo`, looks its user id up with
`GuildRecommendedFriendInfoList::getObjectWithUserID`, and calls
`GuildRecommendedFriendInfo::clone()` on the result **without a null check**,
then opens `FriendRandomSearchFriendRequestScene` in mode 3 with it. That list
is filled only by the response key `8lAroepR`
(`GuildRecommendFriendInfoResponse::readParam` @0x1C64DF4, 46 keys, a full
replace), which the server never sent — so every lookup returned null.

**Fix:** GuildRecomendedMember now sends `8lAroepR` beside `fRaBu6et`, one
profile per card with the same user ids, built from the Social list's own card
(`socialList`) in the same loop, so the two lists cannot disagree
(`gme::invitableFriends`). New KDL struct `GuildRecommendFriendInfo` models the
46 keys with their setters (`tools/readparam_map.py`); the guild id rides
`sD73jd20` here, not FriendInfo's `mgNdrCEe`.

## The invite request

The profile's Invite button does **not** send GuildMemberUpdate, which the
2026-09-17 slice had assumed. Every friend profile (the Hall's candidate, the
Social list, friend search, and the mission/arena/colosseum/vortex-arena result
screens) sends **GuildJoin (`bfa2D1bp`)** with one `aj38Jk10` node: the
player's guild id, the friend's user id, and request type `2`
(`mov w3,#2` at all nine call sites). Type 1 is GuildDetailScene's application
to someone else's guild. GuildJoin was a `{}` stub, so the invite did nothing.

**Fix:** a real `GuildJoin` handler. Type 2 into the player's own guild adds
the friend through `inviteFriends` (the existing authored rule: simulated
friends accept at once, roster-only, member cap enforced). The reply is the
refreshed GuildInfo (guild, roster, exchange), because the profile's back
button rebuilds the Hall and `GuildHallScene::initConnect` re-requests only
GuildRecomendedMember. Refusals (type 1, another guild id, an id not on the
roster, an existing member, a full guild) answer with the unchanged guild
rather than an error, since every handler error closes the session.

## Found, not fixed

GuildMemberUpdate (`ad81b8at`) is sent by the Hall, the member screen and
GuildTeamScene2 — with the player's own id beside
`GUILD_DISBAND_CONFIRMATION_OK`, i.e. leaving/disbanding and rank changes. It is
still handled as an invite, so those actions are silent no-ops.

Neither candidate list can be emptied over the wire: a zero-row array never
reaches readParam, so after the last candidate is invited the client keeps
showing them until a non-empty reply replaces the list. Inviting them again is
harmless (refused, no error).

## Tests

`scripts/test_guild_invite_wire.py` (isolated server on 19960, fresh save copy
with a guild): the two lists pair up by id and order, every profile carries
exactly the 46 client keys, profile and card agree; GuildJoin type 2 adds the
member, the reply carries the new roster and count, the member leaves both
lists; type 1, a foreign guild id, a non-roster id and a repeat invite change
nothing and return no error. 19/19 passed on the 2026-09-26 09:00 build, which
also passed the Research Lab, exchange UI, fusion/merit, feature-visit and
synthesis suites.

Client checklist: open the Hall's invite tab, tap a friend (their profile opens
instead of crashing), press Invite, go back — the friend is in the member list
and the count went up.
