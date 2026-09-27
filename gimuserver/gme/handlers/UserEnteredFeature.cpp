#include "App.hpp"
#include "Handlers.hpp"
#include <gimuserver/gme/common/FeatureVisits.hpp>

// Android createBody @0x1C706F0 sends one visit under 2386Diw1.
// This is visit history only: it never releases a feature or grants rewards.
HANDLEF(UserEnteredFeature)
{
    (void)session;
    ::UserEnteredFeatureReq req{};
    if (const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(req, json); ec)
        co_return HandleResult::error("Invalid feature visit");
    if (req.entered_features.size() != 1)
        co_return HandleResult::error("Expected one feature visit");
    const auto& visit = req.entered_features.front();
    if (visit.feature_id < 0 || visit.dungeon_id < 0 ||
        (visit.feature_id == 0 && visit.dungeon_id == 0) || visit.new_flg != 0)
        co_return HandleResult::error("Invalid feature visit values");

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    auto transaction = co_await theDb()->newTransactionCoro();
    try
    {
        co_await transaction->execSqlCoro(
            "INSERT INTO user_entered_features(user_id,feature_id,dungeon_id) "
            "VALUES ($1,$2,$3) ON CONFLICT(user_id,feature_id,dungeon_id) DO NOTHING;",
            identity.userId, visit.feature_id, visit.dungeon_id);
        ::UserEnteredFeatureResp resp{};
        resp.entered_features = co_await gme::loadFeatureVisits(transaction, identity);
        std::string body;
        if (const auto ec = glz::write_json(resp, body); ec)
            throw std::runtime_error("Cannot serialize feature visits");
        co_return HandleResult::success(body);
    }
    catch (...)
    {
        transaction->rollback();
        throw;
    }
}
