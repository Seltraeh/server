# Diff inventory against mine/main — September 30, 2026

Fetched `mine/main`: `7851dba8ebcc97b4d4793aae358b3d18b47a6ad2`. Current HEAD: `7851dba8ebcc97b4d4793aae358b3d18b47a6ad2`. All changes below are working-tree changes; there are no additional parent commits over this baseline.

This is the combined Claude/Codex diff. There is no committed Session 1/2 boundary, so exact authorship of individual lines cannot be reconstructed. The September 29 QC report identifies inherited work; new session areas are summarized in the September 30 review. This inventory includes new files ordinary `git diff` omits. Backups and staging are listed separately, not treated as authored fixes.

## Modified tracked parent paths (33)

- `deploy/archive/brave_slots.json`
- `deploy/archive/mission.json`
- `deploy/mst/user_level_mst.json`
- `deploy/system/brave_slots.json`
- `gimuserver/db/MigrationManager.cpp`
- `gimuserver/db/PacketInterfaceSchemas.hpp`
- `gimuserver/gme/common/Common.hpp`
- `gimuserver/gme/common/Energy.cpp`
- `gimuserver/gme/common/FeatureGates.hpp`
- `gimuserver/gme/common/Friends.cpp`
- `gimuserver/gme/common/Friends.hpp`
- `gimuserver/gme/common/PermitPlace.cpp`
- `gimuserver/gme/common/PermitPlace.hpp`
- `gimuserver/gme/handlers/AchievementAction.cpp`
- `gimuserver/gme/handlers/AchievementDeliver.cpp`
- `gimuserver/gme/handlers/CampaignDeckGet.cpp`
- `gimuserver/gme/handlers/CampaignEdit.cpp`
- `gimuserver/gme/handlers/DeckEdit.cpp`
- `gimuserver/gme/handlers/DungeonEventUpdate.cpp`
- `gimuserver/gme/handlers/ItemEdit.cpp`
- `gimuserver/gme/handlers/ItemFavorite.cpp`
- `gimuserver/gme/handlers/ItemSell.cpp`
- `gimuserver/gme/handlers/ItemSphereEqp.cpp`
- `gimuserver/gme/handlers/Mission.cpp`
- `gimuserver/gme/handlers/MissionBreak.cpp`
- `gimuserver/gme/handlers/RewardsMenu.cpp`
- `gimuserver/gme/handlers/UnitBondBoost.cpp`
- `gimuserver/gme/handlers/UnitEvo.cpp`
- `gimuserver/gme/handlers/UnitMix.cpp`
- `gimuserver/gme/handlers/UnitSell.cpp`
- `packet-generator`
- `scripts/validate_missions.py`
- `standalone_frontend/DebugCli.cpp`

## New relevant files (untracked) (29)

- `docs/BUGFIX_HANDOFF_2026-09-29.md`
- `docs/BUGFIX_REMAINING_AND_CLIENT_QA_2026-09-29.md`
- `gimuserver/gme/common/BattleItems.hpp`
- `gimuserver/gme/common/GameClock.hpp`
- `gimuserver/gme/common/MissionRuns.hpp`
- `gimuserver/gme/common/Warehouse.cpp`
- `scripts/bf_testkit.py`
- `scripts/gen_battle_simulator.py`
- `scripts/gen_brave_slots_table.py`
- `scripts/gen_metal_parades.py`
- `scripts/inventory_bosses.py`
- `scripts/inventory_trials.py`
- `scripts/run_bugfix_regressions.py`
- `scripts/test_battle_simulator_wire.py`
- `scripts/test_brave_slots_wire.py`
- `scripts/test_bugfix_qc_wire.py`
- `scripts/test_enhancement_sp_wire.py`
- `scripts/test_evolution_qc_wire.py`
- `scripts/test_leader_exp_boost_wire.py`
- `scripts/test_lizeria_qc_wire.py`
- `scripts/test_metal_parades_wire.py`
- `scripts/test_mission_run_edges_wire.py`
- `scripts/test_mission_settlement_wire.py`
- `scripts/test_sphere_qc_wire.py`
- `scripts/test_story_gates_wire.py`
- `scripts/test_summon_reveal_wire.py`
- `scripts/test_unit_text_data.py`
- `scripts/test_warehouse_integration_wire.py`
- `scripts/test_warehouse_placeholder_wire.py`

## Modified packet-generator paths (11)

- `assets/mst/unit.kdl`
- `assets/mst/unit_evo.kdl`
- `assets/mst/unit_ext.kdl`
- `assets/net/achievement.kdl`
- `assets/net/dbb.kdl`
- `assets/net/event.kdl`
- `assets/net/handlers.kdl`
- `assets/net/mission.kdl`
- `assets/net/present.kdl`
- `assets/net/unit.kdl`
- `assets/net/user.kdl`

## Untracked backups/staging excluded from gameplay review (72)

- `deploy/archive/daily_login.json.bak-pre-client-table`
- `deploy/archive/mission.json.bak-pre-bossflag`
- `deploy/archive/mission.json.bak-pre-events`
- `deploy/archive/mission.json.bak-pre-events-20260919`
- `deploy/archive/mission.json.bak-pre-lore`
- `deploy/archive/mission.json.bak-pre-missing`
- `deploy/archive/mission.json.bak-pre-mistral`
- `deploy/archive/mission.json.bak-pre-positions`
- `deploy/archive/mission.json.bak-pre-vortex`
- `deploy/archive/unit.json.bak-pre-sbb`
- `deploy/archive/unit.json.bak-pre-sgtext`
- `deploy/gme.sqlite.bak`
- `deploy/mst/area_mst.json.bak-pre-translate`
- `deploy/mst/arena_rank_mst.json.bak-pre-translate`
- `deploy/mst/challenge_mst.json.bak-pre-translate`
- `deploy/mst/challenge_mvp_mst.json.bak-pre-translate`
- `deploy/mst/challenge_rank_reward_mst.json.bak-pre-translate`
- `deploy/mst/chlng_mission_item_set_mst.json.bak-pre-translate`
- `deploy/mst/chlng_mission_mst.json.bak-pre-translate`
- `deploy/mst/colosseum_class_mst.json.bak-pre-translate`
- `deploy/mst/colosseum_formation_mst.json.bak-pre-translate`
- `deploy/mst/colosseum_support_mst.json.bak-pre-translate`
- `deploy/mst/daily_task_mst.json.bak-pre-sixtasks`
- `deploy/mst/daily_task_prize_mst.json.bak-pre-translate`
- `deploy/mst/dungeon_mst.json.bak-pre-repair`
- `deploy/mst/dungeon_mst.json.bak-pre-translate`
- `deploy/mst/extra_passive_skill_mst.json.bak-pre-translate`
- `deploy/mst/frontier_gate_mst.json.bak-pre-translate`
- `deploy/mst/frontier_gate_support_mst.json.bak-pre-translate`
- `deploy/mst/gacha_info_mst.json.bak-pre-translate`
- `deploy/mst/gacha_mst.json.bak-pre-translate`
- `deploy/mst/gate_mst.json.bak-pre-translate`
- `deploy/mst/gift_item_mst.json.bak`
- `deploy/mst/grand_mission_event_mst.json.bak-pre-translate`
- `deploy/mst/grand_mission_flg_mst.json.bak-pre-translate`
- `deploy/mst/grand_mission_map_mst.json.bak-pre-translate`
- `deploy/mst/grand_mission_mst.json.bak-pre-translate`
- `deploy/mst/grand_mission_reward_mst.json.bak-pre-translate`
- `deploy/mst/grand_mission_spot_mst.json.bak-pre-translate`
- `deploy/mst/help_detail_mst.json.bak-pre-translate`
- `deploy/mst/help_mst.json.bak-pre-translate`
- `deploy/mst/help_sub_mst.json.bak-pre-translate`
- `deploy/mst/item_mst.json.bak-pre-translate`
- `deploy/mst/leader_skill_mst.json.bak-pre-translate`
- `deploy/mst/mission_mst.json.bak-pre-parade-exp`
- `deploy/mst/mission_mst.json.bak-pre-translate`
- `deploy/mst/npc_mst.json.bak-pre-translate`
- `deploy/mst/raid_mission_mst.json.bak-pre-translate`
- `deploy/mst/raid_point_mst.json.bak-pre-translate`
- `deploy/mst/scenario_mst.json.bak-pre-translate`
- `deploy/mst/skill_level_mst.json.bak-pre-translate`
- `deploy/mst/skill_mst.json.bak-pre-repair`
- `deploy/mst/skill_mst.json.bak-pre-translate`
- `deploy/mst/sound_mst.json.bak-pre-translate`
- `deploy/mst/summoner_ability_level_mst.json.bak-pre-translate`
- `deploy/mst/summoner_ability_up_mst.json.bak-pre-translate`
- `deploy/mst/summoner_arm_mst.json.bak-pre-translate`
- `deploy/mst/summoner_element_level_mst.json.bak-pre-translate`
- `deploy/mst/summoner_ex_skill_mst.json.bak-pre-translate`
- `deploy/mst/summoner_image_mst.json.bak-pre-translate`
- `deploy/mst/town_facility_mst.json.bak-pre-translate`
- `deploy/mst/town_location_lv_mst.json.bak-pre-translate`
- `deploy/mst/town_location_mst.json.bak-pre-translate`
- `deploy/mst/trophy_group_mst.json.bak-pre-translate`
- `deploy/mst/trophy_mst.json.bak-pre-translate`
- `deploy/mst/unit_comment_mst.json.bak-pre-translate`
- `deploy/mst/unit_fe_category_mst.json.bak-pre-translate`
- `deploy/mst/unit_fe_skill_mst.json.bak-pre-translate`
- `deploy/mst/unit_mst.json.bak-pre-translate`
- `deploy/mst/unit_type_mst.json.bak-pre-translate`
- `deploy/mst_download_staging/F_FIRST_DESC_MST/Ver1_U1XChi09.dat`
- `deploy/mst_download_staging/F_FIRST_DESC_MST/manifest.json`

## Reproduce the comparison

```powershell
git fetch mine main
git diff mine/main --stat
git diff mine/main
git ls-files --others --exclude-standard
git -C packet-generator diff
```

Source SHA-256 inventory: `out/qc_2026-09-30/inventory.json`. The new independent edge-case test is QC work from this review, not Claude Session 2. This review writes additional Markdown deliverables after the inventory snapshot.
