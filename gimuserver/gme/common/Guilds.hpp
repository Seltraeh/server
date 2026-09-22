#pragma once

#include <gimuserver/gme/common/Common.hpp>

#include <optional>
#include <string>
#include <vector>

// Guilds -- founding one and filling it with your friends.
//
// WHAT IS RECOVERED AND WHAT IS AUTHORED, kept apart because the handbook
// requires it:
//
//   Binary-confirmed.  Every wire shape in net/guild.kdl, including that the
//   invite candidates ARE the player's friends -- GuildRecomendedMemberInfo
//   carries setFriendUserUnitID / setFriendUserUnitLevel /
//   setFriendUserImageType, so the live game populated that list from the
//   friend roster too.  Also the level ladder: GuildInfoMst is 180 rows of
//   (level, exp band, member cap), member cap running 10 up to 60.
//
//   Authored policy.  The founding cost, that invited friends always accept,
//   and anything to do with passive growth.  A real guild needed other players
//   to say yes; here the other side is simulated, so there is nobody to ask and
//   an invite that could fail would only be a dice roll pretending to be a
//   social system.
//
// THE OWNER IS A MEMBER.  They are written into user_guild_members at creation
// rather than inferred from user_guilds.owner_user_id, so member counts, caps
// and later per-member growth all read from one place.
namespace gme
{

/*! One guild, as stored. */
struct GuildRow
{
	int32_t guild_id = 0;
	std::string owner_user_id;
	std::string name;
	std::string description;
	int32_t guild_art_id = 1;
	int32_t experience = 0;
	int32_t prestige_point = 0;
	std::string created_day;
	int32_t members_count = 0;
};

/*!
* What founding a guild costs.  AUTHORED.
*
* GuildCreateCostResponse recovers the SHAPE of the fee -- a currency type and
* an amount -- but no MST carries the values, so the numbers are chosen here.
* Zel, because it is the currency a player at the level guilds unlock has most
* of, and a figure small enough that founding is a decision rather than a grind.
*/
inline constexpr int32_t kGuildCreateCurrency = 3;      // 3 = zel, matching the present-box vocabulary
inline constexpr int64_t kGuildCreateCost = 50000;

/*! Guild art id used when the client sends one we cannot parse. */
inline constexpr int32_t kDefaultGuildArtId = 1;

/*!
* How many guild-raid SEASONS this server has run.
*
* AUTHORED, and load-bearing rather than cosmetic: the ranking screen
* (GuildRaidRankingResultScene) indexes GuildRaidSeasonDataInfoList with NO null
* check, so an empty season list is a hard crash at PE +0x2FEEB1.  One is the
* honest answer -- guild raids have never run here -- and one is enough.
*
* The client names a season itself from `GR_SESSION_%04d_NAME`, which its string
* table answers for 1..66 ("Season 1 Results"), so raising this stays legible.
*/
inline constexpr int32_t kGuildRaidSeason = 1;

/*!
* The guild-raid round clock, as a single row for `Nebq4d8x`.
*
* AUTHORED.  No guild raid has ever been run here, so there is no real round to
* report; this describes a season that has FINISHED, which is the state the
* ranking screen is reached from, with every deadline already in the past and
* the server time set to now so the countdowns render as elapsed rather than as
* a live round the player could act on.
*/
::GuildRaidRoundInfo guildRaidRound();

/*!
* The guild level an experience total earns.
*
* Read from GuildInfoMst, which is 180 rows of (level, need_exp, need_exp_max).
* Falls back to level 1 when the table is missing rather than inventing a curve.
*
* @param experience Total guild experience.
*/
int32_t guildLevelFor(int32_t experience);

/*!
* How many members a guild of this level may hold.
*
* GuildInfoMst.max_member, 10 at level 1 rising to 60.  // UNVERIFIED (named
* from value): the field name is inferred from its range, not from a consumer.
*
* @param level Guild level.
*/
int32_t guildMaxMembers(int32_t level);

/*!
* The caller's guild, or nullopt when they have none.
*
* @param database Database client or transaction.
* @param identity Resolved user.
*/
drogon::Task<std::optional<GuildRow>> loadGuild(
	const db::Database database,
	const UserIdentity identity);

/*!
* Found a guild and install the caller as its first member.
*
* Refuses when the caller already belongs to one -- the client has no concept of
* a second guild, and letting a stale request create one would orphan the first.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @param name Guild name, as typed.
* @param description Blurb, as typed.
* @param artId Chosen crest.
* @return The new guild, or nullopt when the caller already had one.
*/
drogon::Task<std::optional<GuildRow>> createGuild(
	const db::Database database,
	const UserIdentity identity,
	const std::string name,
	const std::string description,
	int32_t artId);

/*!
* Add friends to the caller's guild.  They always accept.
*
* Only ids on the caller's own friend roster are accepted: the recommended list
* is built from that roster, so anything else is an id the player was never
* offered.  Stops at the level's member cap rather than overfilling.
*
* @param database Database client or transaction.
* @param identity Resolved user.
* @param memberIds Friend ids to invite.
* @return How many actually joined.
*/
drogon::Task<int32_t> inviteFriends(
	const db::Database database,
	const UserIdentity identity,
	const std::vector<std::string> memberIds);

/*!
* Build the `IkdSufj5` block.
*
* A SINGLETON on the client -- one full row or none, never a partial.
*
* @param row The guild.
* @param masterName Handle to show as the guild master.
*/
::GuildInfo guildInfoBlock(const GuildRow& row, const std::string& masterName);

/*!
* The guild's roster, as the Guild Hall draws it.
*
* THE HALL NEEDS THESE ROWS OR IT CRASHES.  Tapping a member with the list empty
* is an access violation with no dialog (read 0x230, unmapped) -- the empty-body
* stub keeps the session alive but cannot stop a null dereference.
*
* The owner is rendered from their own account; every other member is a friend,
* so their card is built the same way the helper picker builds one.
*
* @param database Database client or transaction.
* @param identity Resolved user.
*/
drogon::Task<std::vector<::GuildMemberInfo>> guildRoster(
	const db::Database database,
	const UserIdentity identity);

/*!
* The friends the caller could still invite.
*
* Everyone on the roster who is not already in the guild.  The reply this feeds
* is a FULL REPLACE, so this must be the complete set.
*
* @param database Database client or transaction.
* @param identity Resolved user.
*/
drogon::Task<std::vector<::GuildRecomendedMemberInfo>> invitableFriends(
	const db::Database database,
	const UserIdentity identity);

} // namespace gme
