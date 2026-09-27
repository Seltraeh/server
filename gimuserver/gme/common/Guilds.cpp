#include "App.hpp"

#include <ctime>

#include <gimuserver/gme/common/Guilds.hpp>
#include <gimuserver/gme/common/Friends.hpp>
#include <gimuserver/gme/common/Gifts.hpp>   // giftToday(), the shared day key

#include <algorithm>

namespace gme
{

::GuildRaidRoundInfo guildRaidRound()
{
	const auto now = static_cast<int32_t>(std::time(nullptr));
	::GuildRaidRoundInfo out{};
	out.season_id = kGuildRaidSeason;
	out.round_id = 1;
	out.server_time = now;
	// Every deadline in the past: the round is over, which is the only state
	// this server can truthfully report.  Phase 4 is the consolidation/results
	// phase the ranking screen belongs to -- UNVERIFIED, the enumeration is not
	// decoded, but it is the phase after battle end.
	out.preparation_end_time = now - 4 * 86400;
	out.battle_preparation_end_time = now - 3 * 86400;
	out.battle_end_time = now - 2 * 86400;
	out.consolidation_end_time = now - 86400;
	out.current_phase = 4;
	out.current_phase_end_time = now - 86400;
	out.outpost_relocate_remain_time = 0;
	return out;
}

int32_t guildLevelFor(int32_t experience)
{
	const auto& mst = theServer()->cache().guildInfoMst();
	if (mst.empty())
	{
		LOG_WARN << "Guilds: GuildInfoMst is empty; every guild reads as level 1";
		return 1;
	}

	int32_t best = 1;
	for (const auto& row : mst)
	{
		// The bands are contiguous -- each row's need_exp_max is one below the
		// next row's need_exp -- so the highest band whose floor is reached is
		// the level, and the top row absorbs anything past the curve.
		if (experience >= row.need_exp && row.level > best)
			best = row.level;
	}
	return best;
}

int32_t guildMaxMembers(int32_t level)
{
	const auto& mst = theServer()->cache().guildInfoMst();
	for (const auto& row : mst)
	{
		if (row.level == level)
			return row.max_member;
	}
	// Level 1's cap, rather than 0: refusing every invite is a worse failure
	// than allowing a starter guild while the table is unreadable.
	return 10;
}

namespace
{

/*! Read one guild row plus its live member count. */
drogon::Task<std::optional<GuildRow>> guildById(
	const db::Database database,
	const int32_t guildId)
{
	const auto rows = co_await database->execSqlCoro(
		"SELECT guild_id, owner_user_id, name, description, guild_art_id,"
		" experience, prestige_point, created_day FROM user_guilds WHERE guild_id = $1;",
		guildId);
	if (rows.empty())
		co_return std::nullopt;

	GuildRow row{};
	row.guild_id = rows[0]["guild_id"].as<int32_t>();
	row.owner_user_id = rows[0]["owner_user_id"].as<std::string>();
	row.name = rows[0]["name"].as<std::string>();
	row.description = rows[0]["description"].as<std::string>();
	row.guild_art_id = rows[0]["guild_art_id"].as<int32_t>();
	row.experience = rows[0]["experience"].as<int32_t>();
	row.prestige_point = rows[0]["prestige_point"].as<int32_t>();
	row.created_day = rows[0]["created_day"].as<std::string>();
	row.members_count = (co_await database->execSqlCoro(
		"SELECT COUNT(*) AS n FROM user_guild_members WHERE guild_id = $1;",
		guildId))[0]["n"].as<int32_t>();
	co_return row;
}

} // namespace

drogon::Task<std::optional<GuildRow>> loadGuild(
	const db::Database database,
	const UserIdentity identity)
{
	// Through the MEMBER table, not owner_user_id: a player who later joins
	// somebody else's guild must resolve the same way as one who founded their
	// own, and membership is the thing every other query keys on.
	const auto rows = co_await database->execSqlCoro(
		"SELECT guild_id FROM user_guild_members WHERE member_id = $1 LIMIT 1;",
		identity.userId);
	if (rows.empty())
		co_return std::nullopt;
	co_return co_await guildById(database, rows[0]["guild_id"].as<int32_t>());
}

drogon::Task<std::optional<GuildRow>> createGuild(
	const db::Database database,
	const UserIdentity identity,
	const std::string name,
	const std::string description,
	const int32_t artId)
{
	if ((co_await loadGuild(database, identity)).has_value())
	{
		LOG_INFO << "Guilds: " << identity.userId << " already belongs to a guild; not creating another";
		co_return std::nullopt;
	}

	const auto today = giftToday();
	co_await database->execSqlCoro(
		"INSERT INTO user_guilds"
		" (owner_user_id, name, description, guild_art_id, experience, prestige_point, created_day)"
		" VALUES ($1, $2, $3, $4, 0, 0, $5);",
		identity.userId, name, description, artId, today);

	const auto created = co_await database->execSqlCoro(
		"SELECT guild_id FROM user_guilds WHERE owner_user_id = $1"
		" ORDER BY guild_id DESC LIMIT 1;",
		identity.userId);
	if (created.empty())
		co_return std::nullopt;

	const auto guildId = created[0]["guild_id"].as<int32_t>();

	// The founder is a MEMBER, not just an owner -- see the header.
	co_await database->execSqlCoro(
		"INSERT OR IGNORE INTO user_guild_members (guild_id, member_id, joined_day, member_type)"
		" VALUES ($1, $2, $3, $4);",
		guildId, identity.userId, today, kGuildRankMaster);

	LOG_INFO << "Guilds: " << identity.userId << " founded \"" << name << "\" (guild " << guildId << ")";
	co_return co_await guildById(database, guildId);
}

drogon::Task<int32_t> inviteFriends(
	const db::Database database,
	const UserIdentity identity,
	const std::vector<std::string> memberIds)
{
	const auto guild = co_await loadGuild(database, identity);
	if (!guild)
	{
		LOG_INFO << "Guilds: " << identity.userId << " has no guild to invite into";
		co_return 0;
	}

	const auto cap = guildMaxMembers(guildLevelFor(guild->experience));
	auto held = guild->members_count;

	// Only people actually on the roster.  The recommended list is built from
	// it, so an id from anywhere else was never offered to this player.
	const auto roster = co_await loadFriendRoster(database, identity, false);
	const auto today = giftToday();

	int32_t joined = 0;
	for (const auto& id : memberIds)
	{
		if (held >= cap)
		{
			LOG_INFO << "Guilds: guild " << guild->guild_id << " is full at " << held
				<< "/" << cap << "; " << id << " not invited";
			break;
		}

		const auto mate = std::find_if(roster.begin(), roster.end(),
			[&id](const FriendRow& r) { return r.friend_id == id; });
		if (mate == roster.end())
		{
			LOG_WARN << "Guilds: " << id << " is not on " << identity.userId
				<< "'s roster; invite ignored";
			continue;
		}

		const auto result = co_await database->execSqlCoro(
			"INSERT OR IGNORE INTO user_guild_members (guild_id, member_id, joined_day, member_type)"
			" VALUES ($1, $2, $3, $4);",
			guild->guild_id, id, today, kGuildRankMember);
		if (result.affectedRows() > 0)
		{
			++joined;
			++held;
			LOG_INFO << "Guilds: " << mate->handle_name << " joined guild " << guild->guild_id;
		}
	}

	co_return joined;
}

::GuildInfo guildInfoBlock(const GuildRow& row, const std::string& masterName)
{
	const auto level = guildLevelFor(row.experience);

	::GuildInfo info{};
	info.id = row.guild_id;
	info.name = row.name;
	info.description = row.description;
	info.guild_art_id = row.guild_art_id;
	info.level = level;
	// P_RANK: the leaderboard position.  1 while this server has one guild --
	// there is nothing to rank against, and 0 would read as "unranked".
	info.rank = 1;
	info.prestige_point = row.prestige_point;
	info.members_count = row.members_count;
	info.master_name = masterName;
	info.experience = row.experience;
	// The progress bar's numerator: experience earned INSIDE the current level,
	// so it has to be measured from that level's floor, not from zero.
	int32_t floorExp = 0;
	for (const auto& band : theServer()->cache().guildInfoMst())
	{
		if (band.level == level)
		{
			floorExp = band.need_exp;
			break;
		}
	}
	info.experience_obtained = row.experience - floorExp;
	info.board_flag = false;
	info.create_date = row.created_day;
	return info;
}

drogon::Task<std::vector<::GuildMemberInfo>> guildRoster(
	const db::Database database,
	const UserIdentity identity)
{
	std::vector<::GuildMemberInfo> out;

	const auto guild = co_await loadGuild(database, identity);
	if (!guild)
		co_return out;

	const auto peak = co_await playerPeak(database, identity);
	const auto roster = co_await loadFriendRoster(database, identity, false);
	const auto now = static_cast<int64_t>(std::time(nullptr));

	for (const auto& row : co_await database->execSqlCoro(
		"SELECT member_id, member_type FROM user_guild_members WHERE guild_id = $1"
		" ORDER BY joined_day, member_id;",
		guild->guild_id))
	{
		const auto memberId = row["member_id"].as<std::string>();

		::GuildMemberInfo info{};
		info.guild_id = guild->guild_id;
		info.user_id = memberId;
		// Anything outside the named ranks would draw an unnamed rank, which is
		// exactly what the old hard-coded 0 did.
		const auto stored = row["member_type"].as<int32_t>();
		info.member_type = (stored >= kGuildRankVice && stored <= kGuildRankMember)
			? stored : kGuildRankMember;
		info.last_online = now;
		info.active_guild_deck = 1;
		const auto wallet = co_await database->execSqlCoro("SELECT guild_tokens FROM user_info WHERE id=$1;", memberId);
		if (!wallet.empty()) info.tokens = wallet[0]["guild_tokens"].as<int32_t>();

		if (memberId == identity.userId)
		{
			// The owner is a real account, so their row comes from user_info
			// rather than from the simulated-friend machinery.
			const auto me = co_await database->execSqlCoro(
				"SELECT username, level FROM user_info WHERE id = $1;", memberId);
			if (!me.empty())
			{
				info.handle_name = me[0]["username"].as<std::string>();
				info.team_lv = me[0]["level"].as<int32_t>();
			}

			// THE OWNER NEEDS A UNIT ON THEIR CARD.  Every other row gets one from
			// the friend machinery; the founder would otherwise be the one member
			// whose card is blank, and a blank card is what the Hall crashes on.
			// Their own strongest unit, by level -- the same one the Hall would have
			// shown as their lead.
			const auto lead = co_await database->execSqlCoro(
				"SELECT unit_id, unit_lvl, base_hp, base_atk, base_def, base_rec,"
				" skill_id, skill_lv, extra_skill_id, extra_skill_lv, unit_type_id"
				" FROM user_units WHERE user_id = $1 ORDER BY unit_lvl DESC LIMIT 1;",
				memberId);
			if (!lead.empty())
			{
				const auto raw = lead[0]["unit_id"].as<std::string>();
				const auto sep = raw.find('_');
				try { info.unit_id = std::stoi(sep != std::string::npos ? raw.substr(0, sep) : raw); }
				catch (const std::exception&) { info.unit_id = 0; }
				info.unit_lv = lead[0]["unit_lvl"].as<int32_t>();
				info.base_hp = lead[0]["base_hp"].as<int32_t>();
				info.base_atk = lead[0]["base_atk"].as<int32_t>();
				info.base_def = lead[0]["base_def"].as<int32_t>();
				info.base_heal = lead[0]["base_rec"].as<int32_t>();
				info.skill_id = lead[0]["skill_id"].as<int32_t>();
				info.skill_lv = lead[0]["skill_lv"].as<int32_t>();
				info.extra_skill_id = lead[0]["extra_skill_id"].as<int32_t>();
				info.extra_skill_lv = lead[0]["extra_skill_lv"].as<int32_t>();
				// Growth type (Anima/Breaker/etc.) is NOT an artwork variant.
				// Values >=2 append _N to the thumbnail filename and can name
				// nonexistent assets. Guild cards use the base artwork.
				info.unit_img_type = 1;
			}
			// The owner is always the Guild Master, whatever the row says.
			info.member_type = kGuildRankMaster;
			out.push_back(std::move(info));
			continue;
		}

		const auto mate = std::find_if(roster.begin(), roster.end(),
			[&memberId](const FriendRow& r) { return r.friend_id == memberId; });
		if (mate == roster.end())
		{
			// Unfriended since joining.  Send a NAMED row anyway rather than
			// dropping it: members_count comes from this table, so a missing row
			// would leave the Hall one short of its own header and the tap target
			// pointing at nothing.
			info.handle_name = memberId;
			info.team_lv = 999;
			out.push_back(std::move(info));
			continue;
		}

		const auto unit = friendUnitFor(*mate, peak);
		info.handle_name = mate->handle_name;
		info.team_lv = 999;
		info.friend_id = mate->friend_id;
		info.friend_message = mate->is_dev != 0 ? "decompfrontier" : "GG WP";
		info.unit_id = unit.unit_id;
		info.unit_lv = unit.level;
		info.unit_img_type = 1; // Base artwork, independent of growth type.
		info.base_hp = unit.base_hp;
		info.base_atk = unit.base_atk;
		info.base_def = unit.base_def;
		info.base_heal = unit.base_rec;
		info.skill_id = unit.bb_id;
		info.skill_lv = unit.bb_lvl;
		info.extra_skill_id = unit.sbb_id;
		info.extra_skill_lv = unit.sbb_lvl;
		info.equipitem_id = mate->sphere_1;
		info.equipitem_id2 =
			unit.rarity >= kSecondSphereRarity ? mate->sphere_2 : 0;
		out.push_back(std::move(info));
	}

	co_return out;
}

drogon::Task<GuildMemberChange> updateGuildMember(
	const db::Database database,
	const UserIdentity identity,
	const std::string memberId,
	const int32_t memberType)
{
	const auto guild = co_await loadGuild(database, identity);
	if (!guild || guild->owner_user_id != identity.userId)
	{
		LOG_WARN << "Guilds: " << identity.userId << " does not lead a guild; member update ignored";
		co_return GuildMemberChange::Refused;
	}

	if (memberId == identity.userId)
	{
		if (memberType != kGuildRankRemoved)
		{
			LOG_WARN << "Guilds: " << identity.userId << " asked to change their own rank to "
				<< memberType << "; the owner stays Guild Master";
			co_return GuildMemberChange::Refused;
		}
		// Leaving your own guild ends it: there is nobody to hand it to.
		co_await database->execSqlCoro(
			"DELETE FROM user_guild_members WHERE guild_id = $1;", guild->guild_id);
		co_await database->execSqlCoro(
			"DELETE FROM user_guilds WHERE guild_id = $1;", guild->guild_id);
		LOG_INFO << "Guilds: " << identity.userId << " left guild " << guild->guild_id
			<< " (\"" << guild->name << "\"), which is dissolved";
		co_return GuildMemberChange::Dissolved;
	}

	if (memberType == kGuildRankRemoved)
	{
		const auto removed = co_await database->execSqlCoro(
			"DELETE FROM user_guild_members WHERE guild_id = $1 AND member_id = $2;",
			guild->guild_id, memberId);
		if (removed.affectedRows() == 0)
		{
			LOG_WARN << "Guilds: " << memberId << " is not in guild " << guild->guild_id << "; dismissal ignored";
			co_return GuildMemberChange::Refused;
		}
		LOG_INFO << "Guilds: " << identity.userId << " dismissed " << memberId
			<< " from guild " << guild->guild_id;
		co_return GuildMemberChange::Dismissed;
	}

	if (memberType < kGuildRankVice || memberType > kGuildRankMember)
	{
		LOG_WARN << "Guilds: rank " << memberType << " for " << memberId
			<< " refused (only 2-4 can be given; 1 would hand the guild over)";
		co_return GuildMemberChange::Refused;
	}

	const auto updated = co_await database->execSqlCoro(
		"UPDATE user_guild_members SET member_type = $1 WHERE guild_id = $2 AND member_id = $3;",
		memberType, guild->guild_id, memberId);
	if (updated.affectedRows() == 0)
	{
		LOG_WARN << "Guilds: " << memberId << " is not in guild " << guild->guild_id << "; rank change ignored";
		co_return GuildMemberChange::Refused;
	}
	LOG_INFO << "Guilds: " << memberId << " is now rank " << memberType << " in guild " << guild->guild_id;
	co_return GuildMemberChange::RankSet;
}

namespace
{

// The Social list's card, re-keyed for the Hall's profile.  The two classes
// share 43 keys; the guild id is the one that moves (sD73jd20 here, mgNdrCEe on
// FriendInfo), and FriendInfo has no enhancement or Omni fields.
::GuildRecommendFriendInfo profileFromSocialCard(const ::FriendInfo& card)
{
	::GuildRecommendFriendInfo out{};
	out.user_id = card.user_id;
	out.handle_name = card.handle_name;
	out.team_lv = card.team_lv;
	out.friend_type = card.friend_type;
	out.last_login_date = card.last_login_date;
	out.unit_id = card.unit_id;
	out.unit_lv = card.unit_lv;
	out.base_hp = card.base_hp;
	out.base_atk = card.base_atk;
	out.base_def = card.base_def;
	out.base_heal = card.base_heal;
	out.add_hp = card.add_hp;
	out.ext_hp = card.ext_hp;
	out.add_atk = card.add_atk;
	out.ext_atk = card.ext_atk;
	out.add_def = card.add_def;
	out.ext_def = card.ext_def;
	out.add_heal = card.add_heal;
	out.ext_heal = card.ext_heal;
	out.today_yale = card.today_yale;
	out.want_gift = card.want_gift;
	out.favorite = card.favorite;
	out.equipitem_id = card.equipitem_id;
	out.equipitem_id2 = card.equipitem_id2;
	out.friend_id = card.friend_id;
	out.friend_message = card.friend_message;
	out.friend_message_change_time = card.friend_message_change_time;
	out.arena_rank_id = card.arena_rank_id;
	out.ranking_point = card.ranking_point;
	out.unit_type_id = card.unit_type_id;
	out.skill_id = card.skill_id;
	out.skill_lv = card.skill_lv;
	out.extra_skill_id = card.extra_skill_id;
	out.extra_skill_lv = card.extra_skill_lv;
	out.req_time = card.req_time;
	out.elapsed_agree_time = card.elapsed_agree_time;
	out.friend_rc = card.unk_7_x3_p_pb2_c;
	out.friend_hr = card.unk_sv80_k_l5_r;
	out.extra_passive_skill_id = card.extra_passive_skill_id;
	out.extra_passive_skill_id2 = card.extra_passive_skill_id2;
	// Base artwork, as on every other guild card: 2 and up name a _N file that
	// most units do not ship.
	out.unit_img_type = 1;
	out.guild_id = 0; // Simulated friends belong to no guild until invited.
	out.deck_no = card.deck_no;
	out.priority = card.priority;
	return out;
}

} // namespace

drogon::Task<::GuildRecomendedMemberResp> invitableFriends(
	const db::Database database,
	const UserIdentity identity)
{
	::GuildRecomendedMemberResp out{};

	const auto guild = co_await loadGuild(database, identity);
	std::vector<std::string> already;
	if (guild)
	{
		for (const auto& row : co_await database->execSqlCoro(
			"SELECT member_id FROM user_guild_members WHERE guild_id = $1;", guild->guild_id))
		{
			already.push_back(row["member_id"].as<std::string>());
		}
	}

	const auto peak = co_await playerPeak(database, identity);
	const auto roster = co_await loadFriendRoster(database, identity, false);
	const auto now = static_cast<int64_t>(std::time(nullptr));

	// socialList already leaves out anyone whose unit cannot be derived, so a
	// candidate without a drawable card is never offered in the first place.
	for (const auto& card : socialList(roster, peak))
	{
		if (std::find(already.begin(), already.end(), card.user_id) != already.end())
			continue;

		::GuildRecomendedMemberInfo info{};
		info.user_id = card.user_id;
		info.user_name = card.handle_name;
		// The same 999 the helper picker shows for a simulated Summoner; their
		// team level is not modelled, and inventing a spread would be noise.
		info.user_level = card.team_lv;
		info.friend_unit_id = card.unit_id;
		info.friend_unit_level = card.unit_lv;
		info.friend_image_type = 1; // Base artwork, independent of growth type.
		info.last_online_time = now;
		out.members.push_back(std::move(info));
		out.friends.push_back(profileFromSocialCard(card));
	}

	co_return out;
}

} // namespace gme
