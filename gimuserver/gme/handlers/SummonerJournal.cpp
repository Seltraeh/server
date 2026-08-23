#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/SummonerJournal.hpp>

// Summoner Journal — tag 0 on the Rewards menu, scene 101802.
//
// The LAST Rewards tile with no handler at all (see RewardsMenu.cpp for the
// full tile/tag map).  `RewardsTopScene::loadMenuList` @0xE42440 lists it
// first, and §7.14 missed it entirely.
//
// ── The three requests ────────────────────────────────────────────────────
// Every GroupId and key below was read out of .rodata via each Request
// class's getRequestID / getEncodeKey — 12-byte functions that return a
// literal — exactly as §4.2 describes.  None is a guess.
//
//   32Gwida0 / 66B2pDki  SummonerJournalInfoRequest             @0xE37934
//        identity + signal key only (createBody @0xE3794C is 44 bytes).
//        "Give me the journal" — the screen's initial load.
//
//   2y48D13d / 7nm3Dqe9  SummonerJournalTaskRewardsRequest      @0xE37758
//        identity + signal key + version tag, then group `da38tRai`
//        carrying one param `23DaiBpe` (createBody @0xE377A8).  Claiming a
//        single task's reward.
//        ⚠ `da38tRai` is ALSO the dispatch key of
//        SummonerJournalUserTaskInfoResponse — the request echoes the same
//        group shape it will be answered in.
//        `23DaiBpe` uses the std::string overload of JsonNode::addParam
//        (@0xE37898), not the int one, so §3.4's "int setter + quoted wire
//        value" ambiguity does NOT apply here — it really is a string.
//
//   3a83iY3r / 98Tw0ubW  SummonerJournalMilestoneRewardsRequest @0xE379D0
//        identity + signal key only (createBody @0xE379E8 is 44 bytes).
//        Carries NO milestone id, so this claims whatever is currently
//        claimable rather than one named milestone — worth confirming
//        against a real body before the handler assumes it.
//
// ── The six responses ─────────────────────────────────────────────────────
// ALL SIX are in `GameResponseParser::getResponseObject` @0x1392568, so the
// SERVER supplies every part of this screen — including the MSTs.  That is
// the opposite of Daily Spin, whose rewards MST is absent from that table
// and loaded locally by DataMstManager, and it is why the reward catalogue
// there could not be authored server-side but this one can.
//
// Keys taken from the strcmp immediately preceding each constructor:
//
//   T38aBiw3  SummonerJournalTaskMstResponse            @0x139593C
//   ad52Diwq  SummonerJournalMilestoneMstResponse       @0x1395918
//   b2DjiaXp  SummonerJournalRewardsMstResponse         @0x1395960
//   M3dw18eB  SummonerJournalUserInfoResponse           @0x1395984
//   da38tRai  SummonerJournalUserTaskInfoResponse       @0x13959A8
//   r3D28bqW  SummonerJournalUserMilestoneInfoResponse  @0x13959CC
//
// So the screen is three MSTs (the task catalogue, the milestone ladder and
// the reward table) plus three per-user progress lists.
//
// ── Field maps, audited from each readParam (§6.20) ──────────────────────
// Setter names are the binary's own, so these are evidence rather than
// naming guesses.  `str` = the setter takes std::string, `int` = int.
//
//   T38aBiw3  SummonerJournalTaskMst        @0xE3363C   9 fields
//     23DaiBpe str  task_id        s35idar9 str  name
//     H2Dnbr39 str  instructions   v39D198k int  locked_level
//     xNd38t2u int  unlock_type    Sd38Dbt3 int  unlock_value
//     pG2n1A28 int  progress       w3Di51bp int  value
//     da36ky2E int  target_screen
//
//   ad52Diwq  SummonerJournalMilestoneMst   @0xE33EE4   8 fields
//     Uiwd28Vq str  milestone_id   S1B82FHK str  present_id
//     TdDHf59J str  target_id      37moriMq str  target_param
//     ZC0msu2L str  message        9hH0neGa int  points
//     30Kw4WBa int  present_type   wJsB35iH int  target_cnt
//
//   b2DjiaXp  SummonerJournalRewardsMst     @0xE343AC   6 fields
//     23DaiBpe str  task_id        S1B82FHK str  present_id
//     37moriMq str  target_param   ZC0msu2L str  message
//     30Kw4WBa int  present_type   wJsB35iH int  target_cnt
//     ⚠ NO target_id, unlike MilestoneMst — the reward's target must come
//     through present_id/target_param.  Do not assume the two MSTs share a
//     shape just because they share most keys.
//
//   M3dw18eB  SummonerJournalUserInfo       @0xE33D18   3 fields
//     h7eY3sAK str  user_id        9hH0neGa int  points
//     da365dB8 int  summoner_journal_flag
//
//   da38tRai  SummonerJournalUserTaskInfo   @0xE33A40   5 fields
//     h7eY3sAK str  user_id        23DaiBpe str  task_id
//     pG2n1A28 int  progress       2g6adYig int  claim_status
//     b638DwP8 int  is_available
//
//   r3D28bqW  SummonerJournalUserMilestoneInfo @0xE347A4  3 fields
//     h7eY3sAK str  user_id        Uiwd28Vq str  milestone_id
//     da365dB8 int  claim_status
//
// ✅ `30Kw4WBa` / `TdDHf59J` / `wJsB35iH` are the SHARED reward vocabulary
// (present_type 3=zel, 6=unit, 8=gem, 4/5/7=item) already used by Mystery
// Chest and the present system — so the journal's payouts reuse the existing
// reward switch rather than needing a new one.
//
// ⚠ `da365dB8` means DIFFERENT things in two classes: summoner_journal_flag
// on UserInfo, claim_status on UserMilestoneInfo.  The hashes are per field
// NAME, not per field meaning, so a shared key is not a shared concept —
// resolve each one against the class that owns it.
//
// ── Why these are PROBES ──────────────────────────────────────────────────
// They log the body and answer `{}`.  An UNREGISTERED GroupId is rejected
// before the dispatcher can decrypt, so the client shows "Unsupported
// request" or dies with NO log written at all and we learn nothing (§4.2).
// Registered, the next client launch prints the real body — which is how
// every other tile on this menu was identified.
//
// ⚠ Do NOT populate the responses by guessing field names: each response's
// readParam has to be audited IN FULL first (§6.20), because a field left at
// 0/"" is not neutral on this client.  Slots proved that three separate
// ways — an unchecked getObject, an unchecked split element [1], and a
// layout pass that runs regardless of whether setPrizeData bailed.

HANDLEF(SummonerJournalInfo)
{
	LOG_INFO << "SummonerJournalInfo: " << json;

	SummonerJournalInfoReq req{};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	const auto resp = co_await gme::buildSummonerJournal(theDb(), identity);

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		co_return HandleResult::error("Serialization error", glz::format_error(ec, buffer));
	}

	co_return HandleResult::success(buffer);
}

HANDLEF(SummonerJournalTaskRewards)
{
	LOG_INFO << "SummonerJournalTaskRewards: " << json;

	SummonerJournalTaskRewardsReq req{};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// The list is walked rather than assuming one entry, because "Receive All"
	// is expected to claim every finished mission and the group is modelled as
	// a list.  Whether the client batches them here or fires one request each
	// is still unconfirmed — this copes with either.
	uint32_t claimed = 0;
	for (const auto& entry : req.entries)
	{
		if (co_await gme::claimJournalTask(theDb(), identity, entry.task_id))
		{
			++claimed;
		}
	}
	LOG_INFO << "SummonerJournalTaskRewards: claimed " << claimed
		<< " of " << req.entries.size() << " requested";

	// Answered with the whole journal so the rows, the points header and the
	// buttons all re-render from one reply — the same reason PresentReceipt
	// returns the refreshed box.
	const auto resp = co_await gme::buildSummonerJournal(theDb(), identity);

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		co_return HandleResult::error("Serialization error", glz::format_error(ec, buffer));
	}

	co_return HandleResult::success(buffer);
}

HANDLEF(SummonerJournalMilestoneRewards)
{
	LOG_INFO << "SummonerJournalMilestoneRewards: " << json;

	SummonerJournalInfoReq req{};
	if (const auto& ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
	{
		co_return HandleResult::error("Deserialization error", glz::format_error(ec, json));
	}

	const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

	// createBody @0xE379E8 sends identity and the signal key only — no
	// milestone id — so this claims everything the player has earned rather
	// than one named rung.  Reuses SummonerJournalInfoReq for that reason.
	const auto paid = co_await gme::claimJournalMilestones(theDb(), identity);
	LOG_INFO << "SummonerJournalMilestoneRewards: paid " << paid << " milestone(s)";

	const auto resp = co_await gme::buildSummonerJournal(theDb(), identity);

	std::string buffer{};
	if (const auto& ec = glz::write_json(resp, buffer); ec)
	{
		co_return HandleResult::error("Serialization error", glz::format_error(ec, buffer));
	}

	co_return HandleResult::success(buffer);
}
