"""Mission release-readiness validator.

Cross-checks deploy/archive/mission.json against the MSTs it has to agree
with (mission / dungeon / area / item) and against the catalogues its ids
have to resolve in (unit archive, AI archive), then reports every mismatch
grouped by how badly it breaks the game.

WHY A SEPARATE TOOL.  The generators (gen_missing_missions, gen_event_battles,
gen_vortex_battles) each guard their own output, but nothing checked the whole
corpus at once, and the invariants that matter most are the ones that SPAN
files: an archive record whose wave count disagrees with MissionMst.battle_count
is invisible to either file on its own.  Handbook Rule Zero -- a successful
write is not a correct one.

SEVERITIES
  CRITICAL  the client cannot render it, or the content is silently wrong
  HIGH      visibly wrong in play (counts, rewards, text)
  MEDIUM    polish, or a gap worth knowing about before release

WHY A WIRE MODE.  Half of what breaks on release day is not in any file: it is
the gap between what PermitPlace ADVERTISES and what MissionStart can SERVE.
`--wire` asks a running server for its permit list and checks the tiles the
client is actually offered, which is the only way to see that Frontier Gate,
Grand Quest and the Vortex advertise missions the archive has never held.

Usage:
  python scripts/validate_missions.py                  # full report
  python scripts/validate_missions.py --severity HIGH  # HIGH and above
  python scripts/validate_missions.py --check C2       # one check
  python scripts/validate_missions.py --wire           # + live permit checks
  python scripts/validate_missions.py --json out.json  # machine-readable
"""
from __future__ import annotations

import argparse
import collections
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ARCHIVE = ROOT / "deploy" / "archive"
MST = ROOT / "deploy" / "mst"
CONTENT = ROOT / "deploy" / "game_content" / "content"


def _shipped(folder: pathlib.Path) -> set[str]:
    return {p.name for p in folder.iterdir()} if folder.is_dir() else set()


# What the client can actually draw.  Read once; see C11 for why only the
# sprite counts.
UNIT_SPRITES = _shipped(CONTENT / "unit" / "img")
MONSTER_SPRITES = _shipped(CONTENT / "monster" / "img")

# Missions at or above this id belong to a special mode (Frontier Gate, Grand
# Quest, Vortex, Trials).  Mirrors kSpecialIdFloor in gme/common/PermitPlace.cpp.
SPECIAL_ID_FLOOR = 100000

# The battle every unarchived mission borrows -- see MissionStart's
# BATTLE-CONTENT FALLBACK in gme/handlers/Mission.cpp.
TEMPLATE_MISSION_ID = 10

# Battlefield extent, from the positions the live captures actually carry.
POS_X_MAX, POS_Y_MAX = 640, 480

# TreasureDrop.target_type, from archive/archive.hpp.
TREASURE_TYPES = {1: "zel", 2: "karma", 3: "crystal", 4: "item"}

CJK = re.compile(r"[　-〿぀-ゟ゠-ヿ㐀-䶿一-鿿＀-￯]")

# What the campaign regeneration left behind on every monster it could not
# source real numbers for.  gen_missing_missions.py's docstring says so outright
# ("the regenerated campaign archive carries flat 1000/290/50 stats everywhere"),
# and new missions were deliberately cloned to match so they would not stand out.
PLACEHOLDER_HP, PLACEHOLDER_ATK = 1000, 290

# The permit channels PermitPlace fills, from gme/common/PermitPlace.cpp:20-22.
PERMIT_CHANNELS = ("yXNM8kL3", "Y73tHKS8", "Y73mHKS8")


def load(path: pathlib.Path):
    return json.loads(path.read_text(encoding="utf-8"))


def live_permits(host: str) -> dict[str, list[dict]]:
    """The three permit channels as a running server actually sends them.

    Asks UserInfo (cTZ3W2JG) over the real envelope rather than replaying
    PermitPlace's logic in Python: the five special-mode collectors in
    ServerCache each have their own rules and a reimplementation would drift
    from them silently, which is the failure this check exists to catch.
    """
    import base64
    import gzip
    import sqlite3
    import urllib.request

    from Crypto.Cipher import AES
    from Crypto.Util.Padding import pad, unpad

    db = sqlite3.connect((ROOT / "deploy" / "gme.sqlite").as_uri() + '?mode=ro', uri=True)
    user, gumi = db.execute("SELECT id, gumi_user_id FROM user_info LIMIT 1").fetchone()
    db.close()

    key = "ScJx6ywWEb0A3njT"
    inner = {"IKqx1Cn9": [{"iN7buP2h": gumi, "h7eY3sAK": user}]}
    envelope = {
        "F4q6i9xe": {"Hhgi79M1": "cTZ3W2JG", "aV6cLn3v": "validate_missions"},
        "a3vSYuq2": {"Kn51uR4Y": base64.b64encode(AES.new(key.encode(), AES.MODE_ECB).encrypt(pad(json.dumps(inner).encode(), 16))).decode()},
    }
    request = urllib.request.Request(
        host, data=json.dumps(envelope).encode(),
        headers={"Content-Type": "application/json", "Accept-Encoding": "gzip"})
    with urllib.request.urlopen(request, timeout=60) as response:
        raw = response.read()
        if response.headers.get("Content-Encoding") == "gzip":
            raw = gzip.decompress(raw)
    outer = json.loads(raw)
    payload = outer.get("a3vSYuq2", {}).get("Kn51uR4Y")
    if payload is None:
        raise SystemExit(f"server refused the request: {outer.get('b5PH6mZa')}")
    body = json.loads(unpad(AES.new(key.encode(), AES.MODE_ECB).decrypt(base64.b64decode(payload)), 16))
    return {channel: body.get(channel, []) for channel in PERMIT_CHANNELS}


def mst_rows(name: str) -> list[dict]:
    """The rows of a single-key MST file, whatever its wire key is."""
    doc = load(MST / f"{name}.json")
    return doc[next(iter(doc))]


def as_int(value, default: int = 0) -> int:
    try:
        return int(str(value).strip() or default)
    except (TypeError, ValueError):
        return default


class Report:
    def __init__(self) -> None:
        self.findings: dict[str, dict] = {}

    def check(self, code: str, severity: str, title: str, detail: str = "") -> None:
        self.findings[code] = {
            "severity": severity, "title": title, "detail": detail, "items": [],
        }

    def add(self, code: str, subject, note: str = "") -> None:
        self.findings[code]["items"].append({"subject": subject, "note": note})


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--severity", choices=["CRITICAL", "HIGH", "MEDIUM"], default="MEDIUM")
    ap.add_argument("--check", action="append", help="only this check code (repeatable)")
    ap.add_argument("--json", help="write the full report here")
    ap.add_argument("--limit", type=int, default=15, help="examples printed per check")
    ap.add_argument("--wire", action="store_true",
                    help="also check what a running server advertises")
    ap.add_argument("--host", default="http://127.0.0.1:9960/bf/gme/action.php")
    args = ap.parse_args()

    missions = load(ARCHIVE / "mission.json")
    units = {u["id"]: u for u in load(ARCHIVE / "unit.json")}
    ais = {a["id"] for a in load(ARCHIVE / "ai.json")}

    mission_mst = {as_int(r["j28VNcUW"]): r for r in mst_rows("mission_mst")}
    dungeon_mst = {as_int(r["MHx05sXt"]): r for r in mst_rows("dungeon_mst")}
    area_mst = {as_int(r["VjCY7rX4"]): r for r in mst_rows("area_mst")}
    items = {as_int(r["kixHbe54"]) for r in mst_rows("item_mst")}

    archived = {m["id"] for m in missions}

    r = Report()
    # ---- CRITICAL -------------------------------------------------------
    r.check("C1", "CRITICAL", "Archive record with no MissionMst row",
            "The client resolves a mission through MissionMst; an archive-only "
            "record can never be reached and its id may collide later.")
    r.check("C2", "CRITICAL", "Monster unit_id does not resolve and carries no visuals",
            "MonsterCgsMst is built from the unit archive.  With neither source "
            "the client has no art to load for the enemy.")
    r.check("C3", "CRITICAL", "Monster has visuals but unit_id is not 0",
            "archive.hpp: a monster supplying its own visuals must set unit_id "
            "to 0 so no unit lookup is attempted.")
    r.check("C4", "CRITICAL", "Monster ai_id does not resolve in ai.json",
            "The enemy would have no behaviour table.")
    r.check("C5", "CRITICAL", "Mission has no stages, or a stage has no monsters",
            "An empty wave is an unwinnable or instantly-won battle.")
    r.check("C6", "CRITICAL", "unit_drop_id does not resolve in the unit archive",
            "A successful capture would grant a unit the client cannot draw.")
    r.check("C7", "CRITICAL", "Treasure item id does not resolve in item_mst")
    r.check("C8", "CRITICAL", "Treasure target_type outside 1-4",
            "The client switches on this value; anything else falls through.")
    r.check("C9", "CRITICAL", "Mission's dungeon or area does not resolve",
            "The tile has no parent topology, which crashed Frontier Gate "
            "before its areas were permitted.")
    r.check("C10", "CRITICAL", "need_mission_id prerequisite is dangling or self-referential",
            "Content that can never unlock.")
    r.check("C11", "CRITICAL", "Monster's battle sprite is not on disk",
            "The client downloads unit_anime_<id>.png as the battle loads and "
            "shows a modal -- 'Error occurred during download' -- when it 404s, "
            "which ends the run.  C2 does not catch this: the unit resolves "
            "fine in the archive, it just has no sprite shipped.  Of the five "
            "per-unit files, ONLY the sprite is fatal -- cgg, cgs_idle, cgs_atk "
            "and cgs_move are all missing inside missions that have been "
            "cleared, so the client tolerates those and flagging them would "
            "bury this in 1,272 false positives.")
    r.check("C12", "CRITICAL", "Scripted monster id repeats or collides with a wave monster",
            "MissionArchiver refuses the whole mission: MonsterParty::changeMonster "
            "finds the replacement by monster id alone, so it must be unique.")

    # ---- HIGH -----------------------------------------------------------
    r.check("H1", "HIGH", "MissionMst.battle_count disagrees with the authored wave count",
            "The client draws the wave counter from the MST; the battle comes "
            "from the archive.  A mismatch reads as 'Battle 6/5'.")
    r.check("H2", "HIGH", "Boss stage missing, duplicated, or not last")
    r.check("H3", "HIGH", "Archive energy_cost disagrees with MissionMst.energy_use",
            "The quest screen quotes the MST; MissionStart charges the archive.")
    r.check("H4", "HIGH", "Archive zel/karma/exp disagree with MissionMst",
            "The quest screen quotes the MST; MissionEnd pays the archive.")
    r.check("H5", "HIGH", "Untranslated CJK text in an archive name")
    r.check("H6", "HIGH", "Blank or placeholder mission/monster name")
    r.check("H7", "HIGH", "Two monsters share a position inside one stage",
            "They render stacked on top of each other.")
    r.check("H8", "HIGH", "Malformed or out-of-bounds battlefield position")
    r.check("H9", "HIGH", "Monster with zero hp or zero atk")
    r.check("H10", "HIGH", "Capture drop is inconsistent (id without chance, or chance without id)")
    r.check("H11", "HIGH", "Treasure chest chance and drop table disagree")
    r.check("H12", "HIGH", "Treasure drop has zero weight",
            "A zero-weight row can never be selected.")
    r.check("H13", "HIGH", "act_min/act_max are inconsistent")
    r.check("H14", "HIGH", "Mission drops nothing at all",
            "No capture, no chest, no zel and no karma from any enemy.")
    r.check("H15", "HIGH", "Every enemy carries the placeholder stat block",
            f"hp {PLACEHOLDER_HP} atk {PLACEHOLDER_ATK} on every monster in the "
            "mission, so it has no difficulty curve and plays the same as every "
            "other mission in this state.")

    # ---- MEDIUM ---------------------------------------------------------
    r.check("M1", "MEDIUM", "MissionMst.clear_rewards is malformed or names an unknown id")
    r.check("M2", "MEDIUM", "Two missions share a display order inside one dungeon",
            "Informational.  Verified 2026-09-19 that the three dungeons this "
            "reports (Palette of the Gods, Re:Generations, Brave Summer 2019) "
            "ship every row at order 1 in the ORIGINAL table, so the client "
            "falls back to id order there.  It only means something for a "
            "dungeon whose orders were authored here.")
    r.check("M3", "MEDIUM", "Two dungeons share a map position inside one area",
            "Overlapping tiles on the quest map.")
    r.check("M4", "MEDIUM", "The mission's boss cannot use a skill",
            "No monster carries a `skills` block, so the boss resolves to plain "
            "attacks.  MonsterMst.skill_level is not authorable, but the skill "
            "ids are.")
    r.check("W1", "CRITICAL", "Advertised tile has no archive record",
            "MissionStart's BATTLE-CONTENT FALLBACK serves mission "
            f"{TEMPLATE_MISSION_ID}'s waves relabelled as this one, and charges "
            "no energy for them.  The campaign loop and Trial of the Gods guard "
            "against this; Frontier Gate, Grand Quest and the Vortex do not.")
    r.check("W2", "HIGH", "Two reachable dungeons occupy the same map spot",
            "Their tiles draw on top of one another.")
    r.check("W3", "HIGH", "Reachable topology carries damaged or untranslated text",
            "A run of '?' is Japanese destroyed by a lossy transcode; "
            "tools/translate_mst.py --repair rebuilds it from the client's table.")
    r.check("W4", "MEDIUM", "Archived mission is not advertised to THIS save",
            "Authored content the player cannot reach yet.  The permit list is "
            "progression-dependent, so this only means something when run "
            "against a save that has cleared everything.")

    # =====================================================================
    for m in missions:
        mid = m["id"]
        row = mission_mst.get(mid)
        if row is None:
            r.add("C1", mid, m.get("name", ""))
        else:
            dungeon_id = as_int(row.get("MHx05sXt"))
            area_id = as_int(row.get("VjCY7rX4"))
            if dungeon_id and dungeon_id not in dungeon_mst:
                r.add("C9", mid, f"dungeon {dungeon_id} missing")
            if area_id and area_id not in area_mst:
                r.add("C9", mid, f"area {area_id} missing")

            want = as_int(row.get("69vnphig"))
            have = len(m.get("stages", []))
            if want and want != have:
                r.add("H1", mid, f"MST says {want} battles, archive has {have} stages")

            mst_energy = as_int(row.get("A8DEK5ob"))
            if mst_energy != m.get("energy_cost", 0):
                r.add("H3", mid, f"MST {mst_energy} vs archive {m.get('energy_cost')}")

            for field, key in (("zel", "Rs7bCE3t"), ("karma", "HTVh8a65"), ("exp", "d96tuT2E")):
                mst_value = as_int(row.get(key))
                if mst_value != m.get(field, 0):
                    r.add("H4", mid, f"{field}: MST {mst_value} vs archive {m.get(field)}")

            for need in str(row.get("HSRhkf70") or "").replace(":", ",").split(","):
                need_id = as_int(need)
                if need_id == 0:
                    continue
                if need_id == mid:
                    r.add("C10", mid, "requires itself")
                elif need_id not in mission_mst:
                    r.add("C10", mid, f"requires mission {need_id}, which has no MST row")

            rewards = str(row.get("SiYs27Cj") or "").strip()
            if rewards:
                for entry in rewards.split(","):
                    parts = entry.split(":")
                    if len(parts) < 3:
                        r.add("M1", mid, f"malformed reward {entry!r}")
                        continue
                    kind, target, amount = as_int(parts[0]), as_int(parts[1]), as_int(parts[2])
                    if kind == 6 and target and target not in units:
                        r.add("M1", mid, f"unit reward {target} not in the unit archive")
                    elif kind in (4, 5, 7) and target and target not in items:
                        r.add("M1", mid, f"item reward {target} not in item_mst")
                    elif amount <= 0:
                        r.add("M1", mid, f"reward {entry!r} grants nothing")

        name = (m.get("name") or "").strip()
        if not name:
            r.add("H6", mid, "mission name is blank")
        elif CJK.search(name):
            r.add("H5", mid, f"name {name!r}")

        stages = m.get("stages", [])
        if not stages:
            r.add("C5", mid, "no stages")
            continue

        # Randall's Battle Simulator (GameUtils::isSandbag @0x1EC18D8 is
        # DungeonMst.dungeon_type == 6).  Its six practice dummies never act
        # (act 0..0), carry no ATK, drop nothing and are no boss -- the wiki's
        # "enemies will not attack back" -- so the checks that would call
        # that a defect do not apply.  See scripts/gen_battle_simulator.py.
        sandbag = as_int(dungeon_mst.get(as_int((row or {}).get("MHx05sXt")), {}).get("3hPeI1RV")) == 6

        bosses = [i for i, s in enumerate(stages) if s.get("is_boss")]
        if not bosses:
            if not sandbag:
                r.add("H2", mid, "no boss stage")
        elif len(bosses) > 1:
            r.add("H2", mid, f"{len(bosses)} boss stages")
        elif bosses[0] != len(stages) - 1:
            r.add("H2", mid, f"boss is stage {bosses[0] + 1} of {len(stages)}")

        drops_something = False
        boss_has_skill = False
        stat_blocks: set[tuple[int, int]] = set()

        for index, stage in enumerate(stages, start=1):
            monsters = stage.get("battle_monsters", [])
            if not monsters:
                r.add("C5", mid, f"stage {index} has no monsters")
                continue

            seen_positions: dict[str, int] = {}
            for order, mon in enumerate(monsters):
                where = f"{mid} stage {index} slot {order}"

                if mon.get("visuals"):
                    if mon.get("unit_id", 0) != 0:
                        r.add("C3", where, f"unit_id {mon['unit_id']} with visuals present")
                    art = (mon["visuals"].get("img_a") or "").strip()
                    if art and art not in MONSTER_SPRITES:
                        r.add("C11", where, f"monster/img/{art} is not on disk")
                elif mon.get("unit_id") not in units:
                    r.add("C2", where, f"unit_id {mon.get('unit_id')} unresolved")
                elif f"unit_anime_{mon['unit_id']}.png" not in UNIT_SPRITES:
                    r.add("C11", where,
                          f"unit/img/unit_anime_{mon['unit_id']}.png is not on disk "
                          f"({mon.get('name')})")

                if mon.get("ai_id") not in ais:
                    r.add("C4", where, f"ai_id {mon.get('ai_id')}")

                mon_name = (mon.get("name") or "").strip()
                if not mon_name:
                    r.add("H6", where, "monster name is blank")
                elif CJK.search(mon_name):
                    r.add("H5", where, f"monster name {mon_name!r}")

                position = str(mon.get("position", ""))
                if not re.fullmatch(r"-?\d+:-?\d+", position):
                    r.add("H8", where, f"position {position!r}")
                else:
                    x, y = (int(v) for v in position.split(":"))
                    if not (0 <= x <= POS_X_MAX and 0 <= y <= POS_Y_MAX):
                        r.add("H8", where, f"position {position} outside {POS_X_MAX}x{POS_Y_MAX}")
                    if position in seen_positions:
                        r.add("H7", where, f"position {position} also used by slot {seen_positions[position]}")
                    seen_positions[position] = order

                if mon.get("hp", 0) <= 0 or (mon.get("atk", 0) <= 0 and not sandbag):
                    r.add("H9", where, f"hp {mon.get('hp')} atk {mon.get('atk')}")

                act_min, act_max = mon.get("act_min", 0), mon.get("act_max", 0)
                # A dummy never acting is exactly 0..0 (MonsterUnit::initTurnChild
                # @0x1159098 sums rolls over [min, max)); anything else is still
                # inconsistent, sandbag or not.
                if sandbag and act_min == act_max == 0:
                    pass
                elif act_max <= 0 or act_min <= 0 or act_min > act_max:
                    r.add("H13", where, f"act {act_min}..{act_max}")

                drop_id, drop_chance = mon.get("unit_drop_id", 0), mon.get("unit_drop_chance", 0)
                if drop_id and drop_id not in units:
                    r.add("C6", where, f"unit_drop_id {drop_id}")
                if bool(drop_id) != bool(drop_chance):
                    r.add("H10", where, f"drop id {drop_id} chance {drop_chance}")
                if not 0 <= drop_chance <= 100:
                    r.add("H10", where, f"chance {drop_chance} outside 0-100")
                if drop_id and drop_chance:
                    drops_something = True

                chest_chance = mon.get("treasure_chest_chance", 0)
                drops = mon.get("treasure_drops", []) or []
                if chest_chance and not drops:
                    r.add("H11", where, f"chest chance {chest_chance} with an empty table")
                if drops and not chest_chance:
                    r.add("H11", where, f"{len(drops)} treasure rows but chest chance is 0")
                if chest_chance and drops:
                    drops_something = True
                for drop in drops:
                    kind = drop.get("target_type")
                    if kind not in TREASURE_TYPES:
                        r.add("C8", where, f"target_type {kind}")
                    elif kind == 4:
                        target = as_int(drop.get("target_id"))
                        if target not in items:
                            r.add("C7", where, f"item {drop.get('target_id')!r}")
                    if not drop.get("weight", 0):
                        r.add("H12", where, f"weight 0 on {drop!r}")

                if mon.get("zel_max_drop", 0) or mon.get("karma_max_drop", 0):
                    drops_something = True
                if stage.get("is_boss") and mon.get("skills"):
                    boss_has_skill = True
                stat_blocks.add((mon.get("hp", 0), mon.get("atk", 0)))

        # Monsters the client's own mission script swaps in (archive
        # script_monsters).  Same art/AI/stat rules as a wave monster; the
        # server refuses the mission outright if an id repeats or collides
        # with a wave monster, because MonsterParty::changeMonster finds the
        # replacement by id alone.
        wave_ids = {mon.get("id") for st in stages for mon in st.get("battle_monsters", [])}
        scripted_seen: set = set()
        for order, mon in enumerate(m.get("script_monsters") or []):
            where = f"{mid} scripted {order}"
            if mon.get("id") in wave_ids or mon.get("id") in scripted_seen or not mon.get("id"):
                r.add("C12", where, f"monster id {mon.get('id')} repeats or collides with a wave monster")
            scripted_seen.add(mon.get("id"))
            if mon.get("visuals"):
                if mon.get("unit_id", 0) != 0:
                    r.add("C3", where, f"unit_id {mon['unit_id']} with visuals present")
                art = (mon["visuals"].get("img_a") or "").strip()
                if art and art not in MONSTER_SPRITES:
                    r.add("C11", where, f"monster/img/{art} is not on disk")
            elif mon.get("unit_id") not in units:
                r.add("C2", where, f"unit_id {mon.get('unit_id')} unresolved")
            elif f"unit_anime_{mon['unit_id']}.png" not in UNIT_SPRITES:
                r.add("C11", where, f"unit/img/unit_anime_{mon['unit_id']}.png is not on disk")
            if mon.get("ai_id") not in ais:
                r.add("C4", where, f"ai_id {mon.get('ai_id')}")
            if mon.get("hp", 0) <= 0 or mon.get("atk", 0) <= 0:
                r.add("H9", where, f"hp {mon.get('hp')} atk {mon.get('atk')}")
            act_min, act_max = mon.get("act_min", 0), mon.get("act_max", 0)
            if act_max <= 0 or act_min <= 0 or act_min > act_max:
                r.add("H13", where, f"act {act_min}..{act_max}")
            if mon.get("skills"):
                boss_has_skill = True

        # The Summoners' Research Lab (dungeon types 2 and 8) pays through its
        # first-clear reward and nothing else -- "Subsequent victories won't
        # award anything" -- so a lab battle that drops nothing is correct.
        lab = as_int(dungeon_mst.get(as_int((row or {}).get("MHx05sXt")), {}).get("3hPeI1RV")) in (2, 8)
        if not drops_something and not lab and not sandbag:
            r.add("H14", mid, m.get("name", ""))
        if not boss_has_skill and not sandbag:
            r.add("M4", mid, m.get("name", ""))
        if stat_blocks == {(PLACEHOLDER_HP, PLACEHOLDER_ATK)}:
            row = mission_mst.get(mid, {})
            r.add("H15", mid,
                  f"{m.get('name', '')!r} (land {row.get('9C64Qwe0')}, "
                  f"dungeon {row.get('MHx05sXt')})")

    # ---- corpus-wide -----------------------------------------------------
    by_dungeon: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for mid in archived:
        row = mission_mst.get(mid)
        if row:
            by_dungeon[as_int(row.get("MHx05sXt"))].append((as_int(row.get("XuJL4pc5")), mid))
    for dungeon_id, entries in by_dungeon.items():
        seen: dict[int, int] = {}
        for order, mid in sorted(entries):
            if order in seen:
                r.add("M2", dungeon_id, f"missions {seen[order]} and {mid} both at order {order}")
            seen[order] = mid

    by_area: dict[int, dict[str, int]] = collections.defaultdict(dict)
    for dungeon_id, row in dungeon_mst.items():
        area_id = as_int(row.get("VjCY7rX4"))
        spot = f"{row.get('SnNtTh51')},{row.get('M6C1aXfR')}"
        if spot in by_area[area_id]:
            r.add("M3", area_id, f"dungeons {by_area[area_id][spot]} and {dungeon_id} both at {spot}")
        by_area[area_id][spot] = dungeon_id

    if args.wire:
        permits = live_permits(args.host)
        advertised = {kind: {as_int(row[key]) for rows in permits.values()
                             for row in rows if key in row}
                      for kind, key in (("mission", "j28VNcUW"),
                                        ("dungeon", "MHx05sXt"),
                                        ("area", "VjCY7rX4"))}
        print(f"live permits: {len(advertised['mission'])} mission tile(s), "
              f"{len(advertised['dungeon'])} dungeon(s), "
              f"{len(advertised['area'])} area(s)\n")

        for mid in sorted(advertised["mission"] - archived):
            row = mission_mst.get(mid, {})
            r.add("W1", mid, f"{row.get('0iAIR2LP', '?')!r} "
                             f"(land {row.get('9C64Qwe0')}, dungeon {row.get('MHx05sXt')})")

        spots: dict[int, dict[str, int]] = collections.defaultdict(dict)
        for dungeon_id in sorted(advertised["dungeon"]):
            row = dungeon_mst.get(dungeon_id)
            if not row:
                continue
            spot = f"{row.get('SnNtTh51')},{row.get('M6C1aXfR')}"
            area_id = as_int(row.get("VjCY7rX4"))
            if spot in spots[area_id]:
                r.add("W2", area_id,
                      f"dungeon {dungeon_id} {row.get('bWsLFP96')!r} sits on "
                      f"{spots[area_id][spot]} at {spot}")
            else:
                spots[area_id][spot] = dungeon_id

        damaged = re.compile(r"\?{2,}|�")
        for table, ids, fields in (
                (area_mst, advertised["area"], (("V84mzqoX", "name"), ("qp37xTDh", "desc"))),
                (dungeon_mst, advertised["dungeon"], (("bWsLFP96", "name"), ("qp37xTDh", "desc"))),
                (mission_mst, advertised["mission"], (("0iAIR2LP", "name"), ("qp37xTDh", "desc")))):
            for row_id in sorted(ids):
                row = table.get(row_id)
                if not row:
                    continue
                for field, label in fields:
                    value = row.get(field)
                    if isinstance(value, str) and (damaged.search(value) or CJK.search(value)):
                        r.add("W3", row_id, f"{label}: {value[:40]!r}")

        for mid in sorted(archived - advertised["mission"]):
            row = mission_mst.get(mid, {})
            r.add("W4", mid, f"{row.get('0iAIR2LP', '?')!r} "
                             f"(dungeon {row.get('MHx05sXt')})")

    # ---- output ----------------------------------------------------------
    order = {"CRITICAL": 0, "HIGH": 1, "MEDIUM": 2}
    floor = order[args.severity]
    codes = set(args.check) if args.check else None

    total = 0
    print(f"mission archive: {len(missions)} records, {len(mission_mst)} MissionMst rows\n")
    for code, finding in r.findings.items():
        if codes and code not in codes:
            continue
        if order[finding["severity"]] > floor:
            continue
        items = finding["items"]
        total += len(items)
        mark = "FAIL" if items else "ok  "
        print(f"[{mark}] {finding['severity']:8} {code:4} {finding['title']}  ({len(items)})")
        if items and finding["detail"]:
            for line in finding["detail"].split(". "):
                if line.strip():
                    print(f"           {line.strip().rstrip('.')}.")
        for item in items[: args.limit]:
            print(f"           - {item['subject']}: {item['note']}")
        if len(items) > args.limit:
            print(f"           ... and {len(items) - args.limit} more")
        if items:
            print()

    print(f"\n{total} finding(s) at {args.severity} and above.")

    if args.json:
        pathlib.Path(args.json).write_text(
            json.dumps(r.findings, indent=1, ensure_ascii=False), encoding="utf-8")
        print(f"full report written to {args.json}")

    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
