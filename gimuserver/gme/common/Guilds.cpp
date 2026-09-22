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
		"INSERT OR IGNORE INTO user_guild_members (guild_id, member_id, joined_day)"
		" VALUES ($1, $2, $3);",
		guildId, identity.userId, today);

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
			"INSERT OR IGNORE INTO user_guild_members (guild_id, member_id, joined_day)"
			" VALUES ($1, $2, $3);",
			guild->guild_id, id, today);
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
		"SELECT member_id FROM user_guild_members WHERE guild_id = $1 ORDER BY joined_day, member_id;",
		guild->guild_id))
	{
		const auto memberId = row["member_id"].as<std::string>();

		::GuildMemberInfo info{};
		info.guild_id = guild->guild_id;
		info.user_id = memberId;
		info.last_online = now;
		info.active_guild_deck = 1;

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
				info.unit_img_type = lead[0]["unit_type_id"].as<int32_t>();
			}
			// Guild master.  // UNVERIFIED: the member_type vocabulary is not
			// recovered; 1 is used for the founder and 0 for everyone else so the
			// two are at least distinguishable on screen.
			info.member_type = 1;
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
		info.unit_img_type = unit.unit_type_id;
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

drogon::Task<std::vector<::GuildRecomendedMemberInfo>> invitableFriends(
	const db::Database database,
	const UserIdentity identity)
{
	std::vector<::GuildRecomendedMemberInfo> out;

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

	for (const auto& mate : roster)
	{
		if (std::find(already.begin(), already.end(), mate.friend_id) != already.end())
			continue;

		const auto unit = friendUnitFor(mate, peak);
		if (unit.unit_id == 0)
			continue;

		::GuildRecomendedMemberInfo info{};
		info.user_id = mate.friend_id;
		info.user_name = mate.handle_name;
		// The same 999 the helper picker shows for a simulated Summoner; their
		// team level is not modelled, and inventing a spread would be noise.
		info.user_level = 999;
		info.friend_unit_id = unit.unit_id;
		info.friend_unit_level = unit.level;
		info.friend_image_type = unit.unit_type_id;
		info.last_online_time = now;
		out.push_back(std::move(info));
	}

	co_return out;
}

} // namespace gme
