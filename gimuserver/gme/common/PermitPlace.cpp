#include "PermitPlace.hpp"
#include "App.hpp"
#include "Common.hpp"

#include <algorithm>
#include <ctime>
#include <set>
#include <stdexcept>

namespace gme
{
namespace
{
// WHICH PERMIT CHANNEL A ROW BELONGS ON.
//
// PermitPlaceInfoList holds three disjoint partitions and each permit response
// clears only its own at row 0:
//
//   yXNM8kL3 PermitPlace   -> removeNormalObjects  @0x126C9B0
//   Y73tHKS8 PermitPlaceSp -> removeSpecialObjects @0x126C744
//   Y73mHKS8 PermitPlaceML -> removeMLObjects
//
// Each remover resolves a row to its AreaMst -- directly for an area row,
// through DungeonMst or MissionMst for the others -- and asks
// AreaMst::isSpecial (type 1 or 3) or isML (type 10).  Send a special area's
// permits on yXNM8kL3 and removeNormalObjects deliberately SKIPS them, so they
// are appended on every refresh and can never be withdrawn.
//
// That is not a corner case in this data: 340 areas are type 1, 3 are type 3
// and 35 are type 10, leaving 33 of 411 "normal".  A snapshot measured on
// 2026-09-13 was 659 rows, of which 443 resolved to special areas -- rows this
// server had been re-appending at every login and every MissionEnd while
// believing it was sending a replacement.
//
// A row whose area does not resolve (a land or a gate, or an area the client
// was never sent) is skipped by all three removers and cannot be withdrawn by
// anyone; those stay on the normal channel, which is where they have always
// been.
enum class PermitChannel { Normal, Special, ML };

PermitChannel channelForArea(const int32_t areaId)
{
    if (areaId <= 0)
        return PermitChannel::Normal;
    for (const auto& area : theServer()->cache().areaMst())
    {
        if (area.area_id != areaId)
            continue;
        if (area.area_type == 10)
            return PermitChannel::ML;
        // isSpecial: (type | 2) == 3, i.e. 1 or 3.
        if ((area.area_type | 2) == 3)
            return PermitChannel::Special;
        return PermitChannel::Normal;
    }
    return PermitChannel::Normal;
}

PermitChannel channelForDungeon(const int32_t dungeonId)
{
    for (const auto& dungeon : theServer()->cache().dungeonMst())
    {
        if (dungeon.dungeon_id == dungeonId)
            return channelForArea(dungeon.area_id);
    }
    return PermitChannel::Normal;
}

PermitChannel channelForMission(const int32_t missionId)
{
    const auto& byDungeon = theServer()->cache().missionsByDungeon();
    for (const auto& [dungeonId, missions] : byDungeon)
    {
        for (const auto id : missions)
        {
            if (id == missionId)
                return channelForDungeon(dungeonId);
        }
    }
    return PermitChannel::Normal;
}

// Route by the row's entity key, which is also what selects its type.
PermitChannel channelForRow(const std::string_view key, const int64_t id)
{
    if (key == "VjCY7rX4") return channelForArea(static_cast<int32_t>(id));
    if (key == "MHx05sXt") return channelForDungeon(static_cast<int32_t>(id));
    if (key == "j28VNcUW") return channelForMission(static_cast<int32_t>(id));
    // Lands (9C64Qwe0) and gates (0Cq2AlXW) resolve to no area, so no remover
    // touches them whichever channel they ride.  Keep them where they were.
    return PermitChannel::Normal;
}
}

drogon::Task<std::string> buildPermitPlace(const db::Database db, const UserIdentity identity)
{
    // Grand Gaia progression uses the low ID space; special modes have their
    // own topology collectors. This preserves the existing authored policy.
    static constexpr int32_t kSpecialIdFloor = 100000;
    std::set<int32_t> clearedMissions;
    for (const auto& done : co_await getClearedMissions(db, identity))
        clearedMissions.insert(done.mission_id);

    // The three partitions, each filling the channel that can clear it.
    struct Buckets
    {
        std::string normal, special, ml;

        void raw(const PermitChannel channel, const std::string& row)
        {
            std::string& out = channel == PermitChannel::Special ? special
                             : channel == PermitChannel::ML      ? ml
                                                                 : normal;
            if (!out.empty()) out += ',';
            out += row;
        }

        void add(const std::string_view key, const int64_t id)
        {
            std::string row = "{\"";
            row += key;
            row += "\":\"";
            row += std::to_string(id);
            row += "\"}";
            raw(channelForRow(key, id), row);
        }
    };

    static const Buckets kPermitBase = []() {
        Buckets s;
        s.normal.reserve(32'000);
        s.special.reserve(32'000);
        auto add = [&](std::string_view key, int64_t id) { s.add(key, id); };

        for (int i = 1; i <= 100; ++i) add("0Cq2AlXW", i); // gates, incl. 99 (Vortex)

        const auto& fg = theServer()->cache().frontierGatePermits();
        for (const auto id : fg.lands)    add("9C64Qwe0", id);
        for (const auto id : fg.areas)    add("VjCY7rX4", id);
        for (const auto id : fg.dungeons) add("MHx05sXt", id);
        for (const auto id : fg.missions) add("j28VNcUW", id);

        // Grand Quest: without its dungeon here the mode's icon never appears
        // on the quest screen (GameUtils::checkGrandPermit).  Grand ids are
        // above kSpecialIdFloor, so the progression gate below skips them.
        const auto& gq = theServer()->cache().grandQuestPermits();
        for (const auto id : gq.lands)    add("9C64Qwe0", id);
        for (const auto id : gq.areas)    add("VjCY7rX4", id);
        for (const auto id : gq.dungeons) add("MHx05sXt", id);
        for (const auto id : gq.missions) add("j28VNcUW", id);

        const auto& vx = theServer()->cache().vortexPermits();
        for (const auto id : vx.lands)    add("9C64Qwe0", id);
        for (const auto id : vx.areas)    add("VjCY7rX4", id);
        for (const auto id : vx.dungeons) add("MHx05sXt", id);
        for (const auto id : vx.missions) add("j28VNcUW", id);

        return s;
    }();

    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const size_t weekdayIndex = static_cast<size_t>((local.tm_wday + 6) % 7);


    const auto& today = theServer()->cache().vortexDayPermits(weekdayIndex);
    Buckets buckets = kPermitBase;
    {
        const auto append = [&buckets](std::string&, bool&, std::string_view key, int64_t id) {
            buckets.add(key, id);
        };
        std::string permitPlace;   // unused shim so the call sites below read unchanged
        bool first = false;

        const auto& cleared = clearedMissions;

        const auto& cache = theServer()->cache();
        const auto& needs = cache.missionNeeds();

        // Authored policy: ANY prerequisite suffices; multi-prerequisite semantics
        // remain unverified against the client. Preserve the existing rule.
        const auto satisfied = [&cleared](const auto& list) {
            if (list.empty())
                return true;
            bool anyReal = false;
            for (const auto need : list)
            {
                if (need == 0)
                    return true;         // 0 = no prerequisite
                anyReal = true;
                if (cleared.count(need))
                    return true;
            }
            return !anyReal;
        };

        size_t gatedAreas = 0, gatedDungeons = 0, gatedMissions = 0;

        std::set<int32_t> permittedLands;

        for (const auto& area : cache.areaMst())
        {
            if (area.area_id <= 0 || area.area_id >= kSpecialIdFloor)
                continue;
            if (!satisfied(area.need_mission_id))
                continue;
            append(permitPlace, first, "VjCY7rX4", area.area_id);
            ++gatedAreas;
            if (area.land_id > 0)
                permittedLands.insert(area.land_id);
        }

        for (const auto landId : permittedLands)
            append(permitPlace, first, "9C64Qwe0", landId);

        for (const auto& dungeon : cache.dungeonMst())
        {
            if (dungeon.dungeon_id <= 0 || dungeon.dungeon_id >= kSpecialIdFloor)
                continue;
            if (!satisfied(dungeon.need_mission_id))
                continue;
            append(permitPlace, first, "MHx05sXt", dungeon.dungeon_id);
            ++gatedDungeons;

            const auto it = cache.missionsByDungeon().find(dungeon.dungeon_id);
            if (it == cache.missionsByDungeon().end())
                continue;

            // The first mission stays available once its parent dungeon opens.
            const auto entryMission = it->second.empty() ? 0 : it->second.front();

            for (const auto missionId : it->second)
            {
                if (missionId <= 0 || missionId >= kSpecialIdFloor)
                    continue;
                if (missionId != entryMission)
                {
                    const auto need = needs.find(missionId);
                    if (need != needs.end() && !satisfied(need->second))
                        continue;
                }
                append(permitPlace, first, "j28VNcUW", missionId);
                ++gatedMissions;
            }
        }

        std::string landList;
        for (const auto landId : permittedLands)
        {
            if (!landList.empty())
                landList += ',';
            landList += std::to_string(landId);
        }

        LOG_INFO << "PermitPlace: progression gate — " << cleared.size()
                 << " mission(s) cleared, permitting " << gatedAreas << " area(s), "
                 << gatedDungeons << " dungeon(s), " << gatedMissions << " mission(s)"
                 << ", land(s) [" << landList << "]";

        for (const auto id : today.dungeons) append(permitPlace, first, "MHx05sXt", id);
        for (const auto id : today.missions) append(permitPlace, first, "j28VNcUW", id);

        int32_t gatedVortex = 0;
        const auto permitGated = [&](const ServerCache::TopologyPermits& permits) {
            for (const auto& [missionId, need] : permits.gatedMissions)
            {
                if (!satisfied(need))
                    continue;
                append(permitPlace, first, "j28VNcUW", missionId);
                ++gatedVortex;
            }
        };
        permitGated(theServer()->cache().vortexPermits());
        permitGated(today);

        const auto nowEpoch = static_cast<int64_t>(now);

        // Rebuild every parade from persisted state, including expired windows.
        // Never merge old tier missions into this replacement snapshot.
        int32_t keyTierMissions = 0;
        for (const auto& key : theServer()->cache().dungeonKeyMst())
        {
            const auto st = co_await db->execSqlCoro(
                "SELECT active_tier, active_until FROM user_dungeon_keys"
                " WHERE user_id = $1 AND dungeon_key_id = $2;",
                identity.userId, static_cast<int32_t>(key.id));
            if (st.size() == 0)
                continue;

            const auto tier = st[0]["active_tier"].as<int32_t>();
            const auto until = st[0]["active_until"].as<int64_t>();
            const bool live = tier > 0 && nowEpoch < until;

            // A parade is ENTERABLE whether or not a key window is open; the
            // countdown is what says "open".  AreaSPSelectScene::touchEnded
            // checks isEnterable (+0x5b4) BEFORE its key branch and sends a
            // locked dungeon to MissionFlgConfirmScene — "Opening Conditions",
            // which lists only need_mission_id quests, so it drew a blank page
            // for a parade.  The key branch (+0xa1c) reads getRemainingTime
            // (+0xa68): 1 or more enters the parade; below 1 (-1 when there is
            // no countdown) opens the key prompt at +0xce8 — possession, the
            // usage-pattern tiers, or USE_KEY_CONFIRMATION_DONOTHAVE2.  So
            // "enterable, no time left" IS the closed state.  Sending "0" here
            // hid the key prompt behind that blank page.
            // A parade row carries its enterable flag and countdown alongside the
            // dungeon id, so it is built by hand rather than through add() --
            // but it still has to ride the channel that can clear it, or an
            // expired tier could never be withdrawn.  Parade areas are special
            // in this data, which is exactly the case that motivated the split.
            std::string paradeRow =
                R"({"MHx05sXt":")" + std::to_string(key.dungeon_id) + R"(","C1vG0iKh":"1")";
            if (live)
                paradeRow += R"(,"qY49LBjw":")" + std::to_string(until - nowEpoch) + R"(")";
            paradeRow += '}';
            buckets.raw(channelForDungeon(key.dungeon_id), paradeRow);

            if (!live)
                continue;   // never bought, or the window has lapsed

            for (const auto& rung : gme::parseUsagePatternTiers(key.usage_pattern))
            {
                if (rung.keysRequired != tier)
                    continue;
                for (const auto missionId : rung.missionIds)
                {
                    append(permitPlace, first, "j28VNcUW", missionId);
                    ++keyTierMissions;
                }

            }
        }
        if (keyTierMissions)
            LOG_INFO << "PermitPlace: parade tier — permitting " << keyTierMissions
                     << " mission(s) from active dungeon keys";

        LOG_INFO << "PermitPlace: Vortex tier gate — permitting " << gatedVortex
                 << " gated mission(s)";

        LOG_INFO << "PermitPlace: channels — " << std::count(buckets.normal.begin(), buckets.normal.end(), '{')
                 << " normal row(s), " << std::count(buckets.special.begin(), buckets.special.end(), '{')
                 << " special, " << std::count(buckets.ml.begin(), buckets.ml.end(), '{') << " ML";
    }

    // All three keys, always — an empty array never reaches readParam, so a
    // partition with nothing in it simply is not cleared, which is correct.
    std::string out;
    out.reserve(buckets.normal.size() + buckets.special.size() + buckets.ml.size() + 64);
    out += R"("yXNM8kL3":[)";  out += buckets.normal;  out += ']';
    out += R"(,"Y73tHKS8":[)"; out += buckets.special; out += ']';
    out += R"(,"Y73mHKS8":[)"; out += buckets.ml;      out += ']';
    co_return out;
}

void injectPermitPlace(std::string& body, const std::string_view permit)
{
    // THREE keys, not one.  PermitPlaceResponse::readParam @0x13E8E28 clears
    // removeNormalObjects, PermitPlaceSpResponse removeSpecialObjects and
    // PermitPlaceMLResponse removeMLObjects -- three disjoint partitions of one
    // client list, so a snapshot that only sends the first can only ever
    // replace the third of the rows that happens to be normal.  The ctor
    // @0x126B3BC defaults enterable to true.  Keep exactly one entity ID per
    // row and explicit closed parades.
    //
    // The two extra keys are spliced by REMOVING the response's own empty
    // placeholders first, so a body that never had them (older response types)
    // still works and no key is ever emitted twice.
    static constexpr std::string_view normalEmpty  = R"("yXNM8kL3":[])";
    static constexpr std::string_view specialEmpty = R"(,"Y73tHKS8":[])";
    static constexpr std::string_view mlEmpty      = R"(,"Y73mHKS8":[])";

    if (!permit.starts_with(R"("yXNM8kL3":[)") || !permit.ends_with(']'))
        throw std::runtime_error("Invalid PermitPlace snapshot");

    // Drop the serialiser's empty SP/ML fields wherever they appear; the
    // snapshot carries its own copies of both.
    for (const auto placeholder : { specialEmpty, mlEmpty })
    {
        const auto at = body.find(placeholder);
        if (at != std::string::npos)
            body.erase(at, placeholder.size());
    }

    const auto pos = body.find(normalEmpty);
    if (pos == std::string::npos
        || body.find(normalEmpty, pos + normalEmpty.size()) != std::string::npos)
        throw std::runtime_error("Invalid PermitPlace response placeholder");
    body.replace(pos, normalEmpty.size(), permit);
}
}
