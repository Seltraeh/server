#include "DeckEditRequestHandler.hpp"
#include "gme/response/SignalKey.hpp"
#include "gme/response/UserPartyDeckInfo.hpp"
#include "gme/response/NoticeInfo.hpp"
#include "core/System.hpp"
#include <db/DbMacro.hpp>
#include <vector>

void Handler::DeckEditRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
    const std::string groupId = GetGroupId();
    const std::string aesKey  = GetAesKey();
    const std::string userId  = user.info.userID;

    // Capture user data for use in async callbacks
    const Response::UserInfo     capturedInfo     = user.info;
    const Response::UserTeamInfo capturedTeamInfo = user.teamInfo;

    const Json::Value& deckArray = req["dX7S2Lc1"];
    if (!deckArray.isArray() || deckArray.empty())
    {
        Json::Value res;
        { Response::SignalKey v; v.key = "fZnLr4t9"; v.Serialize(res); }
        capturedInfo.Serialize(res);
        capturedTeamInfo.Serialize(res);
        { Response::NoticeInfo v; v.url = System::Instance().ServerConfig().NoticeUrl; v.Serialize(res); }
        cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
        return;
    }

    struct DeckEntry {
        uint32_t deckType = 1, deckNum = 0, memberType = 0, dispOrder = 0, userUnitId = 0;
    };

    std::vector<DeckEntry> entries;
    for (const auto& item : deckArray)
    {
        DeckEntry e;
        try { e.deckType   = (uint32_t)std::stoull(item.get("U9ABSYEp", "1").asString()); } catch (...) {}
        try { e.dispOrder  = (uint32_t)std::stoull(item.get("XuJL4pc5", "0").asString()); } catch (...) {}
        try { e.userUnitId = (uint32_t)std::stoull(item.get("edy7fq3L", "0").asString()); } catch (...) {}
        try { e.memberType = (uint32_t)std::stoull(item.get("gr48vsdJ", "0").asString()); } catch (...) {}
        try { e.deckNum    = (uint32_t)std::stoull(item.get("zsiAn9P1", "0").asString()); } catch (...) {}
        entries.push_back(e);
    }

    const uint32_t deckType = entries[0].deckType;
    const uint32_t deckNum  = entries[0].deckNum;

    // Build response deck info from the parsed entries (echoed back to client)
    Response::UserPartyDeckInfo deckInfo;
    for (const auto& e : entries)
    {
        Response::UserPartyDeckInfo::Data d;
        d.deckType   = e.deckType;
        d.deckNum    = e.deckNum;
        d.memberType = e.memberType;
        d.dispOrder  = e.dispOrder;
        d.userUnitID = e.userUnitId;
        deckInfo.Mst.emplace_back(d);
    }

    // Delete existing entries for this deck slot, then insert the new ones
    GME_DB->execSqlAsync(
        "DELETE FROM user_party_decks WHERE user_id=$1 AND deck_type=$2 AND deck_num=$3",
        [cb, groupId, aesKey, userId, entries, capturedInfo, capturedTeamInfo,
         deckInfo = std::move(deckInfo)](const drogon::orm::Result&) mutable
        {
            for (const auto& e : entries)
            {
                GME_DB->execSqlAsync(
                    "INSERT INTO user_party_decks "
                    "(user_id, deck_type, deck_num, user_unit_id, member_type, disp_order) "
                    "VALUES ($1, $2, $3, $4, $5, $6)",
                    [](const drogon::orm::Result&) {},
                    [](const drogon::orm::DrogonDbException& ex) {
                        LOG_WARN << "DeckEdit: insert failed: " << ex.base().what();
                    },
                    userId, e.deckType, e.deckNum, e.userUnitId, e.memberType, e.dispOrder
                );
            }

            Json::Value res;
            { Response::SignalKey v; v.key = "fZnLr4t9"; v.Serialize(res); }
            capturedInfo.Serialize(res);
            capturedTeamInfo.Serialize(res);
            deckInfo.Serialize(res);
            {
                Response::NoticeInfo v;
                v.url = System::Instance().ServerConfig().NoticeUrl;
                v.Serialize(res);
            }
            cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
        },
        [cb, groupId, aesKey](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "DeckEdit: delete failed: " << e.base().what();
            Json::Value res;
            cb(newGmeOkResponse(groupId.c_str(), aesKey.c_str(), res));
        },
        userId, deckType, deckNum
    );
}
