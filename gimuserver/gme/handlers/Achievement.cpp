#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Achievements.hpp>
#include <gimuserver/gme/common/Common.hpp>

// GetAchievementInfo (YPBU7MD8 / AKjzyZ81) — the Randall Achievement screen.
//
// This is the request the client fires when the menu hosting achievements
// opens.  It is NOT the present box — that assumption cost a build.  YPBU7MD8
// surfaced in an "Unsupported request" dialog while opening the gift tab, which
// made it look like the present list, but it resolves to
// GetAchievementInfoRequest two independent ways: its xref goes to
// `GetAchievementInfoRequest::getRequestID`, and its .rodata block terminates
// with `25GetAchievementInfoRequest` (see tools/ida/groupid_key_pair_audit.py
// and handbook §4.2).  The present box lives at nhjvB52R / bV5xa0ZW.
//
// It used to answer with the signal key ALONE, and deliberately so: the four
// achievement response classes were known but their fields were not, and
// handbook §3.4 says a response key filled with 0/"" is worse than an absent
// one.  The 2026-09-13 readParam audit lifted that (see
// UserAchievementSubjectInfo in net/achievement.kdl): `pG2n1A28` is Progress,
// `rPk8gtY5` is RewardReceiveStatus and `dJNpLc81` is NewFlg, all named from
// other classes that use the same hashes with surviving setters.  Two keys are
// still unnamed and go out as 0 — which is what the client's own constructor
// leaves them at, so for those two sending and omitting are the same thing.
//
// THE MASTER DATA IS THE OTHER HALF, and it travels by a different road.
// `H9ATfJ38`, `82CcMZhp` and `1tJiqKgZ` are NOT getResponseObject keys: they are
// three of the 58 tags GameResponseParser::parseBodyTag routes to
// DataMstManager::save*, which writes the client's own MST store;
// DataMstManager::load* reads it back and HomeScene::loadFiles re-runs that
// load, so a table sent here is live from the next trip through Home.  Nothing
// had ever sent them, so even a player whose counters were full had no
// achievement names to read.  They are ported and date-filtered by
// tools/gen_achievement_mst.py (873 achievements, 181 shop offers, 53 rates).
//
// The request is a QUERY: `mode` 1 wants the subject list and 2 the trade shop,
// narrowed by `category` and `sub_category` (which are the same keys as
// F_ACHIEVEMENT_SUBJECT_MST's own columns, so the two join directly).  The
// client CLEARS the list it is about to receive — createBody branches on mode
// straight after sending it, 1 calling UserAchievementSubjectInfoList::
// removeDataObject and 2 UserAchievementTradeInfoList::removeAllObjects — so an
// omitted list reads as empty rather than stale, and a filtered answer is
// exactly what the screen expects.
//
// Not built yet, and the reason the shop is read-only for now: Accept
// (dx5qvm7L), Deliver (vsaXI4M0), RewardReceive (uq69mTtR) and Trade
// (m9LiF6P2) are all unregistered.
HANDLEF(GetAchievementInfo)
{
	(void)session;

	GetAchievementInfoReq req = {};
	{
		glz::context ctx{};
		if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json, ctx); ec)
			LOG_WARN << "GetAchievementInfo: parse error: " << glz::format_error(ec, json);
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// The selector.  A body with no node asks for everything, which is what the
	// first visit of a session looks like.
	int32_t mode = 0, category = 0, condType = 0;
	for (const auto& node : req.nodes)
	{
		mode = node.mode;
		category = node.category;
		condType = node.sub_category;
		LOG_INFO << "GetAchievementInfo: mode=" << mode << " category=" << category
			<< " sub_category=" << condType;
	}

	GetAchievementInfoResp resp = {};
	resp.signal_key.key = "5EdKHavF";

	// The catalogues.  Cheap to repeat (the client stores them) and the only
	// thing that makes the rows below legible.
	const auto& cache = theServer()->cache();
	resp.subject_mst = cache.achievementSubjectMst();
	resp.trade_mst = cache.achievementTradeMst();
	resp.deliver_rate_mst = cache.achievementDeliverRateMst();

	// The balance the screen prints, which only this block writes.
	resp.achievement_info = co_await gme::loadAchievementInfo(theDb(), identity);

	// Mode 2 is the shop; anything else (including an empty selector) is the
	// achievement list.  Both are answered when the selector is absent, because
	// then the client has cleared neither and is asking for the screen as a
	// whole.
	if (mode != 2)
		resp.subjects = co_await gme::loadAchievementSubjects(theDb(), identity, category, condType);
	if (mode != 1)
		resp.trades = co_await gme::loadAchievementTrades(theDb(), identity);

	// LcFCx1Uz stays empty: the SP tab's conditions are Trials, which this
	// server does not run, and an empty list is how "none" is spelled.

	LOG_INFO << "GetAchievementInfo: " << cache.achievementSubjectMst().size()
		<< " achievement(s), " << cache.achievementTradeMst().size()
		<< " shop offer(s); sent "
		<< (resp.subjects ? resp.subjects->size() : 0u) << " progress row(s) and "
		<< (resp.trades ? resp.trades->size() : 0u) << " purchase row(s) for "
		<< identity.userId << " (" << resp.achievement_info.id << " merit points)";

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		const auto& glze = glz::format_error(ec, buffer);
		LOG_DEBUG << "Gme GetAchievementInfo Error during JSON writing: " << glze;
		co_return HandleResult::error("Serialization error", glze);
	}

	co_return HandleResult::success(buffer);
}
