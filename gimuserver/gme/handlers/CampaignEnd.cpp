#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Campaign.hpp>

#include <set>
#include <sstream>

// CampaignEnd (jF9Kkro4) — a Grand Quest run is over.
//
// CampaignResultInitScene::initConnect @0x154C294 sends it with status 2 when
// the run was cleared and 3 when a loss condition was met; the field's give-up
// sends 3 as well (CampaignNumInfoReq in net/handlers.kdl).  A clear pays:
//
//   * the zel and karma picked up in the run's battles (the last
//     CampaignBattleEnd archive), shown through MissionRewardInfo;
//   * every F_GRAND_MISSION_REWARD_MST row the run earned, shown through
//     p04iC2wr — reward_type 4 on the first clear, 1 when the run's completion
//     reaches the row's percent, 2 when the run set the row's flag.  Rows with
//     init_flag 1 ("First Time ...") pay once per user and are listed in the
//     mission's get_reward (JQ23rIvk); init_flag 0 rows ("Conditions Met
//     Bonus", most "Acquired Treasure") pay on every clear.
//
// Completion is the percents (GrandMissionFlgMst ug9xV4Fz) of the flags the
// run set; every mission's percents sum to exactly 100.
//
// Zel, karma and gems are credited here: CampaignResultScene counts them up on
// the header from team_info, and CampaignResultBonusScene::bonusUpdateHeader
// adds each bonus back as it shows it.  Everything else (units, items, keys,
// medals) goes to the present box, where PresentReceipt already pays and
// refreshes every present type — the same route first-clear quest rewards take.
//
// A run pays once: CampaignStart / DeckEdit / Save / BattleEnd open it and
// this handler closes it, so a repeated end finds it closed.

namespace
{
constexpr int64_t kMaxZelKarma = 99'999'999LL;

std::set<std::string> splitCsv(const std::string& csv)
{
	std::set<std::string> out;
	std::stringstream stream(csv);
	std::string item;
	while (std::getline(stream, item, ','))
	{
		if (!item.empty())
			out.insert(item);
	}
	return out;
}

std::string joinCsv(const std::set<std::string>& items)
{
	// Numeric order reads better in the reward list than string order.
	std::vector<std::string> sorted(items.begin(), items.end());
	std::sort(sorted.begin(), sorted.end(), [](const std::string& a, const std::string& b) {
		return a.size() != b.size() ? a.size() < b.size() : a < b;
	});
	std::string out;
	for (const auto& item : sorted)
	{
		if (!out.empty())
			out += ',';
		out += item;
	}
	return out;
}
} // namespace

HANDLEF(CampaignEnd)
{
	LOG_INFO << "CampaignEnd: " << json;

	CampaignEndReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "CampaignEnd: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	const int32_t status = req.num.empty() ? 3 : req.num.front().mission_status;
	const auto flags = req.event.empty() ? std::set<std::string>{} : splitCsv(req.event.front().mission_flg);

	CampaignEndResp resp{};
	auto transaction = co_await theDb()->newTransactionCoro();
	try
	{
		co_await gme::ensureCampaignState(transaction, identity);
		const auto state = co_await transaction->execSqlCoro(
			"SELECT active_mission_id, run_zel, run_karma, run_open FROM user_campaign_state WHERE user_id = $1;",
			identity.userId);

		std::string missionId = req.mission.empty() ? std::string{} : req.mission.front().mission_id;
		if (missionId.empty())
			missionId = state[0]["active_mission_id"].as<std::string>();
		const auto runZel = state[0]["run_zel"].as<int64_t>();
		const auto runKarma = state[0]["run_karma"].as<int64_t>();
		const bool runOpen = state[0]["run_open"].as<int32_t>() != 0;

		// The run is over whatever happened: close it, keep the loadout as it
		// came back (eqpItemFull merged the bag into it).
		co_await transaction->execSqlCoro(
			"UPDATE user_campaign_state SET active_mission_id = '', active_battle_seed = 0,"
			" run_zel = 0, run_karma = 0, run_open = 0, eqp_items = $1 WHERE user_id = $2;",
			glz::write_json(gme::toCampaignItems(req.eqp_items)).value_or("[]"), identity.userId);

		// Where each deck finished.  Kept rather than cleared: a mission that
		// was abandoned rather than cleared is picked up from the same spot.
		if (req.deck_pos)
			co_await gme::storeCampaignDeckPos(transaction, identity, *req.deck_pos);

		const auto missionRows = co_await transaction->execSqlCoro(
			"SELECT state, clear_count, rewards_got FROM user_campaign_missions"
			" WHERE user_id = $1 AND mission_id = $2;",
			identity.userId, missionId);

		if (status != 2)
		{
			LOG_INFO << "CampaignEnd: mission " << missionId << " ended with status " << status
				<< " — no rewards";
		}
		else if (!gme::isGrandMission(missionId))
		{
			LOG_WARN << "CampaignEnd: '" << missionId << "' is not a Grand Mission — no rewards";
		}
		else if (!runOpen)
		{
			LOG_WARN << "CampaignEnd: mission " << missionId << " — this run was already paid";
		}
		else
		{
			// No row means never played: first clear, nothing claimed yet.
			// There used to be a `state >= 1` "is this mission unlocked" gate
			// here, which only ever passed because CampaignStart seeded a
			// state=1 row for the lowest-id mission.  The client decides what
			// the player may enter (PermitPlace + the area/dungeon rules), and
			// an open run is what authorises the payout.
			const int32_t mission = std::stoi(missionId);
			const bool firstClear = missionRows.empty()
				|| missionRows[0]["clear_count"].as<int32_t>() == 0;
			auto got = splitCsv(missionRows.empty()
				? std::string{} : missionRows[0]["rewards_got"].as<std::string>());

			int32_t percent = 0;
			for (const auto& flag : theServer()->cache().grandMissionFlgMst())
			{
				if (flag.mission_id == mission && flags.contains(std::to_string(flag.id)))
					percent += flag.percents;
			}
			percent = std::min(percent, 100);

			int64_t bonusZel = 0, bonusKarma = 0, bonusGems = 0;
			size_t presents = 0;
			for (const auto& reward : theServer()->cache().grandMissionRewardMst())
			{
				if (reward.mission_id != mission)
					continue;
				const std::string rewardId = std::to_string(reward.id);
				const bool once = reward.init_flag == 1;
				if (once && got.contains(rewardId))
					continue;

				bool earned = false;
				switch (reward.reward_type)
				{
				case 4:  // first clear
					earned = firstClear;
					break;
				case 1:  // completion threshold
				{
					int32_t threshold = 101;
					try { threshold = std::stoi(reward.conditions); }
					catch (const std::exception&) {}
					earned = percent >= threshold;
					break;
				}
				case 2:  // flag
					earned = !reward.conditions.empty() && flags.contains(reward.conditions);
					break;
				default:
					LOG_WARN << "CampaignEnd: reward " << rewardId << " has unknown reward_type "
						<< reward.reward_type;
					break;
				}
				if (!earned)
					continue;

				if (once)
					got.insert(rewardId);

				switch (reward.present_type)
				{
				case 3:  bonusZel += reward.target_cnt; break;
				case 11: bonusKarma += reward.target_cnt; break;
				case 8:  bonusGems += reward.target_cnt; break;
				default:
					co_await gme::addUserPresent(transaction, identity, reward.present_type,
						std::to_string(reward.target_id), std::max(reward.target_cnt, 1), 0,
						reward.message.empty() ? std::string("Grand Quest reward") : reward.message);
					++presents;
					break;
				}

				::CampaignRewardBonusEntry entry{};
				entry.reward_id = rewardId;
				entry.present_type = reward.present_type;
				entry.target_id = std::to_string(reward.target_id);
				entry.target_cnt = reward.target_cnt;
				entry.target_param = reward.target_param;
				entry.reward_type = reward.reward_type;
				if (!resp.bonuses)
					resp.bonuses.emplace();
				resp.bonuses->push_back(std::move(entry));
			}

			// $N in first-appearance order (the sqlite binding gotcha).
			co_await transaction->execSqlCoro(
				"UPDATE user_info SET zel = MIN(zel + $1, $2), karma = MIN(karma + $3, $4), gems = gems + $5"
				" WHERE id = $6;",
				runZel + bonusZel, kMaxZelKarma, runKarma + bonusKarma, kMaxZelKarma, bonusGems,
				identity.userId);

			const auto now = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count());
			// Upsert: the first clear of a mission has no row yet, and state 2
			// is what UserInfo reports as the cleared set.
			co_await transaction->execSqlCoro(
				"INSERT INTO user_campaign_missions"
				" (user_id, mission_id, state, clear_count, attain_percent, last_cleared_at, rewards_got)"
				" VALUES ($1, $2, 2, 1, $3, $4, $5)"
				" ON CONFLICT(user_id, mission_id) DO UPDATE SET state = 2,"
				" clear_count = clear_count + 1, attain_percent = MAX(attain_percent, $3),"
				" last_cleared_at = $4, rewards_got = $5;",
				identity.userId, missionId, percent, now, joinCsv(got));

			// No successor row is written any more.  It used to insert the next
			// Grand Mission at state=1 to "unlock" it, but the client is sent
			// the whole catalogue (loadCampaignMissions) and gates entry on
			// PermitPlace, so a state=1 row unlocked nothing and only claimed
			// progress the player did not have.

			resp.reward.zel = static_cast<int32_t>(runZel);
			resp.reward.karma = static_cast<int32_t>(runKarma);

			LOG_INFO << "CampaignEnd: cleared " << missionId << " at " << percent << "%"
				<< (firstClear ? " (first clear)" : "") << " — " << runZel << " zel + " << runKarma
				<< " karma from battles; " << (resp.bonuses ? resp.bonuses->size() : 0)
				<< " bonus(es): " << bonusZel << " zel, " << bonusKarma << " karma, " << bonusGems
				<< " gem(s), " << presents << " to the present box";
		}

		resp.missions = co_await gme::loadCampaignMissions(transaction, identity);
		resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
	}
	catch (...)
	{
		transaction->rollback();
		throw;
	}

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
