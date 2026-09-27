# Guild invites, ranks and member management

Dated 2026-09-26, ranks added 2026-09-27. Invites are **client-confirmed**
(2026-09-27: no crash, friends join). Ranks and member management are
server-tested and **await client confirmation**. The Guild
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

## Ranks and member management (2026-09-27)

Reported after the invite fix: rank names drew wrong, and promoting a member
crashed (dump at 09:52, `write 0x284`).

**Rank vocabulary (binary-confirmed).** `GameUtils::getNameOfGuildRank`
@0x1EC4178 draws `GUILD_RANK_NAME_<n>`, defined for **1 Guild Master, 2 Vice
Guild Master, 3 Officer, 4 Member**. The server stored no rank and sent 0 for
every friend, which has no name. Ranks now live in
`user_guild_members.member_type` (migration `27092026_AddUserGuildMembersMemberType`,
default 4); the owner is always served as 1.

**GuildMemberUpdate (`ad81b8at`) is member management, not an invite.** The
member screen (`GuildMemberInfoScene::updateEvent` @0x1DD36B8) sends the
member's current type − 1 to promote, + 1 to demote and 0 to dismiss; the Hall
and GuildTeamScene2 send the player's own id with 0 to leave. The captured
promote was `gr48vsdJ: "-1"` (from the old 0).

**The crash.** After a successful edit the client changes its own copy:
`noticeOK` @0x1DD3DD4 finds the member again by id and sets type ∓ 1, and a
dismissal removes the member and lowers the count locally. The server's reply
carried the roster (`csIuech30`), whose reader starts with
`GuildUserGuildInfo::clearGuildMembers()`, so the member the screen held was
freed and the lookup returned null. The reply is now `{}`.

**And the server never saw the change.** The request did not parse: the
client quotes every number it sends (`JsonNode::addParam(const char*, int)`
@0xFDB82C is `IntToString` plus the string overload; captured `"sD73jd20": "1"`),
while `GuildTargetNode.guild_id` was modelled as an unquoted `i32::int`. Glaze
stopped there and the member list was never read (`parse error …
parse_number_failure` in the server log). Now `i32::str`; the same fix went to
`GuildTradeNode` (guild shop), `EventExchangePurchaseNode` (Bazaar count),
`InboxMessageManageNode` and `GuildInviteManageNode`.

**Rules as built** (`gme::updateGuildMember`): ranks 2–4 can be given; 1 (a
hand-over) is refused because the other side is a simulated friend; the owner
cannot change their own rank; 0 dismisses a member (they become invitable
again); the player leaving their own guild dissolves it, since nobody can
inherit it. Refusals reply `{}` like successes, because every handler error
closes the session — the client may show the change until the next GuildInfo.

## Still open

- **GuildUpdate (`92bDoqBi`)** is a stub: the Hall's **Disband** button and
  guild name / description / insignia edits send it, so they do nothing.
  (A leave request, GuildMemberUpdate with the player's own id, does
  dissolve it; whether the Hall offers Leave to a Guild Master is unverified.)
- Neither candidate list can be emptied over the wire: a zero-row array never
  reaches readParam, so after the last candidate is invited the client keeps
  showing them until a non-empty reply replaces the list. Inviting them again
  is harmless (refused, no error).

## Tests

`scripts/test_guild_invite_wire.py` (isolated server on 19960, fresh save copy
with a guild): the two lists pair up by id and order, every profile carries
exactly the 46 client keys, profile and card agree; GuildJoin type 2 adds the
member, the reply carries the new roster and count, the member leaves both
lists; type 1, a foreign guild id, a non-roster id and a repeat invite change
nothing and return no error. 19/19 passed on the 2026-09-26 09:00 build, which
also passed the Research Lab, exchange UI, fusion/merit, feature-visit and
synthesis suites.

Ranks (2026-09-27): owner 1 and friends 4 on the roster; promote 4→3→2 stored
and served with a `{}` reply; 1, 5, −1 and non-numbers refused without an
error; the owner cannot change their own rank; dismissal removes the member,
lowers the count and makes them invitable again (re-invited as 4); leaving
dissolves the guild and GuildInfo falls back to the founding fee.

Client checklist: rank names on the Hall's member list (you Guild Master,
friends Member); promote a friend twice and demote once (no crash, the name
changes each time); dismiss a friend (they leave the list and reappear on the
invite tab).
