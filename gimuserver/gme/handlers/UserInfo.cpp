#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/GachaArchiver.hpp>
#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/Common.hpp>

#include <algorithm>
#include <ctime>
#include <set>

// The Vortex's gate id in GateMst (MST_DUNGEONS_GATE_99_NAME, type 1).  Held
// out of PermitPlace's dense gate range so entry can be gated on progress.
static constexpr int kVortexGateId = 99;

// Boundary between Grand Gaia's numbering and everything else.  Grand Gaia's
// areas, dungeons and missions all sit below this; Vortex (100000+), Frontier
// Hunter (1000000+), Frontier Gate (3000000+), Trial and Grand Quest sit above
// it and are permitted by their own blocks.  The progression gate only applies
// below the floor, so a special subsystem is never accidentally swept into it.
static constexpr int32_t kSpecialIdFloor = 100000;

// The Vortex lock's requirement, expressed the only way the client can hold it.
//
// FeatureGatingHandler::shouldGateLocked is hard-wired to a level comparison —
//     if (LevelGatingInfo::getFeatureID(v) == featureId)
//         return UserTeamInfo::getLv(...) < LevelGatingInfo::getRequiredLevel(v);
// — so there is no way to express "mission not cleared".  We carry the mission
// condition as a level nobody reaches.  Neither drawing path renders the
// number, so it is never shown to the player; it is a sentinel, not a setting.
static constexpr uint32_t kVortexLockRequiredLevel = 9999;

// Requirement type.  MUST be 1: FeatureGatingHandler::addObj bails with
//     if (FeatureGateMst::getReqID(a2) != 1) return CCObject::release(a2);
// before it can build the LevelGatingInfo, so any other value silently
// discards the gate.  Not a choice — the only value that works.
static constexpr uint32_t kVortexLockReqId = 1;

HANDLEF(UserInfo)
{
	UserInfoReq req = {};
	const auto& ec = glz::read_json(req, json);
	if (ec)
	{
		const auto& fmte = glz::format_error(ec, json);
		LOG_DEBUG << "Gme UserInfo Error during JSON read: " << fmte;
		co_return HandleResult::error("Deserialization error", fmte);
	}

	// Copy the cached response and build on top of it.
	UserInfoResp resp = theServer()->cache().userInfoResp();

    const auto db = theDb();
	const auto identity = (co_await gme::getUserIdentity(db, req.login_info, true)).nonEmpty();

	// We must have a valid user entry in the database at this point.
	resp.login_info = std::move((co_await gme::getLoginInfo(db, identity)).nonEmpty());
	resp.team_info = std::move((co_await gme::getTeamInfo(db, identity)).nonEmpty());

	// UserInfo is the session-level refresh point for owned units. Clear any
	// persisted new flags before returning the full collection.
	//
	// The client keeps its own in-memory new-unit list and clears it during normal
	// gameplay, so the server only needs to reset the stored flags when the client
	// asks for a fresh UserInfo snapshot.
	co_await db::DatabaseInterface::update(
		db,
		"user_units",
		{
			db::Data("new", false),
			db::Lookup("user_id", identity.userId),
		});

    // We should always have at least one unit, since the game will not let users
    // delete their only unit on the squad.
	resp.unit_info = std::move((co_await db::PacketInterfaceFor<UserUnitInfo>::read(
		db,
		"user_units",
		{ db::Lookup("user_id", identity.userId) })).nonEmpty());

	auto partyDeckInfo = (co_await db::PacketInterfaceFor<UserPartyDeckInfo>::read(
		db,
		"user_decks",
		{ db::Lookup("user_id", identity.userId) })).nonEmpty();
	resp.party_deck_info = std::move(partyDeckInfo);

	resp.unit_dictionary = std::move((co_await db::PacketInterfaceFor<UserUnitDictionary>::read(
		db,
		"user_unit_dictionary",
		{ db::Lookup("user_id", identity.userId) })).data);

	// Owned items now come from the user_items table (seeded by the tutorial,
	// grown by mission drops) instead of the old hardcoded potion.  The full
	// inventory populates warehouse_info; battle-consumable items (ItemMst
	// item_type == 1, e.g. the tutorial healing potion) also populate
	// equip_info so they appear in the mission item bar.  Stacks at 0 keep
	// their row (ItemSell / ItemSphereEqp decrement without deleting so
	// instance ids stay stable) but are filtered off the wire; every species
	// ever stacked still feeds the item dictionary, and favorited stacks are
	// reported through item_favorite (VSRPkdId) so locks survive a reload.
	auto warehouse = (co_await db::PacketInterfaceFor<UserWarehouseInfo>::read(
		db,
		"user_items",
		{ db::Lookup("user_id", identity.userId) })).data;

	std::vector<UserWarehouseInfo> visibleStacks;
	visibleStacks.reserve(warehouse.size());
	for (auto& stack : warehouse)
	{
		resp.item_dictionary_info.push_back(UserItemDictionaryInfo{
			.item_id = stack.item_id,
		});

		if (stack.item_num == 0)
			continue;

		if (stack.favorite_flg != 0)
		{
			// "::" — the packet struct, not GmeHandlers::ItemFavorite (the
			// handler declared in Handlers.hpp shadows it in this scope).
			resp.item_favorite.push_back(::ItemFavorite{
				.instance_id = stack.instance_id,
				.favorite = 1,
			});
		}

		visibleStacks.push_back(std::move(stack));
	}
	resp.warehouse_info = std::move(visibleStacks);

	// Battle-item loadout (71U5wzhI) — the 5 slots on the quest-prep screen.
	//
	// This is REPORTED, never derived.  It used to be synthesised here by
	// walking the warehouse and emitting every item whose ItemMst.item_type
	// was 1, numbering the slots by inventory order — so the prep screen came
	// back stuffed with whatever consumables the player happened to own and the
	// loadout they actually picked was nowhere.  ItemEdit (ruoB7bD8) now
	// persists the player's choice and this reads it back verbatim, slot
	// numbers included (createBody uses a +100 band for its second run, so the
	// stored disp_order is echoed rather than renumbered).
	{
		const auto rows = co_await db->execSqlCoro(
			"SELECT disp_order, item_id, item_num FROM user_equip_items"
			" WHERE user_id=$1 ORDER BY disp_order;",
			identity.userId);
		resp.equip_info.reserve(rows.size());
		for (const auto& row : rows)
		{
			resp.equip_info.push_back(UserEquipItemInfo{
				.item_id    = row["item_id"].as<uint32_t>(),
				.disp_order = row["disp_order"].as<uint32_t>(),
				.item_num   = row["item_num"].as<uint32_t>(),
			});
		}
	}

	// Cleared-mission history (UT1SVg59) — THE progression driver.  The client
	// evaluates feature unlocks against this list: F_FUNCTION_RELEASE_MST rows
	// (condition type 2 = "mission <param> cleared") gate functions 8-19, and
	// the hardcoded early-feature gates (town etc.) key off the same set plus
	// tutorial_status.  Backed by user_campaign_missions state=2 rows, written
	// by MissionEnd and CampaignBattleEnd.
	//
	// Shared with UpdateInfoLight via gme::getClearedMissions — the poll has to
	// report the identical set or a mid-session refresh would contradict the
	// login snapshot.
	resp.clear_mission_info = co_await gme::getClearedMissions(db, identity);

	// Favorited/locked units (3kcmQy7B) — UnitFavorite persists the flag;
	// reporting it back makes locks survive a reload.
	{
		const auto rows = co_await db->execSqlCoro(
			"SELECT user_unit_id FROM user_units"
			" WHERE user_id = $1 AND favorite_flg = 1;",
			identity.userId);
		for (const auto& row : rows)
		{
			resp.favorite.push_back(::UserFavorite{
				.user_unit_id = row["user_unit_id"].as<int32_t>(),
				.favorite = 1,
				.unit_img_type = 0,
			});
		}
	}

	// The cleared-mission set, derived once.  It gates the town below and the
	// Grand Gaia half of PermitPlace further down, and the two must agree —
	// they are the same progression signal read twice.
	std::set<int32_t> clearedMissions;
	for (const auto& done : resp.clear_mission_info)
		clearedMissions.insert(done.mission_id);

	// Town state — the three arrays travel together (§6.8): every location in
	// town_location_info needs a matching town_location_detail entry or the
	// town scene loader null-derefs.  Rows are provisioned by provisionTown.
	//
	// The detail array is the resource tiles' live harvest state, and this is
	// the only place the client ever learns it: a fresh period is rolled here
	// when the last one has aged out, because there is no other request that
	// runs on entering town.  Field semantics in tools/TOWN_STATE_MODEL.md.
	resp.town_facility_info = co_await gme::Town::facilityState(db, identity);
	co_await gme::Town::locationState(
		db, identity, clearedMissions, resp.town_location_info, resp.town_location_detail);

	// Synthesis menu (51yQrDBR).  MyTownItemListScene::setRecipeList and its
	// sphere counterpart iterate PermitRecipeInfoList and nothing else, so an
	// absent list is exactly why both Synthesis facilities have been rendering
	// empty.  Derived from the player's facility levels.
	resp.permit_receipes = co_await gme::Town::permittedRecipes(db, identity, clearedMissions);

    // Summon catalog (1IR86sAv doors + IBs49NiH banner rail).  Identical to
    // what GachaList returns, and deliberately duplicated here.
    //
    // The tutorial never performs a GachaList round trip: tuto15.txt drives
    // `change_gacha_top_scene` straight from the script, so
    // SummonsCategorizationScene opens with whatever the client already holds.
    // With these sent only on GachaList, GachaCategoryMstList and GachaInfoList
    // were both empty at that moment and tapping Summon crashed the client with
    // no request reaching us at all — the last thing logged was a successful
    // BadgeInfo, and dlc_404.log was clean.
    //
    // Sending them from UserInfo works because the key -> response-class
    // registry is global: GameResponseParser::getResponseObject (0x1392568)
    // maps `1IR86sAv` to GachaInfoResponse and `IBs49NiH` to
    // GachaCategoryMstResponse regardless of which handler's payload carries
    // them.  Initialize already pushes the raw 1305-row gacha MST at 5Y4GJeo3
    // the same way.
    resp.gacha_info = GachaArchiver::instance().populateAllPackets();
    resp.gacha_categories = theServer()->cache().gachaListRsp().gacha_categories;

    resp.campaign_info.current_day = 1;
    resp.campaign_info.total_days = 96;
    resp.campaign_info.first_for_the_day = false;
    resp.campaign_info.id = 1;

    resp.summoner_journal.user_id = identity.userId;
    resp.signal_key.key = "5EdKHavF";

    // Vortex dungeon keys (eFU7Qtb0).  UserInfo is where the client first
    // learns how many Metal / Jewel keys it holds and whether today's is
    // claimable — the Administration Office badge is drawn from this before
    // GetDistributeDungeonKeyInfo is ever sent.
    //
    // Also seeds the per-user rows on first read, so accounts that predate the
    // dungeon-key table pick them up on their next login.
    resp.dungeon_key_info = co_await gme::dungeonKeyState(db, identity);

    // Vortex progression.  Computed here, BEFORE serialisation, because the
    // The Vortex is NOT gated.  It is always permitted, and the server sends no
    // FeatureGatingInfo for it.
    //
    // This reverses an earlier progression change of ours.  We had withheld the
    // Vortex until a Mistral story mission was cleared and tried to draw
    // vortex_quest_locked.png over the tiles.  Two independent things say that
    // was wrong: the shipped GateMst gives gate 99 need_mission_id 0 (where
    // Ishgria's gate 2 has 1577), and footage of the live game near shutdown
    // shows the Vortex neither locked nor gated.  The lock art exists in the
    // binary and in level_lock/, but the live game did not use it here.
    //
    // Feature gating itself is a real, game-wide system — see handbook §8.40;
    // it drives Randall town, Daily Task, the Arena/Home "NEW" badges and the
    // level-up unlock popups.  Only the Vortex rows are gone.

    std::string buffer{};
    const auto& ec2 = glz::write_json(resp, buffer);
    if (ec2)
    {
        const auto& glze = glz::format_error(ec2, buffer);
        LOG_DEBUG << "Gme UserInfo Error during JSON writing: " << glze;
        co_return HandleResult::error("Serialization error", glze);
    }

    // Inject PermitPlace unlock data.  The generated PermitPlace struct is a
    // stub ("INVALID" key), so we replace the serialised empty array in-place.
    // Each entry unlocks one entity type — the client reads exactly one key per
    // entry to determine which area/land/gate/mission/dungeon is accessible.
    //   VjCY7rX4 = area, 9C64Qwe0 = land, 0Cq2AlXW = gate,
    //   j28VNcUW = mission, MHx05sXt = dungeon
    //
    // Category roles (empirically established — see handbook §6.9):
    //   - Areas are the MISSION-PARENT TOPOLOGY LINK.  Narrow → no missions
    //     resolve under any land → world map renders empty → click crashes.
    //     Keep at the full 1-1000 range.
    //   - Lands are the CUTSCENE GATE.  Each visible land plays its
    //     `mapN-open.txt` intro cutscene the first time the player enters
    //     Grand Gaia.  Keeping lands at 1-2 caps the cascade at one
    //     cutscene (Mistral intro).
    //   - Gates / missions / dungeons are availability flags only.  Safe
    //     to leave full.
    //
    // TODO: when Cordelica's click-crash is solved, widen lands incrementally
    // (1-3, 1-5, etc.) to expose more chapters.  Each additional land adds
    // one intro cutscene on first session entry.
    //
    // Static-category ranges (from version_info_mst.json):
    //   F_AREA_MST    669 entries, max id ~ 411  → 1-1000  (topology)
    //   F_LAND_MST    147 entries, ~26 unique ids → 1-2    (cutscene gate)
    //   F_GATE_MST     95 entries, ~5  unique ids → 1-100
    //   F_MISSION_MST 1118 entries, 3433 count   → 1-4000
    //   F_DUNGEON_MST 1002 entries, 1532 count   → 1-2000
    // The client silently ignores entries for IDs that don't exist in its local MST.
    //
    // FRONTIER GATE (added 2026-08-07): the dense ranges above cover the Grand
    // Gaia numbering space and NOTHING ELSE.  Frontier Gate's backing ids live
    // in a completely different space — its dungeons are 3000001..800000015 and
    // its prerequisite missions 80000001..100016020 — so **0 of 94 gates** had
    // a permitted dungeon or mission.  The client owns the rows (dungeon_mst
    // contains 3000001, mission_mst contains 80000001); nothing permitted them.
    // That is the same failure the area note above describes, one layer down.
    //
    // These cannot be covered by widening the ranges: enumerating to 800000015
    // is not a payload anyone wants.  Instead permit exactly the ids the gate
    // catalog references — 94 gates contribute ~150 entries, against the ~7100
    // the dense ranges already emit.
    static constexpr std::string_view kEmptyPermit = R"("yXNM8kL3":[])";

    // Appends one {"<key>":"<id>"} permit entry.  `first` guards the comma; the
    // static base below always emits at least one entry, so the per-request
    // suffix can assume it is never the first.
    const auto append = [](std::string& s, bool& first, std::string_view key, int64_t id) {
        if (!first) s += ',';
        s += "{\"";
        s += key;
        s += "\":\"";
        s += std::to_string(id);
        s += "\"}";
        first = false;
    };

    // Everything that never changes between requests, built once and reused.
    // Deliberately left UNCLOSED — the Vortex weekday rotation is appended per
    // request below and the ']' goes on after it.
    // PROGRESSION GATE (added 2026-08-09).  The dense ranges described above
    // permitted the WHOLE Grand Gaia numbering space unconditionally, which is
    // why every Mistral area and every mission inside it showed up as "NEW" on
    // a save with nothing cleared.  The real game reveals one dungeon at a
    // time.
    //
    // The rule is in the data: MissionMst, DungeonMst and AreaMst all carry
    // need_mission_id (HSRhkf70), and Mistral is a strict chain through it —
    // mission 1 need 0, 2 need 1, 10 need 2, ... 85 need 84; dungeon 20 need
    // 12, 30 need 23; area "Morgan" need 85.  So the gated topology is emitted
    // per request against the player's cleared set instead of as a static
    // range.  Restricted to ids below kSpecialIdFloor: everything above it is
    // Vortex / Frontier Gate / Trial, which have their own blocks below and
    // must not be swept in here.
    //
    // Only the parts that cannot vary by progress stay static.
    static const std::string kPermitBase = [&append]() {
        std::string s;
        s.reserve(32'000);
        s += R"("yXNM8kL3":[)";
        bool first = true;
        auto add = [&](std::string_view key, int64_t id) { append(s, first, key, id); };

        // NOTE: Grand Gaia lands are NOT emitted here.  They are derived per
        // request from the areas the progression gate actually permits — see
        // the land block below.  Only the special-space lands (Frontier Gate,
        // Vortex) are static, and they are added by their own collectors.

        for (int i = 1; i <= 100; ++i) add("0Cq2AlXW", i); // gates, incl. 99 (Vortex)

        // Frontier Gate's own topology.  Collected at boot in ServerCache — see
        // frontierGatePermits() for why the gates' dungeons alone were not
        // enough: a gate's MISSION also has a land and an area, and gate 91's
        // mission 9010001 is land 99 / area 3000001, both far outside the dense
        // ranges above.  Without those the mission downloaded its assets and
        // then crashed with its parent topology unreachable.
        const auto& fg = theServer()->cache().frontierGatePermits();
        for (const auto id : fg.lands)    add("9C64Qwe0", id);
        for (const auto id : fg.areas)    add("VjCY7rX4", id);
        for (const auto id : fg.dungeons) add("MHx05sXt", id);
        for (const auto id : fg.missions) add("j28VNcUW", id);

        // VORTEX (added 2026-08-08): the same story a third time.  Vortex is
        // gate 99 / land 99 with area ids 100000-101800, dungeons 100000-102920
        // and missions 100000-102923 — so 88 of 88 areas, 104 of 104 dungeons
        // and 292 of 292 missions sat outside the dense ranges above.  Nothing
        // was permitted, so the Vortex rendered with no tiles at all.
        //
        // Collected at boot in ServerCache (see vortexPermits()) and scoped to
        // the Vortex block rather than all of land 99, which would also open
        // Frontier Hunter, Trial and Grand Quest.  ~494 entries.
        //
        // Only the always-open content lives here.  The weekday rotation is
        // appended per request below, since this base is built once.
        const auto& vx = theServer()->cache().vortexPermits();
        for (const auto id : vx.lands)    add("9C64Qwe0", id);
        for (const auto id : vx.areas)    add("VjCY7rX4", id);
        for (const auto id : vx.dungeons) add("MHx05sXt", id);
        for (const auto id : vx.missions) add("j28VNcUW", id);

        return s;
    }();

    // Today's Vortex rotation.  Appended per request rather than baked into the
    // static base above: a server left running across midnight would otherwise
    // keep yesterday's dungeon open until it was restarted.
    //
    // Local time, not UTC — the rotation is what the player sees as "today",
    // and the rest of the server already reads wall-clock local time (see
    // GimuServer::tryOpenHttpDumpLog).  tm_wday is 0 = Sunday, so it is
    // remapped to the 0 = Monday indexing vortexDayPermits() uses.
    // std::localtime shares a static buffer, matching existing usage; a torn
    // read across midnight would at worst serve the wrong day for one request.
    const auto now = std::time(nullptr);
    const auto local = *std::localtime(&now);
    const size_t weekdayIndex = static_cast<size_t>((local.tm_wday + 6) % 7);

    static constexpr std::string_view kDayNames[] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };

    const auto& today = theServer()->cache().vortexDayPermits(weekdayIndex);
    std::string permitPlace = kPermitBase;
    {
        bool first = false; // the base always emitted entries

        // --- Grand Gaia, gated on progress -------------------------------
        const auto& cleared = clearedMissions;

        const auto& cache = theServer()->cache();
        const auto& needs = cache.missionNeeds();

        // ANY prerequisite satisfied is enough, not ALL.  99% of rows carry a
        // single id so the distinction is moot there; the 36 comma-pair rows
        // are the open question, and this is the direction that fails SAFE —
        // guessing ALL on something that meant ANY would make content
        // permanently unreachable, which on a preservation server is the worse
        // error.  // UNVERIFIED: no capture distinguishes the two.
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

        // Lands are derived from the areas we permit, never hardcoded.
        //
        // They used to be a static range 1-2, which was wrong in BOTH
        // directions.  Land 2 holds exactly one area — Cordelica (20000),
        // which needs mission 234 — so on an early save the client rendered
        // an empty Cordelica shell on the world map with nothing enterable
        // inside it.  Meanwhile land 100 owns area 20100 (need 0, always
        // open), so the gate permitted that area/dungeon/mission while its
        // parent land was never permitted — the same orphaned-topology bug
        // the Frontier Gate block above exists to fix, one layer up.
        //
        // Deriving keeps the two in lockstep by construction: a land appears
        // exactly when it has something to enter, and widens on its own as
        // areas unlock.  This supersedes the old "widen lands incrementally
        // once Cordelica's click-crash is solved" TODO — an empty land shell
        // was very likely that crash.
        //
        // Lands are also the cutscene gate (§6.9): each visible land plays
        // its mapN-open.txt intro on first entry.  Deriving means a fresh
        // save sees exactly Mistral's intro, because Mistral is the only
        // Grand Gaia land with a satisfied area.
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

            // Missions live under their dungeon.  A mission with no entry in
            // missionNeeds() has no prerequisite and is always available.
            const auto it = cache.missionsByDungeon().find(dungeon.dungeon_id);
            if (it == cache.missionsByDungeon().end())
                continue;

            // THE ENTRY MISSION IS ALWAYS PERMITTED.  Access is controlled at
            // the DUNGEON level; the chain inside it only controls order.
            //
            // Without this a dungeon can be visible with nothing enterable:
            // Mistral's first playable dungeon holds missions 10/11/12, and
            // mission 10 needs mission 2 — which lives in the tutorial dungeon.
            // A save whose clear history was wiped (debug `resetmap`, or any
            // account that never recorded the tutorial) therefore saw the
            // dungeon rendered with an empty quest list, which the client
            // labels "CLEAR".  Same for dungeon 80: it only becomes visible
            // once mission 73 is cleared, and its own first mission 81 needs
            // exactly that, so permitting it adds nothing that the dungeon
            // gate did not already allow.
            //
            // m_missionsByDungeon is sorted ascending at boot, and mission ids
            // ascend with disp_order within a dungeon, so front() is the entry.
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

        LOG_INFO << "UserInfo: progression gate — " << cleared.size()
                 << " mission(s) cleared, permitting " << gatedAreas << " area(s), "
                 << gatedDungeons << " dungeon(s), " << gatedMissions << " mission(s)"
                 << ", land(s) [" << landList << "]";

        // Vortex.  Gate 99 and the always-open topology are in kPermitBase; only
        // the weekday rotation has to be rebuilt per request, because a server
        // running across midnight would otherwise keep serving yesterday's list.
        for (const auto id : today.dungeons) append(permitPlace, first, "MHx05sXt", id);
        for (const auto id : today.missions) append(permitPlace, first, "j28VNcUW", id);

        permitPlace += ']';
    }

    const auto pos = buffer.find(kEmptyPermit);
    if (pos != std::string::npos)
    {
        buffer.replace(pos, kEmptyPermit.size(), permitPlace);
        LOG_INFO << "UserInfo: PermitPlace injected — Grand Gaia areas/lands/"
                    "dungeons/missions gated on progress (see the line above), "
                    "gates 1-100, plus Frontier Gate; Vortex open, rotation "
                 << kDayNames[weekdayIndex]
                 << " = " << today.dungeons.size() << " dungeon(s)"
                 << " (" << permitPlace.size() << " bytes)";
    }
    else
        LOG_WARN << "UserInfo: yXNM8kL3 token not found in serialised buffer — PermitPlace not injected";

    // Emit per-user Unit Selector Gacha info (CGHaOZda) so the "use ticket"
    // button appears on selector banners. Hypothesis (from the CGHaOZda readParam
    // audit + handbook §7.11.5-7, which investigated but never solved the hidden
    // button): the client gates the button on a non-empty UnitSelectorGachaUserInfo
    // array, and our server never emitted it — this is the untried fix. Each entry
    // is {XIvaD6Jp selector_id, H6k1LIxC remaining count}.
    //
    // EXPERIMENT: emits every selector from the catalog with count 1 to test
    // whether emitting CGHaOZda at all restores the button. Once confirmed, narrow
    // to the selectors the user actually holds V2 tickets for, with real counts.
    {
        const auto& selectors = theServer()->cache().unitSelectorGacha();
        std::string sel = R"("CGHaOZda":[)";
        bool first = true;
        for (const auto& s : selectors)
        {
            if (!first) sel += ',';
            sel += R"({"XIvaD6Jp":")" + std::to_string(s.selector_id)
                 + R"(","H6k1LIxC":"1"})";
            first = false;
        }
        sel += ']';

        // Insert as a top-level field before the closing brace of the root object.
        const auto pos = buffer.rfind('}');
        if (pos != std::string::npos && !selectors.empty())
        {
            buffer.insert(pos, "," + sel);
            LOG_INFO << "UserInfo: emitted CGHaOZda selector info for "
                     << selectors.size() << " selectors (button-visibility experiment)";
        }
    }

    co_return HandleResult::success(buffer);
}
