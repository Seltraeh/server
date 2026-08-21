#include "MysteryChest.hpp"

#include "Common.hpp"

#include <gimuserver/archive/archive.hpp>
#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/utils/JsonFile.hpp>

#include <algorithm>
#include <chrono>

namespace gme
{

namespace
{

/*!
* The authored chest definitions, loaded once at startup.
*/
std::vector<MysteryChestDef> g_chests;

/*!
* Current unix time in seconds.
*/
int64_t nowSeconds()
{
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

/*!
* Finds a chest definition by its archive key.
*/
const MysteryChestDef* chestByKey(const std::string& key)
{
	const auto it = std::find_if(g_chests.begin(), g_chests.end(),
		[&key](const MysteryChestDef& c) { return c.key == key; });
	return it == g_chests.end() ? nullptr : &*it;
}

/*!
* Formats a unix timestamp the way the client's preformatted date fields expect.
*
* These are display-only — the int fields are what the countdown reads — so the
* exact shape matters less than it being legible and stable.
*/
std::string formatDate(const int64_t ts)
{
	const auto t = static_cast<std::time_t>(ts);
	std::tm tm{};
#ifdef _WIN32
	gmtime_s(&tm, &t);
#else
	gmtime_r(&t, &tm);
#endif
	char buf[32]{};
	std::strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M", &tm);
	return buf;
}

/*!
* Builds the wire row for one stored chest.
*/
MysteryBoxInfo toWire(const MysteryChestDef& def, const std::string& boxId,
	const int64_t rewardTs, const int64_t expiryTs)
{
	return MysteryBoxInfo{
		.box_id = boxId,
		.reward_date = static_cast<int32_t>(rewardTs),
		.expiry_date = static_cast<int32_t>(expiryTs),
		.reward_date_str = formatDate(rewardTs),
		.expiry_date_str = formatDate(expiryTs),
		.rank = def.rank,
		.skin = def.skin,
		.box_type = def.box_type,
		.text = def.text,
		.name = def.name,
	};
}

} // namespace

void loadMysteryChestArchive(const std::string& archiveRoot)
{
	if (archiveRoot.empty())
	{
		LOG_WARN << "MysteryChest: archive_root is empty, chest definitions not loaded";
		return;
	}

	try
	{
		g_chests = LoadJson<std::vector<MysteryChestDef>>(archiveRoot, "mystery_chest.json");
	}
	catch (const std::exception& e)
	{
		LOG_ERROR << "MysteryChest: failed to load mystery_chest.json: " << e.what();
		return;
	}

	LOG_INFO << "MysteryChest: loaded " << g_chests.size()
		<< " chest definitions from " << archiveRoot << "/mystery_chest.json";
}

drogon::Task<void> provisionMysteryChests(const db::Database database, const UserIdentity& identity)
{
	if (g_chests.empty())
	{
		co_return;
	}

	const auto existing = (co_await db::DatabaseInterface::read(
		database,
		"user_mystery_boxes",
		{
			db::Data("chest_key"),
			db::Lookup("user_id", identity.userId),
		})).data;

	std::vector<std::string> known;
	for (const auto& row : existing)
	{
		known.push_back(row["chest_key"].as<std::string>());
	}

	const auto now = nowSeconds();
	for (const auto& def : g_chests)
	{
		// Granted once and only once.  Presence of ANY row — claimed or not —
		// means this chest has already been handed to the player.
		if (std::find(known.begin(), known.end(), def.key) != known.end())
		{
			continue;
		}

		co_await db::DatabaseInterface::insert(
			database,
			"user_mystery_boxes",
			{
				db::Data("user_id", identity.userId),
				db::Data("box_id", def.key),
				db::Data("chest_key", def.key),
				db::Data("reward_ts", std::to_string(now)),
				db::Data("expiry_ts", std::to_string(now + static_cast<int64_t>(def.lifetime_days) * 86400)),
				db::Data("claimed", "0"),
			});

		LOG_INFO << "MysteryChest: granted '" << def.key << "' to " << identity.userId;
	}
}

drogon::Task<std::vector<MysteryBoxInfo>> listMysteryChests(
	const db::Database database,
	const UserIdentity& identity)
{
	std::vector<MysteryBoxInfo> out;

	const auto rows = (co_await db::DatabaseInterface::read(
		database,
		"user_mystery_boxes",
		{
			db::Data("box_id"),
			db::Data("chest_key"),
			db::Data("reward_ts"),
			db::Data("expiry_ts"),
			db::Data("claimed"),
			db::Lookup("user_id", identity.userId),
		})).data;

	const auto now = nowSeconds();
	for (const auto& row : rows)
	{
		if (row["claimed"].as<int32_t>() != 0)
		{
			continue;
		}

		const auto expiry = row["expiry_ts"].as<int64_t>();
		if (expiry <= now)
		{
			// The client would draw this as a zero countdown; drop it here so
			// both sides agree the chest is gone.
			continue;
		}

		const auto key = row["chest_key"].as<std::string>();
		const auto* def = chestByKey(key);
		if (def == nullptr)
		{
			LOG_WARN << "MysteryChest: stored chest '" << key
				<< "' has no archive definition — skipped";
			continue;
		}

		out.push_back(toWire(*def, row["box_id"].as<std::string>(),
			row["reward_ts"].as<int64_t>(), expiry));
	}

	co_return out;
}

drogon::Task<bool> claimMysteryChest(
	const db::Database database,
	const UserIdentity& identity,
	const std::string& boxId,
	std::vector<MysteryBoxRewardInfo>& rewards)
{
	const auto rows = (co_await db::DatabaseInterface::read(
		database,
		"user_mystery_boxes",
		{
			db::Data("chest_key"),
			db::Data("expiry_ts"),
			db::Data("claimed"),
			db::Lookup("user_id", identity.userId),
			db::Lookup("box_id", boxId),
		})).data;

	if (rows.empty())
	{
		LOG_WARN << "MysteryChest: " << identity.userId << " claimed unknown chest '" << boxId << "'";
		co_return false;
	}

	const auto& row = rows.front();
	if (row["claimed"].as<int32_t>() != 0)
	{
		LOG_WARN << "MysteryChest: chest '" << boxId << "' was already opened";
		co_return false;
	}
	if (row["expiry_ts"].as<int64_t>() <= nowSeconds())
	{
		LOG_WARN << "MysteryChest: chest '" << boxId << "' has expired";
		co_return false;
	}

	const auto key = row["chest_key"].as<std::string>();
	const auto* def = chestByKey(key);
	if (def == nullptr)
	{
		LOG_WARN << "MysteryChest: chest '" << key << "' has no archive definition";
		co_return false;
	}

	// Mark it opened FIRST.  A reward that throws half way through should not
	// leave a chest that can be opened again for the rest of it.
	co_await db::DatabaseInterface::update(
		database,
		"user_mystery_boxes",
		{
			db::Data("claimed", "1"),
			db::Lookup("user_id", identity.userId),
			db::Lookup("box_id", boxId),
		});

	int64_t zel = 0, gems = 0;
	for (const auto& reward : def->rewards)
	{
		switch (reward.present_type)
		{
		case 3:
			zel += reward.target_cnt;
			break;
		case 8:
			gems += reward.target_cnt;
			break;
		case 6:
		{
			const auto& unitMst = theServer()->cache().unitMst();
			const auto unit = std::find_if(unitMst.begin(), unitMst.end(),
				[&reward](const auto& u) { return u.id == reward.target_id; });
			if (unit == unitMst.end())
			{
				LOG_WARN << "MysteryChest: reward unit " << reward.target_id
					<< " not in unit_mst — skipped";
				break;
			}
			for (uint32_t i = 0; i < reward.target_cnt; ++i)
			{
				co_await gme::addUserUnit(database, identity, *unit);
			}
			break;
		}
		case 4:
		case 5:
		case 7:
			co_await gme::addUserItem(database, identity, reward.target_id, reward.target_cnt);
			break;
		default:
			LOG_WARN << "MysteryChest: present_type " << reward.present_type
				<< " not supported — skipped";
			break;
		}

		rewards.push_back(MysteryBoxRewardInfo{
			.item_id = static_cast<int32_t>(reward.target_id),
			.present_type = static_cast<int32_t>(reward.present_type),
			.target_id = static_cast<int32_t>(reward.target_id),
			.target_cnt = static_cast<int32_t>(reward.target_cnt),
			.description = reward.description,
		});
	}

	if (zel > 0 || gems > 0)
	{
		// Clamped to the display widths — past them the client silently renders
		// zero, so an over-generous chest would read as a loss (handbook §10).
		co_await database->execSqlCoro(
			"UPDATE user_info SET"
			" zel  = MIN(zel  + $1, $2),"
			" gems = MIN(gems + $3, $4)"
			" WHERE id = $5;",
			zel, static_cast<int64_t>(99'999'999), gems, static_cast<int64_t>(9'999),
			identity.userId);
	}

	LOG_INFO << "MysteryChest: " << identity.userId << " opened '" << boxId
		<< "' (" << def->name << ") for " << rewards.size() << " reward(s)";
	co_return true;
}

} // namespace gme
