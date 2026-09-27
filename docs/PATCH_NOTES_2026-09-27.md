# Gameplay fixes and portable Debug builds — September 27, 2026

## Guild follow-up

Server `4e23268` and packet-generator `2efdaa5` are published to their respective
mine/main branches. Invites are now player-confirmed working without a crash.
The follow-up corrects rank IDs, quoted request numbers and the promotion reply:
it no longer replaces the roster while the client still holds a member object.
Promotion/demotion, dismissal, re-inviting and leaving passed isolated-save tests.
Rank labels and promotion still await a client retest. The handbook includes a
new-chat handoff and identifies GuildUpdate as the next unfinished guild handler.

## Confirmed in the Windows client

- Burst Queen fusion raises BB and SBB to level 10 and unlocks UBB on an eligible unit.
- Mystery Frog fusion resets the unit's level and changes its growth type.
- Four-star Tilith (50253) is framed correctly on Home using the corrected MST crop.
- Merit Exchange purchases work (Honor Fang tested).
- Permanent exchange stock no longer shows expiry timers. The player reports no
  further warnings in Guild Hall or Randall's exchange hall.

These are player confirmations, separate from the automated checks below.
Event Bazaar tile visibility and boss battles still need specific client testing.

## Gameplay and protocol changes

- Restore Merit and Guild exchange catalogs, including evolution materials and
  Legend Stone; implement the authored Event Bazaar catalogs and currency handling.
- Use the client's negative no-expiry sentinel for permanent stock and Bazaar tiles.
- Use base artwork for Guild cards instead of interpreting unit growth type as an
  artwork variant. Include the matching invite profiles and handle GuildJoin invites.
- Fix BB/SBB fusion progression, fixed/random Mystery Frogs, level/type growth and
  compensation presents. Reject invalid mixed/duplicate/self fusion requests.
- Make synthesis stock/payment updates transactional; persist entered-feature state.
- Include the pending Research Lab work: Trials 001–003 and The Creation God,
  scripted transformations, ordered AI conditions, unlocks and first-clear rewards.
  Boss fidelity is model/server-tested, not yet confirmed in the client.
- Include the pending mission archive expansion (4 to 1,338 missions), 11 AI
  records, and gameplay handbook/work queue. Authored
  encounter values and unfinished features remain documented in feature reports;
  this release does not claim full gameplay parity.

## Developer setup

- Share the required packaging/CMake inputs, build script, presets and cache sources.
  Configure generates Ninja files locally; generated build trees are not committed.
- Locate the installed Visual Studio x64 tools and Ninja, bootstrap pinned vcpkg
  when requested, generate packet headers and build Debug from a fresh source tree.
- Repair generated-header/PCH dependencies and the debugger's multi-config target
  paths. Create a default configuration only when none exists.
- Include the optional developer console in source. addunit now uses the gameplay
  unit-grant schema/defaults, dictionary and acquisition counter within a transaction.
  It no longer inserts the obsolete unit_lv column or pre-maxes BB/SBB.
- Add Windows CI for recursive checkout, compilation and isolated startup with
  Debug symbols and the developer console. Portable release builds disable the console.

## Validation and remaining limits

- Actual CLI commands for 750004, 730302, 730322, 50253 and 10017 succeeded on a
  copied save. Duplicate grants, normal BB/SBB defaults, locked second sphere slot,
  dictionary/counter updates, invalid IDs and forced-failure rollback passed.
- Exchange, fusion, Research Lab, feature-visit, synthesis and guild-invite regression
  scripts are included. See the feature reports for their coverage and prior results.
- Fresh-source build/startup results are recorded in DEVELOPMENT.md. Local validation
  reuses installed vcpkg dependencies; it is not a test on a blank Windows VM.
- Debug symbols and startup are checked automatically. Interactive Visual Studio
  breakpoint stepping is not automated by the smoke test.
- Client artwork is distributed separately. Existing asset mirrors need the current
  UnitMst download files for Tilith; see scripts/refresh_unit_mst.py.

## Pull and build

```powershell
git pull
git submodule update --init --recursive
.\rebuild.bat -BootstrapVcpkg -VcpkgRoot .tools\vcpkg
```

Prerequisites and F5 instructions: [DEVELOPMENT.md](DEVELOPMENT.md).
Live saves, asset ZIPs, local configuration changes and backup files are not included in this patch.
