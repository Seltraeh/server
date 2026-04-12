#include "GetUserInfoRequestHandler.hpp"
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
#include "gme/response/UserTeamArenaArchive.hpp"
#include "gme/response/UserUnitDictionary.hpp"
#include "gme/response/UserFavorite.hpp"
#include "gme/response/UserArenaInfo.hpp"
#include "gme/response/UserGiftInfo.hpp"
#include "gme/response/VideoAdInfo.hpp"
#include "gme/response/VideoAdRegion.hpp"
#include "gme/response/SummonerJournalUserInfo.hpp"
#include "gme/response/SignalKey.hpp"
#include "gme/response/PermitPlace.hpp"
#include <json/reader.h>
#include <memory>

static const char* kUnitQuery =
    "SELECT id, unit_id, unit_lv, "
    "base_hp, add_hp, ext_hp, limit_over_hp, "
    "base_atk, add_atk, ext_atk, limit_over_atk, "
    "base_def, add_def, ext_def, limit_over_def, "
    "base_heal, add_heal, ext_heal, limit_over_heal, "
    "exp, total_exp, skill_id, skill_lv, "
    "extra_skill_id, extra_skill_lv, leader_skill_id, "
    "element, fe_bp, fe_max_usable_bp, unit_type_id, "
    "eqip_item_id, eqip_item_frame_id, eqip_item_id2, eqip_item_frame_id2 "
    "FROM user_units WHERE user_id = $1 ORDER BY id ASC LIMIT 4000";

static const char* kWarehouseQuery =
    "SELECT id, item_id, possession, new_flg, unknown_flg "
    "FROM user_warehouse_items WHERE user_id = $1 ORDER BY id ASC";

static const char* kDeckQuery =
    "SELECT deck_type, deck_num, user_unit_id, member_type, disp_order "
    "FROM user_party_decks WHERE user_id=$1 ORDER BY deck_type, deck_num, disp_order";

namespace {
    Response::UserUnitInfo::Data UnitDataFromRow(const drogon::orm::Row& row,
                                                 const std::string& userId)
    {
        Response::UserUnitInfo::Data d;
        d.userID     = userId;
        d.userUnitID = row["id"].as<uint32_t>();

        std::string rawId = row["unit_id"].as<std::string>();
        auto underscore = rawId.find('_');
        d.unitID = (uint32_t)std::stoull(
            underscore == std::string::npos ? rawId : rawId.substr(0, underscore)
        );

        d.unitLv        = row["unit_lv"].as<uint32_t>();
        d.baseHp        = row["base_hp"].as<uint32_t>();
        d.addHp         = row["add_hp"].as<uint32_t>();
        d.extHp         = row["ext_hp"].as<uint32_t>();
        d.limitOverHP   = row["limit_over_hp"].as<uint32_t>();
        d.baseAtk       = row["base_atk"].as<uint32_t>();
        d.addAtk        = row["add_atk"].as<uint32_t>();
        d.extAtk        = row["ext_atk"].as<uint32_t>();
        d.limitOverAtk  = row["limit_over_atk"].as<uint32_t>();
        d.baseDef       = row["base_def"].as<uint32_t>();
        d.addDef        = row["add_def"].as<uint32_t>();
        d.extDef        = row["ext_def"].as<uint32_t>();
        d.limitOverDef  = row["limit_over_def"].as<uint32_t>();
        d.baseHeal      = row["base_heal"].as<uint32_t>();
        d.addHeal       = row["add_heal"].as<uint32_t>();
        d.extHeal       = row["ext_heal"].as<uint32_t>();
        d.limitOverHeal = row["limit_over_heal"].as<uint32_t>();
        d.exp           = row["exp"].as<uint32_t>();
        d.totalExp      = row["total_exp"].as<uint32_t>();
        d.skillID       = row["skill_id"].as<uint32_t>();
        d.skillLv       = row["skill_lv"].as<uint32_t>();
        d.extraSkillID  = row["extra_skill_id"].as<uint32_t>();
        d.extraSkillLv  = row["extra_skill_lv"].as<uint32_t>();
        d.leaderSkillID = row["leader_skill_id"].as<uint32_t>();
        d.element       = row["element"].as<std::string>();
        d.FeBP          = row["fe_bp"].as<uint32_t>();
        d.FeMaxUsableBP = row["fe_max_usable_bp"].as<uint32_t>();
        d.unitTypeID    = row["unit_type_id"].as<uint32_t>();
        d.newFlg        = 1;
        d.receiveDate   = 100;
        d.FeSkillInfo   = "";
        d.eqipItemID       = row["eqip_item_id"].as<uint32_t>();
        d.eqipItemFrameID  = row["eqip_item_frame_id"].as<uint32_t>();
        d.equipItemID2     = row["eqip_item_id2"].as<uint32_t>();
        d.eqipItemFrameID2 = row["eqip_item_frame_id2"].as<uint32_t>();
        d.ExtraPassiveSkillID = d.ExtraPassiveSkillID2 = d.AddExtraPassiveSkillID = 0;

        return d;
    }

    Response::UserUnitInfo BuildUnitInfo(const drogon::orm::Result& unitResult,
                                         const std::string& userId)
    {
        Response::UserUnitInfo unitInfo;
        for (const auto& row : unitResult)
            unitInfo.Mst.emplace_back(UnitDataFromRow(row, userId));
        return unitInfo;
    }

    Response::UserWarehouseInfo BuildWarehouseInfo(const drogon::orm::Result& result)
    {
        Response::UserWarehouseInfo info;
        for (const auto& row : result)
        {
            Response::UserWarehouseInfo::Data d;
            d.userItemID = row["id"].as<uint32_t>();
            d.itemID     = row["item_id"].as<std::string>();
            d.possession = row["possession"].as<uint32_t>();
            d.newFlg     = row["new_flg"].as<uint32_t>();
            d.unknownFlg = row["unknown_flg"].as<uint32_t>();
            info.Mst.emplace_back(d);
        }
        return info;
    }

    Response::UserPartyDeckInfo BuildDeckInfo(const drogon::orm::Result& deckResult)
    {
        Response::UserPartyDeckInfo deckInfo;
        for (const auto& row : deckResult)
        {
            Response::UserPartyDeckInfo::Data d;
            d.deckType   = row["deck_type"].as<uint32_t>();
            d.deckNum    = row["deck_num"].as<uint32_t>();
            d.userUnitID = row["user_unit_id"].as<uint32_t>();
            d.memberType = row["member_type"].as<uint32_t>();
            d.dispOrder  = row["disp_order"].as<uint32_t>();
            deckInfo.Mst.emplace_back(d);
        }
        return deckInfo;
    }

    // Prefer unit 51317 as party leader; fall back to the first unit in the list.
    Response::UserPartyDeckInfo CreateDefaultDeck(const Response::UserUnitInfo& unitInfo)
    {
        Response::UserPartyDeckInfo deckInfo;
        if (unitInfo.Mst.empty())
            return deckInfo;

        auto it = std::find_if(unitInfo.Mst.begin(), unitInfo.Mst.end(),
            [](const Response::UserUnitInfo::Data& d) { return d.unitID == 51317; });
        uint32_t leaderUnitID = (it != unitInfo.Mst.end())
            ? it->userUnitID
            : unitInfo.Mst[0].userUnitID;

        Response::UserPartyDeckInfo::Data d;
        d.deckNum    = 0;
        d.deckType   = 1;
        d.dispOrder  = 0;
        d.memberType = 0;
        d.userUnitID = leaderUnitID;
        deckInfo.Mst.emplace_back(d);
        return deckInfo;
    }

    // Build the complete GetUserInfo response from fully-loaded data.
    void SendFullResponse(
        const Handler::HandlerBase* handler,
        UserInfo& user,
        const Handler::DrogonCallback& cb,
        const Response::UserUnitInfo& unitInfo,
        const Response::UserWarehouseInfo& warehouseInfo,
        const Response::UserPartyDeckInfo& deckInfo)
    {
        Json::Value res;
        user.info.Serialize(res);
        user.teamInfo.Serialize(res);

        Response::UserLoginCampaignInfo campaign;
        campaign.currentDay = 1;
        campaign.totalDays = 96;
        campaign.firstForTheDay = true;
        campaign.Serialize(res);

        unitInfo.Serialize(res);
        deckInfo.Serialize(res);

        Response::UserTeamArchive{}.Serialize(res);
        Response::UserTeamArenaArchive{}.Serialize(res);
        Response::UserUnitDictionary{}.Serialize(res);
        Response::UserFavorite{}.Serialize(res);
        Response::UserClearMissionInfo{}.Serialize(res);
        warehouseInfo.Serialize(res);
        Response::ItemFavorite{}.Serialize(res);
        Response::UserItemDictionaryInfo{}.Serialize(res);
        Response::UserArenaInfo{}.Serialize(res);
        Response::UserGiftInfo{}.Serialize(res);

        Response::SummonerJournalUserInfo journal;
        journal.userId = user.info.userID;
        journal.Serialize(res);

        Response::SignalKey signalKey;
        signalKey.key = "5EdKHavF";
        signalKey.Serialize(res);

        {
            Response::PermitPlace v;
            { Response::PermitPlace::Data d; d.setAreaID("100");   v.Mst.emplace_back(d); }
            { Response::PermitPlace::Data d; d.setLandID("1");     v.Mst.emplace_back(d); }
            { Response::PermitPlace::Data d; d.setGateID("1");     v.Mst.emplace_back(d); }
            { Response::PermitPlace::Data d; d.setMissionID("10"); v.Mst.emplace_back(d); }
            { Response::PermitPlace::Data d; d.setDungeonID("10"); v.Mst.emplace_back(d); }
            v.Serialize(res);
        }

        System::Instance().MstConfig().CopyUserInfoMstTo(res);

        cb(newGmeOkResponse(handler->GetGroupId(), handler->GetAesKey(), res));
    }
}

void Handler::GetUserInfoRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const {
    LOG_INFO << "UserInfoHandler: raw user_id: " << user.info.userID;

    GME_DB->execSqlAsync(
        "SELECT id FROM users WHERE id = $1",
        [this, &user, cb, req](const drogon::orm::Result& idResult)
        {
            if (idResult.empty())
                LOG_WARN << "UserInfoHandler: user_id not found in DB: " << user.info.userID;
            HandleResolved(user, cb, req);
        },
        [this, &user, cb, req](const drogon::orm::DrogonDbException& e)
        {
            LOG_ERROR << "UserInfoHandler: resolution query failed: " << e.base().what();
            HandleResolved(user, cb, req);
        },
        user.info.userID
    );
}

void Handler::GetUserInfoRequestHandler::HandleResolved(UserInfo& user, DrogonCallback cb, const Json::Value& req) const {
    LOG_INFO << "UserInfoHandler: resolved user_id: " << user.info.userID;

    // Handle unit inventory or squad management requests separately
    std::string action = req.isMember("action") ? req["action"].asString() : "";
    if (action == "load_unit_inventory" || action == "Zw3WIoWu" || action == "load_squad_management") {
        const std::string userId = user.info.userID;
        GME_DB->execSqlAsync(
            kUnitQuery,
            [this, &user, cb, userId](const drogon::orm::Result& unitResult) {
                LOG_INFO << "Found " << unitResult.size() << " units for user " << userId;

                auto unitPtr = std::make_shared<Response::UserUnitInfo>(BuildUnitInfo(unitResult, userId));

                GME_DB->execSqlAsync(
                    kDeckQuery,
                    [this, &user, cb, unitPtr](const drogon::orm::Result& deckResult)
                    {
                        Json::Value res;
                        unitPtr->Serialize(res);
                        if (!deckResult.empty())
                            BuildDeckInfo(deckResult).Serialize(res);
                        else
                            CreateDefaultDeck(*unitPtr).Serialize(res);
                        cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
                    },
                    [this, cb, unitPtr](const drogon::orm::DrogonDbException& e)
                    {
                        LOG_WARN << "GetUserInfo: deck query failed: " << e.base().what();
                        Json::Value res;
                        unitPtr->Serialize(res);
                        CreateDefaultDeck(*unitPtr).Serialize(res);
                        cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
                    },
                    userId
                );
            },
            [this, cb](const drogon::orm::DrogonDbException& e) { OnError(e, cb); },
            user.info.userID
        );
        return;
    }

    // Main user info loading logic
    GME_DB->execSqlAsync(
        "SELECT level, exp, max_unit_count, max_friend_count, zel, karma, brave_coin, "
        "max_warehouse_count, want_gift, free_gems, paid_gems, active_deck, summon_tickets, "
        "rainbow_coins, colosseum_tickets, active_arena_deck, total_brave_points, "
        "avail_brave_points, energy FROM userinfo WHERE id = $1",
        [this, &user, cb](const drogon::orm::Result& result) {
            if (result.size() > 0) {
                auto& sql = result[0];
                int col = 0;
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
                user.teamInfo.ActionPoint = sql[col++].as<uint32_t>();
            }
            else {
                auto sc = System::Instance().MstConfig().StartInfo();
                user.teamInfo.UserID = user.info.userID;
                user.teamInfo.Level = sc.Level;
                user.teamInfo.Exp = 0;
                user.teamInfo.MaxUnitCount = user.teamInfo.WarehouseCount = 4000;
                user.teamInfo.MaxFriendCount = sc.FriendCount;
                user.teamInfo.Zel = sc.Zel;
                user.teamInfo.Karma = sc.Karma;
                user.teamInfo.BraveCoin = 0;
                user.teamInfo.FreeGems = sc.FreeGems;
                user.teamInfo.PaidGems = sc.PaidGems;
                user.teamInfo.SummonTicket = sc.SummonTickets;
                user.teamInfo.ColosseumTicket = sc.ColosseumTickets;

                GME_DB->execSqlAsync(
                    "INSERT INTO userinfo (id, level, exp, max_unit_count, max_friend_count, "
                    "zel, karma, brave_coin, max_warehouse_count, want_gift, free_gems, paid_gems, "
                    "active_deck, summon_tickets, rainbow_coins, colosseum_tickets, active_arena_deck, "
                    "total_brave_points, avail_brave_points, energy) "
                    "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20)",
                    [](const drogon::orm::Result&) { LOG_INFO << "Inserted default userinfo for new user"; },
                    [](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "Failed to insert userinfo: " << e.base().what();
                    },
                    user.info.userID, user.teamInfo.Level, user.teamInfo.Exp, user.teamInfo.MaxUnitCount,
                    user.teamInfo.MaxFriendCount, user.teamInfo.Zel, user.teamInfo.Karma, user.teamInfo.BraveCoin,
                    user.teamInfo.WarehouseCount, user.teamInfo.WantGift, user.teamInfo.FreeGems, user.teamInfo.PaidGems,
                    user.teamInfo.ActiveDeck, user.teamInfo.SummonTicket, user.teamInfo.RainbowCoin,
                    user.teamInfo.ColosseumTicket, user.teamInfo.ArenaDeckNum, user.teamInfo.BravePointsTotal,
                    user.teamInfo.CurrentBravePoints, user.teamInfo.ActionPoint
                );
            }

            const auto& msts = System::Instance().MstConfig();
            const auto& p = msts.GetProgressionInfo().Mst.at(user.teamInfo.Level - 1);
            user.teamInfo.DeckCost = p.deckCost;
            user.teamInfo.MaxFriendCount = p.friendCount;
            user.teamInfo.AddFriendCount = p.addFriendCount;
            user.teamInfo.MaxActionPoint = p.actionPoints;
            if (user.teamInfo.ActionPoint == 0)
                user.teamInfo.ActionPoint = user.teamInfo.MaxActionPoint;

            // Load user units
            GME_DB->execSqlAsync(
                kUnitQuery,
                [this, &user, cb](const drogon::orm::Result& unitResult) {
                    LOG_INFO << "Found " << unitResult.size() << " units for user " << user.info.userID;

                    auto unitPtr = std::make_shared<Response::UserUnitInfo>(
                        BuildUnitInfo(unitResult, user.info.userID));

                    // Load warehouse items
                    GME_DB->execSqlAsync(
                        kWarehouseQuery,
                        [this, &user, cb, unitPtr](const drogon::orm::Result& whResult) mutable
                        {
                            LOG_INFO << "Found " << whResult.size() << " warehouse items for user " << user.info.userID;

                            auto whPtr = std::make_shared<Response::UserWarehouseInfo>(
                                BuildWarehouseInfo(whResult));

                            // Load saved party deck
                            GME_DB->execSqlAsync(
                                kDeckQuery,
                                [this, &user, cb, unitPtr, whPtr](const drogon::orm::Result& deckResult)
                                {
                                    Response::UserPartyDeckInfo deckInfo = deckResult.empty()
                                        ? CreateDefaultDeck(*unitPtr)
                                        : BuildDeckInfo(deckResult);
                                    SendFullResponse(this, user, cb, *unitPtr, *whPtr, deckInfo);
                                },
                                [this, &user, cb, unitPtr, whPtr](const drogon::orm::DrogonDbException& e)
                                {
                                    LOG_WARN << "GetUserInfo: deck query failed: " << e.base().what();
                                    Response::UserPartyDeckInfo deckInfo = CreateDefaultDeck(*unitPtr);
                                    SendFullResponse(this, user, cb, *unitPtr, *whPtr, deckInfo);
                                },
                                user.info.userID
                            );
                        },
                        [this, cb](const drogon::orm::DrogonDbException& e) { OnError(e, cb); },
                        user.info.userID
                    );
                },
                [this, cb](const drogon::orm::DrogonDbException& e) { OnError(e, cb); },
                user.info.userID
            );
        },
        [this, cb](const drogon::orm::DrogonDbException& e) { OnError(e, cb); },
        user.info.userID
    );
}
