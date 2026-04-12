#!/usr/bin/env python3
"""
generate_handlers.py

Generates C++ handler stubs for the Brave Frontier game server based on
APK binary analysis data (handlers_export.json + typed_responses/*.hpp).

Steps:
  1. Copy all typed_responses/*.hpp -> gimuserver/gme/response/  (always overwrite)
  2. Generate handler .hpp + .cpp for each unimplemented handler in handlers_export.json
     - If a matching {Base}Response.hpp exists in typed_responses, use it in the .cpp
     - Otherwise generate an empty-response stub
  3. GmeController_Handlers.cpp is NOT modified — handlers are enabled manually

Usage:
    python generate_handlers.py [--dry-run] [--force]

Options:
    --dry-run   Print what would be done without writing any files
    --force     Overwrite existing handler .hpp/.cpp files (response files are
                always overwritten regardless of this flag)
"""

import json
import os
import shutil
import argparse
import sys
from pathlib import Path

# ── Configuration ─────────────────────────────────────────────────────────────

HANDLERS_EXPORT_JSON = Path("C:/Users/Evan/Desktop/BraveFrontier/handlers_export.json")
TYPED_RESPONSES_DIR  = Path("C:/Users/Evan/Desktop/BraveFrontier/typed_responses")
SERVER_ROOT          = Path("C:/Users/Evan/BF/BF-WorkingDir/server/gimuserver")
HANDLERS_OUT_DIR     = SERVER_ROOT / "gme" / "handlers"
RESPONSE_OUT_DIR     = SERVER_ROOT / "gme" / "response"

# Handlers that already have hand-written implementations — skip .hpp/.cpp generation.
EXISTING_HANDLERS = {
    "BadgeInfoRequestHandler",
    "DeckEditRequestHandler",
    "FriendGetRequestHandler",
    "GachaActionRequestHandler",
    "GachaListRequestHandler",
    "GetUserInfoRequestHandler",
    "InitializeRequest2Handler",
    "MissionStartRequestHandler",
    "UnitFavoriteRequestHandler",
    "ControlCenterEnterRequestHandler",
    "UpdateInfoLightRequestHandler",
    "HomeInfoRequestHandler",
    "ChallengeArenaResetInfoRequestHandler",
}

# ── Templates ─────────────────────────────────────────────────────────────────

HPP_TEMPLATE = """\
#pragma once

#include "../GmeHandler.hpp"

HANDLER_NS_BEGIN
class {class_name} : public HandlerBase
{{
public:
\tconst char* GetGroupId() const override {{ return "{request_id}"; }}
\tconst char* GetAesKey() const override {{ return "{encode_key}"; }}

\tvoid Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const override;
}};
HANDLER_NS_END
"""

CPP_TEMPLATE_TYPED = """\
#include "{class_name}.hpp"
#include "gme/response/{response_file}"

void Handler::{class_name}::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{{
\tJson::Value res;
\tResponse::{response_struct} resp;
\tresp.Serialize(res);
\tcb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}}
"""

CPP_TEMPLATE_EMPTY = """\
#include "{class_name}.hpp"

void Handler::{class_name}::Handle(UserInfo& user, DrogonCallback cb, const Json::Value& req) const
{{
\tJson::Value res;
\tcb(newGmeOkResponse(GetGroupId(), GetAesKey(), res));
}}
"""

# ── Helpers ───────────────────────────────────────────────────────────────────

def get_response_base(handler_name: str) -> str:
    """
    Derive the base name used to look up a typed response file.
    Strip 'RequestHandler' if present; otherwise strip just 'Handler'.

    Examples:
        ChallengeArenaShopInfoRequestHandler -> ChallengeArenaShopInfo
        GRGuardianDetailHandler              -> GRGuardianDetail
        GachaActionRequest_SGHandler         -> GachaActionRequest_SG
    """
    if handler_name.endswith("RequestHandler"):
        return handler_name[: -len("RequestHandler")]
    if handler_name.endswith("Handler"):
        return handler_name[: -len("Handler")]
    return handler_name


def find_typed_response(handler_name: str, typed_response_files: set) -> str | None:
    """
    Return the response filename (e.g. 'ChallengeArenaShopInfoResponse.hpp')
    if it exists in typed_responses, else None.
    """
    base = get_response_base(handler_name)
    candidate = base + "Response.hpp"
    return candidate if candidate in typed_response_files else None


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Generate BF handler stubs.")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print actions without writing files.")
    parser.add_argument("--force", action="store_true",
                        help="Overwrite existing handler .hpp/.cpp files.")
    args = parser.parse_args()

    dry_run = args.dry_run
    force   = args.force

    # ── Load data ──────────────────────────────────────────────────────────────
    with open(HANDLERS_EXPORT_JSON, encoding="utf-8") as f:
        handlers = json.load(f)

    typed_response_files = set(os.listdir(TYPED_RESPONSES_DIR))

    print(f"Handlers in export JSON : {len(handlers)}")
    print(f"Typed response files    : {len(typed_response_files)}")
    print(f"Existing handlers (skip): {len(EXISTING_HANDLERS)}")
    print()

    # ── Validate handler names ─────────────────────────────────────────────────
    bad = [h["name"] for h in handlers if not h["name"].endswith("Handler")]
    if bad:
        print(f"WARNING: {len(bad)} handler names do not end in 'Handler': {bad}",
              file=sys.stderr)

    # ── Step 1: Copy typed_responses -> gme/response/ (always overwrite) ──────
    print("=== Step 1: Copying typed responses ===")
    copied = 0
    for fname in sorted(typed_response_files):
        src = TYPED_RESPONSES_DIR / fname
        dst = RESPONSE_OUT_DIR / fname
        if dry_run:
            print(f"  [DRY-RUN] COPY {fname}")
        else:
            shutil.copy2(src, dst)
        copied += 1
    print(f"  {'Would copy' if dry_run else 'Copied'} {copied} response file(s) to gme/response/\n")

    # ── Step 2: Generate handler stubs ────────────────────────────────────────
    print("=== Step 2: Generating handler stubs ===")
    gen_typed   = 0
    gen_empty   = 0
    skipped_existing  = 0
    skipped_exists    = 0

    for h in handlers:
        name       = h["name"]
        request_id = h["request_id"]
        encode_key = h["encode_key"]

        # Skip hand-implemented handlers
        if name in EXISTING_HANDLERS:
            skipped_existing += 1
            continue

        hpp_path = HANDLERS_OUT_DIR / f"{name}.hpp"
        cpp_path = HANDLERS_OUT_DIR / f"{name}.cpp"

        # Skip if already generated (unless --force)
        if not force and (hpp_path.exists() or cpp_path.exists()):
            skipped_exists += 1
            continue

        hpp_content = HPP_TEMPLATE.format(
            class_name=name,
            request_id=request_id,
            encode_key=encode_key,
        )

        resp_file = find_typed_response(name, typed_response_files)
        if resp_file:
            response_struct = resp_file.replace(".hpp", "")
            cpp_content = CPP_TEMPLATE_TYPED.format(
                class_name=name,
                response_file=resp_file,
                response_struct=response_struct,
            )
            gen_typed += 1
        else:
            cpp_content = CPP_TEMPLATE_EMPTY.format(class_name=name)
            gen_empty += 1

        if dry_run:
            resp_note = f"  [uses {resp_file}]" if resp_file else "  [empty]"
            print(f"  [DRY-RUN] WRITE {name}.hpp + .cpp{resp_note}")
        else:
            hpp_path.write_text(hpp_content, encoding="utf-8")
            cpp_path.write_text(cpp_content, encoding="utf-8")

    print()
    print("=== Summary ===")
    print(f"  Response files copied         : {copied}")
    print(f"  Handlers generated (typed)    : {gen_typed}")
    print(f"  Handlers generated (empty)    : {gen_empty}")
    print(f"  Handlers skipped (implemented): {skipped_existing}")
    if skipped_exists:
        print(f"  Handlers skipped (file exists): {skipped_exists}  (use --force to overwrite)")
    print()
    print("NOTE: GmeController_Handlers.cpp was NOT modified.")
    print("      Enable handlers manually by adding #include and REGISTER() calls.")


if __name__ == "__main__":
    main()
