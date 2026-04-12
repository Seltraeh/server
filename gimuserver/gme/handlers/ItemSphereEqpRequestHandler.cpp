#include "ItemSphereEqpRequestHandler.hpp"
#include <db/DbMacro.hpp>
#include <core/System.hpp>
#include <sstream>
#include <vector>

// Actual request format (captured from client log):
//   "wx1ZLFj9": [{"a2utCvs8": "wh1:wh2:userUnitId:itemId1:itemId2[,wh1:wh2:userUnitId:itemId1:itemId2...]"}]
//
// Multiple units may be equipped simultaneously as a comma-separated list.
// Each segment: wh1:wh2:userUnitId:masterItemId1:masterItemId2
//   wh1/wh2   = warehouse instance row id (unused server-side)
//   userUnitId = user_units.id of the unit being equipped
//   itemId1/2  = sphere master item id (0 = clear slot)

namespace {
    // Apply sphere changes to one unit: read old state, update slots, adjust warehouse counts.
    // All DB calls are fire-and-forget since the caller responds immediately after dispatching.
    void ApplySphereOp(const std::string& userId, uint32_t userUnitId,
                       uint32_t newItem1, uint32_t newFrame1,
                       uint32_t newItem2, uint32_t newFrame2)
    {
        GME_DB->execSqlAsync(
            "SELECT eqip_item_id, eqip_item_id2 FROM user_units WHERE id=$1 AND user_id=$2",
            [userId, userUnitId, newItem1, newFrame1, newItem2, newFrame2]
            (const drogon::orm::Result& oldRows)
            {
                const uint32_t oldItem1 = oldRows.empty() ? 0 : oldRows[0]["eqip_item_id"].as<uint32_t>();
                const uint32_t oldItem2 = oldRows.empty() ? 0 : oldRows[0]["eqip_item_id2"].as<uint32_t>();

                // Update unit sphere slots
                GME_DB->execSqlAsync(
                    "UPDATE user_units "
                    "SET eqip_item_id=$1, eqip_item_frame_id=$2, "
                    "    eqip_item_id2=$3, eqip_item_frame_id2=$4 "
                    "WHERE id=$5 AND user_id=$6",
                    [](const drogon::orm::Result&) {},
                    [userUnitId](const drogon::orm::DrogonDbException& e) {
                        LOG_ERROR << "ItemSphereEqp: unit update failed id=" << userUnitId
                            << ": " << e.base().what();
                    },
                    newItem1, newFrame1, newItem2, newFrame2, userUnitId, userId
                );

                // Adjust warehouse for each slot
                auto adjustWarehouse = [userId](uint32_t oldId, uint32_t newId)
                {
                    if (oldId == newId) return;
                    if (oldId != 0)
                        GME_DB->execSqlAsync(
                            "UPDATE user_warehouse_items SET possession = possession + 1 "
                            "WHERE user_id=$1 AND item_id=$2",
                            [](const drogon::orm::Result&) {},
                            [oldId](const drogon::orm::DrogonDbException& e) {
                                LOG_WARN << "ItemSphereEqp: return failed item=" << oldId;
                            },
                            userId, std::to_string(oldId)
                        );
                    if (newId != 0)
                        GME_DB->execSqlAsync(
                            "UPDATE user_warehouse_items SET possession = MAX(0, possession - 1) "
                            "WHERE user_id=$1 AND item_id=$2",
                            [](const drogon::orm::Result&) {},
                            [newId](const drogon::orm::DrogonDbException& e) {
                                LOG_WARN << "ItemSphereEqp: consume failed item=" << newId;
                            },
                            userId, std::to_string(newId)
                        );
                };

                adjustWarehouse(oldItem1, newItem1);
                adjustWarehouse(oldItem2, newItem2);
            },
            [userUnitId](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "ItemSphereEqp: read old state failed id=" << userUnitId
                    << ": " << e.base().what();
            },
            userUnitId, userId
        );
    }
} // namespace

void Handler::ItemSphereEqpRequestHandler::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{
    LOG_INFO << "ItemSphereEqpRequest: " << req.toStyledString();

    if (!req.isMember("wx1ZLFj9") || !req["wx1ZLFj9"].isArray() || req["wx1ZLFj9"].empty())
    {
        LOG_WARN << "ItemSphereEqpRequest: unrecognised request structure — acking without DB update";
        Json::Value res;
        cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
        return;
    }

    const std::string packed = req["wx1ZLFj9"][0].get("a2utCvs8", "").asString();
    const std::string userId  = user.info.userID;
    const auto& mst = System::Instance().MstConfig();

    // Split by comma to get individual unit operations, then process each
    std::istringstream opStream(packed);
    std::string segment;
    while (std::getline(opStream, segment, ','))
    {
        if (segment.empty()) continue;

        std::vector<std::string> parts;
        {
            std::istringstream ss(segment);
            std::string token;
            while (std::getline(ss, token, ':'))
                parts.push_back(token);
        }

        if (parts.size() < 5)
        {
            LOG_WARN << "ItemSphereEqpRequest: segment has fewer than 5 parts: " << segment;
            continue;
        }

        uint32_t userUnitId = 0, itemId1 = 0, itemId2 = 0;
        try { userUnitId = (uint32_t)std::stoull(parts[2]); } catch (...) {}
        try { itemId1    = (uint32_t)std::stoull(parts[3]); } catch (...) {}
        try { itemId2    = (uint32_t)std::stoull(parts[4]); } catch (...) {}

        const uint32_t frameId1 = itemId1 ? (uint32_t)mst.GetItemSphereType(std::to_string(itemId1)) : 0;
        const uint32_t frameId2 = itemId2 ? (uint32_t)mst.GetItemSphereType(std::to_string(itemId2)) : 0;

        LOG_INFO << "ItemSphereEqpRequest: unitId=" << userUnitId
            << " slot1=(" << itemId1 << ",frame=" << frameId1 << ")"
            << " slot2=(" << itemId2 << ",frame=" << frameId2 << ")";

        ApplySphereOp(userId, userUnitId, itemId1, frameId1, itemId2, frameId2);
    }

    // All DB operations are dispatched; respond immediately
    Json::Value res;
    cb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}
