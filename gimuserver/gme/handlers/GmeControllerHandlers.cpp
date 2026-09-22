#include "App.hpp"
#include "Handlers.hpp"
#include <gimuserver/utils/BfCrypt.hpp>

/*!
* Simple structure to hold handler definitions.
*/
struct GmeHandler
{
	/*!
	* Handler name.
	*/
	const char* name;

	/*!
	* JSON AES cryptation key.
	*/
	const char* key;

	/*!
	* Pointer to a function handler.
	*/
	HandlerFunc func;
};

/*!
* Calculate the hash of a string view.
* @param[in] in String to hash
* @return Hashed string
*/
static constexpr auto hash(std::string_view in)
{
	uint64_t hash = 0;
	for (char c : in) {
		hash = (hash * 131) + c;
	}
	return hash;
}

/*!
* Hashing extension for C strings.
* @param[in] str String to hash
* @param[in] len Length of the string
* @return Hashed string
*/
static constexpr auto operator"" _hash(const char* str, size_t len)
{
	return hash(std::string_view(str, len));
}

/*!
* Registers a new handler.
* @param[in] id Handler ID
* @param[in] func Handler function
* @param[in] key Handler AES key
*/
#define REGISTER(id, func, key) case id##_hash: return { #func, key, GmeHandlers::func }

/*!
* Gets the handler of a message.
* @param cmd Message ID
* @return Output handler
*/
static GmeHandler getHandler(std::string_view cmd)
{
	switch (hash(cmd))
	{
	default:
		return { nullptr, nullptr, nullptr };


	REGISTER("MfZyu1q9", Initialize, "EmcshnQoDr20TZz1");
	REGISTER("Zw3WIoWu", ChallengeArenaResetInfo, "KlwYMGF1");
	REGISTER("nJ3A7qFp", BadgeInfo, "bGxX67KB");
	REGISTER("uYF93Mhc", ControlCenterEnter, "d0k6LGUu");
	REGISTER("m2Ve9PkJ", DeckEdit, "d7UuQsq8");
	REGISTER("2o4axPIC", FriendGet, "EoYuZ2nbImhCU1c0");
	REGISTER("F7JvPk5H", GachaAction, "bL9fipzaSy7xN2w1");
	REGISTER("Uo86DcRh", GachaList, "8JbxFvuSaB2CK7Ln");
	REGISTER("k57TdKDj", UnitSelectorGachaTicket, "1IJ8SaNk");
	REGISTER("NiYWKdzs", HomeInfo, "f6uOewOD");
	// Records / Archive screens.  GroupId and AES key recovered from the
	// .rodata block: the key sits at GroupId+9 and the mangled class name
	// ends it -- vUQrAV65 / 7pW4xF9H / actionSymbol/DheJ07aI.php /
	// 20GetPlayerInfoRequest.
	REGISTER("vUQrAV65", GetPlayerInfo, "7pW4xF9H");
	REGISTER("9TvyNR5H", MissionEnd, "oINq0rfUFPx5MgmT");
	REGISTER("jE6Sp0q4", MissionStart, "csiVLDKkxEwBfR70");
	// An interrupted battle: the pay-to-revive prompt after a wipe, and the
	// resume screen the next login lands on.  Both were unregistered, so both
	// would have closed the session on use -- and without the first there is
	// nothing for the second to resume.  See gme/handlers/MissionBreak.cpp.
	REGISTER("p8B2i9rJ", MissionContinue, "G3FwvQfy5hcxHMen");
	REGISTER("IP96ys7T", MissionRestart,  "0Zy3G9eD");
	REGISTER("ruoB7bD8", ItemEdit, "DHEfRexCu0q5TAQm");
	REGISTER("0IXGiC9t", ItemSphereEqp, "CZE56XAY");
	REGISTER("I8il6EiI", ItemFavorite, "aRoIftRy");
	REGISTER("qDQerU74", ItemSell, "73aFNjPu");
	REGISTER("4P5GELTF", ItemMix, "AFqKIJ8Z4mHPB9xg");
	REGISTER("TA4MnZX8", NgwordCheck, "r4Smw5TX");
	REGISTER("uV6yH5MX", CreateUser, "4agnATy2DrJsWzQk");
	REGISTER("d36DaiJl", TutorialSkip, "p3qD61db");
	REGISTER("T1nCVvx4", TutorialUpdate, "7hqzmR3T");
	REGISTER("ynB7X5P9", UpdateInfoLight, "7kH9NXwC");
	// Home's background polls (UpdateInfoLight.cpp).  Each closed the session
	// with "Unsupported request" before it was registered — see the KDL.
	REGISTER("RUV94Dqz", UpdateInfo,            "hy0P9xjsGJ6MAgb2");
	REGISTER("68pTQAJv", NoticeUpdate,          "WHfcd53M");
	REGISTER("5fc8bf2c", UserLoginCampaignInfo, "4eb7ce1b");

	// Guild.  Every pair below came out of tools/bin/so_groupids.py, which reads
	// <Name>Request::getRequestID and ::getEncodeKey straight from libgame.so --
	// verified against three known-good pairs before the sweep was trusted.
	//
	// GuildJoinedList asks the same question as GuildInfo ("what guild am I in?")
	// with the same identity-only body, so it shares the handler.
	REGISTER("138ba8d4", GuildInfo,             "23gD81ia");
	REGISTER("3890ab5j", GuildInfo,             "820b38z5");
	REGISTER("g298Da10", GuildCreate,           "G23Bd01d");
	REGISTER("ja5Enusw", GuildRecomendedMember, "8upheqaC");
	REGISTER("ad81b8at", GuildMemberUpdate,     "2b1bDo2m");
	// The message box.  Not guild-named, so the Guild sweep missed it and
	// "View messages" answered Unsupported request: rYSfaC4P.
	REGISTER("rYSfaC4P", InboxMessageManage,    "0R9ZPaSf");

	// The rest of the guild set, answered with a benign empty body.
	//
	// NOT laziness -- it is the difference between a blank panel and what looks
	// like a crash.  An unregistered GroupId and a handler error produce the SAME
	// GmeErrorCommand::Close, so any guild sub-screen this slice does not cover
	// would drop the session with no http_log and no dump to explain it.  With a
	// stub the screen simply has nothing in it, which is diagnosable.
	//
	// Every pair recovered by tools/bin/so_groupids.py.  Replace a line here with
	// a real handler as each feature lands.
	REGISTER("W1Dgsfnz", GuildUnimplemented, "eMfdsGVJ");
	REGISTER("J93ki3Bw", GuildUnimplemented, "m83D19ib");
	REGISTER("X89bDai1", GuildUnimplemented, "Bi9Ralbq");
	REGISTER("38b67ie1", GuildUnimplemented, "hG738a5b");
	REGISTER("bk30i39b", GuildUnimplemented, "3bkb98a1");
	REGISTER("adk28bij", GuildUnimplemented, "fgd3uu2b");
	REGISTER("yDDcC0vW", GuildUnimplemented, "NOeugPyv");
	REGISTER("1D8bba8D", GuildUnimplemented, "bUd2bd0e");
	REGISTER("bfa2D1bp", GuildUnimplemented, "9b3abdk1");
	REGISTER("oRa3ztp8", GuildUnimplemented, "pVg9L9Uw");
	REGISTER("dsRW32K", GuildUnimplemented, "Afs43Dc4");
	REGISTER("9b98aKj1", GuildUnimplemented, "b8dAl1ic");
	REGISTER("MTzXyuFL", GuildUnimplemented, "MkV5xHDL");
	REGISTER("36jhZ9YZ", GuildUnimplemented, "Zd3d2zGx");
	REGISTER("bka03Bi1", GuildUnimplemented, "F91Dalb8");
	REGISTER("v87b3Diq", GuildUnimplemented, "Da0m39b1");
	REGISTER("zGk5R8Dd", GuildUnimplemented, "DRJQatky");
	REGISTER("Xdi3ebD9", GuildUnimplemented, "TrDi19Bd");
	REGISTER("Ie01B83k", GuildUnimplemented, "P93Db8q1");
	REGISTER("L3D9eK19", GuildUnimplemented, "9Ur3Dkb3");
	REGISTER("Z3d9b0ew", GuildUnimplemented, "Bd83Dakb");
	REGISTER("v83Diq7b", GuildUnimplemented, "m39bDa01");
	REGISTER("d735ub8o", GuildUnimplemented, "bk7eob01");
	REGISTER("fd54Sey", GuildUnimplemented, "F32dfFS");
	REGISTER("W1Daxfnz", GuildUnimplemented, "eMMfFDVJ");
	REGISTER("7di8aie9", GuildUnimplemented, "yh8ak18b");
	REGISTER("U83BiqDw", GuildUnimplemented, "Cv3DaI3W");
	REGISTER("83kBdiqD", GuildUnimplemented, "93Di3Ge8");
	REGISTER("Q8Eib8Xv", GuildUnimplemented, "UI3Da1B7");
	REGISTER("M2dD4b0A", GuildUnimplemented, "I2ixn4Ac");
	REGISTER("26ZGiseY", GuildUnimplemented, "00dU9t9M");
	REGISTER("ZZ9xaJoi", GuildUnimplemented, "sdtfUs9y");
	REGISTER("2b9D01b4", GuildRanking,       "23Djab0e");
	REGISTER("7ekSBz2y", GuildRankingDetail, "tWF58aK0");
	REGISTER("0Dl4rdsn", GuildUnimplemented, "ndk4sS0s");
	REGISTER("aXPZmq9h", GuildUnimplemented, "J90g7sZK");
	REGISTER("Xfpo7jE2", GuildUnimplemented, "tVBMO5GW");
	REGISTER("R38ba9M3", GuildUnimplemented, "0D18dQn4");
	REGISTER("cXi7b58e", GuildUnimplemented, "jK18btd0");
	REGISTER("38adiJeb", GuildUnimplemented, "ja3biAqb");
	REGISTER("92bDoqBi", GuildUnimplemented, "w3Bne038");
	REGISTER("a38B82bG", GuildUnimplemented, "7Ykwq038");
	REGISTER("38bad198", GuildUnimplemented, "d38bHiqj");
	REGISTER("cTZ3W2JG", UserInfo, "ScJx6ywWEb0A3njT");
	REGISTER("2p9LHCNh", UnitFavorite,            "cb4ESLa1");
	REGISTER("0gUSE84e", UnitEvo,                 "biHf01DxcrPou5Qt");
	// Omni+ Boost.  A PROBE that logs and refuses — see UnitOmniEvo.cpp.
	REGISTER("4Dk4spf9", UnitOmniEvo,             "4s3lsODp");
	REGISTER("Mw08CIg2", UnitMix,                 "JnegC7RrN3FoW8dQ");
	REGISTER("Ri3uTq9b", UnitSell,                "92VqcGFWuPkmT60U");
	REGISTER("CuQ5oB8U", TownUpdate,              "w1eo2ZDJ");
	REGISTER("8v43tz7g", TownFacilityUpdate,       "rq7Yd1nG");
	REGISTER("f49als4D", EventTokenInfo,           "94lDsgh4");

	// Quest / world-map entry point.
	REGISTER("Zds63G5y", AreaInfo,             "YfAh7gqojdXEtFR1");

	// Campaign subsystem (see HANDLER_BLUEPRINT.md §7).
	REGISTER("6Y0gaPQN", CampaignStart,        "WM6yr4ej");
	REGISTER("RSm6p2d4", CampaignMissionGet,   "5jzXN7AH");
	REGISTER("C3a0VnQK", CampaignDeckGet,      "q2ZtYJ6P");
	REGISTER("h1RjcD3S", CampaignBattleStart,  "4CKoVAq0");
	REGISTER("pTNB6yw3", CampaignBattleEnd,    "t06HFsXP");
	REGISTER("5Imq3wC0", CampaignReceipt,      "4DAgP80B");
	REGISTER("jF9Kkro4", CampaignEnd,          "4X9tBSg8");
	REGISTER("D74TYRf1", CampaignDeckEdit,     "e2k4s6jc");
	REGISTER("W2VU91I7", CampaignItemEdit,     "2Jd10iwn");
	REGISTER("Utzc3oj5", CampaignSave,         "6xc3GhQF");
	REGISTER("Ht2jeWV8", CampaignRestart,      "vm7LYZz4");

	// Frontier Gate.  GroupIds/AES keys from the legacy handler registry; the
	// remaining five (End cAJp7U4l, Save Ng73nFHJ, Continue uiFIMUH6, Ranking
	// 26zW90oG, Restart v0m1FU0g, Retry 9pzHMBzq) are listed in
	// tools/ida/audits/dPM7oJDl_audit.txt and land with tier 3.
	// Survey Office hub — the Frontier Gate entrance sits behind it, so without
	// this the client errors before it ever requests M17pPotk.
	REGISTER("nUAW2B0a", ChallengeBase,        "uE5Tsv6P");
	REGISTER("2Kxi7rIB", ChallengeRanking,     "v1PzNE9f");
	// The Frontier Hunter lobby's own state request.  It was unregistered until
	// 2026-09-13, and deploy/log has it closing a live session at 07:28:53 that
	// day right after the "challenge" intro played.
	REGISTER("jF3AS4cp", ChallengeUserInfo,    "Nst6MK5m");
	// Generic shop spend — reached by the Frontier Gate "use 1 Gem to restore
	// Hunter Orbs" prompt.  Currently logs the body to capture ShopUseType.
	REGISTER("xe8tiSf4", ShopUse,              "qthMXTQSkz3KfH9R");
	REGISTER("M17pPotk", FrontierGateInfo,     "sochkegz");
	REGISTER("l3lkDBSc", FrontierGateStart,    "vYPCD34q");
	// Run control — the Continue / Pause / Retire prompt between floors.
	REGISTER("uiFIMUH6", FrontierGateContinue, "ZiosS4cd");
	REGISTER("Ng73nFHJ", FrontierGateSave,     "4SdtoczN");
	REGISTER("cAJp7U4l", FrontierGateEnd,      "Vvpy7qZR");
	REGISTER("9pzHMBzq", FrontierGateRetry,    "njsKMqGT");
	REGISTER("v0m1FU0g", FrontierGateRestart,  "whb7Y2wX");

	REGISTER("gLRIn74v", FixGiftInfo,          "15gTE9ft");
	// The other half of the gift feature.  Unregistered until 2026-09-15, and
	// GiftRecieveConnectScene::initConnect fires it just to OPEN the screen --
	// so the inbox closed the session the way the Shop tab used to.
	REGISTER("ifYoPJ46", GetGiftInfo,          "6zHEYa9U");
	REGISTER("oim9TU1D", ArenaInfo,            "oqQxdFfa");
	REGISTER("SfMN9w4p", FriendDelete,         "yWXI80UKQNBZFozw");
	REGISTER("WUNi08YL", FriendApply,          "gbpdAEZuv8IP40UF");
	REGISTER("983D5Dii", UserEnteredFeature,   "Dr6pwV3i");
	REGISTER("29slks49", UserGemShardInfo,     "930sDd3i");
	REGISTER("29s22s49", VideoAdSlotsClaimBonus, "93055d2i");

	// World-map / Grand Gaia entry sequence stubs.
	REGISTER("BjAt1D6b", DungeonEventUpdate,     "k5EiNe9x");
	REGISTER("VRfsv4e3", GetScenarioPlayingInfo, "Bh4WqR01");
	REGISTER("R38qvphm", RaidUpScenarioInfo,     "72EyFbW8");

	// Present box.  Recovered with tools/ida/groupid_key_pair_audit.py: in
	// .rodata each handler occupies a block laid out as
	//     <GroupId>\0<AesKey>\0 actionSymbol <field keys...> <ClassName>
	// with the key ALWAYS at GroupId+9 (8 chars + NUL).  That offset held for
	// 10/10 known-good pairs used as controls, and PresentReceipt's block also
	// contains `o6uWU0Z7` — the exact group key its createBody emits — which
	// independently confirms the attribution.
	REGISTER("nhjvB52R", PresentList,    "6F9sMzBxEv8jXpau");
	REGISTER("bV5xa0ZW", PresentReceipt, "X2QFqAKfomPIg3rG");

	// Achievements.  This is the request the gift-tab screen actually fires —
	// YPBU7MD8 is GetAchievementInfo, NOT the present list (§4.2).  It answers
	// empty for now; registering it stops the unhandled-GroupId error from
	// killing the screen before its other requests run.
	REGISTER("YPBU7MD8", GetAchievementInfo, "AKjzyZ81");
	// The Merit Point loop: claim a finished achievement, spend the points,
	// and the detail page's Start / Give Up button.  Deliver (vsaXI4M0) is the
	// Trade Zel / Karma / Units / Spheres screens; its cond_type says which.
	REGISTER("uq69mTtR", AchievementRewardReceive, "cbE74zBZ");
	REGISTER("m9LiF6P2", AchievementTrade,         "0IWC9LVq");
	REGISTER("dx5qvm7L", AchievementAccept,        "g9N1y7bc");
	REGISTER("vsaXI4M0", AchievementDeliver,       "2Lj5hIEG");

	// TODO(IAP) -- the Shop tab (scene 700, ShopTopScene) is REACHABLE and no
	// longer crashes, but nothing behind it is built.
	//
	// The hard crash on tapping Shop was NOT an unregistered request: scene
	// 700's `layout_shop_topV2.csv` was missing from the content drop while all
	// thirteen of its images were present, and a missing layout is a null deref
	// (LayoutCacheList::getObject returns nullptr, LayoutCache::getX is
	// `ldr s0,[x0,#0x30]`).  It is authored now at
	// content/bundleshop/layout_shop_topV2.csv.  ⚠ deploy/game_content is
	// gitignored, so a fresh content drop LOSES it and the crash returns --
	// tools/asset_preflight.py catches that in one run.
	//
	// The scene is inert on purpose.  ShopTopScene::updateEvent @0x1AD65D4 only
	// asks for a catalogue refresh when a category's end date has passed, and
	// its very first test is `if (getCount() < 1) skip` -- so with no
	// BundlePacksCategoryInfo rows sent (we send none) the refresh never fires
	// and none of these four can be reached:
	//
	//     swarOb4u / GeJa9uTe   BundleCategoryRefresh
	//     D3gyT3b3 / a8pE3x39   BundlePurchase
	//     kD298bt6 / u45Bi2p0   BundlePurchaseIap
	//     u06y2UE4 / c2kjaKv8   SetPurchaseInfo
	//
	// WHEN IAP IS RESTORED, register those four and fill `7itHlDro`
	// (BundlePacksCategoryInfo: T2bifltU id, In7lGGLn image, 2r4EoNt4 order,
	// fru8e2rL bundle-id list, qA7M9EjP/SzV0Nps7 start/end as LONGS) plus
	// `K72eC4nz` (BundlePacksInfo).  The plan is free "purchases" on a rotating
	// cycle rather than real money, so BundlePurchase is the one that matters
	// and BundlePurchaseIap can stay unregistered.
	//
	// ⚠ AN ERROR REPLY CLOSES THE SESSION.  The dispatcher turns both an
	// unregistered id AND a handler error into GmeErrorCommand::Close, so a
	// "politely refuse" handler has to return success with a benign body, not
	// an error.  See the Close branch above.

	// Dual Brave Burst.  The Bond button on a unit's detail page only exists
	// once DbbMst is on the wire, which UserInfo now does — so this had to be
	// registered in the same change, or the first tap would close the session.
	REGISTER("0EtanubR", DbbBond,       "Tr7dR4dR");
	// The rank-up.  Named Unit*, not Dbb*, which is how it was missed on the
	// first pass through this subsystem — see gme/handlers/UnitBondBoost.cpp.
	REGISTER("tr5rOwro", UnitBondBoost, "fus9A2ut");

	// Three requests with no state behind them here.  Registering them only
	// stops an unhandled GroupId from closing the session, which looks like a
	// crash on whatever screen fired it — see gme/handlers/Notice.cpp.
	REGISTER("a5k36D28", BannerClick,      "a63Ghbi2");
	REGISTER("5s4aVWfc", NoticeList,       "miMBpUZ3");
	REGISTER("cuKwx5rF", NoticeReadUpdate, "o2rhxCmg");

	// Brave Points & Rewards.  NOTE the 7-character key — every other key here
	// is 8 or 16, and this one is correct: confirmed by a live decrypt.  It also
	// lives outside the aligned handler table (see DailyTask.cpp).
	REGISTER("m7g0Ekb5", DailyTaskUserInfo, "Hd8c3Y6");
	// Claim on the Milestone Rewards / Redeem Prizes tabs.  Both were
	// reachable with this unregistered, so the button closed the session.
	REGISTER("oP3bn47e", DailyTaskClaimReward, "ut0j9h3K");
	REGISTER("1MJT6L3W", UpdatePermitPlaceInfo,  "3zip5Htw");
	REGISTER("rCB7ZI8x", UpdateEventInfo,        "L1o4eGbi");
	REGISTER("5o8ZlDGX", Chronology,             "SNrhAG29");

	// Vortex dungeon keys (Metal / Jewel Parade).  GroupIds and AES keys from
	// the legacy handler registry, where all three were empty-OK stubs.
	// All answer with the key inventory under eFU7Qtb0 — see DungeonKey.cpp.
	REGISTER("1mr9UsYz", GetDistributeDungeonKeyInfo, "r0ZA3pn5");
	REGISTER("WCJE0xe2", DungeonKeyReceipt,           "V4pfQo5C");
	REGISTER("aGT5S6qZ", DungeonKeyUse,               "3rPx6tTw");

	// Rewards menu — the Slots / Mystery Chest / Daily Spin tiles.  Every id and
	// key here was read out of .rodata via each Request class's getRequestID /
	// getEncodeKey (12-byte functions returning a literal, key at GroupId+9);
	// none is a guess.  All four are probes that log and answer {} — see
	// RewardsMenu.cpp for the tile map, the request shapes and the response
	// classes still to be audited.
	REGISTER("vChFp73J", SlotAction,      "hm9X6BQj");
	REGISTER("pAJ2Xesw", MysteryBoxList,  "DaswA3rE");
	REGISTER("2paswUpR", MysteryBoxClaim, "kadRadU5");
	REGISTER("4aClzokO", DailyLogin,      "stI81haQ");

	// Summoner Journal (tag 0, scene 101802) — the last unbuilt tile.  Ids and
	// keys read from getRequestID / getEncodeKey the same way; all three are
	// probes until a real client body has been seen.  SummonerJournal.cpp
	// carries the six response classes and their dispatch keys.
	// The Summoner Avatar arc.  All three were unregistered while the arc was
	// reachable, and UserSummonerInfoEdit is the FIRST request it makes.
	REGISTER("ZJYkXcHo", UserSummonerInfoEdit, "lyR0us9b");
	REGISTER("nv4d3O7F", SummonerMix,          "vM65bAB4");
	REGISTER("qWkYyw5i", SummonerSkillGet,     "mrXjLLJB");

	REGISTER("32Gwida0", SummonerJournalInfo,             "66B2pDki");
	REGISTER("2y48D13d", SummonerJournalTaskRewards,      "7nm3Dqe9");
	REGISTER("3a83iY3r", SummonerJournalMilestoneRewards, "98Tw0ubW");

	}
}

drogon::Task<GmeAction> GmeController::Handle(drogon::SessionPtr session, const GmeAction& gme)
{
	const auto& body = gme.body.value();
	const auto& header = gme.header;
	const auto& handler = getHandler(header.id);

	GmeAction resp{};
	resp.header = gme.header;
	resp.header.client_id = "---";

	if (!handler.func || !handler.key)
	{
		// Logged because nothing else records it: the request cannot be
		// decrypted without its key, the client closes the session on this
		// reply, and the missing http_log file is what once made a Home
		// background poll (UpdateInfo) look like a Vortex crash.
		LOG_ERROR << "Unsupported request " << header.id << " — no handler registered";
		DumpLog logReq;
		theServer()->tryOpenHttpDumpLog(header.id, logReq);
		logReq << "UNSUPPORTED REQUEST: " << header.id
			<< " (no handler registered; the client closes the session)\n";

		GmeError err{};
		err.cmd = GmeErrorCommand::Close;
		err.flag = GmeErrorFlags::IsInError;
		err.message = std::format("Unsupported request: {}", header.id);
		resp.error = err;
	}
	else
	{
		const auto& decryptedGme = BfCrypt::ReadGME(gme, handler.key);
		if (!decryptedGme.has_value())
		{
			//LOG_ERROR << "Cannot read GME for " << handler.key;
			GmeError err{};
			err.cmd = GmeErrorCommand::Close;
			err.flag = GmeErrorFlags::IsInError;
			err.message = std::format("Unable to decode request: {}", header.id);
			resp.error = err;
		}
		else
		{
			const auto& inputJson = decryptedGme.value();
			DumpLog logReq;
			theServer()->tryOpenHttpDumpLog(header.id, logReq);

			logReq << "REQUEST: " << inputJson << "\n";

			try
			{
				const auto& outputJson = co_await handler.func(session, inputJson);

				if (outputJson.isError())
				{
					logReq << "RESPONSE IN ERROR: " << outputJson.errorMsg << " ex: " << outputJson.exceptionMsg << "\n";
					GmeError err{};
					err.cmd = GmeErrorCommand::Close;
					err.flag = GmeErrorFlags::IsInError;
					err.message = std::format("Unable to handle request: \"{}\", Error: \"{}\"", header.id, outputJson.errorMsg);
					resp.error = err;
				}
				else
				{
					logReq << "RESPONSE: " << outputJson.successJson << "\n";
					resp.body = BfCrypt::BuildGME(outputJson.successJson, handler.key);
				}
			}
			catch (const drogon::orm::DrogonDbException& ex)
			{
				LOG_ERROR << "Handler error " << header.id << " (" << handler.name << ") database exception: " << ex.base().what();
				logReq << "EXCEPTION (db): " << ex.base().what() << "\n";
				GmeError err{};
				err.cmd = GmeErrorCommand::Close;
				err.flag = GmeErrorFlags::IsInError;
				err.message = std::format("Unable to run database query: \"{}\"", header.id);
				resp.error = err;
			}
			catch (const std::exception& ex)
			{
				LOG_ERROR << "Handler error " << header.id << " (" << handler.name << ") exception: " << ex.what();
				logReq << "EXCEPTION (std): " << ex.what() << "\n";
				GmeError err{};
				err.cmd = GmeErrorCommand::Close;
				err.flag = GmeErrorFlags::IsInError;
				err.message = std::format("Unable to handle request: \"{}\", Error: \"{}\"", header.id, ex.what());
				resp.error = err;
			}
		}
	}

	co_return resp;
}
