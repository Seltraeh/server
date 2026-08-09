#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>

#include <array>
#include <string>

// ChallengeRanking (2Kxi7rIB / v1PzNE9f) — the Frontier Hunter ranking board,
// reached from the "Guild Ranking" / ranking button on the Survey Office event
// screen.  Unregistered until now, which is why viewing it answered
// "Unsupported request: 2Kxi7rIB".
//
// Request : group cvg8hzp9 carrying c5yZnpB4, the event id whose board is being
//           viewed — the same id ChallengeBase hands out, so the two are a pair.
// Response: key Hmiwj75u (ChallengeRankingResponse), an ARRAY of 18-field rows.
//
// Every one of those 18 fields is a real named setter from
// readparam_analysis.json — unlike dPM7oJDl, this struct contains no guessed
// names.  Fields 14-17 (Sex, Element, HairID, ArmID) are the Summoner avatar,
// so each row draws the player's character next to their score.
//
// EMULATED BOARD.  A single-player offline server has no other players, so
// there is no real leaderboard to serve.  Rather than return an empty array —
// which renders a blank board and tells us nothing — this seeds the viewer's own
// row from their actual save plus a handful of synthetic rivals, the same
// approach FriendGet takes with DecompFriend.  The scores are invented and
// labelled as such; nothing here is a decoded value.

namespace
{
// Synthetic rivals bracketing the player, so the board shows a plausible spread
// rather than a single lonely row.  Names deliberately look like emulator
// fixtures rather than real handles.
struct Rival
{
    const char* handle;
    int32_t     team_lv;
    int32_t     score;
    const char* unit_id;
    int32_t     unit_lv;
    int32_t     element;
};

constexpr std::array<Rival, 5> kRivals{{
    { "DecompHunter", 120, 985000, "60357", 120, 6 },
    { "GateRunner",    98, 742000, "50246",  99, 2 },
    { "NebulaBreaker", 87, 610500, "40155",  85, 4 },
    { "HallVeteran",   71, 388000, "30013",  70, 1 },
    { "OrbCollector",  54, 145200, "20112",  55, 3 },
}};

// // UNVERIFIED: the NH65Wj0f (InfoType) enum is not decoded.  0 is used for
// every row; if the client needs the viewer's own row flagged differently, this
// is the field to vary first.
constexpr int32_t kInfoType = 0;
}

HANDLEF(ChallengeRanking)
{
    LOG_INFO << "ChallengeRanking: " << json;

    ::ChallengeRankingReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "ChallengeRanking: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();
    const auto eventId = req.challenge.frohun_id;

    ::ChallengeRankingResp resp{};

    // The viewer's own row, from their real save so the board is not entirely
    // fictional.  Their score is 0 until Frontier Hunter scoring exists, which
    // puts them below every rival — correct for a player who has not competed.
    std::string handleName = "Summoner";
    int32_t teamLv = 1;
    try
    {
        const auto row = co_await db::DatabaseInterface::read(
            theDb(),
            "user_info",
            {
                db::Data("username"),
                db::Data("level"),
                db::Lookup("id", identity.userId),
            });
        handleName = row.front<std::string>("username");
        teamLv = row.front<int32_t>("level");
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        LOG_WARN << "ChallengeRanking: could not read viewer row: " << ex.base().what();
    }

    int32_t rank = 1;
    const auto missionId = eventId != 0 ? std::to_string(eventId) : std::string{};

    for (const auto& rival : kRivals)
    {
        ::ChallengeRankingEntry entry{};
        entry.user_id       = "RIVAL" + std::to_string(rank);
        entry.handle_name   = rival.handle;
        entry.team_lv       = rival.team_lv;
        entry.unit_id       = rival.unit_id;
        entry.unit_lv       = rival.unit_lv;
        entry.point1        = std::to_string(rival.score);
        entry.point2        = rival.score;
        entry.point3        = rival.score;
        entry.rank_order    = rank;
        entry.info_type     = kInfoType;
        entry.rank          = rank;
        entry.mission_id    = missionId;
        entry.unit_img_type = 1;
        entry.ep3_flg       = 0;
        entry.sex           = 1;
        entry.element       = rival.element;
        entry.hair_id       = "1";
        entry.arm_id        = "1";
        resp.ranking.emplace_back(std::move(entry));
        ++rank;
    }

    ::ChallengeRankingEntry self{};
    self.user_id       = identity.userId;
    self.handle_name   = std::move(handleName);
    self.team_lv       = teamLv;
    self.unit_id       = "10017";
    self.unit_lv       = 1;
    self.point1        = "0";
    self.point2        = 0;
    self.point3        = 0;
    self.rank_order    = rank;
    self.info_type     = kInfoType;
    self.rank          = rank;
    self.mission_id    = missionId;
    self.unit_img_type = 1;
    self.ep3_flg       = 0;
    self.sex           = 1;
    self.element       = 1;
    self.hair_id       = "1";
    self.arm_id        = "1";
    resp.ranking.emplace_back(std::move(self));

    LOG_INFO << "ChallengeRanking: served " << resp.ranking.size()
             << " emulated rows for event " << eventId;

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
