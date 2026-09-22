#pragma once

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/FriendPoints.hpp>

#include <optional>
#include <string>
#include <vector>

// The friend roster -- several Summoners to borrow a helper from, and the
// ability to drop one.
//
// ⚠ EVERYTHING HERE IS AUTHORED POLICY.  The identities, the seeding, and the
// scaling rule are choices made for this server; none of it is recovered
// behaviour, and the handbook requires that distinction be kept explicit.
//
// THREE CACHES, NOT ONE.  A friend has to be emitted into BOTH `xZH6EIQ7`
// (ReinforcementInfo -- the pre-battle helper picker) AND `tojMy68W`
// (FriendInfo -- the Social list).  `T_FIXED_REINFORCEMENT` is a third,
// separate path.  Populating one and expecting the others to follow is a
// documented way to ship a feature that looks finished and is not.
//
// friend_type MUST BE 1.  `FriendInfoList::existTypeOK` @0x12607CC returns true
// only for exactly 1, and that is what decides whether borrowing this helper
// pays the friend Honor rate or the stranger rate.
//
// THE SCALING RULE (authored):
//   A friend is shown at the form of their own evolution chain whose rarity
//   matches the player's strongest unit, at a level within +/-5 of it, clamped
//   to what that form can legally reach.
//
// So friends grow as the player does -- and a friend whose chain TOPS OUT below
// the player (a 6-star-max species once the player is fielding 7-star units)
// can no longer keep up.  Those go STALE: they stop appearing in the picker,
// and the player has to unfriend them by hand to make room.  That is the
// intended mechanic, not a bug to paper over.
namespace gme
{

/*! One authored developer identity. */
struct DevFriend
{
	std::string name;
	int32_t base_unit_id = 0;
	std::string unit;
};

/*! One roster row, as stored. */
struct FriendRow
{
	std::string friend_id;
	std::string handle_name;
	int32_t base_unit_id = 0;
	int32_t is_dev = 0;
	int32_t favorite = 0;
	/*! Spheres as STORED.  0 means not rolled yet; see friendSpheresFor. */
	int32_t sphere_1 = 0;
	int32_t sphere_2 = 0;
};

/*! How many friends a fresh roster is seeded with.  AUTHORED. */
inline constexpr int32_t kSeedFriendCount = 5;

/*! How far a friend's level may sit either side of the player's. AUTHORED. */
inline constexpr int32_t kFriendLevelSpread = 5;

/*!
* One day in N carries a developer encounter.  AUTHORED.
*
* At 45 the expected wait for the next encounter is about six weeks of daily
* play, so a player meets their first developer inside a month or two and
* collects the rest over a long while.  1200 was tried first and works out at
* several YEARS per developer, which is not a rarity, it is a dead feature.
*
* `devencounters` in the debug CLI reads the schedule ahead without changing
* it, which is the practical way to check a rate change landed.
*/
inline constexpr int32_t kDevEncounterOneIn = 45;

/*! The encounter's reserved user-unit id, clear of the roster's block. */
inline constexpr int32_t kEncounterUnitId = 990900;

/*!
* How many strangers the helper picker offers alongside the roster.  AUTHORED.
*
* The live game listed other Summoners under your friends so there was always
* someone to borrow, and finishing a mission with one is what raises the "add
* as a friend?" prompt.  Without them the picker only ever shows people you
* already know and the friend loop has no entry point.
*/
inline constexpr int32_t kSuggestionCount = 10;

/*! First reserved user-unit id for a suggested stranger, clear of both blocks. */
inline constexpr int32_t kStrangerUnitIdBase = 991000;

/*!
* The rarity at which a unit's SECOND sphere slot opens.
*
* Matches the player's own units: `user_units.eqip_item_frame_id` carries two
* slots and ItemSphereEqp enforces slot 2, so a friend fielding a 7-star is
* expected to show two spheres and anything below it one.
*/
inline constexpr int32_t kSecondSphereRarity = 7;

/*! A friend's equipped spheres; item ids, 0 for an empty slot. */
struct FriendSpheres
{
	int32_t first = 0;
	int32_t second = 0;
};


/*!
* First reserved user-unit id for a roster friend.
*
* DecompFriend's 999999 was chosen to avoid colliding with real player units;
* several friends need several ids, and they must not collide with each other
* either or two friends become the same unit to anything keying on it.  990000
* upward leaves 999999 alone.
*/
inline constexpr int32_t kFriendUnitIdBase = 990000;

/*! How far the player has actually got -- the yardstick every friend is measured against. */
struct PlayerPeak
{
	int32_t rarity = 1;
	int32_t level = 1;
};

/*! A friend's fielded unit, derived rather than stored. */
struct FriendUnit
{
	int32_t unit_id = 0;
	int32_t level = 1;
	int32_t rarity = 1;
	int32_t element = 1;
	int32_t unit_type_id = 1;
	int32_t base_hp = 0;
	int32_t base_atk = 0;
	int32_t base_def = 0;
	int32_t base_rec = 0;
	int32_t bb_id = 0;
	int32_t bb_lvl = 1;
	int32_t sbb_id = 0;
	int32_t sbb_lvl = 1;
};

void loadFriendArchive(const std::string& archiveRoot);

/*!
* The player's strongest unit, by rarity then level.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @return Peak rarity and level, defaulting to 1/1 on an empty box.
*/
drogon::Task<PlayerPeak> playerPeak(
	const db::Database database,
	const UserIdentity identity);

/*!
* Work out what a friend is fielding right now.
*
* Walks their chain to the player's rarity (or as far as it goes), then picks a
* level within kFriendLevelSpread of the player's, clamped to what that form can
* legally reach.  Stats are interpolated from UnitMst min..max across 1..max_lv
* -- the same curve scaleUnitBaseStats and UnitMix use, so a friend's card reads
* like a real unit rather than a flat statline.
*
* Deterministic per (friend, player level): the same friend does not jitter a
* few levels every time the picker is opened.
*
* @param row  Roster row.
* @param peak The player's own ceiling.
* @return The derived unit; unit_id 0 when the chain base is unknown.
*/
FriendUnit friendUnitFor(const FriendRow& row, const PlayerPeak& peak);

/*!
* Roll a sphere loadout.
*
* AUTHORED.  Nothing on the wire tells the server what a helper "should" equip
* and the player cannot pick for them, so the loadout is generated -- but
* generated the way the game's own rules would allow:
*
*   - TWO SPHERES OF THE SAME CATEGORY CANNOT BE WORN AT ONCE.  `ItemMst
*     .sphere_type` (1..14) is that category -- it is what ItemSphereEqp writes
*     into eqip_item_frame_id and what the client draws the slot frame from --
*     so the second roll is drawn from a DIFFERENT sphere_type than the first.
*   - The second slot only exists from kSecondSphereRarity up.
*   - A sphere is only offered if its own rarity is within the unit's, so a
*     low-tier helper is not walking around in endgame gear.
*
* ⚠ THIS ONLY ROLLS.  A friend's kit is LOCKED IN when they are added and lives
* on the roster row from then on -- it is deliberately NOT re-derived as they
* evolve.  The loadout is meant to behave like a Pokemon's IVs: what you got is
* what you got, so late-game optimisation means choosing who to keep and who to
* unfriend, and unfriending is the only thing that rerolls.
*
* BOTH SPHERES ARE ROLLED NOW, even for a friend who is nowhere near 7-star.
* Locking the kit and gating the roll on current rarity would mean a friend
* added early could never have a second sphere at all; instead the pair is
* fixed here and the second is simply not SHOWN until their unit reaches
* kSecondSphereRarity.  The roll is judged against the top of their evolution
* chain for the same reason -- what they will grow into, not where they are.
*
* @param seed      Distinguishes this roll -- the friend id, or a stranger's id
*                  plus the day so a passing offer is stable while on screen.
* @param maxRarity Ceiling for sphere rarity; the chain's top, not today's form.
*/
FriendSpheres rollFriendSpheres(const std::string& seed, int32_t maxRarity);

/*! The authored developer identities, empty until the archive loads. */
const std::vector<DevFriend>& devFriends();

/*!
* The top rarity a unit's evolution chain can reach.
*
* Chains are consecutive ids whose rarity steps by one -- verified across the
* archive: 1,206 consecutive pairs step rarity+1 and only 7 do not, and those
* seven are chain boundaries.  So walking id+1 while the rarity keeps climbing
* finds the chain's top.
*
* @param baseUnitId Lowest-rarity form of the chain.
* @return Highest rarity reachable, or 0 when the id is unknown.
*/
int32_t chainTopRarity(int32_t baseUnitId);

/*!
* The form of a chain at a given rarity, or its top when it cannot reach.
*
* @param baseUnitId Lowest-rarity form of the chain.
* @param rarity     Rarity wanted.
* @return Unit id of that form, or 0 when the id is unknown.
*/
int32_t chainFormAtRarity(int32_t baseUnitId, int32_t rarity);

/*!
* Read the roster, seeding it on first use, and drop anyone who cannot keep up.
*
* Stale friends are filtered from the RESULT but left in the table: the player
* is meant to see them in the Social list and decide to unfriend, so deleting
* them here would quietly do it for them.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @param forPicker true to hide friends whose chain cannot reach the player's
*                  rarity (the helper picker); false to return everyone (the
*                  Social list, where they still need managing).
* @return Roster rows.
*/
drogon::Task<std::vector<FriendRow>> loadFriendRoster(
	const db::Database database,
	const UserIdentity identity,
	bool forPicker);

/*!
* Today's developer encounter, if there is one.
*
* Rolled deterministically from (user, day) so it needs no storage and cannot
* be re-rolled by reopening the screen -- the same trick the daily gift uses.
* A developer already on the roster is never offered again.
*
* The encounter is emitted into the PICKER ONLY.  That is what makes the
* recruit work: MissionResultFriendRequestScene::initialize @0x18C16BC asks
* `FriendInfoList::exist(reinforcementUserId)` and offers to add the helper
* only when they are NOT already a friend.  Putting an encounter in the Social
* list would answer that question "yes" and suppress the offer.
*
* @param alreadyFriends Roster the player already has.
* @param userId         Resolved user id.
* @param day            Day key, as gme::giftToday() formats it.
* @return The developer met today, or nullopt.
*/
std::optional<DevFriend> devEncounterFor(
	const std::vector<FriendRow>& alreadyFriends,
	const std::string& userId,
	const std::string& day);

/*! The user id a developer is offered under -- stable, so FriendApply can name them. */
std::string devFriendId(const DevFriend& dev);

/*!
* The user id a suggested stranger is offered under.
*
* THE ID CARRIES THE CHAIN.  "NEW" plus the zero-padded chain base means
* FriendApply can reconstruct exactly who the player just met from the id alone
* -- no pending-request table, no re-rolling the day's suggestions and hoping
* they come out the same.  The client is free to sit on the offer across a
* restart and it still resolves.
*/
std::string strangerFriendId(int32_t baseUnitId);

/*!
* The strangers to list under the roster in the helper picker.
*
* Deterministic per (user, day), so the picker is stable while the player
* browses it and turns over tomorrow.
*
* A chain the player already has a friend on is never offered again -- that is
* the "you cannot friend the same unit twice" rule.  DEVELOPERS DO NOT CONSUME
* THEIR CHAIN: having Evan as a friend must still allow friending an ordinary
* Krantz, because the developer is an identity, not that unit's representative.
*
* Chains that cannot reach the player's rarity are skipped, so a suggestion is
* never born stale.
*
* @param roster The player's existing friends.
* @param userId Resolved user id.
* @param day    Day key, as gme::giftToday() formats it.
* @param peak   The player's own ceiling.
* @return Stranger rows, already filtered; is_dev is 0 on all of them.
*/
std::vector<FriendRow> strangerSuggestions(
	const std::vector<FriendRow>& roster,
	const std::string& userId,
	const std::string& day,
	const PlayerPeak& peak);

/*!
* Accept a friend request on the other Summoner's behalf.
*
* Handles both kinds of offer: a developer encounter (`devFriendId`) and an
* ordinary stranger (`strangerFriendId`).  There is no pending state and no
* refusal -- the whole point is that the far side always accepts.
*
* Refuses when the roster is at the capacity the client is showing, and when
* the chain is already represented, so a resent or stale apply cannot duplicate
* a friend or slip past the one-per-chain rule.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @param friendId The user id the picker offered.
* @return true when a row was written.
*/
drogon::Task<bool> recruitFriend(
	const db::Database database,
	const UserIdentity identity,
	const std::string& friendId);

/*!
* The Social list (`tojMy68W`), built from an already-loaded roster.
*
* ONE DEFINITION ON PURPOSE.  Both FriendGet and FriendApply have to emit this
* list, and `FriendInfoResponse::readParam` @0x13D93C0 clears the whole list on
* row 0 field 0 -- so it is a FULL REPLACE and both senders must produce the
* same complete thing.  Two hand-built copies would drift and the shorter one
* would silently delete friends.
*
* Everyone is included, stale or not: the Social list is where the player sees
* who has fallen behind and decides to unfriend them.
*
* Pure rather than a coroutine so the caller passes the roster and peak it has
* already read -- the SQLite pool has one connection and re-reading them here
* would double the queries on every send.
*
* @param roster Rows from loadFriendRoster.
* @param peak   The player's own ceiling.
*/
std::vector<::FriendInfo> socialList(
	const std::vector<FriendRow>& roster,
	const PlayerPeak& peak);

/*!
* Whether this user id is on the player's roster.
*
* Decides the Honor rate a finished mission pays, and MUST agree with the
* friend_type the card was drawn with -- see gme::honorForMission.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @param friendId The helper recorded at MissionStart.
*/
drogon::Task<bool> isFriend(
	const db::Database database,
	const UserIdentity identity,
	const std::string& friendId);

/*!
* Remove one friend from the roster.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @param friendId The friend's user id.
* @return true when a row was actually removed.
*/
drogon::Task<bool> removeFriend(
	const db::Database database,
	const UserIdentity identity,
	const std::string& friendId);

} // namespace gme
