#include "App.hpp"
#include "Handlers.hpp"

#include <gimuserver/archive/GachaArchiver.hpp>
#include <gimuserver/db/PacketInterface.hpp>
#include <gimuserver/gme/common/BraveSlots.hpp>
#include <gimuserver/gme/common/Common.hpp>
#include <gimuserver/gme/common/Dbb.hpp>
#include <gimuserver/gme/common/EventTokens.hpp>
#include <gimuserver/gme/common/FriendPoints.hpp>
#include <gimuserver/gme/common/MissionBreak.hpp>
#include <gimuserver/gme/common/PermitPlace.hpp>
#include <gimuserver/gme/common/SelectorBanners.hpp>
#include <gimuserver/gme/common/SummonTickets.hpp>

#include <algorithm>
#include <ctime>
#include <set>

// The Vortex's gate id in GateMst (MST_DUNGEONS_GATE_99_NAME, type 1).  Held
// out of PermitPlace's dense gate range so entry can be gated on progress.
static constexpr int kVortexGateId = 99;

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

	// The header's tap-to-refill Energy says "max" when energy >= DefineMst
	// eRQvzLeF, which our data never set (ctor 0: it said "max" every time).
	// Its reader keeps every other define, so send just the threshold: this
	// player's max energy.  HomeInfo and MissionEnd re-send it (net/handlers.kdl).
	resp.energy_recover.action_point_threshold = resp.team_info.max_action_point;

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

	// Through the shared builder, not a raw read: the dictionary row carries
	// the alternate-art unlock (2pAyFjmZ), which is derived rather than stored
	// on the row, and a direct read reported it as 0 on every login.
	resp.unit_dictionary = co_await gme::loadUnitDictionary(db, identity);

	// Owned items come from user_items (seeded by the tutorial, grown by drops
	// and rewards).  gme::loadWarehouseSnapshot is the one builder every reply
	// that carries the warehouse uses — see it for the zero-stack and
	// dictionary rules.  Favorited stacks are reported through item_favorite
	// (VSRPkdId) so locks survive a reload.  The dictionary and favorites are
	// appended, as they always were, to whatever the cached response holds.
	{
		auto warehouse = co_await gme::loadWarehouseSnapshot(db, identity);
		resp.item_dictionary_info.insert(resp.item_dictionary_info.end(),
			warehouse.dictionary.begin(), warehouse.dictionary.end());
		resp.item_favorite.insert(resp.item_favorite.end(),
			warehouse.favorites.begin(), warehouse.favorites.end());
		resp.warehouse_info = std::move(warehouse.warehouse);
	}

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

	// The BONUS item bar (nAligJSQ) — the second row of slots on the same prep
	// screen, reported the same way and for the same reason.  The client carries
	// these into the fight too (MissionStartRequest::createBody @0x13AB3C4 reads
	// the same list) and subtracts them from the warehouse listing, so leaving
	// them unreported did not just blank the slots: it also let the warehouse
	// offer an item that was already in one.
	{
		const auto rows = co_await db->execSqlCoro(
			"SELECT disp_order, item_id, item_num FROM user_equip_bonus_items"
			" WHERE user_id=$1 ORDER BY disp_order;",
			identity.userId);
		resp.equip_boost_item_info.reserve(rows.size());
		for (const auto& row : rows)
		{
			resp.equip_boost_item_info.push_back(UserEquipBoostItemInfo{
				.item_id    = row["item_id"].as<int32_t>(),
				.disp_order = row["disp_order"].as<int32_t>(),
				.item_num   = row["item_num"].as<int32_t>(),
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

	// Lifetime battle statistics behind the Trophy / Arena Archive / Colosseum
	// Archive screens.  Only the eight counters MissionEnd can feed are real;
	// the rest of UserTeamArchive stays 0 until something instruments them.
	// Count today's login BEFORE reading the archive, so the Records screen
	// shows the streak including the session the player is opening right now.
	// Idempotent per calendar day, so the repeated UserInfo calls a session
	// makes count once.
	co_await gme::touchLoginStreak(db, identity);
	resp.archive = co_await gme::loadTeamArchive(db, identity);

	// Must be a real row, not [].  PQ56vbkI is a SINGLETON readParam, so an
	// empty array never runs it and the client draws uninitialised memory --
	// which is exactly what the Battle Record's Arena section was showing.
	resp.arena_archive = gme::zeroedArenaArchive(identity);
	resp.arena_info = gme::zeroedArenaInfo(identity);

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
    {
        // ...including the selector tiles, which are per player — see
        // gme/common/SelectorBanners.hpp.  IBs49NiH is a full replace, so a
        // login that left them out would clear the rail the last GachaList
        // built.
        auto selectors = co_await gme::selectorGachaCategories(db, identity);
        resp.gacha_categories.insert(resp.gacha_categories.end(),
            std::make_move_iterator(selectors.begin()),
            std::make_move_iterator(selectors.end()));
    }

    // The world-area table, which is what draws the Vortex tile list.  Curated
    // at boot (ServerCache::clientAreaMst) to drop the Vortex areas that are
    // empty shells, untranslated, or a second copy of a tile the player already
    // has, and to float Parade Garden to the top of the list.
    //
    // ⚠ Sending this key REPLACES the client's whole AreaMstList — row 0 of
    // AreaMstResponse::readParam calls AreaMstList::removeAllObjects() first.
    // That is what makes the curation take effect, and it is also why the whole
    // table goes out rather than the Vortex slice: anything left out stops
    // existing for the client.  Our deploy/mst/area_mst.json is byte-identical
    // to the F_AREA_MST_Ver669 the client already downloaded from our own CDN
    // (deploy/game_content/mst/Ver669_vj6fEsg7.dat), so every row it does not
    // curate is the row the client already had.
    resp.area_mst = theServer()->cache().clientAreaMst();

    // Dual Brave Burst.  The catalogue is what makes the feature exist at all
    // on the client — DbbMstList is empty without it, and an empty list means
    // no unit has a DBB, no Bond button and no shared skill in battle.  Both
    // tables have been in the cache since the July MST port with nothing
    // reading them; see gme/common/Dbb.hpp.
    resp.dbb_mst = theServer()->cache().dbbMst();
    resp.dbb_bond_recipe_mst = theServer()->cache().dbbBondRecipeMst();
    co_await gme::fillDbb(db, identity, resp);

    resp.campaign_info.current_day = 1;
    resp.campaign_info.total_days = 96;
    resp.campaign_info.first_for_the_day = false;
    resp.campaign_info.id = 1;

    resp.summoner_journal.user_id = identity.userId;
    // ⚠ THE REWARDS TILE DOES NOT EXIST WHILE THIS IS 0.
    // RewardsTopScene::loadMenuList @0xE42440 reads
    // getSummonerJournalFlag() first thing and `cbz w0` past the whole tile
    // (@0xE424B0), so the menu renders SEVEN tiles instead of eight — which
    // is why the 32Gwida0 / 2y48D13d / 3a83iY3r probes never captured a
    // body: the screen they belong to was unreachable.
    //
    // It has to ride here rather than on the Journal's own response, because
    // loadMenuList needs it before the screen can be opened at all.
    //
    // Per the wiki the Journal is new-Summoner-only and disappears once every
    // mission is done and every prize claimed, so 0 is the legitimate
    // "finished" state and this is the switch that retires the feature.
    resp.summoner_journal.journal_flag = 1;
    resp.summoner_journal.points = 0;
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

    // Brave Medals — the currency Brave Slots runs on, seeded on first read.
    // Without this the balance is 0 and RandallSlotActionScene refuses the pull
    // itself (RANDALL_SLOTGAME_MEDAL_ERROR), so no amount of care in the result
    // handler makes the machine playable.  See handbook §7.14.
    resp.medal_info = co_await gme::loadBraveMedals(db, identity);

    // Summoner Arms the player owns (dhMmbm5p).  The block was already wired
    // into this response but its struct was a stub, so the array was always
    // empty and an arm could never be owned — which is what made first-clear
    // reward type 18 unclaimable.  Shared with PresentReceipt, which pays arms.
    resp.summoner_arm_info = co_await gme::loadSummonerArms(db, identity);

    // Merit Points (the Randall achieve-point currency).  The client prints
    // this exact field as the balance: RandallAchievementDedicateScene::
    // setAchievePoint @0x1A39D58 reads UserAchievementInfo +0x18, which is
    // idfCDG70.  Frontier Gate pays it at the end of a run.
    resp.achievement_info.id = (co_await db::DatabaseInterface::read(
        db, "user_info", { db::Data("achieve_point"), db::Lookup("id", identity.userId) }))
        .front<int32_t>("achieve_point");

    // The Unit Selector CATALOGUE (JukkSeNA) and the player's tickets
    // (CGHaOZda).  Both are needed and only the second was being sent: the
    // catalogue is what SummonsDetailScene::initSummontickets matches against
    // the open gate to decide that SUMMON opens the unit picker instead of
    // charging for a pull, so without it a selector gate behaved like an
    // ordinary paid gate.  See the field docs in net/handlers.kdl.
    resp.unit_selector_gacha = theServer()->cache().unitSelectorGacha();
    resp.selector_ticket_info = co_await gme::loadSelectorTickets(db, identity);

    // V2 summon tickets (a3d5d12i) — the per-type inventory.  Also load-bearing:
    // SummonsDetailScene::touchBegan @0x16A7FA8 sums the amounts of the ticket
    // types whose MST row targets the open gate and refuses the pull when the
    // total is 0, so a ticket paid into the box could never be spent while this
    // list was absent.  See gme::loadSummonTicketsV2.
    resp.summon_ticket_v2_user = co_await gme::loadSummonTicketsV2(db, identity);

    // Event tokens (l234vdKs) — the per-event currencies, token 8 being the
    // Frontier Gate's Rift Token.  Also a full replace; see gme::loadEventTokens.
    resp.event_token_info = co_await gme::loadEventTokens(db, identity);

    // What a borrowed Summoner Helper is worth in Honor (6e4b7sQt) — a
    // SINGLETON whose ctor defaults both amounts to "0", so until this was sent
    // every helper card in the quest-prep picker read "Honor +0".  See
    // gme::friendPointInfo; FriendGet sends it too, on the reply that draws
    // those cards.
    resp.friend_point_info = gme::friendPointInfo();

    // An interrupted battle (5PR2VmH1).  THIS BLOCK IS THE TRIGGER: LoginScene::
    // changeNextScene @0x174C544 sends the client to the resume screen only when
    // its state is non-zero, so without it a revival the player paid a gem for
    // is forgotten as soon as the client closes.  A SINGLETON -- one full row,
    // state 0 when nothing is open.  See gme::loadMissionBreak.
    resp.mission_break = co_await gme::loadMissionBreak(db, identity);

    // The Summoner itself (n5mdIUqj) — a SINGLETON, so exactly one row.  Built
    // by gme::loadSummonerInfo, shared with PresentReceipt (which pays SP);
    // see it for why every field but sp is init()'s own value.
    {
        auto summoner = co_await gme::loadSummonerInfo(db, identity);
        resp.summoner_info.clear();
        resp.summoner_info.push_back(std::move(summoner));
    }

    std::string buffer{};
    const auto& ec2 = glz::write_json(resp, buffer);
    if (ec2)
    {
        const auto& glze = glz::format_error(ec2, buffer);
        LOG_DEBUG << "Gme UserInfo Error during JSON writing: " << glze;
        co_return HandleResult::error("Serialization error", glze);
    }

    // Normal PermitPlace is a full replacement, rebuilt from current state.
    gme::injectPermitPlace(buffer, co_await gme::buildPermitPlace(db, identity));

    co_return HandleResult::success(buffer);
}
