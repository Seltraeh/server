#include "UserInfoHandler.hpp"
#include <db/DbMacro.hpp>
#include <core/Utils.hpp>
#include <core/System.hpp>
#include "gme/response/UserTeamInfo.hpp"
#include "gme/response/UserLoginCampaignInfo.hpp"
#include "gme/response/NoticeInfo.hpp"
#include "gme/response/UserItemDictionaryInfo.hpp"
#include "gme/response/UserTeamArchive.hpp"
#include "gme/response/UserUnitInfo.hpp"
#include "gme/response/UserPartyDeckInfo.hpp"
#include "gme/response/UserWarehouseInfo.hpp"
#include "gme/response/UserClearMissionInfo.hpp"
#include "gme/response/ItemFavorite.hpp"
#include "gme/response/UserItemDictionaryInfo.hpp"
#include "gme/response/UserTeamArenaArchive.hpp"
#include "gme/response/UserUnitDictionary.hpp"
#include "gme/response/UserFavorite.hpp"
#include "gme/response/UserArenaInfo.hpp"
#include "gme/response/UserGiftInfo.hpp"
#include "gme/response/VideoAdInfo.hpp"
#include "gme/response/VideoAdRegion.hpp"
#include "gme/response/SummonerJournalUserInfo.hpp"
#include "gme/response/SignalKey.hpp"

void Handler::UserInfoHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
	GME_DB->execSqlAsync(
		"SELECT level, exp, max_unit_count, max_friend_count, "
		"zel, karma, brave_coin, max_warehouse_count, want_gift, "
		"free_gems, paid_gems, active_deck, summon_tickets, "
		"rainbow_coins, colosseum_tickets, active_arena_deck, "
		"total_brave_points, avail_brave_points "
		"FROM userinfo WHERE id = $1", [this, &user, cb](const drogon::orm::Result& result) {

			if (result.size() > 0)
			{
				int col = 0;
				auto& sql = result[0];
				user.teamInfo.UserID = user.info.userID;
				user.teamInfo.Level = sql[col++].as<uint32_t>();
				user.teamInfo.Exp = sql[col++].as<int64_t>();
				user.teamInfo.MaxUnitCount = sql[col++].as<uint32_t>();
				user.teamInfo.MaxFriendCount = sql[col++].as<uint32_t>();
				user.teamInfo.Zel = sql[col++].as<uint64_t>();
				user.teamInfo.Karma = sql[col++].as<uint64_t>();
				user.teamInfo.BraveCoin = sql[col++].as<uint32_t>();
				user.teamInfo.WarehouseCount = sql[col++].as<uint32_t>();
				user.teamInfo.WantGift = sql[col++].as<std::string>();
				user.teamInfo.FreeGems = sql[col++].as<uint32_t>();
				user.teamInfo.PaidGems = sql[col++].as<uint32_t>();
				user.teamInfo.ActiveDeck = sql[col++].as<uint32_t>();
				user.teamInfo.SummonTicket = sql[col++].as<uint32_t>();
				user.teamInfo.RainbowCoin = sql[col++].as<uint32_t>();
				user.teamInfo.ColosseumTicket = sql[col++].as<uint32_t>();
				user.teamInfo.ArenaDeckNum = sql[col++].as<uint32_t>();
				user.teamInfo.BravePointsTotal = sql[col++].as<uint32_t>();
				user.teamInfo.CurrentBravePoints = sql[col++].as<uint32_t>();
			}
			else
			{
				auto sc = System::Instance().MstConfig().StartInfo();
				user.teamInfo.UserID = user.info.userID;
				user.teamInfo.Level = sc.Level;
				user.teamInfo.Exp = 0;
				user.teamInfo.MaxUnitCount = sc.UnitCount;
				user.teamInfo.MaxFriendCount = sc.FriendCount;
				user.teamInfo.Zel = sc.Zel;
				user.teamInfo.Karma = sc.Karma;
				user.teamInfo.BraveCoin = 0;
				user.teamInfo.WarehouseCount = sc.UnitCount;
				user.teamInfo.FreeGems = sc.FreeGems;
				user.teamInfo.PaidGems = sc.PaidGems;
				user.teamInfo.SummonTicket = sc.SummonTickets;
				user.teamInfo.ColosseumTicket = sc.ColosseumTickets;
			}

			Json::Value res;
			user.info.Serialize(res);
			user.teamInfo.Serialize(res);

			{
				Response::UserLoginCampaignInfo v;
				v.currentDay = 1;
				v.totalDays = 96;
				v.firstForTheDay = true;
				v.Serialize(res);
			}

			{
				Response::UserTeamArchive v;
				v.Serialize(res);
			}

			{
				Response::UserTeamArenaArchive v;
				v.Serialize(res);
			}

			{
				Response::UserUnitInfo v;
				v.Serialize(res);
			}

			{
				Response::UserPartyDeckInfo v;
				v.Serialize(res);
			}

			{
				Response::UserUnitDictionary v;
				v.Serialize(res);
			}

			{
				Response::UserFavorite v;
				v.Serialize(res);
			}

			{
				Response::UserClearMissionInfo v;
				v.Serialize(res);
			}

			{
				Response::UserWarehouseInfo v;
				v.Serialize(res);
			}

			{
				Response::ItemFavorite v;
				v.Serialize(res);
			}

			{
				Response::UserItemDictionaryInfo v;
				v.Serialize(res);
			}

			{
				Response::UserArenaInfo v;
				v.Serialize(res);
			}

			{
				Response::UserGiftInfo v;
				v.Serialize(res);
			}

			{
				// SummonerJournal
				Response::SummonerJournalUserInfo v;
				v.userId = user.info.userID;
				v.Serialize(res);
			}

			{
				Response::SignalKey v;
				v.key = "5EdKHavF";
				v.Serialize(res);
			}

			System::Instance().MstConfig().CopyUserInfoMstTo(res);

			cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
		}, 
		[this, cb](const drogon::orm::DrogonDbException& e) { OnError(e, cb); },
		user.info.userID
	);
}
