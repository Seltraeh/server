#pragma once

#include <gimuserver/gme/common/Common.hpp>

namespace gme
{
// Complete snapshot, not a delta: the client reader replaces its list.
inline drogon::Task<std::vector<::UserEnteredFeatureInfo>> loadFeatureVisits(
    const db::Database database, const UserIdentity identity)
{
    const auto rows = co_await database->execSqlCoro(
        "SELECT feature_id, dungeon_id FROM user_entered_features "
        "WHERE user_id=$1 ORDER BY feature_id, dungeon_id;", identity.userId);
    std::vector<::UserEnteredFeatureInfo> result;
    for (const auto& row : rows)
    {
        ::UserEnteredFeatureInfo entry{};
        entry.feature_id = row["feature_id"].as<int32_t>();
        entry.dungeon_id = row["dungeon_id"].as<int32_t>();
        entry.new_flg = 0;
        result.push_back(entry);
    }
    co_return result;
}
}
