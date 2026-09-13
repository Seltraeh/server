#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/UnitArchiver.hpp>
#include <gimuserver/gme/common/Achievements.hpp>
#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <string>

// The Merit Point loop, and the two buttons on an achievement's detail page.
//
//   AchievementRewardReceive  uq69mTtR / cbE74zBZ   claim a finished achievement
//   AchievementTrade          m9LiF6P2 / 0IWC9LVq   buy from the Merit shop
//   AchievementAccept         dx5qvm7L / g9N1y7bc   Start / Give Up
//
// Every request shape comes from its createBody (@0x139E3A8, @0x139E600 and
// @0x139DCA0) -- see net/achievement.kdl.  The last request of this family,
// Deliver (vsaXI4M0), is still unregistered: it trades units and currency for
// points through AchievementDeliverRateMst, which needs its colon-packed band
// decoded first.
//
// The shop's reward vocabulary is the shared present vocabulary, with one
// entry that grants nothing physical: type 15 is ALTERNATE UNIT ART.  The
// illustrations already ship with the client (unit_ills_full_<id>_2.png); what
// is bought is permission to show them, and that permission is one bit on the
// unit-dictionary row -- see gme::loadUnitDictionary.
//
// Merit Points are NOT part of the currency block.  `Bnc4LpM8` is its own
// singleton and only UserAchievementInfoResponse::readParam writes the client's
// copy, so both replies carry it or the number on screen does not move --
// the same gap that made the Frontier Gate look broken on 2026-09-12.

namespace
{

/*! Reads the colon-packed reward of a shop offer: "<type>:<id>:<count>:…". */
struct TradeReward
{
	int32_t type = 0;
	std::string target;
	int32_t count = 1;
};

TradeReward parseReward(const std::string& packed)
{
	TradeReward reward{};
	size_t start = 0, field = 0;
	while (start <= packed.size() && field < 3)
	{
		const auto end = packed.find(':', start);
		const auto part = packed.substr(start, end == std::string::npos ? end : end - start);
		try
		{
			if (field == 0) reward.type = std::stoi(part);
			else if (field == 1) reward.target = part;
			else reward.count = std::max(std::stoi(part), 1);
		}
		catch (const std::exception&) { /* leave the default */ }
		if (end == std::string::npos)
			break;
		start = end + 1;
		++field;
	}
	return reward;
}

} // namespace

// ---------------------------------------------------------------------------
HANDLEF(AchievementRewardReceive)
{
	(void)session;

	AchievementRewardReceiveReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "AchievementRewardReceive: parse error: " << glz::format_error(ec, json);
	}
	if (req.nodes.empty())
		co_return HandleResult::error("Invalid achievement request", "no achievement named");

	const auto& node = req.nodes.front();
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// The achievement has to exist, be finished, and not have paid already.
	const auto& catalogue = theServer()->cache().achievementSubjectMst();
	const auto subject = std::find_if(catalogue.begin(), catalogue.end(),
		[&node](const ::AchievementSubjectMst& s)
		{ return std::to_string(s.id) == node.subject_id; });
	if (subject == catalogue.end())
		co_return HandleResult::error("Invalid achievement request", "unknown achievement " + node.subject_id);

	AchievementRewardReceiveResp resp{};
	resp.signal_key.key = "5EdKHavF";
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// Guarded on reward_received = 0, so a replayed body pays once.
			const auto claimed = co_await transaction->execSqlCoro(
				"INSERT INTO user_achievement_subjects (user_id, subject_id, reward_received)"
				" VALUES ($1, $2, 1)"
				" ON CONFLICT(user_id, subject_id) DO UPDATE SET reward_received = 1"
				" WHERE user_achievement_subjects.reward_received = 0"
				" RETURNING subject_id;",
				identity.userId, node.subject_id);
			if (claimed.empty())
			{
				transaction->rollback();
				LOG_WARN << "AchievementRewardReceive: " << node.subject_id
					<< " was already claimed by " << identity.userId;
				co_return HandleResult::error("Invalid achievement request", "already claimed");
			}

			// ...and it has to actually be finished.  Checked AFTER the latch so
			// the two cannot race, and rolled back when it is not.
			if (!co_await gme::achievementComplete(transaction, identity, *subject))
			{
				transaction->rollback();
				LOG_WARN << "AchievementRewardReceive: " << node.subject_id
					<< " is not complete for " << identity.userId;
				co_return HandleResult::error("Invalid achievement request", "not complete");
			}

			if (subject->point > 0)
			{
				co_await transaction->execSqlCoro(
					"UPDATE user_info SET achieve_point = MIN(achieve_point + $1, 999999)"
					" WHERE id = $2;",
					subject->point, identity.userId);
			}

			resp.achievement_info = co_await gme::loadAchievementInfo(transaction, identity);
			resp.subjects = co_await gme::loadAchievementSubjects(
				transaction, identity, node.category, node.cond_type);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	LOG_INFO << "AchievementRewardReceive: " << identity.userId << " claimed "
		<< node.subject_id << " for " << subject->point << " merit point(s); balance "
		<< resp.achievement_info.id;

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}

// ---------------------------------------------------------------------------
HANDLEF(AchievementTrade)
{
	(void)session;

	AchievementTradeReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "AchievementTrade: parse error: " << glz::format_error(ec, json);
	}
	if (req.nodes.empty())
		co_return HandleResult::error("Invalid trade request", "no offer named");

	const auto& node = req.nodes.front();
	const auto count = std::max(node.count, 1);
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	const auto& shop = theServer()->cache().achievementTradeMst();
	const auto offer = std::find_if(shop.begin(), shop.end(),
		[&node](const ::AchievementTradeMst& o)
		{ return std::to_string(o.id) == node.trade_id; });
	if (offer == shop.end())
		co_return HandleResult::error("Invalid trade request", "unknown offer " + node.trade_id);

	const auto reward = parseReward(offer->reward_info);
	const auto cost = static_cast<int64_t>(offer->price) * count;

	AchievementTradeResp resp{};
	resp.signal_key.key = "5EdKHavF";
	{
		auto transaction = co_await theDb()->newTransactionCoro();
		try
		{
			// The purchase limit, enforced in the same statement that records
			// the buy so two taps cannot both pass the check.
			const auto bought = co_await transaction->execSqlCoro(
				"INSERT INTO user_achievement_trades (user_id, trade_id, count)"
				" VALUES ($1, $2, $3)"
				" ON CONFLICT(user_id, trade_id) DO UPDATE SET count = count + $3"
				" WHERE user_achievement_trades.count + $3 <= $4"
				" RETURNING count;",
				identity.userId, node.trade_id, count, offer->limit_count);
			if (bought.empty())
			{
				transaction->rollback();
				co_return HandleResult::error("Invalid trade request", "purchase limit reached");
			}

			// The points, debited only if there are enough.
			const auto paid = co_await transaction->execSqlCoro(
				"UPDATE user_info SET achieve_point = achieve_point - $1"
				" WHERE id = $2 AND achieve_point >= $1 RETURNING achieve_point;",
				cost, identity.userId);
			if (paid.empty())
			{
				transaction->rollback();
				LOG_WARN << "AchievementTrade: " << identity.userId
					<< " cannot afford offer " << node.trade_id << " (" << cost << " points)";
				co_return HandleResult::error("Invalid trade request", "not enough merit points");
			}

			// The goods.  Same present vocabulary the box uses; the porter has
			// already dropped every offer whose type is not one of these, so an
			// unknown type here is data drift rather than a normal path.
			switch (reward.type)
			{
			case 3:  // zel
			case 8:  // gem
			case 11: // karma
			{
				const char* column = reward.type == 3 ? "zel" : (reward.type == 8 ? "gems" : "karma");
				co_await transaction->execSqlCoro(
					std::string("UPDATE user_info SET ") + column + " = " + column +
					" + $1 WHERE id = $2;",
					reward.count * count, identity.userId);
				resp.team_info = std::move((co_await gme::getTeamInfo(transaction, identity)).nonEmpty());
				break;
			}
			case 6:  // unit
			{
				uint32_t unitId = 0;
				try { unitId = static_cast<uint32_t>(std::stoul(reward.target)); }
				catch (const std::exception&)
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid trade request", "bad unit id");
				}
				auto unit = gme::fromArchivedUnit(unitId, UnitArchiver::getRandomType());
				if (!unit)
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid trade request", "unit has no archive record");
				}
				std::vector<::UserUnitInfo> granted;
				for (int32_t i = 0; i < reward.count * count; ++i)
					granted.push_back(std::move((co_await gme::addUserUnit(transaction, identity, *unit)).nonEmpty()));
				resp.unit_info = std::move(granted);
				resp.unit_dictionary = co_await gme::loadUnitDictionary(transaction, identity);
				break;
			}
			case 4:  // item
			case 5:  // material
			case 7:  // sphere
			{
				uint32_t itemId = 0;
				try { itemId = static_cast<uint32_t>(std::stoul(reward.target)); }
				catch (const std::exception&)
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid trade request", "bad item id");
				}
				(co_await gme::addUserItem(transaction, identity, itemId,
					static_cast<uint32_t>(reward.count * count))).nonEmpty();
				auto snapshot = co_await gme::loadWarehouseSnapshot(transaction, identity);
				resp.warehouse_info = std::move(snapshot.warehouse);
				resp.item_dictionary_info = std::move(snapshot.dictionary);
				break;
			}
			case 15: // alternate unit art
			{
				uint32_t unitId = 0;
				try { unitId = static_cast<uint32_t>(std::stoul(reward.target)); }
				catch (const std::exception&)
				{
					transaction->rollback();
					co_return HandleResult::error("Invalid trade request", "bad unit id");
				}
				// The unlock is per SPECIES and has no count: the offer's
				// limit_count is what stops it being bought twice.
				co_await transaction->execSqlCoro(
					"INSERT INTO user_unit_alt_art (user_id, unit_id) VALUES ($1, $2)"
					" ON CONFLICT(user_id, unit_id) DO NOTHING;",
					identity.userId, unitId);
				// The dictionary carries the flag that turns the swap on, and
				// it is a full replace, so the whole thing goes back.
				resp.unit_dictionary = co_await gme::loadUnitDictionary(transaction, identity);
				break;
			}
			default:
				transaction->rollback();
				LOG_WARN << "AchievementTrade: offer " << node.trade_id
					<< " pays reward type " << reward.type << ", which this server cannot grant";
				co_return HandleResult::error("Invalid trade request", "unsupported reward");
			}

			resp.achievement_info = co_await gme::loadAchievementInfo(transaction, identity);
			resp.trades = co_await gme::loadAchievementTrades(transaction, identity);
		}
		catch (...)
		{
			transaction->rollback();
			throw;
		}
	}

	LOG_INFO << "AchievementTrade: " << identity.userId << " bought \"" << offer->name
		<< "\" (offer " << node.trade_id << ") x" << count << " for " << cost
		<< " merit point(s); balance " << resp.achievement_info.id;

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}


// ---------------------------------------------------------------------------
// Start / Give Up.  One request serves both buttons, told apart by mnZ5K4Ii:
// challengeStart @0x1A570CC writes 1, QuitRequestState::updateEvent @0x1A571D8
// writes 2.
//
// Nothing is stored.  In the original, accepting is what starts an achievement
// counting and what starts its VDKB0Y5h countdown, with
// RandallAchievementCommon::getMaxAchievementAcceptCnt capping how many may be
// held at once.  Here every counter behind an achievement is tracked
// unconditionally -- the progress numbers are read off user_team_archive and
// friends on every request -- so there is nothing for accepting to switch on
// and nothing for giving up to lose.  The row is reported as In Progress (state
// 1) or Completed (state 2) from its own condition, never as "not started",
// which is why these buttons should not normally be on screen at all.
//
// It is registered because they CAN be: an unregistered GroupId closes the
// session, and on the detail page that looks exactly like a crash.  The reply
// has to carry the list too, because createBody calls removeDataObject on the
// way out and the client has already dropped the page it is looking at.
HANDLEF(AchievementAccept)
{
	(void)session;

	AchievementAcceptReq req{};
	{
		glz::context ctx{};
		if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "AchievementAccept: parse error: " << glz::format_error(ec, json);
	}
	if (req.nodes.empty())
		co_return HandleResult::error("Invalid achievement request", "no achievement named");

	const auto& node = req.nodes.front();
	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	AchievementAcceptResp resp{};
	resp.signal_key.key = "5EdKHavF";
	resp.achievement_info = co_await gme::loadAchievementInfo(theDb(), identity);
	resp.subjects = co_await gme::loadAchievementSubjects(
		theDb(), identity, node.category, node.cond_type);

	LOG_INFO << "AchievementAccept: " << identity.userId
		<< (node.accept_flag == 2 ? " gave up " : " started ") << node.subject_id
		<< "; nothing stored (progress here is unconditional), "
		<< resp.subjects.size() << " row(s) re-sent";

	co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
