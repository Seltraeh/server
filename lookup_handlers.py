#!/usr/bin/env python3
"""
lookup_handlers.py  —  Brave Frontier server handler lookup tool

Indexes all request handlers from:
  - exported_handlers vault  (Desktop/BraveFrontier/exported_handlers/)
  - live server handlers     (gimuserver/gme/handlers/)

For each handler it extracts:
  - GroupId (request_id) and AesKey from the .hpp
  - Response types included / Serialize() calls from the .cpp
  - Request keys accessed via req["..."] in the .cpp
  - Whether it is a stub (empty) or implemented

Usage:
    python lookup_handlers.py                   # index both vaults, print summary
    python lookup_handlers.py -k 60subGk3       # find handlers that use a specific key
    python lookup_handlers.py -n UnitMix        # search handler by partial name
    python lookup_handlers.py --dump-json        # write full index to handler_index.json
    python lookup_handlers.py -n UnitMix --show # print raw source of matched handlers
"""

import os
import re
import json
import argparse
from pathlib import Path
from collections import defaultdict

# ── Paths ─────────────────────────────────────────────────────────────────────

VAULT_DIR  = Path("C:/Users/Evan/Desktop/BraveFrontier/exported_handlers")
SERVER_DIR = Path("C:/Users/Evan/BF/BF-WorkingDir/server/gimuserver/gme/handlers")
INDEX_OUT  = Path("C:/Users/Evan/BF/BF-WorkingDir/server/handler_index.json")

# ── Regex patterns ────────────────────────────────────────────────────────────

RE_GROUP_ID  = re.compile(r'GetGroupId\(\).*?return\s+"([^"]+)"')
RE_AES_KEY   = re.compile(r'GetAesKey\(\).*?return\s+"([^"]+)"')
RE_REQ_KEY   = re.compile(r'req\["([^"]+)"\]')
RE_INCLUDE   = re.compile(r'#include\s+"([^"]+\.hpp)"')
RE_SERIALIZE = re.compile(r'(\w+)\.Serialize\(')
RE_RESPONSE  = re.compile(r'Response::(\w+)')
RE_EMPTY     = re.compile(r'Json::Value\s+res\s*;\s*cb\(newGmeOkResponse')

# ── Parser ────────────────────────────────────────────────────────────────────

def parse_hpp(text: str) -> dict:
    gid = RE_GROUP_ID.search(text)
    aes = RE_AES_KEY.search(text)
    return {
        "group_id": gid.group(1) if gid else None,
        "aes_key":  aes.group(1) if aes else None,
    }


def parse_cpp(text: str) -> dict:
    req_keys    = sorted(set(RE_REQ_KEY.findall(text)))
    includes    = RE_INCLUDE.findall(text)
    serializers = sorted(set(RE_SERIALIZE.findall(text)))
    responses   = sorted(set(RE_RESPONSE.findall(text)))

    # Detect stubs: body is just "Json::Value res; cb(newGmeOkResponse(...))"
    # with no real logic in between
    body_lines = [l.strip() for l in text.splitlines()
                  if l.strip() and not l.strip().startswith("//")]
    logic_lines = [l for l in body_lines
                   if not l.startswith("#") and len(l) > 5]
    is_stub = (
        len(req_keys) == 0 and
        len(serializers) == 0 and
        bool(RE_EMPTY.search(text))
    )

    return {
        "req_keys":    req_keys,
        "includes":    includes,
        "serializers": serializers,
        "responses":   responses,
        "is_stub":     is_stub,
        "loc":         sum(1 for l in text.splitlines() if l.strip() and not l.strip().startswith("//")),
    }


def load_handler(hpp_path: Path, cpp_path: Path, source: str) -> dict | None:
    name = hpp_path.stem
    entry = {"name": name, "source": source}
    if hpp_path.exists():
        entry.update(parse_hpp(hpp_path.read_text(encoding="utf-8", errors="replace")))
    if cpp_path.exists():
        entry.update(parse_cpp(cpp_path.read_text(encoding="utf-8", errors="replace")))
        entry["cpp_path"] = str(cpp_path)
    entry.setdefault("hpp_path", str(hpp_path))
    return entry


def scan_directory(directory: Path, source: str) -> list[dict]:
    handlers = []
    seen = set()
    for f in sorted(directory.glob("*.hpp")):
        name = f.stem
        if name in seen:
            continue
        seen.add(name)
        cpp = directory / f"{name}.cpp"
        h = load_handler(f, cpp, source)
        if h:
            handlers.append(h)
    return handlers


def build_index() -> dict:
    vault_handlers  = scan_directory(VAULT_DIR,  "vault")  if VAULT_DIR.exists()  else []
    server_handlers = scan_directory(SERVER_DIR, "server") if SERVER_DIR.exists() else []

    # Merge: server entries override vault entries for the same handler name
    by_name: dict[str, dict] = {}
    for h in vault_handlers:
        by_name[h["name"]] = h
    for h in server_handlers:
        # Mark vault entry as "overridden" if it exists
        if h["name"] in by_name:
            h["vault_stub"] = by_name[h["name"]]
        by_name[h["name"]] = h

    # Build inverted index: key → [handler names]
    key_to_handlers: dict[str, list[str]] = defaultdict(list)
    for h in by_name.values():
        for k in h.get("req_keys", []):
            key_to_handlers[k].append(h["name"])

    return {
        "handlers": list(by_name.values()),
        "key_index": dict(key_to_handlers),
        "stats": {
            "total":       len(by_name),
            "vault_only":  sum(1 for h in by_name.values() if h["source"] == "vault"),
            "server":      sum(1 for h in by_name.values() if h["source"] == "server"),
            "stub":        sum(1 for h in by_name.values() if h.get("is_stub", True)),
            "implemented": sum(1 for h in by_name.values() if not h.get("is_stub", True)),
        },
    }

# ── CLI ───────────────────────────────────────────────────────────────────────

def fmt_handler(h: dict, show_source: bool = False) -> str:
    lines = []
    tag = "[SERVER]" if h["source"] == "server" else "[VAULT] "
    stub = " (stub)" if h.get("is_stub") else ""
    lines.append(f"{tag} {h['name']}{stub}")
    if h.get("group_id"):
        lines.append(f"  GroupId : {h['group_id']}")
    if h.get("aes_key"):
        lines.append(f"  AesKey  : {h['aes_key']}")
    if h.get("req_keys"):
        lines.append(f"  ReqKeys : {', '.join(h['req_keys'])}")
    if h.get("responses"):
        lines.append(f"  Responses: {', '.join(h['responses'])}")
    if h.get("serializers"):
        lines.append(f"  Serialize: {', '.join(h['serializers'])}")
    if show_source and h.get("cpp_path"):
        src = Path(h["cpp_path"]).read_text(encoding="utf-8", errors="replace")
        lines.append("  ── source ──")
        for ln in src.splitlines():
            lines.append(f"  {ln}")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="BF handler lookup tool")
    parser.add_argument("-k", "--key",   help="Find handlers that access this request key")
    parser.add_argument("-n", "--name",  help="Search handlers by partial name (case-insensitive)")
    parser.add_argument("--dump-json",   action="store_true", help="Write full index to handler_index.json")
    parser.add_argument("--show",        action="store_true", help="Print source of matched handlers")
    parser.add_argument("--stubs-only",  action="store_true", help="List only stub handlers")
    parser.add_argument("--implemented", action="store_true", help="List only implemented handlers")
    args = parser.parse_args()

    print("Building handler index...")
    idx = build_index()
    s   = idx["stats"]
    print(f"  Total: {s['total']}  |  Server: {s['server']}  |  Vault-only: {s['vault_only']}"
          f"  |  Implemented: {s['implemented']}  |  Stubs: {s['stub']}\n")

    if args.dump_json:
        # Strip verbose vault_stub to keep file manageable
        slim = {k: v for k, v in idx.items() if k != "handlers"}
        slim["handlers"] = [{k2: v2 for k2, v2 in h.items() if k2 != "vault_stub"}
                            for h in idx["handlers"]]
        INDEX_OUT.write_text(json.dumps(slim, indent=2), encoding="utf-8")
        print(f"Index written to {INDEX_OUT}")
        return

    handlers = idx["handlers"]

    if args.stubs_only:
        handlers = [h for h in handlers if h.get("is_stub")]
    if args.implemented:
        handlers = [h for h in handlers if not h.get("is_stub")]

    if args.key:
        matched = idx["key_index"].get(args.key, [])
        if not matched:
            print(f"No handlers found using key '{args.key}'")
        else:
            print(f"Handlers using key '{args.key}':")
            for name in matched:
                h = next((h for h in handlers if h["name"] == name), None)
                if h:
                    print(fmt_handler(h, args.show))
                    print()
        return

    if args.name:
        q = args.name.lower()
        matched = [h for h in handlers if q in h["name"].lower()]
        if not matched:
            print(f"No handlers matching '{args.name}'")
        else:
            for h in matched:
                print(fmt_handler(h, args.show))
                print()
        return

    # Default: print summary table
    print(f"{'Handler':<55} {'Source':<8} {'GroupId':<12} {'ReqKeys'}")
    print("-" * 110)
    for h in sorted(handlers, key=lambda x: x["name"]):
        stub = "*" if h.get("is_stub") else " "
        gid  = h.get("group_id", "?") or "?"
        keys = ", ".join(h.get("req_keys", [])[:4])
        if len(h.get("req_keys", [])) > 4:
            keys += " ..."
        print(f"{stub} {h['name']:<53} {h['source']:<8} {gid:<12} {keys}")
    print(f"\n* = stub (no implementation)")


if __name__ == "__main__":
    main()
