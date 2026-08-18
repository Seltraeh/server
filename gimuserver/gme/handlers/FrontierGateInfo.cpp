#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/db/DatabaseInterface.h>
#include <gimuserver/gme/common/Common.hpp>

#include <chrono>
#include <string>
#include <unordered_map>

// FrontierGateInfo (M17pPotk) — the player's per-gate Frontier Gate progress.
//
// Request  : identity only.  createBody @0x13BB8C8 emits the three BaseRequest
//            tags and ZERO addParam/addGroup calls, so there is nothing to read
//            beyond the login envelope.
// Response : key "dPM7oJDl" (FrontierGateInfoResponse), an ARRAY of 11-field
//            entries.  Empty array = "no progress on any gate", the correct
//            idle shape.
//
// Where the gate CATALOG comes from (corrected 2026-08-07 — an earlier version
// of this comment said the server sends it under olzMKWSZ, which is wrong):
// nothing in this server emits olzMKWSZ; `auto_cache` only builds a LoadJson
// wrapper, not a response field.  The client already HAS the catalog — its
// Initialize manifest (KeC10fuL) reports F_FROGATE_MST at ver=562,
// F_FROGATE_SUPPORT_MST 153, F_FROGATE_AREA_MST 135, F_FROGATE_REWARD_MST 315,
// all matching deploy/mst/version_info_mst.json, so there is nothing to sync.
// This response supplies per-user progress on top of a catalog the client owns.
//
// Full decode evidence: tools/ida/audits/dPM7oJDl_audit.txt.
// State model / build order: tools/FRONTIER_GATE_STATE_MODEL.md.
//
// FrontierGateInfoReq/Resp and FrontierGateInfo are generated from the KDL
// (packet-generator/assets/net/{handlers,frontier_gate}.kdl).

HANDLEF(FrontierGateInfo)
{
    LOG_INFO << "FrontierGateInfo: " << json;

    // Generated types are qualified with `::` throughout: the handler function
    // is GmeHandlers::FrontierGateInfo, which otherwise shadows the packet
    // struct of the same name.  Same convention as BadgeInfo.cpp.
    ::FrontierGateInfoReq req{};
    {
        glz::context ctx{};
        if (const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(req, json, ctx); ec)
            LOG_WARN << "FrontierGateInfo: parse error: " << glz::format_error(ec, json);
    }

    const auto identity = (co_await gme::getUserIdentity(theDb(), req.login_info)).nonEmpty();

    ::FrontierGateInfoResp resp{};

    db::InterfaceResult<db::Result> rows{};
    try
    {
        rows = co_await db::DatabaseInterface::read(
            theDb(),
            "user_frontier_gates",
            {
                db::Data("frogate_id"),
                db::Data("state"),
                db::Data("progress"),
                db::Data("score"),
                db::Data("mission_id"),
                db::Data("sel_support_id"),
                db::Lookup("user_id", identity.userId),
            });
    }
    catch (const drogon::orm::DrogonDbException& ex)
    {
        // Empty OK rather than a closed session (handbook §5) — the FG screen
        // then renders every gate as unstarted instead of retry-looping.
        LOG_WARN << "FrontierGateInfo: SELECT failed: " << ex.base().what();
        co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
    }

    // Index the user's stored progress so it can be overlaid onto the catalog.
    struct Progress
    {
        int32_t state = 0;
        int32_t progress = 0;
        int32_t score = 0;
        int32_t support_id = 0;
        std::string mission_id;
    };
    std::unordered_map<int32_t, Progress> savedById;
    for (const auto& row : rows.data)
    {
        Progress p{};
        p.state      = row["state"].as<int32_t>();
        p.progress   = row["progress"].as<int32_t>();
        p.score      = row["score"].as<int32_t>();
        p.support_id = row["sel_support_id"].as<int32_t>();
        p.mission_id = row["mission_id"].as<std::string>();
        savedById.emplace(row["frogate_id"].as<int32_t>(), std::move(p));
    }

    // THIS RESPONSE IS THE TILE LIST, not just a progress overlay.
    //
    // Verified 2026-08-07 against a live client: returning {"dPM7oJDl":[]}
    // renders the Frontier Gate "Select a Quest" screen completely EMPTY even
    // though the client already holds F_FROGATE_MST at the matching version.
    // The client builds its gate list from FrontierGateInfoList (see
    // FrontierGateInfoList::getPermitFrogateList), which this response
    // populates — so a gate the server does not send is a gate that does not
    // exist as far as the UI is concerned.
    //
    // Therefore: emit one entry per PERMITTED gate from the catalog, and
    // overlay the user's stored progress where a row exists.  A gate with no
    // row is simply an unstarted gate, not a missing one.
    const auto& gateMst = theServer()->cache().frontierGateMst();

    // AVAILABILITY WINDOW — every gate is advertised as open.
    //
    // The MST's own dates cannot be forwarded verbatim.  Two independent
    // problems, both of which drop the gate out of the client's permit list:
    //
    //  1. 24 of the 94 gates carry -62169984000 (year 1) as a "no date"
    //     sentinel.  That value does NOT FIT IN A 32-BIT INT (min is
    //     -2147483648), so any atoi/StrToInt on the client overflows.  We keep
    //     it as i64 in mst/frontier_gate.kdl for exactly this reason; sending
    //     it on the wire pushes the problem onto a client we do not control.
    //  2. 63 of the 94 expired between 2016 and 2022.  Against a 2026 clock
    //     they are simply over.
    //
    // Verified 2026-08-07: forwarding the raw MST dates produced 31 entries and
    // a completely EMPTY quest-select screen.
    //
    // Since this is an offline preservation server with no live event calendar,
    // the useful behaviour is that all archived content stays reachable, so
    // every gate gets a permissive window instead of its historical one.
    // FORMAT: "YYYY-MM-DD HH:MM:SS", not unix seconds.
    //
    // CAPTURED FROM: deploy/mst/challenge_mst.json, which carries this exact
    // hash pair (qA7M9EjP / SzV0Nps7) as "2014-03-04 08:00:00".  That table is
    // Frontier Hunter — the sibling feature sharing the Survey Office — and
    // mst/frontier_hunter.kdl types both fields `datetime`.  mst/guild.kdl
    // agrees.  Only mst/frontier_gate.kdl stores them as raw unix seconds,
    // which is why forwarding the MST values verbatim was wrong twice over:
    // wrong format AND a sentinel that overflows int32.
    //
    // Verified 2026-08-07: sending unix seconds ("1420070400") produced 94
    // entries and an EMPTY quest-select screen.
    constexpr const char* kOpenedAt  = "2014-01-01 00:00:00";  // before every real start date
    constexpr const char* kFarFuture = "2037-12-31 23:59:59";  // inside int32 range if parsed as epoch

    resp.gates.reserve(gateMst.size());
    for (const auto& gate : gateMst)
    {
        ::FrontierGateInfo entry{};
        entry.frogate_id = gate.id;

        entry.start_date = kOpenedAt;
        entry.end_date   = kFarFuture;

        // ENTRY MISSION AND BATTLE COUNT.
        //
        // Verified 2026-08-07 against a live client by probing seven gates with
        // different field combinations: ONLY the gates carrying a non-empty
        // j28VNcUW rendered a tile.  state, progress_max and ranking made no
        // difference to visibility — an entry without a mission id simply does
        // not exist as far as the quest-select screen is concerned.
        //
        // A gate's missions are the rows of its dungeon: gate 1's dungeon
        // 3000001 holds missions 3000010..3000014.  So the entry mission is the
        // lowest id in that dungeon, and the count is what the tile prints as
        // "Battles N" — the same probe showed progress_max=7 rendering as
        // "Battles 7", which also confirms 69bpUIXR is the battle count and is
        // NOT swapped with ranking.
        const auto& byDungeon = theServer()->cache().missionsByDungeon();
        if (const auto dit = byDungeon.find(gate.dungeon_id);
            dit != byDungeon.end() && !dit->second.empty())
        {
            entry.mission_id   = std::to_string(dit->second.front());
            entry.progress_max = static_cast<int32_t>(dit->second.size());
        }
        else
        {
            // No missions under this gate's dungeon: the client would render an
            // unenterable tile, so omit the gate entirely.  All 94 gates resolve
            // today, so this is a guard against future data drift, not a filter.
            LOG_WARN << "FrontierGateInfo: gate " << gate.id << " has no missions under dungeon "
                     << gate.dungeon_id << ", skipping";
            continue;
        }

        if (const auto it = savedById.find(gate.id); it != savedById.end())
        {
            entry.state      = it->second.state;
            entry.progress   = it->second.progress;
            entry.score      = it->second.score;
            // A stored mission id means a run reached that floor; fall back to
            // the entry mission when the row predates that being tracked.
            if (!it->second.mission_id.empty())
                entry.mission_id = it->second.mission_id;
            // One support slot per run.  The wire field is a comma list because
            // the client parses it with CommonUtils::parseList; 0 means "none
            // picked" and must stay an EMPTY list, not a list containing 0.
            if (it->second.support_id != 0)
                entry.support_id_list.push_back(it->second.support_id);
        }
        else
        {
            // Unstarted gate.  DECODED 2026-08-07 from the live probe: the gate
            // sent with state=2 rendered with a "CLEAR" banner while the one
            // sent with state=1 rendered plain, so 1 = available and 2 =
            // cleared.  (0 and 3 were also probed but those gates carried no
            // mission id, so they proved nothing about the enum.)  This agrees
            // with the same wire key in CampaignMissionInfoResponse, where >=1
            // is unlocked.
            entry.state = 1;
        }

        // progress_max (69bpUIXR), ranking (2wHGmJqm) and reward_info_list
        // (JQ23rIvk) stay at their defaults.  The first two are named only from
        // vtable slot position and may even be swapped for each other, so there
        // is nothing honest to put in them; handbook §3.4 warns that guessing a
        // value is worse than leaving the client its own default.

        resp.gates.emplace_back(std::move(entry));
    }

    co_return HandleResult::success(glz::write_json(resp).value_or("{}"));
}
