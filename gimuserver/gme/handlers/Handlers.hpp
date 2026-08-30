#pragma once

/*!
* Class to hold results of an handler.
*/
struct HandleResult
{
	/*!
	* Success JSON.
	*/
	std::string successJson;

	/*!
	* Error message.
	*/
	std::string errorMsg;

	/*!
	* Exception message.
	*/
	std::string exceptionMsg;

	/*!
	* Checks if it's in error.
	* @return true if the result is an error, otherwise false
	*/
	constexpr bool isError() const { return !errorMsg.empty(); }

	/*!
	* Create an error result
	* @param error Error message
	* @param ex Exception
	* @return An error result
	*/
	static inline HandleResult error(const std::string& error, const std::string& ex = "") {
		return HandleResult("", error, ex);
	}

	/*!
	* Create a successfull result
	* @param success Success message
	* @return A success result
	*/
	static inline HandleResult success(const std::string& success) {
		return HandleResult(success, "", {});
	}

private:
	/*!
	* Private constructor for a result.
	* @param success Success JSON (or empty in case of no success)
	* @param error Error message (or empty in case of a success)
	* @param ex Optional glaze error
	*/
	explicit HandleResult(const std::string& success, const std::string& error, const std::string& ex) : successJson(success), errorMsg(error), exceptionMsg(ex)
	{}
};

/*!
* Defines how an handler function is constructer.
* @param session Drogon session handler
* @param json Decrypted input JSON
* @return Output decrypted JSON
*/
using HandlerFunc = std::function<drogon::Task<HandleResult>(drogon::SessionPtr session, std::string json)>;

/*!
* Defines the prototype of an handler.
* @param name Name of the handler
*/
#define HANDLE(name) drogon::Task<HandleResult> name(drogon::SessionPtr session, std::string json)

/*!
* Defines the prototype of an handler with namespace.
* @param name Name of the handler
*/
#define HANDLEF(name) HANDLE(GmeHandlers::name)

// list all available handlers
namespace GmeHandlers
{
	HANDLE(Initialize);
	HANDLE(GetPlayerInfo);
	HANDLE(BadgeInfo);
	HANDLE(ChallengeArenaResetInfo);
	HANDLE(ControlCenterEnter);
	HANDLE(DeckEdit);
	HANDLE(FriendGet);
	HANDLE(GachaAction);
	HANDLE(GachaList);
	HANDLE(UnitSelectorGachaTicket);
	HANDLE(HomeInfo);
	HANDLE(MissionEnd);
	HANDLE(MissionStart);
	HANDLE(ItemEdit);
	HANDLE(ItemSphereEqp);
	HANDLE(ItemFavorite);
	HANDLE(ItemSell);
	HANDLE(ItemMix);
	HANDLE(NgwordCheck);
	HANDLE(CreateUser);
	HANDLE(TutorialSkip);
	HANDLE(TutorialUpdate);
	HANDLE(UpdateInfoLight);
	HANDLE(UserInfo);
	HANDLE(UnitFavorite);
	HANDLE(UnitEvo);
	HANDLE(UnitMix);
	HANDLE(UnitSell);
	HANDLE(TownUpdate);
	HANDLE(TownFacilityUpdate);
	HANDLE(EventTokenInfo);
	HANDLE(AreaInfo);
	HANDLE(CampaignStart);
	HANDLE(CampaignMissionGet);
	HANDLE(CampaignDeckGet);
	HANDLE(CampaignBattleStart);
	HANDLE(CampaignBattleEnd);
	HANDLE(CampaignReceipt);
	HANDLE(CampaignEnd);
	HANDLE(ChallengeBase);
	HANDLE(ChallengeRanking);
	HANDLE(ShopUse);
	HANDLE(FrontierGateInfo);
	HANDLE(FrontierGateStart);
	HANDLE(FrontierGateContinue);
	HANDLE(FrontierGateSave);
	HANDLE(FrontierGateEnd);
	HANDLE(FrontierGateRetry);
	HANDLE(FrontierGateRestart);
	HANDLE(FixGiftInfo);
	HANDLE(DungeonEventUpdate);
	HANDLE(GetScenarioPlayingInfo);
	HANDLE(RaidUpScenarioInfo);

	// Present box.  Declared and implemented, NOT yet registered — PresentList
	// is GroupId YPBU7MD8 but its AES key is unknown, and PresentReceipt's
	// GroupId is unknown too.  See the header comment in Present.cpp for how to
	// recover both.
	HANDLE(PresentList);
	HANDLE(PresentReceipt);

	// Achievements.  Registered, but answers empty until the four response
	// classes are decoded — see the header comment in Achievement.cpp.
	HANDLE(GetAchievementInfo);

	// Brave Points & Rewards (daily tasks / milestones / BP prize shop).
	HANDLE(DailyTaskUserInfo);
	HANDLE(UpdatePermitPlaceInfo);
	HANDLE(UpdateEventInfo);
	HANDLE(Chronology);
	HANDLE(GetDistributeDungeonKeyInfo);
	HANDLE(DungeonKeyReceipt);
	HANDLE(DungeonKeyUse);

	// Rewards menu — the tiles that had no handler at all.  Probes for now:
	// they log the body and answer {}.  See RewardsMenu.cpp for the tile/tag
	// map and where each request is fired from.
	HANDLE(SlotAction);
	HANDLE(MysteryBoxList);
	HANDLE(MysteryBoxClaim);
	HANDLE(DailyLogin);

	// Summoner Journal — tag 0, the last tile with no handler.  Probes: they
	// log the body and answer {}.  See SummonerJournal.cpp for the request
	// shapes and the six response classes the screen expects.
	HANDLE(SummonerJournalInfo);
	HANDLE(SummonerJournalTaskRewards);
	HANDLE(SummonerJournalMilestoneRewards);
}
