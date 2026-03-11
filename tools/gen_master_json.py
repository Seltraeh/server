#!/usr/bin/env python3
"""
gen_master_json.py — Generate slim server master data from bravefrontier_data-master.

Run from the repository root:
    python tools/gen_master_json.py

Inputs  (deploy/system/bravefrontier_data-master/):
    info.json     — unit master data
    items.json    — item catalogue
    missions.json — mission/dungeon list

Outputs (deploy/system/):
    unit_master.json    — per-unit stats + skill IDs needed by the server
    item_master.json    — item seed list (id + quantity) for new users
    mission_master.json — all mission IDs (for "all missions cleared" response)
"""

import json
import os
import sys

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
REPO_ROOT  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW_DIR    = os.path.join(REPO_ROOT, "deploy", "system", "bravefrontier_data-master")
OUT_DIR    = os.path.join(REPO_ROOT, "deploy", "system")

MAXWELL_ID = "51147"   # Inception God Maxwell (Rarity 8) — always seeded first

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def load_json(path: str) -> dict:
    print(f"  Loading {path} ...", end="", flush=True)
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    print(f" {len(data):,} entries")
    return data

def save_json(path: str, data) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, separators=(",", ":"))
    size_kb = os.path.getsize(path) / 1024
    print(f"  -> {path}  ({size_kb:.1f} KB, {len(data) if hasattr(data,'__len__') else '?'} top-level keys)")

# ---------------------------------------------------------------------------
# 1. unit_master.json
# ---------------------------------------------------------------------------
def gen_unit_master(info: dict) -> dict:
    """
    Extract per-unit lord-type max stats and skill IDs.
    Maxwell (MAXWELL_ID) is always the first entry so the server seeds him first,
    giving him the lowest AUTOINCREMENT id and making him the deck leader.
    """
    units = {}

    def extract(uid: str, u: dict) -> dict | None:
        lord = u.get("stats", {}).get("_lord", {})
        hp  = lord.get("hp",  0)
        atk = lord.get("atk", 0)
        def_ = lord.get("def", 0)
        rec = lord.get("rec", 0)

        # Skip units with no usable stats (special NPC/placeholder entries)
        if hp == 0 and atk == 0:
            return None

        return {
            "element": u.get("element", "fire"),
            "max_hp":  int(hp),
            "max_atk": int(atk),
            "max_def": int(def_),
            "max_rec": int(rec),
            "ls_id":   int(u.get("leader skill", {}).get("id", 0) or 0),
            "bb_id":   int(u.get("bb",  {}).get("id", 0) or 0),
            "sbb_id":  int(u.get("sbb", {}).get("id", 0) or 0),
            "es_id":   int(u.get("extra skill", {}).get("id", 0) or 0),
        }

    # Maxwell goes first
    if MAXWELL_ID in info:
        entry = extract(MAXWELL_ID, info[MAXWELL_ID])
        if entry:
            units[MAXWELL_ID] = entry
    else:
        print(f"  WARNING: Maxwell ID {MAXWELL_ID} not found in info.json!", file=sys.stderr)

    for uid, u in info.items():
        if uid == MAXWELL_ID:
            continue
        entry = extract(uid, u)
        if entry:
            units[uid] = entry

    print(f"  Units extracted: {len(units):,}  (Maxwell first: {'yes' if MAXWELL_ID in units else 'NO'})")
    return {"units": units}


# ---------------------------------------------------------------------------
# 2. item_master.json
# ---------------------------------------------------------------------------
# Quantity rules:
#   sphere / ls_sphere  → 1  (equip-like, limited makes sense)
#   raid == True        → skip (raid-exclusive drops, not freely available)
#   everything else     → 99
SPHERE_TYPES = {"sphere", "ls_sphere"}
ITEM_QTY_SPHERE = 1
ITEM_QTY_DEFAULT = 99

def gen_item_master(items: dict) -> dict:
    seed = []
    skipped_raid = 0
    for iid, item in items.items():
        if item.get("raid", False):
            skipped_raid += 1
            continue
        qty = ITEM_QTY_SPHERE if item.get("type") in SPHERE_TYPES else ITEM_QTY_DEFAULT
        seed.append({"id": int(iid), "quantity": qty})

    # Sort by id for deterministic output
    seed.sort(key=lambda x: x["id"])
    print(f"  Items in seed: {len(seed):,}  (skipped {skipped_raid} raid-only)")
    return {"seed_items": seed}


# ---------------------------------------------------------------------------
# 3. mission_master.json
# ---------------------------------------------------------------------------
def gen_mission_master(missions: dict) -> dict:
    ids = sorted(int(k) for k in missions.keys() if k.isdigit())
    print(f"  Mission IDs extracted: {len(ids):,}")
    return {"mission_ids": ids}


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    print("=== gen_master_json.py ===\n")

    print("Loading raw data files:")
    info_path     = os.path.join(RAW_DIR, "info.json")
    items_path    = os.path.join(RAW_DIR, "items.json")
    missions_path = os.path.join(RAW_DIR, "missions.json")

    for p in (info_path, items_path, missions_path):
        if not os.path.exists(p):
            print(f"ERROR: {p} not found. Run from the repo root after committing bravefrontier_data-master.", file=sys.stderr)
            sys.exit(1)

    info     = load_json(info_path)
    items    = load_json(items_path)
    missions = load_json(missions_path)

    print("\nGenerating unit_master.json:")
    unit_data = gen_unit_master(info)

    print("\nGenerating item_master.json:")
    item_data = gen_item_master(items)

    print("\nGenerating mission_master.json:")
    mission_data = gen_mission_master(missions)

    print("\nWriting output files:")
    save_json(os.path.join(OUT_DIR, "unit_master.json"),    unit_data)
    save_json(os.path.join(OUT_DIR, "item_master.json"),    item_data)
    save_json(os.path.join(OUT_DIR, "mission_master.json"), mission_data)

    print("\nDone.")


if __name__ == "__main__":
    main()
