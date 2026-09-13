#include "App.hpp"
#include "MigrationManager.hpp"

using MigrationEntry = std::pair<std::string, std::function<void(drogon::orm::DbClientPtr&)>>;
using MigrationMap = std::vector<MigrationEntry>;

#define migrate(name, func) map.emplace_back(name, [](drogon::orm::DbClientPtr& p) func )

/*!
* Register all the available migrations
* @param map Map to register
*/
static void RegisterMigrations(MigrationMap& map)
{
	// DDMMYYYY_MigrationName

	migrate("29082025_CreateDefaultTables", {
		p->execSqlSync(
			"CREATE TABLE gumi_live_users("
			"id TEXT PRIMARY KEY NOT NULL"
			");"
		);
		p->execSqlSync(
			"CREATE TABLE user_info("
			"id TEXT PRIMARY KEY NOT NULL,"
			"gumi_user_id TEXT NOT NULL,"
			"device_id TEXT NOT NULL,"
			"username TEXT NOT NULL DEFAULT '',"
			"level INTEGER(10) NOT NULL DEFAULT 1,"
			"debug_mode INTEGER(1) NOT NULL DEFAULT 0,"
			"exp INTEGER(10) NOT NULL DEFAULT 0,"
			"max_unit_count INTEGER(9) NOT NULL DEFAULT 0,"
			"max_friend_count INTEGER(9) NOT NULL DEFAULT 0,"
			"zel INTEGER(9) NOT NULL DEFAULT 0,"
			"karma INTEGER(9) NOT NULL DEFAULT 0,"
			"brave_coin INTEGER(9) NOT NULL DEFAULT 0,"
			"max_warehouse_count INTEGER(9) NOT NULL DEFAULT 0,"
			"want_gift TEXT NOT NULL DEFAULT '',"
			"friend_points INTEGER(9) NOT NULL DEFAULT 0,"
			"gems INTEGER(4) NOT NULL DEFAULT 0,"
			"active_deck INTEGER(1) NOT NULL DEFAULT 0,"
			"tutorial_status INTEGER(3) NOT NULL DEFAULT 1,"
			"summon_tickets INTEGER(4) NOT NULL DEFAULT 0,"
			"rainbow_coins INTEGER(4) NOT NULL DEFAULT 0,"
			"colosseum_tickets INTEGER(4) NOT NULL DEFAULT 0,"
			"active_arena_deck INTEGER(1) NOT NULL DEFAULT 0,"
			"total_brave_points INTEGER(9) NOT NULL DEFAULT 0,"
			"avail_brave_points INTEGER(9) NOT NULL DEFAULT 0,"
			"energy INTEGER(10) NOT NULL DEFAULT 0,"
			"energy_full_ts INTEGER(10) NOT NULL DEFAULT 0"
			");"
		);
	});

	migrate("08032025_CreateUserUnitsTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_units ("
			"user_unit_id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"user_id TEXT NOT NULL,"
			"unit_id INTEGER NOT NULL,"
			"unit_type_id INTEGER NOT NULL,"
			"unit_lvl INTEGER NOT NULL DEFAULT 0,"
			"base_hp INTEGER NOT NULL,"
			"base_atk INTEGER NOT NULL,"
			"base_def INTEGER NOT NULL,"
			"base_rec INTEGER NOT NULL,"
			"ext_hp INTEGER NOT NULL DEFAULT 0,"
			"ext_atk INTEGER NOT NULL DEFAULT 0,"
			"ext_def INTEGER NOT NULL DEFAULT 0,"
			"ext_rec INTEGER NOT NULL DEFAULT 0,"
			"bb_id TEXT NOT NULL DEFAULT '',"
			"bb_lvl INTEGER NOT NULL DEFAULT 0,"
			"sbb_id TEXT NOT NULL DEFAULT '',"
			"sbb_lvl INTEGER NOT NULL DEFAULT 0,"
			"new INTEGER NOT NULL DEFAULT 0"
			");"
		);
		// Keep local user unit ids away from low values that the client treats
		// as special ids, such as the summoner unit id 20.
		p->execSqlSync(
			"INSERT INTO sqlite_sequence(name, seq) "
			"SELECT 'user_units', 999 "
			"WHERE NOT EXISTS ("
			"SELECT 1 FROM sqlite_sequence WHERE name = 'user_units'"
			");"
		);
	});

	migrate("07062026_CreateUserDecksTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_decks ("
			"user_id TEXT NOT NULL,"
			"user_unit_id INTEGER NOT NULL,"
			"deck_type INTEGER NOT NULL,"
			"deck_num INTEGER NOT NULL,"
			"member_type INTEGER NOT NULL,"
			"disp_order INTEGER NOT NULL,"
			"PRIMARY KEY (user_id, deck_type, deck_num, disp_order)"
			");"
		);
	});

	// NOTE: the unit_lv / base_heal / ext_heal columns added here duplicate the
	// unit_lvl / base_rec / ext_rec columns 08032025 already created, and
	// add_heal / limit_over_heal use the wrong vocabulary for a column.
	// 06082026_ConsolidateUserUnitStatColumns (bottom of this file) collapses
	// all five.  This migration is left as-is rather than corrected in place so
	// that databases which already ran it and databases created fresh converge
	// on the same schema.
	//
	// Extra user_units columns used by the quests-branch handlers
	// (UnitMix/UnitEvo/UnitSell/UnitFavorite, GachaAction, FriendGet,
	// CampaignBattleStart).  Consolidates the former
	// 13032025_AddStatsToUserUnitsTable + 09042026_AddSphereSlotsToUserUnits +
	// 14042026_AddFavoriteFlgToUserUnits migrations onto the upstream table
	// shape.  Upstream columns (unit_lvl/base_rec/ext_rec/bb_*) remain the
	// source of truth for upstream handlers; these serve the not-yet-ported
	// quests handlers and are consolidated away as each moves to
	// PacketInterface.
	migrate("02072026_ExtendUserUnitsForUnitOps", {
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN unit_lv INTEGER NOT NULL DEFAULT 1");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN base_heal INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN add_hp INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN add_atk INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN add_def INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN add_heal INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN ext_heal INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN limit_over_hp INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN limit_over_atk INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN limit_over_def INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN limit_over_heal INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN exp INTEGER NOT NULL DEFAULT 1");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN total_exp INTEGER NOT NULL DEFAULT 1");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN skill_id INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN skill_lv INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN extra_skill_id INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN extra_skill_lv INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN leader_skill_id INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN element TEXT NOT NULL DEFAULT 'fire'");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN fe_bp INTEGER NOT NULL DEFAULT 100");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN fe_max_usable_bp INTEGER NOT NULL DEFAULT 200");
		// Sphere equipment slots (UserUnitInfo: Ge8Yo32T/0R3qTPK9, mZA7fH2v/RXfC31FA).
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN eqip_item_id INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN eqip_item_frame_id INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN eqip_item_id2 INTEGER NOT NULL DEFAULT 0");
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN eqip_item_frame_id2 INTEGER NOT NULL DEFAULT 0");
		// Lock/favorite flag (UnitFavoriteRequest: req["3kcmQy7B"][0]["5JbjC3Pp"]).
		p->execSqlSync("ALTER TABLE user_units ADD COLUMN favorite_flg INTEGER NOT NULL DEFAULT 0");
	});

	migrate("25042026_CreateUserTownTables", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_town_facilities ("
			"user_id     TEXT    NOT NULL,"
			"facility_id INTEGER NOT NULL,"
			"lv          INTEGER NOT NULL DEFAULT 1,"
			"karma       INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, facility_id)"
			");"
		);
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_town_locations ("
			"user_id     TEXT    NOT NULL,"
			"location_id INTEGER NOT NULL,"
			"lv          INTEGER NOT NULL DEFAULT 1,"
			"karma       INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, location_id)"
			");"
		);
	});

	migrate("03072026_CreateUserUnitDictionaryTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_unit_dictionary ("
			"user_id TEXT NOT NULL,"
			"unit_id INTEGER NOT NULL,"
			"img_type_flag INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, unit_id)"
			");"
		);
	});

	// Owned-item inventory: potions, materials, spheres.  One row per stack.
	// instance_id is the warehouse row id the client references (UserWarehouse
	// n6E8iMf3 / legacy ItemSphereEqp wh ids).  Populated naturally — the
	// tutorial seeds a test potion in CreateUser, mission drops append here.
	migrate("05072026_CreateUserItemsTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_items ("
			"instance_id  INTEGER PRIMARY KEY AUTOINCREMENT,"
			"user_id      TEXT    NOT NULL,"
			"item_id      INTEGER NOT NULL,"
			"item_num     INTEGER NOT NULL DEFAULT 1,"
			"favorite_flg INTEGER NOT NULL DEFAULT 0,"
			"disp_order   INTEGER NOT NULL DEFAULT 0,"
			"UNIQUE(user_id, item_id)"
			");"
		);
	});

	// Viewed-cutscene state: one row per scenario the user has watched.
	// GetScenarioPlayingInfo returns this set (sBbp47fi) so the client skips
	// already-seen cutscenes; RaidUpScenarioInfo appends to it.  Structural
	// only — never seeded; a fresh account sees every cutscene once.
	migrate("17072026_CreateUserScenariosTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_scenarios ("
			"user_id     TEXT    NOT NULL,"
			"scenario_id INTEGER NOT NULL,"
			"viewed_at   INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, scenario_id)"
			");"
		);
	});

	migrate("25042026_CreateUserCampaignTables", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_campaign_missions ("
			"user_id          TEXT    NOT NULL,"
			"mission_id       TEXT    NOT NULL,"
			"state            INTEGER NOT NULL DEFAULT 0,"
			"attain_percent   INTEGER NOT NULL DEFAULT 0,"
			"clear_count      INTEGER NOT NULL DEFAULT 0,"
			"last_cleared_at  INTEGER NOT NULL DEFAULT 0,"
			"reward_claimed   INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, mission_id)"
			");"
		);
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_campaign_decks ("
			"user_id      TEXT    NOT NULL,"
			"deck_num     INTEGER NOT NULL,"
			"member_type  INTEGER NOT NULL,"
			"user_unit_id INTEGER NOT NULL,"
			"disporder    INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, deck_num, disporder)"
			");"
		);
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_campaign_state ("
			"user_id            TEXT PRIMARY KEY,"
			"active_mission_id  TEXT    NOT NULL DEFAULT '',"
			"active_battle_seed INTEGER NOT NULL DEFAULT 0,"
			"saved_state        TEXT    NOT NULL DEFAULT ''"
			");"
		);
	});

	migrate("13052026_CreateUserSummonTicketsV2", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_summon_tickets_v2 ("
			"user_id   TEXT    NOT NULL,"
			"ticket_id INTEGER NOT NULL,"
			"count     INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, ticket_id)"
			");"
		);
	});

	// Collapses the duplicate stat columns 02072026 added beside the ones
	// 08032025 already created.  They exist because the client names the same
	// stat differently in different packets — UserUnitInfo says base_rec /
	// ext_rec / unit_lvl, while FriendInfo and ReinforcementInfo say base_heal
	// / ext_heal / unit_lv (both spellings come from IDA; see
	// net/friends.kdl).  A column was added per spelling, so a unit could hold
	// two recovery values that disagree.
	//
	// One column per concept from here.  The rec/lvl spelling wins because
	// 08032025_CreateUserUnitsTable established it; PacketInterfaceFor<T> maps
	// either packet vocabulary onto it, which is what it is for.
	//
	// Merge rule: the duplicate wins only where the canonical column is still
	// at its default, since the quests-branch handlers wrote the duplicates
	// while upstream handlers wrote the canonical ones.  Where both hold real
	// values they are expected to agree; if they do not, the canonical value is
	// kept.
	migrate("06082026_ConsolidateUserUnitStatColumns", {
		p->execSqlSync("UPDATE user_units SET unit_lvl = unit_lv "
			"WHERE unit_lvl = 0 AND unit_lv != 0;");
		p->execSqlSync("UPDATE user_units SET base_rec = base_heal "
			"WHERE base_rec = 0 AND base_heal != 0;");
		p->execSqlSync("UPDATE user_units SET ext_rec = ext_heal "
			"WHERE ext_rec = 0 AND ext_heal != 0;");

		p->execSqlSync("ALTER TABLE user_units DROP COLUMN unit_lv;");
		p->execSqlSync("ALTER TABLE user_units DROP COLUMN base_heal;");
		p->execSqlSync("ALTER TABLE user_units DROP COLUMN ext_heal;");

		// These two had no canonical counterpart — they are not duplicates,
		// just the wrong vocabulary for a column.
		p->execSqlSync("ALTER TABLE user_units RENAME COLUMN add_heal TO add_rec;");
		p->execSqlSync("ALTER TABLE user_units "
			"RENAME COLUMN limit_over_heal TO limit_over_rec;");
	});

	// Frontier Gate — per-user, per-gate progress.  Feeds FrontierGateInfo
	// (M17pPotk) response key dPM7oJDl; see tools/ida/audits/dPM7oJDl_audit.txt
	// and tools/FRONTIER_GATE_STATE_MODEL.md.
	//
	// Deliberately NARROWER than the 11-field packet (handbook §6.15.0 — the
	// KDL is the wire vocabulary and should be maximal, the schema is state and
	// should be minimal).  Four of the packet's fields are intentionally absent:
	//
	//   start_date / end_date  — the gate's availability window is already in
	//       FrontierGateMst (qA7M9EjP / SzV0Nps7, 94 rows).  §6.15 rule 3: if
	//       the cache can answer, a per-user copy is a second source that drifts.
	//   progress_max           — likewise derivable from the gate definition,
	//       AND its name is only MEDIUM confidence (see below).
	//   ranking                — no leaderboard exists single-player, and the
	//       client gates the display behind FrontierGateMst::getRankingDispFlg.
	//
	// 69bpUIXR/progress_max and 2wHGmJqm/ranking are named from vtable slot
	// position, not from a real setter, and could be swapped for one another.
	// Persisting a column under a name we cannot yet prove is exactly the debt
	// §6.15 warns about, so neither gets a column until a capture settles it.
	//
	// To delete this table later: it is consumed only by FrontierGateInfo.cpp.
	migrate("07082026_CreateUserFrontierGatesTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_frontier_gates ("
			"user_id         TEXT    NOT NULL,"
			"frogate_id      INTEGER NOT NULL,"
			"state           INTEGER NOT NULL DEFAULT 0,"
			"progress        INTEGER NOT NULL DEFAULT 0,"
			"score           INTEGER NOT NULL DEFAULT 0,"
			"mission_id      TEXT    NOT NULL DEFAULT '',"
			"sel_support_id  INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, frogate_id)"
			");"
		);
	});

	// Frontier Gate tier 2 — the active run.  Feeds FrontierGateStart's
	// response key Mg8K8Y1a (FrontierGateNumResponse).
	//
	// ONE ROW PER USER, and the row's EXISTENCE is the "run in progress" flag —
	// which is why there is no `active` column and no separate suspended-info
	// table.  FrontierGateSuspendedInfoResponse (gmeGFd2s) carries exactly one
	// field, hBNPQAU0, so "is there a run and on which gate" is a projection of
	// this row rather than state of its own.  Ending a run deletes the row.
	//
	// frogate_num is a SERVER-OWNED run handle.  FrontierGateEnd/Continue/Retry
	// read it back out of the client's shared FrontierGateNum singleton
	// (FrontierGateNum::getFrogateNum) and echo it as their 2FQdEbG3, so the
	// client never authors it — it is opaque to the client and only has to be
	// stable for the life of the run.
	//
	// The three power-up rates in the packet get NO columns: the client parses
	// them with StrToFloat and divides by 100, but where the server is supposed
	// to source them (gate MST battle params? the support effect?) is not
	// decoded, so there is nothing to store.  The handler returns the neutral
	// 100 (= x1.0).  Add columns only once a real source is identified.
	//
	// To delete this table later: consumed only by FrontierGateStart.cpp.
	// One-off scene intros the client has already played.
	//
	// The client reports these as a trailing-comma list in its login envelope
	// under 9yVsu21R — "randall," on the world map, growing to
	// "randall,frontiergate_v2," once Frontier Gate has been entered (captured
	// 2026-08-07 from DungeonEventUpdate requests).  LoginInfoResp carries the
	// same key back as user_special_scenario_info, so the round trip is what
	// keeps an intro marked as seen.
	//
	// Until now LoginInfoReq did not even declare the key, so the value was
	// dropped by the lenient read and nothing was ever echoed — which is the
	// working theory for why the Frontier Gate intro replays every session.
	//
	// One TEXT column rather than a row-per-scene table: the client hands us
	// the whole list every request and reads the whole list back, so there is
	// no per-scene query to serve and normalising it would only add a join.
	// Distinct from user_scenarios, which holds numeric F_SCENARIO_MST ids for
	// GetScenarioPlayingInfo — a different mechanism that the client does not
	// appear to consult on this path (0 VRfsv4e3 calls in a full FG session).
	migrate("07082026_AddSpecialScenarioInfo", {
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN special_scenario_info TEXT NOT NULL DEFAULT '';");
	});

	// Hunter Orbs — the Frontier Gate / Frontier Hunter attempt currency.
	//
	// The client calls these fight points: UserTeamInfo carries YS2JG9no
	// (fight_point) and 9m5FWR8q (max_fight_point), which the Survey Office
	// header renders as the "Hunter Orbs" x/y counter.  Nothing populated them,
	// so both went out as 0 and every gate answered "You have no Hunter Orbs
	// left" — verified 2026-08-07 in a live UserInfo capture.
	//
	// Two columns, and both pass the §6.15 test: they are mutable per-user
	// state (spent on an attempt, restored by a gem or by time), the client
	// demonstrably blocks the feature without them, and no MST can answer them
	// — F_USER_LEVEL_MST has no fight-point column, unlike energy.
	//
	// Deliberately NOT added: a regeneration timestamp.  defines_mst gives the
	// cadence (recover_time_fight = 3600s, one orb per hour; frohun uses
	// 10800s) but energy regeneration is itself still a static placeholder in
	// this server, so orb regen lands with it rather than growing a column now.
	//
	// Seeded at 3/3 rather than the 1/1 the client showed: the real cap source
	// is not decoded, and 3 gives room to exercise spend-and-restore.
	migrate("07082026_AddFightPointColumns", {
		p->execSqlSync("ALTER TABLE user_info ADD COLUMN fight_point INTEGER NOT NULL DEFAULT 3;");
		p->execSqlSync("ALTER TABLE user_info ADD COLUMN max_fight_point INTEGER NOT NULL DEFAULT 3;");
	});

	migrate("07082026_CreateUserFrontierGateRunTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_frontier_gate_run ("
			"user_id         TEXT    NOT NULL PRIMARY KEY,"
			"frogate_id      INTEGER NOT NULL,"
			"frogate_num     INTEGER NOT NULL DEFAULT 0,"
			"mission_status  INTEGER NOT NULL DEFAULT 0,"
			"sel_support_id  INTEGER NOT NULL DEFAULT 0"
			");"
		);
	});

	// The run's party, kept so a continued or retried floor still has one.
	//
	// Only FrontierGateStart and FrontierGateSave carry the party (MxmCpDRC +
	// 52xDBRGr); Continue sends nothing and Retry sends the deck alone.  With
	// nothing stored, continuing to the next floor left the client with an
	// empty FrontierGateUserUnitInfoList — the party vanished and the leader
	// showed as dead (observed 2026-08-07).
	//
	// Stored as two JSON blobs rather than normalised rows: the client hands us
	// the whole party and reads the whole party back, so there is no per-member
	// query to serve and a deck/unit table would only add joins.  Same call as
	// special_scenario_info.  If a future feature needs per-member queries
	// (rewards by unit, say), normalise then.
	migrate("07082026_AddFrontierGateRunParty", {
		p->execSqlSync(
			"ALTER TABLE user_frontier_gate_run ADD COLUMN party_deck_json TEXT NOT NULL DEFAULT '';");
		p->execSqlSync(
			"ALTER TABLE user_frontier_gate_run ADD COLUMN party_units_json TEXT NOT NULL DEFAULT '';");
	});

	// Vortex dungeon keys — one row per (user, DungeonKeyMst.id), i.e. three
	// rows per player (Metal / Jewel / Imp).  Feeds eFU7Qtb0
	// (UserDungeonKeyInfoResponse) out of UserInfo and all three dungeon-key
	// handlers.  Field semantics: tools/ida/audits/eFU7Qtb0_audit.txt.
	//
	// ONLY THE DURABLE STATE GETS COLUMNS.  Two of the response's six confirmed
	// fields are derived and are computed per request instead:
	//   receipt_possible_flg (g85qMNxf)       — "can I claim right now", a pure
	//       function of today's weekday vs DungeonKeyMst.distribute_days,
	//       last_receipt_day, and possession vs possession_limit.
	//   next_receipt_possible_date (b6QR1CH5) — the next weekday in
	//       distribute_days after today.
	// Storing either would go stale the moment the clock crossed midnight
	// without a write, which is the whole failure mode a day-gated feature has
	// to avoid.  §6.15: no column without a reason it must persist.
	//
	// active_type (BY8fZ7M1) DOES get a column: it records which parade tier
	// the player last opened, which nothing else can reconstruct.
	//
	// cnt (H6k1LIxC) gets NO column — its semantic is still unresolved (see the
	// audit), the KDL leaves it optional, and it is omitted from the wire.
	// Add a column only once an IDA run names it.
	//
	// To delete this table later: it is consumed by DungeonKey.cpp and by
	// UserInfo's dungeon_key_info population.
	migrate("08082026_CreateUserDungeonKeysTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_dungeon_keys ("
			"user_id          TEXT    NOT NULL,"
			"dungeon_key_id   INTEGER NOT NULL,"
			"possession       INTEGER NOT NULL DEFAULT 0,"
			"last_receipt_day INTEGER NOT NULL DEFAULT 0,"
			"active_type      INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, dungeon_key_id)"
			");"
		);
	});

	// The OTHER half of the cutscene state.
	//
	// LoginInfo carries two scenario fields and only one of them was being
	// persisted.  9yVsu21R (special_scenario_info) is the comma list of named
	// one-off intros — "randall,frontiergate_v2,challenge," — and has worked
	// since the Frontier Gate pass.  N4XVE1uA is a separate composite marker
	// the client also grows across sessions (observed going from "|0,0" to
	// "0,0@0@2@1&|0,0" as content was seen), and it was never stored: UserInfo
	// echoed back "" on every login, so whatever it tracks replayed forever.
	//
	// Stored verbatim, exactly like special_scenario_info: the client sends the
	// whole marker every time and only ever extends it, so there is nothing to
	// merge.  The format is NOT decoded — '&' and '|' separated groups of
	// comma/at-joined ints — and deliberately not parsed, because round-tripping
	// an opaque blob needs no schema.
	migrate("09082026_AddUserScenarioInfo", {
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN scenario_info TEXT NOT NULL DEFAULT '';");
	});

	// The pre-mission battle-item loadout (the 5 slots on the quest-prep
	// screen).  Previously nowhere: ItemEdit acked and discarded the
	// selection, and UserInfo synthesised the equip list by walking the
	// whole warehouse and emitting EVERY battle consumable the player owned,
	// so the slots came back full of whatever happened to be in inventory
	// order and the player's actual choice never survived.
	//
	// Keyed by (user_id, disp_order) — disp_order IS the slot, taken
	// verbatim from the wire.  ItemEditRequest::createBody emits two runs
	// into the same 71U5wzhI group: the main loadout at slot 0..n and a
	// second run at slot+100, so the +100 band is stored as-is rather than
	// being folded into the first.
	migrate("11082026_CreateUserEquipItemsTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_equip_items ("
			"user_id     TEXT    NOT NULL,"
			"disp_order  INTEGER NOT NULL,"
			"item_id     INTEGER NOT NULL,"
			"item_num    INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, disp_order)"
			");"
		);
	});

	// tutorial_end_flag (sv6BEI8X).  Previously DERIVED in getLoginInfo as
	// `tutorial_status >= 12`.  That threshold was this fork's own invention —
	// the legacy server's Tutorial{Update,Skip} handlers are both empty stubs,
	// so nothing was ported — and the tutorial does not end at 12: the
	// client-bundled scripts run to tuto16.txt, with tuto13 = item use,
	// tuto14 = battle UI, tuto15 = the free summon, tuto16 = farewell.
	// Deriving the flag at 12 reports "tutorial over" with four scripts still
	// pending, and makes the status pointer untestable (any status >= 12 forces
	// the flag true).
	//
	// The client already answers this for us: every login envelope carries
	// sv6BEI8X next to 9sQM2XcN.  Store what the client reports and echo it
	// back, the same treatment scenario_info gets above, rather than inferring
	// it from a threshold nobody verified.
	//
	// Backfilled with the old derivation so existing accounts keep the state
	// they already had.
	migrate("14082026_AddTutorialEndFlag", {
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN tutorial_end_flag INTEGER NOT NULL DEFAULT 0;");
		p->execSqlSync(
			"UPDATE user_info SET tutorial_end_flag = 1 WHERE tutorial_status >= 12;");
	});

	// Backfill the starting battle-item loadout.
	//
	// CreateUser now seeds a user_equip_items slot-0 row with the tutorial
	// potion, but that only fires for accounts created after the fix.  Accounts
	// made between 11082026_CreateUserEquipItemsTable (which turned equip_info
	// into a verbatim read of that table) and the fix own the potion in
	// user_items yet have an empty loadout — and tuto1.txt blocks forever on
	// `change_item_scene:tuto_use_item` when the in-battle item menu is empty.
	//
	// Only touches accounts with NO loadout at all, so a player who has since
	// arranged their own slots through ItemEdit is left alone.
	migrate("14082026_SeedTutorialBattleItemLoadout", {
		p->execSqlSync(
			"INSERT INTO user_equip_items (user_id, disp_order, item_id, item_num)"
			" SELECT i.user_id, 0, i.item_id, 1"
			" FROM user_items i"
			" WHERE i.item_id = 20000 AND i.item_num > 0"
			"   AND NOT EXISTS ("
			"     SELECT 1 FROM user_equip_items e WHERE e.user_id = i.user_id);");
	});

	// The present box (sEA41vFK / UserPresentInfoResponse).
	//
	// Columns mirror the decoded readParam setters one-for-one so the handler
	// never has to translate: PresentID/PresentType/TargetID/TargetCnt/
	// TargetParam/ReceiptType/PresentDate/ReceiptDate/IsReceipt.  The two
	// *DateStr display fields are deliberately NOT stored — they are formatted
	// from their epoch siblings at send time, so the pair can never disagree.
	//
	// present_type shares the campaign reward vocabulary (same 30Kw4WBa hash
	// CampaignReceipt dispatches on): 3 = zel, 8 = gem, 6 = unit,
	// 4/5/7 = item/material/sphere.
	//
	// present_id is the autoincrement rowid rendered as a string on the wire;
	// PresentReceipt echoes it straight back, so it only has to be stable and
	// unique per user.
	migrate("15082026_CreateUserPresentsTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_presents ("
			"present_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
			"user_id      TEXT    NOT NULL,"
			"present_type INTEGER NOT NULL,"
			"target_id    TEXT    NOT NULL DEFAULT '',"
			"target_cnt   INTEGER NOT NULL DEFAULT 1,"
			"target_param TEXT    NOT NULL DEFAULT '',"
			"receipt_type INTEGER NOT NULL DEFAULT 0,"
			"description  TEXT    NOT NULL DEFAULT '',"
			"present_date INTEGER NOT NULL DEFAULT 0,"
			"receipt_date INTEGER NOT NULL DEFAULT 0,"
			"is_receipt   INTEGER NOT NULL DEFAULT 0"
			");"
		);
		p->execSqlSync(
			"CREATE INDEX IF NOT EXISTS idx_user_presents_user"
			" ON user_presents (user_id, is_receipt);");
	});

	// Harvest state for the four town resource tiles.  Lives on the existing
	// location row because it is 1:1 with it — UserTownLocationDetail (s8TCo2MS)
	// and UserTownLocationInfo (yj46Q2xw) are the same (user, location) key.
	//
	// The server pre-rolls a whole period of drops into drop_info and the client
	// replays them tap by tap, so these three columns ARE the tile: without them
	// every tile reports 0 taps forever and never sparkles.  See
	// tools/TOWN_STATE_MODEL.md.
	migrate("18082026_AddTownLocationHarvestState", {
		// Unix seconds the current harvest period began; 0 = never rolled.
		p->execSqlSync(
			"ALTER TABLE user_town_locations ADD COLUMN period_start INTEGER NOT NULL DEFAULT 0");
		// Taps REMAINING, matching the client's own decTapCnt semantics.
		p->execSqlSync(
			"ALTER TABLE user_town_locations ADD COLUMN tap_cnt INTEGER NOT NULL DEFAULT 0");
		// "<itemId>:<zel>:<karma>,..." — one element per tap of this period.
		p->execSqlSync(
			"ALTER TABLE user_town_locations ADD COLUMN drop_info TEXT NOT NULL DEFAULT ''");
	});

	// The tile level the current period was rolled at.  Drops are pre-rolled a
	// whole period ahead, so upgrading a tile mid-period would otherwise leave
	// the player harvesting the old level's pool until the period expired —
	// the upgrade appears to do nothing.  Town::locationState compares this
	// against the live level and re-rolls the remaining taps when they differ.
	migrate("18082026_AddTownLocationPeriodLevel", {
		p->execSqlSync(
			"ALTER TABLE user_town_locations ADD COLUMN period_lv INTEGER NOT NULL DEFAULT 0");
	});

	// Per-recipe craft tally behind PermitRecipe.craft_count (H6k1LIxC).  The
	// client bumps its own copy after each craft (GameUtils::updatePermitRecipe),
	// so the count has to survive a relaunch to stay in step.
	migrate("18082026_CreateUserRecipeCraftsTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_recipe_crafts ("
			"user_id     TEXT    NOT NULL,"
			"recipe_id   INTEGER NOT NULL,"
			"craft_count INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, recipe_id)"
			");"
		);
	});

	// Daily Spin (Rewards menu -> task_dailyloginspin).  Per-user, one row.
	//
	//   spin_day     which day of the reward cycle the player is on (1-based).
	//                The live game's table runs 29 days and then repeats its
	//                last row, so this only ever counts up.
	//   spins_used   spins taken TODAY.  This is the value that goes out as
	//                35JXN4Ay, and the client treats < 1 as "hasn't spun",
	//                which makes Home force-open the wheel -- see the warning
	//                in Initialize.cpp before changing how it is derived.
	//   last_spin_utc_day  days since the unix epoch, i.e. floor(now/86400).
	//                Stored rather than a date string so the rollover check is
	//                an integer compare and cannot be tripped by formatting.
	// Mystery Chest (Rewards menu -> task_mysterychest).  One row per chest a
	// player holds.
	//
	//   box_id      the id sent as rEFRefr8 and echoed back by the claim.
	//   chest_key   which archive definition this chest is; the contents live
	//               in deploy/archive/mystery_chest.json, not here, so a chest
	//               already granted follows edits to its rewards.
	//   reward_ts   unix SECONDS the chest becomes claimable.
	//   expiry_ts   unix SECONDS it disappears.  The client counts down from
	//               this (getPresentTimeLeftType @0x1CB30B4 divides
	//               expiry - server epoch by 86400) and treats non-positive as
	//               expired, so the server and the client agree without a flag.
	//   claimed     1 once opened.  Kept rather than deleted so a chest cannot
	//               be re-granted to someone who already opened it.
	// Brave Medals — the currency Brave Slots runs on.
	//
	// One row per (user, medal).  The slot machine names the medal it spends in
	// brave_slots.json's b5yeVr61 ("1"), which reaches the client as
	// SlotgameInfo::setSlotUseMedal and is looked up through
	// UserBraveMedalInfoList::getPossessionWithMedalID(string) — so it is an
	// ID, not a cost.
	//
	// Offline there is nothing to earn medals from: the live sources were Brave
	// Points and events, both unbuilt.  A stock is seeded on first visit
	// instead, which is why this table exists rather than a userinfo column —
	// the wire shape is a LIST keyed by medal id and may hold more than one.
	migrate("21082026_CreateUserBraveMedalsTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_brave_medals ("
			"user_id    TEXT    NOT NULL,"
			"medal_id   TEXT    NOT NULL,"
			"possession INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, medal_id)"
			");"
		);
	});

	// Summoner's Journal progress, one row per (user, task).
	//
	// A generic counter table rather than a column per mission: there are 45
	// missions and each needs its own tally, so widening a table for every one
	// would be unworkable.  Rows are created on first increment, so a mission
	// that has never ticked simply has no row and reads as 0.
	migrate("23082026_CreateUserJournalTasksTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_journal_tasks ("
			"user_id  TEXT    NOT NULL,"
			"task_key TEXT    NOT NULL,"
			"progress INTEGER NOT NULL DEFAULT 0,"
			"claimed  INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, task_key)"
			");"
		);
	});

	// Lifetime battle statistics -- the progress half of the trophy system.
	//
	// UserTeamArchive (zI2tJB7R) carries 39 counters and every one is answered
	// by PlayerInfoBattleResultScene::getActual @0x1791EEC, which maps exactly
	// one trophy id per field.  Only these eight are populated: they are the
	// ones the client already reports in every MissionEnd request, byte
	// identical keys, so they cost nothing to collect.  The other 31 need
	// instrumenting at their own call sites (unit fusion, item mixing, gifts,
	// logins) and are left at 0 until someone does that.
	//
	// SUM vs MAX per column is taken from each trophy's label in
	// deploy/mst/trophy_mst.json -- 累計/総合 sums, 最大 maxes -- NOT from the
	// setter name.  See tools/SERVER_COMPONENT_AUDIT.md.
	migrate("24082026_CreateUserTeamArchiveTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_team_archive ("
			"user_id                TEXT    NOT NULL,"
			// cumulative
			"b_crystal              INTEGER NOT NULL DEFAULT 0,"
			"h_crystal              INTEGER NOT NULL DEFAULT 0,"
			"battle_spark_cnt       INTEGER NOT NULL DEFAULT 0,"
			"battle_skill_cnt       INTEGER NOT NULL DEFAULT 0,"
			"quest_mimic_cnt        INTEGER NOT NULL DEFAULT 0,"
			// high-water marks
			"battle_turn_max_damage INTEGER NOT NULL DEFAULT 0,"
			"battle_turn_max_spark  INTEGER NOT NULL DEFAULT 0,"
			"turn_max_unit_damage   INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id)"
			");"
		);
	});

	// Second tranche of trophy counters.  Every one is a SUM: the classifier is
	// each trophy's own label in deploy/mst/trophy_mst.json -- 累計 / 総合 /
	// 回数 / 数 are all cumulative, and none of these carries 最大.
	//
	// Unlike the first eight (which arrive pre-measured in MissionEnd's
	// rXvA1E5y), these are incremented by the handler that performs the action,
	// so each is worth exactly one +1 or one += amount at the point of truth.
	migrate("31082026_UserTeamArchiveCountersTrancheTwo", {
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN zel_get INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN karma_get INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN zel_use INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN karma_use INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN zel_unit_sale INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN unit_mix_cnt INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN unit_mix_elem_cnt INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN unit_evo_cnt INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN town_harvest_cnt INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN quest_challenge_cnt INTEGER NOT NULL DEFAULT 0;");
			p->execSqlSync("ALTER TABLE user_team_archive ADD COLUMN quest_clear_cnt INTEGER NOT NULL DEFAULT 0;");
	});

	migrate("21082026_CreateUserMysteryBoxesTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_mystery_boxes ("
			"user_id   TEXT    NOT NULL,"
			"box_id    TEXT    NOT NULL,"
			"chest_key TEXT    NOT NULL,"
			"reward_ts INTEGER NOT NULL DEFAULT 0,"
			"expiry_ts INTEGER NOT NULL DEFAULT 0,"
			"claimed   INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, box_id)"
			");"
		);
	});

	migrate("20082026_CreateUserDailySpinTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_daily_spin ("
			"user_id           TEXT    NOT NULL,"
			"spin_day          INTEGER NOT NULL DEFAULT 1,"
			"spins_used        INTEGER NOT NULL DEFAULT 0,"
			"last_spin_utc_day INTEGER NOT NULL DEFAULT 0,"
			"last_reward_id    INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id)"
			");"
		);
	});

	// Summoner Arm ownership.  The client reads it back as UserSummonerArmsInfo
	// (wire dhMmbm5p): user_id, summoner_arm_id, exp, level -- exactly the four
	// keys UserSummonerArmsInfoResponse::readParam @0x144E824 sets, and nothing
	// more.  An arm is OWNED ONCE, so (user_id, summoner_arm_id) is the key and
	// a second grant of the same arm is a no-op rather than a duplicate row.
	//
	// This is what makes first-clear reward type 18 claimable; without a table
	// to put it in, PresentReceipt could only log "not supported".
	migrate("04092026_CreateUserSummonerArmsTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_summoner_arms ("
			"user_id         TEXT    NOT NULL,"
			"summoner_arm_id TEXT    NOT NULL,"
			"exp             INTEGER NOT NULL DEFAULT 0,"
			"level           INTEGER NOT NULL DEFAULT 1,"
			"PRIMARY KEY (user_id, summoner_arm_id)"
			");"
		);
	});

	// Summoner SP — the currency reward type 17 pays.
	//
	// ONE COLUMN ON PURPOSE.  UserSummonerInfo (wire n5mdIUqj) carries 32
	// fields, but 31 of them belong to a Summoner subsystem nobody has built:
	// summoner level and element mastery are earned in battle, abilities are
	// bought in SummonerAbilityListScene, equipment is chosen on the summoner
	// screen — and none of those handlers exist.  Giving them columns now
	// would create writeless state that can only go stale.  UserInfo emits
	// those fields at the values UserSummonerInfo::init() @0x127E9E8 already
	// assigns, so they persist nowhere because nothing can change them.
	//
	// When the summoner subsystem lands, this table grows level/exp/element
	// columns and the emit stops using init() defaults for them.
	//
	// Consumed by PresentReceipt case 17 and UserInfo's summoner_info block.
	migrate("04092026_CreateUserSummonerTable", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_summoner ("
			"user_id TEXT    NOT NULL,"
			"sp      INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id)"
			");"
		);
	});

	// The Sphere Frog's extra sphere slot.
	//
	// The rule is the game's own, from MST_HELP_SUBTOPIC_100_8_DESCRIPTION
	// ("Sphere Capacity Increase"): fusing the unit "Sphere Frog" as material
	// raises the base unit's sphere capacity, ONE extra slot per unit and no
	// more, and "those units who have increased their sphere capacity will
	// inherit this trait even when they evolve".
	//
	// A column on user_units rather than a table, because it is one bit that
	// belongs to a unit instance and dies with it.  Evolution needs no work:
	// UnitEvo updates the base row in place, so the bit rides along, which is
	// exactly the inheritance the help text promises.
	//
	// Consumed by UnitMix (which sets it), ItemSphereEqp (which enforces it)
	// and the UserUnitInfo packet mapping (which sends it as ext_count).
	migrate("04092026_AddUserUnitsSphereExt", {
		p->execSqlSync(
			"ALTER TABLE user_units ADD COLUMN sphere_ext INTEGER NOT NULL DEFAULT 0;");
	});

	// Login-day trophies.  TROPHY 100010 (login days) and 100020 (consecutive
	// login days) are two of the 39 the Records screen draws, and both were
	// stuck at 0 because nothing counted a login.
	//
	// The client read is already decoded: PlayerInfoBattleResultScene::getActual
	// @0x1791EEC answers trophy 100010 from UserTeamArchive.login_cnt (dD64grYH)
	// and 100020 from serial_login_day (3w6YDS4z).  Only the source was missing.
	//
	// last_login_day is the third column and the only one that is not itself a
	// trophy: it is the epoch-day token the streak is computed against, the same
	// once-per-day device user_dungeon_keys uses.  Without it a streak cannot
	// tell "logged in again today" from "logged in the next day".
	// The rest of the Records counters the handlers can actually source.
	// Every one maps 1:1 to a trophy the client already knows how to draw --
	// PlayerInfoBattleResultScene::getActual @0x1791EEC does the lookup, and
	// net/user.kdl records which field answers which id:
	//   zel_item_sale 100060, unit_sum_cnt 100190, item_mix_cnt 100250,
	//   item_mix_elem_cnt 100260, sphere_mix_cnt 100270, b_crystal_max 100290,
	//   h_crystal_max 100300, quest_win_cnt 200030.
	// Written by ItemSell, addUserUnit, ItemMix and MissionEnd respectively.
	migrate("04092026_AddUserTeamArchiveRecordCounters", {
		for (const auto* col : {
			"zel_item_sale     INTEGER NOT NULL DEFAULT 0",
			"unit_sum_cnt      INTEGER NOT NULL DEFAULT 0",
			"item_mix_cnt      INTEGER NOT NULL DEFAULT 0",
			"item_mix_elem_cnt INTEGER NOT NULL DEFAULT 0",
			"sphere_mix_cnt    INTEGER NOT NULL DEFAULT 0",
			"b_crystal_max     INTEGER NOT NULL DEFAULT 0",
			"h_crystal_max     INTEGER NOT NULL DEFAULT 0",
			"quest_win_cnt     INTEGER NOT NULL DEFAULT 0" })
		{
			p->execSqlSync(std::string("ALTER TABLE user_team_archive ADD COLUMN ") + col + ";");
		}
	});

	migrate("04092026_AddUserTeamArchiveLoginCounters", {
		for (const auto* col : {
			"login_cnt INTEGER NOT NULL DEFAULT 0",
			"serial_login_day INTEGER NOT NULL DEFAULT 0",
			"last_login_day INTEGER NOT NULL DEFAULT 0" })
		{
			p->execSqlSync(std::string("ALTER TABLE user_team_archive ADD COLUMN ") + col + ";");
		}
	});

	// SPHERE CAPACITY SENTINEL.  eqip_item_frame_id2 gained a third meaning:
	// -1 = "this unit has no second sphere slot", alongside 0 = empty slot and
	// 1..14 = the sphere type occupying it.  The column was created DEFAULT 0,
	// so every row on every existing save says "second slot present" and the
	// client duly draws two slots on every unit.  The Sphere Frog had nothing
	// left to grant.  gme::kNoSecondSphereSlot has the client-side proof.
	//
	// A MIGRATION RATHER THAN A LOCAL DB EDIT, deliberately: this is not a
	// one-off data fix (which belongs in deploy/gme.sqlite by hand) but a
	// change to what a value MEANS.  Every save in existence carries the wrong
	// one, so the correction has to travel with the code.
	//
	// Guarded on sphere_ext = 0 so a unit that genuinely earned the slot keeps
	// it, and on eqip_item_id2 = 0 so a sphere already sitting in slot 2 is
	// never orphaned by taking the slot away underneath it -- such a row would
	// need its sphere returned to the warehouse first, and this migration
	// deliberately does not do that silently.  It leaves the pair disagreeing,
	// which ItemSphereEqp then reads as "has the slot"; the generous reading is
	// the right one when the alternative is eating a player's sphere.
	migrate("05092026_UnitSphereCapacitySentinel", {
		p->execSqlSync(
			"UPDATE user_units SET eqip_item_frame_id2 = -1 "
			"WHERE sphere_ext = 0 AND eqip_item_id2 = 0;");
	});

	// THE PARADE'S 30-MINUTE WINDOW.  DungeonKeyMst.limit_sec is 1800 on all
	// three keys, but nothing ever tracked when a key was spent, so the window
	// had nowhere to live: DungeonKeyUse set active_type and the parade was
	// open forever in the DB while the client re-locked it instantly.
	//
	// It re-locked because UserDungeonKeyInfo.cnt (H6k1LIxC) was never assigned
	// and went out as 0 -- the client reads it as the seconds remaining on the
	// active tier, so every reply said "zero seconds left".  That also settles
	// the field's open question in net/user.kdl, which had it CONFIRMED as
	// setCnt but UNVERIFIED as to which count.
	//
	// Stored as an absolute epoch second rather than a countdown so it survives
	// a restart and needs no ticking; cnt is derived from it per response.
	migrate("06092026_AddUserDungeonKeysActiveUntil", {
		p->execSqlSync(
			"ALTER TABLE user_dungeon_keys ADD COLUMN active_until INTEGER NOT NULL DEFAULT 0;");
	});

	// WHICH TIER was bought.  active_type cannot answer that: DungeonKeyMst's
	// usage_pattern gives tiers 1 and 3 the SAME active_type (1), so the spend
	// count is the only thing that identifies the rung -- and the rung is what
	// says which missions the player just paid to enter.
	//
	// Without it PermitPlace had to permit every parade mission unconditionally,
	// which showed the Super and Mega variants alongside the basic ones you
	// actually bought.
	migrate("07092026_AddUserDungeonKeysActiveTier", {
		p->execSqlSync(
			"ALTER TABLE user_dungeon_keys ADD COLUMN active_tier INTEGER NOT NULL DEFAULT 0;");
	});

	// Grand Quest rewards (CampaignEnd).
	//
	// rewards_got: the once-only Grand Mission rewards (F_GRAND_MISSION_REWARD_MST
	// rows with init_flag 1) this user has earned on a mission, as the comma list
	// the client reads back from CampaignMissionInfo.get_reward (JQ23rIvk) —
	// CampaignRewardScene::isGetRewardCheck splits it on ','.
	migrate("11092026_AddUserCampaignMissionsRewardsGot", {
		p->execSqlSync(
			"ALTER TABLE user_campaign_missions ADD COLUMN rewards_got TEXT NOT NULL DEFAULT '';");
	});

	// The run in progress: the zel/karma totals the client reports in every
	// CampaignBattleEnd archive (wVTBA6b5 — cumulative for the run, so the last
	// one wins), paid out by a clearing CampaignEnd; run_open, which a
	// CampaignEnd closes so a repeated end cannot pay twice; and the Grand Quest
	// item loadout (equipped / reserve lists as JSON), which lives outside the
	// warehouse while it is set.
	migrate("11092026_AddUserCampaignStateRun", {
		p->execSqlSync("ALTER TABLE user_campaign_state ADD COLUMN run_zel INTEGER NOT NULL DEFAULT 0;");
		p->execSqlSync("ALTER TABLE user_campaign_state ADD COLUMN run_karma INTEGER NOT NULL DEFAULT 0;");
		p->execSqlSync("ALTER TABLE user_campaign_state ADD COLUMN run_open INTEGER NOT NULL DEFAULT 0;");
		p->execSqlSync("ALTER TABLE user_campaign_state ADD COLUMN eqp_items TEXT NOT NULL DEFAULT '';");
		p->execSqlSync("ALTER TABLE user_campaign_state ADD COLUMN rsv_items TEXT NOT NULL DEFAULT '';");
	});

	// Unit Selector tickets the user holds, keyed by the selector MST's ticket id
	// (UnitSelectorGachaMst XIvaD6Jp).  They are not V2 summon tickets: a
	// present of type 8005 grants them and UnitSelectorGachaTicket spends them;
	// the client reads the counts from UnitSelectorGachaUserInfo (CGHaOZda).
	// A spent-out row stays at 0 so the zero can still be sent.
	migrate("11092026_CreateUserSelectorTickets", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_selector_tickets ("
			"user_id   TEXT    NOT NULL,"
			"ticket_id TEXT    NOT NULL,"
			"count     INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, ticket_id)"
			");"
		);
	});

	// Frontier Gate rewards.
	//
	// user_frontier_gates.rewards_got: the F_FROGATE_REWARD_MST ids this user
	// has been paid on the gate, as the comma list FrontierGateInfo sends back
	// as reward_info_list (JQ23rIvk) — FrontierGateRewardScene::setList draws
	// the "obtained" mark from exactly that list, so every reward pays once.
	//
	// The run row gains the state each Frontier Gate MissionEnd reports
	// (eIQ79KO2): floors cleared, the running score, the bonus-tally note and
	// the overdrive info.  The client only keeps them between battles if the
	// server hands them back (FrontierBattleInfoResponse is their sole setter),
	// and the end of the run is paid from them — FrontierGateEnd carries none.
	migrate("11092026_AddFrontierGateRewards", {
		p->execSqlSync("ALTER TABLE user_frontier_gates ADD COLUMN rewards_got TEXT NOT NULL DEFAULT '';");
		p->execSqlSync("ALTER TABLE user_frontier_gate_run ADD COLUMN run_score INTEGER NOT NULL DEFAULT 0;");
		p->execSqlSync("ALTER TABLE user_frontier_gate_run ADD COLUMN run_progress INTEGER NOT NULL DEFAULT 0;");
		p->execSqlSync("ALTER TABLE user_frontier_gate_run ADD COLUMN run_note TEXT NOT NULL DEFAULT '';");
		p->execSqlSync("ALTER TABLE user_frontier_gate_run ADD COLUMN run_od_info TEXT NOT NULL DEFAULT '';");
	});

	// Frontier Gate rewards pay the moment the run reaches them, not when it
	// ends: a run abandoned by starting the gate again would otherwise lose
	// everything it earned.  run_rewards is what this run has been paid, so the
	// result screen can still list the whole run.
	//
	// achieve_point is the Merit Point balance — the number the Randall
	// Achievement screen prints (RandallAchievementDedicateScene::setAchievePoint
	// @0x1A39D58 reads UserAchievementInfo +0x18, which is wire key idfCDG70).
	// Frontier Gate pays it at the end of a run.
	migrate("12092026_AddFrontierGateRunRewardsAndMerit", {
		p->execSqlSync("ALTER TABLE user_frontier_gate_run ADD COLUMN run_rewards TEXT NOT NULL DEFAULT '';");
		p->execSqlSync("ALTER TABLE user_info ADD COLUMN achieve_point INTEGER NOT NULL DEFAULT 0;");
	});

	// The quest-prep screen's BONUS item slots (nAligJSQ), the second bar
	// beside the battle items.  Same shape as user_equip_items, including the
	// +100 second run ItemEditRequest::createBody writes.
	migrate("12092026_CreateUserEquipBonusItems", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_equip_bonus_items ("
			"user_id     TEXT    NOT NULL,"
			"disp_order  INTEGER NOT NULL,"
			"item_id     INTEGER NOT NULL,"
			"item_num    INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, disp_order)"
			");"
		);
	});

	// Event tokens — the per-event currencies (token 8 "Rift Token" is the
	// Frontier Gate's, paid by 505 F_FROGATE_REWARD_MST rows as present type
	// 8004).  Token ids stay TEXT because the wire form is a string and the
	// client normalises "0008" to "8" itself (readParam @0x1C492EC).
	migrate("12092026_CreateUserEventTokens", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_event_tokens ("
			"user_id  TEXT    NOT NULL,"
			"token_id TEXT    NOT NULL,"
			"count    INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, token_id)"
			");"
		);
	});

	// Where each Grand Quest deck stands on the map.  The client owns the
	// position while a run is on screen and reports it back in the Yusr3Zg5
	// group of CampaignSave / CampaignBattleEnd / CampaignEnd
	// (createBodySaveDataPos @0x13B4170); CampaignStart sends it back so a
	// resumed run picks up where it stopped instead of at point 0.
	migrate("13092026_CreateUserCampaignDeckPos", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_campaign_deck_pos ("
			"user_id       TEXT    NOT NULL,"
			"deck_num      INTEGER NOT NULL,"
			"now_point_num INTEGER NOT NULL DEFAULT 0,"
			"arrival_order INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, deck_num)"
			");"
		);
	});

	// Randall Achievements.  `reward_received` is the claim latch: an
	// achievement's Merit Points are paid once, by AchievementRewardReceive, and
	// the row exists only once something has happened to it (progress itself is
	// derived from the counters, not stored).
	migrate("13092026_CreateUserAchievementSubjects", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_achievement_subjects ("
			"user_id         TEXT    NOT NULL,"
			"subject_id      TEXT    NOT NULL,"
			"reward_received INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, subject_id)"
			");"
		);
	});

	// Purchases from the Merit Point shop, counted against
	// AchievementTradeMst.limit_count.
	migrate("13092026_CreateUserAchievementTrades", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_achievement_trades ("
			"user_id  TEXT    NOT NULL,"
			"trade_id TEXT    NOT NULL,"
			"count    INTEGER NOT NULL DEFAULT 0,"
			"PRIMARY KEY (user_id, trade_id)"
			");"
		);
	});

	// Dual Brave Burst bonds -- one row per PAIR, keyed by the main unit.
	//
	// Keyed that way because the client is: UserUnitDbbInfoList is a dictionary
	// on the main user_unit_id and builds its own reverse map for the sub, so a
	// mirror row would make each unit claim the other as its partner.  dbb_id
	// is cached on the row rather than rediscovered, because the level list is
	// keyed by it and a unit can be fused away under a stored bond.
	migrate("13092026_CreateUserUnitDbb", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_unit_dbb ("
			"user_id             TEXT    NOT NULL,"
			"user_unit_id        INTEGER NOT NULL,"
			"bonded_user_unit_id INTEGER NOT NULL,"
			"dbb_id              TEXT    NOT NULL,"
			"bond_level          INTEGER NOT NULL DEFAULT 1,"
			"PRIMARY KEY (user_id, user_unit_id)"
			");"
		);
	});

	// Alternate unit art the player has unlocked (Merit shop reward type 15).
	//
	// Kept apart from user_unit_dictionary even though the flag it produces
	// lives on that row: a dictionary row means "this player has SEEN this
	// species", and inserting one to hold an art purchase would put an
	// un-obtained unit in the Unit Guide.  The purchase is recorded here and
	// the flag is derived at read time, so buying art for a unit you do not
	// own yet simply lights up when you get it.
	migrate("13092026_CreateUserUnitAltArt", {
		p->execSqlSync(
			"CREATE TABLE IF NOT EXISTS user_unit_alt_art ("
			"user_id TEXT    NOT NULL,"
			"unit_id INTEGER NOT NULL,"
			"PRIMARY KEY (user_id, unit_id)"
			");"
		);
	});

	// Honor ("Friend Point") lifetime totals -- trophies 100090 and 100100.
	//
	// Trophies.hpp already pointed 100090/100100 at UserTeamArchive::friend_p_get
	// and friend_p_use, but no column fed them, so both trophies read 0 and
	// Achievements.hpp had to leave the 2000 band (Honor Points accumulated)
	// at -1 with a note saying user_info.friend_points is the CURRENT balance
	// and falls when spent.  friend_p_get is that missing lifetime total.
	migrate("13092026_AddUserTeamArchiveHonorCounters", {
		for (const auto* col : {
			"friend_p_get INTEGER NOT NULL DEFAULT 0",
			"friend_p_use INTEGER NOT NULL DEFAULT 0" })
		{
			p->execSqlSync(std::string("ALTER TABLE user_team_archive ADD COLUMN ") + col + ";");
		}
	});

	// Which Summoner Helper the open mission borrowed, so MissionEnd can pay
	// the Honor for it.
	//
	// MissionEnd's request does not carry the helper -- MissionEndRequest::
	// createBody @0x13A78F8 sends 75 keys and h7eY3sAK is not among them -- so
	// the id has to survive from MissionStart, which does carry it (captures
	// show "0" for a solo run and the helper's user id otherwise).  It is
	// durable rather than in-memory on purpose: a battle can outlive the
	// process, and a crashed client still ends its mission on the next launch.
	migrate("13092026_AddUserInfoReinforceUserId", {
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN reinforce_user_id TEXT NOT NULL DEFAULT '';");
	});

	// Hunter Orbs -- the Frontier Gate / Frontier Hunter attempt currency.
	//
	// They are NOT team_info's fight_point (that is the Arena Orbs, per
	// ShopHelFightScene's "Arena Orbs" title).  They live in the client's
	// ChallengeHeaderInfo singleton, written only by the kN2i7qds response,
	// which this server had never sent -- so the count sat at 0 forever and
	// FrontierGateConditionScene::startCheck took its no-orbs branch on every
	// entry.
	//
	// Stored the way energy is: the count, plus the unix time the orb currently
	// regenerating will land.  `rest_ts` 0 means nothing is pending (the count
	// is at the cap, or has never been spent).  Starting the column at the cap
	// rather than 0 so an existing save is not retroactively broke; the cap
	// itself is DefineMst max_frohun_p, read at runtime.
	migrate("13092026_AddUserInfoHunterOrbs", {
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN hunter_orbs INTEGER NOT NULL DEFAULT 3;");
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN hunter_orb_rest_ts INTEGER NOT NULL DEFAULT 0;");
	});

	// An interrupted battle, so the next login can offer to resume it.
	//
	// The battle STATE is the client's -- MissionRestartScene reloads the blob
	// from its own SaveData -- but the trigger is not: LoginScene only reaches
	// the resume screen when the server's 5PR2VmH1 carries a non-zero state.
	// Without somewhere to keep that, a revival paid for in gems is forgotten
	// the moment the client closes.
	//
	// One battle at a time, so these live on user_info rather than a table:
	// MissionInfo is a client singleton and the player cannot be in two.
	// `state` 0 means nothing open, which is also the column's default, so an
	// existing save is untouched until it actually pays for a continue.
	migrate("13092026_AddUserInfoMissionBreak", {
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN mission_break_serial TEXT NOT NULL DEFAULT '';");
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN mission_break_info TEXT NOT NULL DEFAULT '';");
		p->execSqlSync(
			"ALTER TABLE user_info ADD COLUMN mission_break_state INTEGER NOT NULL DEFAULT 0;");
	});

}

/*!
* Gets all the runned migration (and optionally create the table)
* @param p Database pointer
* @param hash Output where to store all the hashes
*/
static void GetMigrationStatus(drogon::orm::DbClientPtr& p, std::vector<std::string>& hash)
{
	p->execSqlSync(
		"CREATE TABLE IF NOT EXISTS migration_status("
		"hash TEXT PRIMARY KEY NOT NULL"
		");"
	);
	auto res = p->execSqlSync(
		"SELECT hash FROM migration_status;"
	);
	if (res.size() > 0)
	{
		for (const auto& x : res)
		{
			const auto& str = x[0].as<std::string>();
			LOG_DEBUG << "Migration found: " << str;
			hash.emplace_back(str);
		}
	}
}

void MigrationManager::RunMigrations(drogon::orm::DbClientPtr ptr)
{
	MigrationMap migrations;
	RegisterMigrations(migrations);

	// Migrations are a vector so they run in declared order (a hash map ran them
	// unordered).  The vector doesn't dedup, so guard the uniqueness the map used
	// to give us: a duplicate name would run twice / mask an intended migration.
	std::vector<std::string> seenNames;
	for (const auto& [name, _] : migrations)
	{
		if (std::find(seenNames.begin(), seenNames.end(), name) != seenNames.end())
		{
			LOG_ERROR << "Duplicate migration name: " << name;
			drogon::app().quit();
			return;
		}
		seenNames.push_back(name);
	}

	std::vector<std::string> runnedMigratons;
	GetMigrationStatus(ptr, runnedMigratons);

	try
	{
		for (const auto& [k, v] : migrations)
		{
			if (std::find(runnedMigratons.begin(), runnedMigratons.end(), k) != runnedMigratons.end())
				continue;

			LOG_INFO << "Execute migration " << k;
			v(ptr);
			ptr->execSqlSync("INSERT INTO migration_status(hash) VALUES ($1);", k);
		}
	}
	catch (drogon::orm::DrogonDbException ex)
	{
		LOG_ERROR << "Cannot execute migration: " << ex.base().what();
		drogon::app().quit();
	}

}

