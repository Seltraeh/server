"""Client acceptance harness for the offline-proxy content provider.

Runs a proxy candidate in a SIDE-BY-SIDE test package (its own identity, so
its own LocalState) against an isolated server with a copied save, never the
installed client or the live save.

Ports.  The proxy rewrites EVERY game connection to one compile-time port
(OFFLINEMOD_PORT), so the port is a property of the candidate DLL; the harness
reads it from the DLL's hash (KNOWN_DLLS) and never guesses:

  29960  the isolated-port build of the reviewed candidate (preferred).  The
         test package and its server use 29960 only; the player's normal
         server (9960) and the installed client keep running untouched.
  9960   the reviewed candidate itself.  It shares the live port, so this is a
         SWITCH-OVER: the normal server and the installed client must be closed
         first (the harness refuses otherwise) and the normal server restarted
         afterwards (docs/CLAUDE_SESSION_9_HANDOFF.md).

Subcommands, in the order a run uses them:

  check    [--exe FILE]               read-only pre-flight: Developer Mode, layout identity, the
                                      layout DLL and its port, registration, loopback exemption,
                                      fixture, server candidate hash, the port and, for 9960 only,
                                      running servers/clients.
  layout   --appx DIR --dll FILE      test package folder under out/qa/current/content_client:
                                      hard links to the extracted client (no data copied) +
                                      a manifest with identity gumi.BraveFrontier.ContentTest
                                      and the installed package's publisher (CN=BraveFrontier,
                                      so the family is ..._99p3jr0gh0z6w) + the candidate
                                      libcurl.dll (a KNOWN_DLLS hash, or --port for another
                                      build).  Prints the register and loopback-exemption
                                      commands (the exemption needs an elevated prompt and is
                                      the maintainer's to run).  Refuses while the test package
                                      is registered (its install location is this folder).
  dll      --file FILE                replaces the libcurl.dll of an EXISTING layout (registered or
                                      not) with another KNOWN isolated-port build, without
                                      re-registering: the package's LocalState and settings are
                                      untouched.  Refuses while the game runs, keeps the replaced
                                      DLL under dll_history/ and records both in layout.json;
                                      rolling back is the same command with the kept file.
  state    [--stale-town]             copies the player's client account state (*.dat,
                                      *.xml, *.ini -- no assets) from the installed client's
                                      LocalState into the test package's LocalState, and with
                                      --stale-town plants the UNFRAMED shipped MapVillage.sam
                                      there as a deliberately stale cached copy.
  fixture                             broker fixture root: copies of the 21 owned files only.
  server   [--broker on|off] [--exe]  isolated server on the layout DLL's port with a backup-API
                                      copy of deploy/gme.sqlite, the real game_content as
                                      document root and the broker reading the fixture root.
                                      --mst-root DIR serves a test copy of deploy/mst instead
                                      (scripts/make_gate_test_mst.py: deterministic summon gates).
                                      Prints READY only when the started process is the port's
                                      only listener and its fps_cap and content status answer
                                      as configured; any failure exits nonzero with the log
                                      path after stopping only what it started.
  server   ... --manifest FILE        with --policy: install FILE (scripts/startup_coverage.py
                                      manifest) as the harness system root's content_startup.json,
                                      so the broker answers /offline_mod/content/v1/coverage for it.
                                      Without it the broker reports "no startup manifest" and the
                                      Session 16 DLL keeps the game's own census and prompt.
  coverage [--expect complete|incomplete]
                                      read-only: the running harness broker's startup coverage
                                      report for the layout EXE's build identity (what the DLL
                                      itself fetches before its census and prompt decisions):
                                      verdict, reason, per-set counts and every problem name.
                                      --expect exits 1 unless the verdict is the one named.
  progress                            read-only: how much of the legacy asset cache the test
                                      LocalState holds, against the installed client's.
  edit     --height 985|1161          same-size edit of the fixture MapVillage.sam header
                                      (labels move ~44 pt between the two).
  snapshot NAME [--all]               sizes + SHA-256 of the owned names in the test LocalState
                                      (compare two snapshots: no persistent asset writes); --all
                                      also records every other asset file, marked not owned (what
                                      the game's own download path wrote).
  collect                             copies the provider log out of the package's AC\\Temp
                                      (and keeps each collect under collected/<time>).

Disposable content (replacement and recovery against a generated policy):

  server --policy P --disposable      the broker serves a DISPOSABLE root: a hard-link mirror of
                                      exactly the files P owns (no bytes copied), kept across
                                      restarts until P changes.  The real game_content is never
                                      opened for writing: every change below replaces or moves a
                                      LINK (os.replace / os.rename), never an original's bytes.
  swap NAME --like OTHER              NAME (owned) now serves OTHER's bytes (owned, same extension)
  park NAME                           NAME's file leaves the root (its bundle is withdrawn)
  restore [NAME]                      re-links NAME (default: every change) to the original
  disposable                          read-only: what is changed, and that every other file is
                                      still the original (same file id)

Only the identity's work folder (out/qa/current/content_client*, or
out/qa/current/session16/content_client_c16 for c16) and the test package's own
data folder are written.  The installed client's LocalState is only read.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bf_testkit import (LIVE_PORT, LIVE_SAVE, ROOT, Fixture, IsolatedServer, ServerStartError,  # noqa: E402
                        fps_cap_check, qa_dir)

WORK = qa_dir('content_client')
LIVE_PFN = 'gumi.BraveFrontier_99p3jr0gh0z6w'
# The installed package's publisher; its PublisherId is the 99p3jr0gh0z6w above.
# BraveFrontierAppxClient's own manifest says CN=MyBraveFrontier, which would
# give the test package a different family, so layout pins this one.
LIVE_PUBLISHER = 'CN=BraveFrontier'
TEST_NAME = 'gumi.BraveFrontier.ContentTest'
TEST_PFN = TEST_NAME + '_99p3jr0gh0z6w'      # same publisher -> same publisher hash
ISOLATED_PORT = 29960
# Candidate DLLs by SHA-256 and the port their connection hooks rewrite every
# game request to (compile-time OFFLINEMOD_PORT).  The 29960 build is the
# reviewed one rebuilt with -DOFFLINEMOD_PORT=29960 and nothing else: a byte
# diff against it shows only the four `push 9960` immediates and the link
# metadata a no-change rebuild also changes (docs/CLAUDE_SESSION_9_HANDOFF.md).
REVIEWED_DLL_SHA256 = 'e24234c11765275f775a093e7c47e12a44e6a6ba05591962474c7b466eef3f7b'
ISOLATED_DLL_SHA256 = '01786e642ec6afcc2e9b89594d9ecb4a8af29dbb18420e9c9fbd2be4a5c1b48f'
KNOWN_DLLS = {
    REVIEWED_DLL_SHA256: {'port': LIVE_PORT, 'label': 'reviewed Session 7 candidate (live port: switch-over)'},
    ISOLATED_DLL_SHA256: {'port': ISOLATED_PORT, 'label': 'reviewed candidate rebuilt for the isolated port'},
}
# Phase C1 (Session 10): the provider plus the FileLoader enqueue filter
# (addLoadFile / addLoadFilePriority hooks), built in the same tree for each
# port (out/qa/current/session10/proxy_29960_c1, proxy_9960_c1).
C1_ISOLATED_DLL_SHA256 = '523c77ed23479174f6f9ce2d222331f6209159048f8372987dbdd156aa21ef76'
C1_LIVE_DLL_SHA256 = '00df5a39c674350d8b93b4d07a9cab64c9f1a36f0b07ed220183529bc267034b'
KNOWN_DLLS[C1_ISOLATED_DLL_SHA256] = {'port': ISOLATED_PORT,
                                      'label': 'Phase C1 candidate (enqueue filter), isolated port; automated QC passed, client pending'}
KNOWN_DLLS[C1_LIVE_DLL_SHA256] = {'port': LIVE_PORT,
                                  'label': 'Phase C1 candidate (enqueue filter), live port; not QC-reviewed'}
# Session 15 (#24): C1 plus the summon gate flash fix (OfflineMod.win32/content/
# cp_gate_flash.cpp), built in the same tree for each port
# (out/qa/current/session15/proxy_29960_gateflash, proxy_9960_gateflash).
GATE_ISOLATED_DLL_SHA256 = 'f1334e145f96aa72ad87337948905dc03413604b62db4bb248ef82e94ee5b679'
GATE_LIVE_DLL_SHA256 = '0e41bffdd0c6539eec769e1e092a38d0df0986bd14109ef7956c7c4f5f325d1a'
KNOWN_DLLS[GATE_ISOLATED_DLL_SHA256] = {'port': ISOLATED_PORT,
                                       'label': 'Session 15 gate flash candidate (C1 + #24 fix), isolated port; QC pending'}
KNOWN_DLLS[GATE_LIVE_DLL_SHA256] = {'port': LIVE_PORT,
                                   'label': 'Session 15 gate flash candidate (C1 + #24 fix), live port; not QC-reviewed'}
# Session 15, final: the gate flash fix plus the audio reader hooks (fopen_s /
# fread / fclose; sounds may be owned), out/qa/current/session15/proxy_*_s15.
# 'audio': only DLLs that hook the audio reader may be given a policy owning sounds.
S15_ISOLATED_DLL_SHA256 = 'af96f41b2df3359cc2326bb57e58bb9c34610251e07d3a54bd7de2ab8f6e5200'
S15_LIVE_DLL_SHA256 = '78a95fb022490e091b697a6dd744f061cc4294d25ad04f5fbd019110bb5b4279'
KNOWN_DLLS[S15_ISOLATED_DLL_SHA256] = {'port': ISOLATED_PORT, 'audio': True,
                                      'label': 'Session 15 candidate (C1 + #24 gate flash + audio reader), isolated port; client confirmed'}
KNOWN_DLLS[S15_LIVE_DLL_SHA256] = {'port': LIVE_PORT, 'audio': True,
                                  'label': 'Session 15 candidate (C1 + #24 gate flash + audio reader), live port; not QC-reviewed'}
POST_QUEST_DLL_SHA256 = '09056ee55d52936ad584a1ec79e044369c817da0b904901a9e3deb7947ba9c24'
KNOWN_DLLS[POST_QUEST_DLL_SHA256] = {'port': ISOLATED_PORT, 'audio': True,
    'label': 'Session 15 + post-quest prompt removal, isolated port; player confirmed'}
# Session 16: the post-quest candidate plus aggregated decision logging (one totals line per
# scene, a full QUEUED listing) and the startup coverage decisions (LocalState census x86
# 0xE415D0 and the background-download prompt read at 0x8E51A8, decided from the broker's
# /coverage report), out/qa/current/session16/proxy_*_s16.
# The first Session 16 pair (October 11) trusted a bare "complete 1" and let a provider that was
# still starting leave the census out (QC_SESSION_16_REVIEW.md): known so a layout holding it can be
# swapped, never a candidate.
S16_PRECORRECTION_ISOLATED_DLL_SHA256 = 'bf634d4b39f76337b0f227f4a2af24a59d56e2aa0b2715144a5c2206220313bf'
S16_PRECORRECTION_LIVE_DLL_SHA256 = 'e5287b9041c67af840f6f7f16a137cbbd34e61bc16de5deb1af020cd17e2a6a5'
KNOWN_DLLS[S16_PRECORRECTION_ISOLATED_DLL_SHA256] = {'port': ISOLATED_PORT, 'audio': True, 'startup': True,
    'label': 'SUPERSEDED Session 16 pre-correction build, isolated port; do not use'}
KNOWN_DLLS[S16_PRECORRECTION_LIVE_DLL_SHA256] = {'port': LIVE_PORT, 'audio': True, 'startup': True,
    'label': 'SUPERSEDED Session 16 pre-correction build, live port; do not use'}
# Session 16 after the QC corrections (October 20): a coverage report counts only when everything
# it states proves completeness, and the census is the game's until the provider is ACTIVE on the
# report's generation; out/qa/current/session16/proxy_*_s16q.
S16_ISOLATED_DLL_SHA256 = '8634857aeb98d1ab2b269d4c92c23f5681ece9a76b633f084631bee8d4ca0095'
S16_LIVE_DLL_SHA256 = '86b062ef51234a0225118cff896e511516d19c31eb387d1fbc3d0ad5a094d0b1'
# On the C16 client (October 21, collected 20261021-055758) this pair's startup path was DECLINED:
# it checked its sites only after the content provider had detoured one of them (censusLookup,
# "byte +0 is E9, expected 55").  Known so a layout holding it can be swapped; never a candidate.
KNOWN_DLLS[S16_ISOLATED_DLL_SHA256] = {'port': ISOLATED_PORT, 'audio': True, 'startup': True,
    'label': 'SUPERSEDED Session 16 corrected build (startup path declined after the provider\'s own lookup detour), isolated port; do not use'}
KNOWN_DLLS[S16_LIVE_DLL_SHA256] = {'port': LIVE_PORT, 'audio': True, 'startup': True,
    'label': 'SUPERSEDED Session 16 corrected build (startup path declined after the provider\'s own lookup detour), live port; do not use'}
# Session 17 (October 21): the same decisions; the startup sites are checked on the untouched image
# before ContentProvider_Attach and again after it, where only the provider's own recorded detour of
# the lookup may differ; out/qa/current/session17/proxy_*_s17.
S17_ISOLATED_DLL_SHA256 = '45a2c1b18e3ad1da07db735282636bf62f52003c5552e43347ba05ea1a41c1c8'
S17_LIVE_DLL_SHA256 = '5d2f3970fdde3a7a20f8a22126144ff4385bafa93fa2cd746534d2f532683d00'
KNOWN_DLLS[S17_ISOLATED_DLL_SHA256] = {'port': ISOLATED_PORT, 'audio': True, 'startup': True,
    'label': 'Session 17 build (post-quest + strict startup coverage decisions, startup sites verified around the provider\'s attach), isolated port; QC-verified on C16 (fallback route); a Session 18 broker\'s report is not its protocol, so it keeps the stock census and prompt'}
KNOWN_DLLS[S17_LIVE_DLL_SHA256] = {'port': LIVE_PORT, 'audio': True, 'startup': True,
    'label': 'Session 17 build (post-quest + strict startup coverage decisions, startup sites verified around the provider\'s attach), live port; not QC-reviewed'}
# Session 18 (October 22): the same, and a complete coverage report counts only when this client
# holds the files its manifest was derived from (the report's I rows: master tables in LocalState,
# the package's resourceMap, by SHA-256, and no other version of a named table);
# out/qa/current/session18/proxy_*_s18.
S18_ISOLATED_DLL_SHA256 = 'cfb825da79de8ca7a68db12009e9a9c66f5181bdb0af38d6a7660d3b44aba545'
S18_LIVE_DLL_SHA256 = '37d30448d215de69e40d250e34618677c06f068c962cd9eeabadfce383d8716a'
KNOWN_DLLS[S18_ISOLATED_DLL_SHA256] = {'port': ISOLATED_PORT, 'audio': True, 'startup': True,
    'label': 'Session 18 candidate (Session 17 + coverage bound to the client\'s own master tables), isolated port; QC pending'}
KNOWN_DLLS[S18_LIVE_DLL_SHA256] = {'port': LIVE_PORT, 'audio': True, 'startup': True,
    'label': 'Session 18 candidate (Session 17 + coverage bound to the client\'s own master tables), live port; not QC-reviewed'}
# Test package identities (--identity).  'test' is the Session 7-9 package: it
# is registered on the maintainer's machine and its LocalState and settings
# (including a stored background-download answer) are in use -- never reset.
# 'c1' is a separate, empty package for cache-free acceptance: its first launch
# downloads nothing it does not need and shows the background-download prompt.
# 'c16' is Session 16's cold package (no cached asset, fresh Settings; account state copied with
# `state`, as for c1): its work folder lives under out/qa/current/session16.  'c1' stays the
# accepted fallback and is never re-laid out for Session 16.
IDENTITIES = {
    'test': ('gumi.BraveFrontier.ContentTest', '[ContentTest]', 'content_client'),
    'c1': ('gumi.BraveFrontier.ContentTestC1', '[ContentTest C1]', 'content_client_c1'),
    'c1yes': ('gumi.BraveFrontier.ContentTestC1Yes', '[ContentTest C1 Yes]', 'content_client_c1_yes'),
    'c16': ('gumi.BraveFrontier.ContentTestC16', '[ContentTest C16]', 'session16/content_client_c16'),
}
LABEL = IDENTITIES['test'][1]


def select_identity(key: str) -> None:
    global TEST_NAME, TEST_PFN, LABEL, WORK, LAYOUT_RECORD
    TEST_NAME, LABEL, work = IDENTITIES[key]
    TEST_PFN = TEST_NAME + '_99p3jr0gh0z6w'
    first, *rest = work.split('/')
    WORK = qa_dir(first).joinpath(*rest)
    LAYOUT_RECORD = WORK / 'layout.json'

ISOLATED_DLL = ROOT / 'out' / 'qa' / 'current' / 'session9' / 'proxy_29960' / 'libcurl.dll'
DEFAULT_SERVER = ROOT / 'out' / 'qa' / 'current' / 'session8' / 'bin' / 'gimuserverw.exe'
NORMAL_SERVER = ROOT / 'out' / 'build' / 'debug-win64' / 'standalone_frontend' / 'Debug' / 'gimuserverw.exe'
PACKAGES = Path(os.environ['LOCALAPPDATA']) / 'Packages'
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'
POLICY = ROOT / 'deploy' / 'system' / 'content_provider.json'
SAM_HEADER = struct.Struct('<4si B hhHH')     # magic, version, rate, x, y, w, h (twips)
STATE_SUFFIXES = ('.dat', '.xml', '.ini')
NOT_LINKED = {'appxmanifest.xml', 'libcurl.dll', 'appxsignature.p7x', 'appxblockmap.xml'}
LAYOUT_RECORD = WORK / 'layout.json'


def load_policy() -> dict:
    text = '\n'.join(l for l in POLICY.read_text(encoding='utf-8').splitlines() if not l.lstrip().startswith('//'))
    return json.loads(text)


def owned_targets(policy: dict | None = None) -> dict:
    """flat name -> relative path, resolved exactly like the broker for the shipped policy."""
    policy = load_policy() if policy is None else policy
    names = {m.lower() for ms in policy['bundles'].values() for m in ms}
    aliases = {k.lower(): v for k, v in policy['aliases'].items()}
    exact = {}
    patterns = []
    for path in policy['scope']:
        if '*' in path or '?' in path:
            patterns.append(path)
        else:
            exact.setdefault(Path(path).name.lower(), path)
    out = {}
    unresolved = []
    for name in names:
        if name in aliases:
            out[name] = aliases[name]
        elif name in exact:
            out[name] = exact[name]
        else:
            unresolved.append(name)
    # The patterns are globbed once, and only for names nothing else resolves (C7: 75,308 names; a
    # folder glob per unresolved name took 72 s); the last match in pattern-then-glob order wins.
    if unresolved and patterns:
        by_pattern = {}
        for pattern in patterns:
            for cand in CONTENT.glob(pattern):
                by_pattern[cand.name.lower()] = cand.relative_to(CONTENT).as_posix()
        for name in unresolved:
            if name in by_pattern:
                out[name] = by_pattern[name]
    return out


def observation_targets(args) -> dict:
    """Configured names to observe; actual serving is verified in provider logs."""
    path = getattr(args, 'policy', None)
    return owned_targets(json.loads(Path(path).read_text(encoding='utf-8'))) if path else owned_targets()


def sha256(path: Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def registered_pfn() -> str:
    return subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command',
                           f'(Get-AppxPackage -Name {TEST_NAME}).PackageFamilyName'],
                          capture_output=True, text=True).stdout.strip()


def layout_port(layout: Path | None = None):
    """(port, label) of the DLL in the test package layout, or (None, why)."""
    dll = (layout or WORK / 'package') / 'libcurl.dll'
    if not dll.exists():
        return None, f'no {dll} (run: layout)'
    digest = sha256(dll)
    if digest in KNOWN_DLLS:
        return KNOWN_DLLS[digest]['port'], KNOWN_DLLS[digest]['label']
    try:
        record = json.loads(LAYOUT_RECORD.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        record = {}
    if record.get('dll_sha256') == digest and record.get('port'):
        return int(record['port']), f'unreviewed DLL {digest[:16]} declared for port {record["port"]} at layout'
    return None, f'unknown DLL {digest[:16]}: not in KNOWN_DLLS and no matching --port record (rerun layout)'


def cmd_layout(args) -> int:
    appx = Path(args.appx)
    digest = sha256(args.dll)
    known = KNOWN_DLLS.get(digest)
    if known and args.port and args.port != known['port']:
        print(f'refusing: {args.dll} is the {known["label"]}, built for port {known["port"]}, not {args.port}')
        return 1
    if not known and not args.port:
        print(f'refusing: {args.dll} (sha256 {digest}) is not a known candidate; pass --port with the'
              ' OFFLINEMOD_PORT it was built with')
        return 1
    port = known['port'] if known else args.port
    if registered_pfn():
        print(f'refusing: {TEST_NAME} is registered and installed from this layout; remove it first:')
        print(f'  powershell -NoProfile -Command "Get-AppxPackage {TEST_NAME} | Remove-AppxPackage"')
        return 1
    layout = WORK / 'package'
    if layout.exists():
        shutil.rmtree(layout)
    linked = 0
    for src in appx.rglob('*'):
        rel = src.relative_to(appx)
        if rel.parts[0].lower() == 'appxmetadata' or src.name.lower() == 'braveFrontier.windows.exe.i64'.lower():
            continue
        dst = layout / rel
        if src.is_dir():
            dst.mkdir(parents=True, exist_ok=True)
            continue
        if src.name.lower() in NOT_LINKED and len(rel.parts) == 1:
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        os.link(src, dst)
        linked += 1
    manifest = (appx / 'AppxManifest.xml').read_text(encoding='utf-8')
    old_identity = 'Name="gumi.BraveFrontier"'
    assert manifest.count(old_identity) == 1, 'unexpected manifest identity'
    manifest = manifest.replace(old_identity, f'Name="{TEST_NAME}"')
    # Separate test-package revision permits an in-place label update.
    manifest = re.sub(r'(<Identity\b[^>]*\bVersion=")[^"]*"',
                      r'\g<1>2.19.6.1"', manifest, count=1)
    publishers = re.findall(r'<Identity [^>]*Publisher="([^"]*)"', manifest)
    assert len(publishers) == 1, f'unexpected Identity publisher(s): {publishers}'
    manifest = manifest.replace(f'Publisher="{publishers[0]}"', f'Publisher="{LIVE_PUBLISHER}"', 1)
    manifest = manifest.replace('<DisplayName>', f'<DisplayName>{LABEL} ', 1)
    # Start uses the application label, independently of Properties/DisplayName.
    manifest, labels = re.subn(r'(<(?:\w+:)?VisualElements\b[^>]*\bDisplayName=")[^"]*"',
                              r'\g<1>' + LABEL + ' Brave Frontier"', manifest)
    assert labels == 1, f'unexpected VisualElements label count: {labels}'
    (layout / 'AppxManifest.xml').write_text(manifest, encoding='utf-8')
    shutil.copyfile(args.dll, layout / 'libcurl.dll')
    LAYOUT_RECORD.write_text(json.dumps({'dll_sha256': digest, 'port': port, 'source': str(Path(args.dll).resolve()),
                                         'label': known['label'] if known else 'unreviewed (--port)'}, indent=1),
                             encoding='utf-8')
    print(f'layout: {layout} ({linked} hard links, manifest identity {TEST_NAME} / {LIVE_PUBLISHER}'
          f' (was {publishers[0]}), libcurl.dll sha256 {digest} = '
          f'{known["label"] if known else "an UNREVIEWED build"}, port {port})')
    print('register (developer mode, current user):')
    print(f'  powershell -NoProfile -Command "Add-AppxPackage -Register \'{layout / "AppxManifest.xml"}\'"')
    print('loopback exemption (ELEVATED prompt, maintainer):')
    print(f"  CheckNetIsolation.exe LoopbackExempt -a '-n={TEST_PFN}'")
    print('remove afterwards:')
    print(f'  powershell -NoProfile -Command "Get-AppxPackage {TEST_NAME} | Remove-AppxPackage"')
    print(f"  CheckNetIsolation.exe LoopbackExempt -d '-n={TEST_PFN}'   (elevated)")
    return 0


def cmd_dll(args) -> int:
    """Swap the layout's libcurl.dll for another known isolated-port build (no re-registration)."""
    new = Path(args.file)
    digest = sha256(new)
    known = KNOWN_DLLS.get(digest)
    if not known:
        print(f'refusing: {new} (sha256 {digest}) is not in KNOWN_DLLS')
        return 1
    if known['port'] != ISOLATED_PORT:
        print(f'refusing: {new} is the {known["label"]} (port {known["port"]}); the test package only takes '
              f'builds for the isolated port {ISOLATED_PORT}')
        return 1
    layout = WORK / 'package'
    target = layout / 'libcurl.dll'
    if not target.exists():
        print(f'refusing: no {target} (run: layout)')
        return 1
    running = [q for q in processes() if q[1].lower() == 'braveFrontier.windows.exe'.lower()
               and str(layout).lower() in (q[2] or '').lower()]
    if running:
        print(f'refusing: the test client is running (pid {", ".join(q[0] for q in running)}); close it first')
        return 1
    current = sha256(target)
    if current == digest:
        print(f'{target} already is {known["label"]} ({digest})')
        return 0
    history = WORK / 'dll_history'
    history.mkdir(parents=True, exist_ok=True)
    kept = history / f'{current}.dll'
    if not kept.exists():
        shutil.copyfile(target, kept)
    if sha256(kept) != current:
        print(f'refusing: the kept copy {kept} does not match the installed DLL')
        return 1
    tmp = target.with_name('libcurl.dll.new')
    shutil.copyfile(new, tmp)
    os.replace(tmp, target)
    if sha256(target) != digest:
        print(f'ERROR: {target} does not hash to {digest} after the copy; restore with: dll --file {kept}')
        return 1
    try:
        record = json.loads(LAYOUT_RECORD.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        record = {}
    previous = {k: record[k] for k in ('dll_sha256', 'port', 'source', 'label') if k in record}
    record.setdefault('history', []).append(dict(previous, replaced_at=time.strftime('%Y-%m-%d %H:%M:%S'),
                                                 kept=str(kept)))
    record.update({'dll_sha256': digest, 'port': known['port'], 'source': str(new.resolve()),
                   'label': known['label']})
    LAYOUT_RECORD.write_text(json.dumps(record, indent=1), encoding='utf-8')
    previous_label = KNOWN_DLLS.get(current, {}).get('label', 'an unlisted build')
    print(f'{target}: now {known["label"]} (sha256 {digest}, port {known["port"]})')
    print(f'replaced: {previous_label} (sha256 {current}), kept as {kept}')
    print(f'registered as: {registered_pfn() or "not registered"} (no re-registration needed; LocalState untouched)')
    print('roll back with:')
    print(f'  python scripts\\content_provider_client_test.py --identity {args.identity} dll --file {kept}')
    return 0


def test_localstate() -> Path:
    return PACKAGES / TEST_PFN / 'LocalState'


def cmd_state(args) -> int:
    live = PACKAGES / LIVE_PFN / 'LocalState'
    target = test_localstate()
    if not (PACKAGES / TEST_PFN).exists():
        print(f'{PACKAGES / TEST_PFN} does not exist: register the test package and launch it once first')
        return 1
    target.mkdir(parents=True, exist_ok=True)
    copied = []
    for entry in os.scandir(live):
        if entry.is_file() and entry.name.lower().endswith(STATE_SUFFIXES):
            shutil.copy2(entry.path, target / entry.name)
            copied.append(entry.name)
    print(f'copied {len(copied)} client state file(s) (no assets) into {target}')
    if args.stale_town:
        shipped = Path(args.appx) / 'Assets' / 'Resources' / 'MapVillage.sam'
        data = shipped.read_bytes()
        h = SAM_HEADER.unpack_from(data, 0)[6] // 20
        shutil.copyfile(shipped, target / 'MapVillage.sam')
        print(f'planted stale MapVillage.sam (shipped, height {h} px, sha256 {hashlib.sha256(data).hexdigest()[:16]})')
    return 0


def cmd_fixture(args) -> int:
    root = WORK / 'broker_root'
    if root.exists():
        shutil.rmtree(root)
    for name, rel in sorted(owned_targets().items()):
        dst = root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(CONTENT / rel, dst)
    print(f'fixture root {root}: {sum(1 for _ in root.rglob("*") if _.is_file())} files')
    return 0


def disposable_paths() -> tuple[Path, Path, Path]:
    """(root, state file, parked folder) of the disposable broker root, all under WORK."""
    return WORK / 'disposable_root', WORK / 'disposable.json', WORK / 'disposable_parked'


def disposable_state() -> dict:
    _root, state, _parked = disposable_paths()
    try:
        return json.loads(state.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def save_disposable_state(st: dict) -> None:
    disposable_paths()[1].write_text(json.dumps(st, indent=1), encoding='utf-8')


def build_disposable(policy: dict, policy_file: str) -> Path:
    """Hard-link mirror of exactly the files the policy owns; kept while the policy is unchanged."""
    root, _state, parked = disposable_paths()
    digest = sha256(policy_file)
    st = disposable_state()
    if root.exists() and st.get('policy_sha256') == digest:
        print(f'disposable root {root}: kept from earlier runs ({len(st.get("changes", {}))} change(s) in place)')
        return root
    if root.exists():
        shutil.rmtree(root)                   # removes links only; the originals keep their other name
    if parked.exists():
        shutil.rmtree(parked)
    targets = owned_targets(policy)
    for rel in sorted(set(targets.values())):
        dst = root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        os.link(CONTENT / rel, dst)
    save_disposable_state({'policy': str(Path(policy_file).resolve()), 'policy_sha256': digest,
                           'files': len(set(targets.values())), 'changes': {}})
    print(f'disposable root {root}: {len(set(targets.values()))} hard links to the policy\'s files (no bytes copied)')
    return root


def disposable_target(name: str) -> tuple[str, Path, Path]:
    """(rel, disposable path, original path) of an owned flat name, or SystemExit."""
    st = disposable_state()
    if not st:
        raise SystemExit('no disposable root yet: run  server --policy P --disposable  first')
    targets = owned_targets(json.loads(Path(st['policy']).read_text(encoding='utf-8')))
    rel = targets.get(name.lower())
    if not rel:
        raise SystemExit(f'{name} is not owned by {st["policy"]}')
    root = disposable_paths()[0]
    return rel, root / rel, CONTENT / rel


def broker_index(port: int) -> tuple[dict, dict] | None:
    """({owned name: digest}, {unavailable name: reason}) from the running harness broker, or None."""
    import urllib.request
    try:
        with urllib.request.urlopen(f'http://127.0.0.1:{port}/offline_mod/content/v1/index?since=0&epoch=',
                                    timeout=30) as r:
            text = r.read().decode()
    except OSError:
        return None
    owned, unavailable = {}, {}
    for line in text.splitlines():
        p = line.split('\t')
        if p[0] == 'F' and len(p) > 5:
            owned[p[3].lower()] = p[5]
        elif p[0] == 'X' and len(p) > 3:
            unavailable[p[2].lower()] = p[3]
    return owned, unavailable


def await_broker(done, what: str, timeout: float = 240.0) -> int:
    """Wait until the running broker has published a disposable change (the client adopts it at
    its next scene change).  A change that touches a whole folder costs the broker a rescan that
    re-checks every owned file's digest: about 35 s at C2 scale (Session 11)."""
    port, _label = layout_port()
    start = time.time()
    while time.time() - start < timeout:
        idx = broker_index(port) if port else None
        if idx is None:
            # A timeout can mean the running broker is still rescanning. Never
            # report publication success without observing the requested index.
            time.sleep(2)
            continue
        if done(*idx):
            print(f'published by the broker after {time.time() - start:.0f} s: {what}.'
                  ' Change scenes in the client now (the provider adopts it at a scene change).')
            return 0
        time.sleep(2)
    print(f'NOT published within {timeout:.0f} s: {what}; check the server log')
    return 1


def relink(original: Path, target: Path) -> None:
    """Atomically make `target` a hard link to `original` again."""
    tmp = target.with_name(target.name + '.relink')
    if tmp.exists():
        tmp.unlink()
    target.parent.mkdir(parents=True, exist_ok=True)
    os.link(original, tmp)
    os.replace(tmp, target)


def cmd_swap(args) -> int:
    rel, target, original = disposable_target(args.name)
    other_rel, _other_target, other_original = disposable_target(args.like)
    if Path(rel).suffix.lower() != Path(other_rel).suffix.lower():
        print(f'refusing: {args.name} and {args.like} have different extensions')
        return 1
    tmp = target.with_name(target.name + '.swap')
    shutil.copyfile(other_original, tmp)      # a NEW file in the disposable root
    os.replace(tmp, target)                   # replaces the link; the original is untouched
    st = disposable_state()
    st['changes'][rel] = {'kind': 'swap', 'like': other_rel, 'sha256': sha256(target)}
    save_disposable_state(st)
    want = 'sha256:' + sha256(target)
    print(f'{rel} now holds the bytes of {other_rel} (sha256 {want[7:23]}); waiting for the broker...')
    return await_broker(lambda owned, _x: owned.get(Path(rel).name.lower()) == want, f'{Path(rel).name} swapped')


def cmd_park(args) -> int:
    rel, target, _original = disposable_target(args.name)
    if not target.exists():
        print(f'{rel} is already parked')
        return 1
    parked = disposable_paths()[2] / rel
    parked.parent.mkdir(parents=True, exist_ok=True)
    os.replace(target, parked)                # moves the link out of the broker root
    st = disposable_state()
    st['changes'][rel] = {'kind': 'park'}
    save_disposable_state(st)
    name = Path(rel).name.lower()
    print(f'{rel} parked: the broker withdraws it and every member of its bundle; waiting for the broker...')
    return await_broker(lambda owned, unavailable: name not in owned and name in unavailable, f'{Path(rel).name} withdrawn')


def cmd_restore(args) -> int:
    st = disposable_state()
    if not st:
        print('no disposable root')
        return 1
    rels = list(st['changes'])
    if args.name:
        rels = [disposable_target(args.name)[0]]
    root, _state, parked = disposable_paths()
    want = {}
    for rel in rels:
        relink(CONTENT / rel, root / rel)
        if (parked / rel).exists():
            (parked / rel).unlink()
        st['changes'].pop(rel, None)
        want[Path(rel).name.lower()] = 'sha256:' + sha256(CONTENT / rel)
        print(f'{rel} restored (hard link to the original again)')
    save_disposable_state(st)
    if not want:
        return 0
    print('waiting for the broker...')
    return await_broker(lambda owned, unavailable: all(owned.get(n) == d and n not in unavailable
                                                      for n, d in want.items()),
                        f'{len(want)} requested name(s) restored')


def cmd_disposable(args) -> int:
    st = disposable_state()
    if not st:
        print('no disposable root')
        return 1
    root = disposable_paths()[0]
    print(f'disposable root {root} for {st["policy"]}: {st["files"]} files; changes: {st["changes"] or "none"}')
    unchanged = bad = 0
    targets = owned_targets(json.loads(Path(st['policy']).read_text(encoding='utf-8')))
    for rel in sorted(set(targets.values())):
        if rel in st['changes']:
            continue
        p = root / rel
        if p.exists() and os.path.samefile(p, CONTENT / rel):
            unchanged += 1
        else:
            bad += 1
            print(f'  NOT the original: {rel}')
    print(f'{unchanged} unchanged file(s) are the originals themselves (same file id); {bad} differ')
    return 1 if bad else 0


def processes() -> list:
    """(pid, name, path) of every server and client process, read-only."""
    script = ("Get-CimInstance Win32_Process -Filter \"Name='gimuserverw.exe' OR Name='BraveFrontier.Windows.exe'\""
              " | ForEach-Object { '{0}|{1}|{2}' -f $_.ProcessId, $_.Name, $_.ExecutablePath }")
    out = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command', script],
                         capture_output=True, text=True).stdout
    return [tuple(line.split('|', 2)) for line in out.splitlines() if line.count('|') == 2]


def installed_client(procs) -> list:
    return [p for p in procs if p[1].lower() == 'braveFrontier.windows.exe'.lower() and 'WindowsApps' in (p[2] or '')]


def broker_check(mode: str, owned: int | None):
    """(path, judge) for /offline_mod/content/v1/status: the bfcp 2 broker the DLL
    polls, in the requested state -- enabled, indexed, the expected owned names,
    no collisions or unavailable names -- or dormant for --broker off."""
    def judge(status: int, body: str):
        lines = body.splitlines()
        first = lines[0] if lines else ''
        if status != 200 or first != 'bfcp 2':
            return False, f'content status answered HTTP {status} {first[:40]!r}, want 200 "bfcp 2"'
        f = dict(l.split(' ', 1) for l in lines[1:] if ' ' in l)
        state = ', '.join(f'{k} {f.get(k, "?")}' for k in ('enabled', 'ready', 'owned', 'collisions', 'unavailable'))
        state += f', reason {f["reason"]}' if 'reason' in f else ''
        if mode == 'off':
            return f.get('enabled') == '0', f'broker should be dormant: {state}'
        want_owned = f.get('owned') if owned is None and f.get('owned', '0') != '0' else str(owned)
        ok = (f.get('enabled'), f.get('ready'), f.get('owned'), f.get('collisions'), f.get('unavailable')) == \
            ('1', '1', want_owned, '0', '0')
        return ok, f'broker not ready for the proof (want enabled 1, ready 1, owned {owned}, collisions 0, unavailable 0): {state}'
    return '/offline_mod/content/v1/status', judge


def serve(server: IsolatedServer, *, broker: str, poll: float = 5.0) -> int:
    """Start, announce READY only on proven readiness, then watch until Ctrl+C.

    0: it was ready and the operator stopped it.  1: it never became ready, or
    lost exclusive ownership / exited while serving.  130: interrupted before
    it was ready.  Every path stops exactly what server.start() launched."""
    port = server.fx.port
    try:
        server.start()                        # stops what it launched on any failure
    except ServerStartError as e:
        print(f'NOT READY: {e}')
        print('No server is serving the test package.  Fix the cause above and rerun.')
        return 1
    except KeyboardInterrupt:
        print(f'interrupted before the server was ready; stopped what this harness started (log {server.log_path})')
        return 130
    try:
        print(f'READY  pid {server.proc.pid} is the only listener on 127.0.0.1:{port}; fps_cap and the content'
              f' status answered (broker {broker}).  Log {server.log_path}.  Ctrl+C stops it.', flush=True)
        while True:
            server._sleep(poll)
            problem = server.ownership_problem()
            if problem:
                print(f'FAILED: {problem}.  Stopping the harness server (pid {server.proc.pid}) and only it;'
                      f' close the test package now -- its requests may reach another server.  Log {server.log_path}')
                return 1
    except KeyboardInterrupt:
        print(f'stopped by Ctrl+C (pid {server.proc.pid}); log {server.log_path}')
        return 0
    finally:
        server.stop()


SOUND_SUFFIXES = ('.mp3', '.wav')


def audio_guard(policy_file) -> str | None:
    """Why the layout DLL must not get this policy, or None.  A policy that owns
    sounds needs a DLL that hooks the audio reader (x86 0xEC0030 bypasses
    getFileData): any other DLL reports them downloaded, so the game stops
    downloading them, and an uncached one plays silent."""
    if not policy_file:
        return None
    policy = json.loads(Path(policy_file).read_text(encoding='utf-8'))
    sounds = sorted(m for ms in policy.get('bundles', {}).values() for m in ms if m.lower().endswith(SOUND_SUFFIXES))
    if not sounds:
        return None
    dll = WORK / 'package' / 'libcurl.dll'
    known = KNOWN_DLLS.get(sha256(dll)) if dll.exists() else None
    if known and known.get('audio'):
        return None
    return (f'{policy_file} owns {len(sounds)} sound(s) (e.g. {sounds[0]}), but the test package\'s DLL '
            f'({known["label"] if known else "an unlisted build"}) does not hook the audio reader: those sounds '
            'would stop downloading and play silent.  Install a DLL with the audio hook first (dll --file ...)')


STARTUP_FORMAT = 'bfcp-startup 1'


def layout_build(layout: Path | None = None) -> str | None:
    """The layout EXE's build identity (TimeDateStamp-SizeOfImage-AddressOfEntryPoint, what the
    Session 16 DLL sends as ?build=), or None without a layout."""
    exe = (layout or WORK / 'package') / 'BraveFrontier.Windows.exe'
    if not exe.is_file():
        return None
    from startup_coverage import client_build
    return client_build(exe)


def manifest_problem(path) -> str | None:
    """Why FILE cannot be the harness broker's content_startup.json, or None."""
    try:
        data = json.loads(Path(path).read_text(encoding='utf-8'))
    except (OSError, ValueError) as e:
        return f'{path} is not a readable JSON file ({e})'
    if not isinstance(data, dict) or data.get('format') != STARTUP_FORMAT:
        got = data.get('format') if isinstance(data, dict) else None
        return f'{path} is not a startup manifest (format {got!r}, want {STARTUP_FORMAT!r})'
    build = layout_build()
    if build is None:
        return f'no client EXE in the layout {WORK / "package"} to compare the manifest\'s build with (run: layout)'
    if data.get('build') != build:
        return f'{path} was derived for client build {data.get("build")}, but the layout EXE is {build}'
    return None


def fetch_coverage(port: int, build: str) -> tuple[int, str]:
    """(HTTP status, body) of the harness broker's coverage report, every name listed; (0, why)
    when nothing answers."""
    import urllib.error
    import urllib.parse
    import urllib.request
    url = (f'http://127.0.0.1:{port}/offline_mod/content/v1/coverage?'
           + urllib.parse.urlencode({'build': build, 'detail': '1'}))
    try:
        with urllib.request.urlopen(url, timeout=60) as r:
            return r.status, r.read().decode('utf-8', 'replace')
    except urllib.error.HTTPError as e:
        return e.code, ''
    except OSError as e:
        return 0, str(e)


COVERAGE_CLASSES = ('absent', 'unowned', 'missing', 'ambiguous', 'unavailable')
COVERAGE_SET_FIELDS = ('required', 'ok') + COVERAGE_CLASSES
COVERAGE_HEADER = ('complete', 'enabled', 'ready', 'epoch', 'generation', 'policy', 'manifest', 'manifest_build',
                   'client_build', 'build_match', 'unknown', 'excluded', 'inputs')
COVERAGE_FIELDS = set(COVERAGE_HEADER) | {'manifest_error', 'reason'}


def _u64(value: str) -> bool:
    """cp_core.cpp ParseU64: 1-20 ASCII digits, no overflow past 2^64 - 1."""
    return bool(re.fullmatch(r'[0-9]{1,20}', value)) and int(value) < 2 ** 64


def _safe_flat_name(s: str) -> bool:
    """cp_core.cpp IsSafeFlatName."""
    return (0 < len(s) <= 255 and s not in ('.', '..') and not s.endswith(('.', ' '))
            and not any(ord(c) < 0x20 or ord(c) == 0x7F or c in '/\\:*?"<>|' for c in s))


def _safe_input_path(root: str, path: str) -> bool:
    """cp_core.cpp SafeInputPath: a flat LocalState name, or '/'-separated flat names in the package."""
    if not path or len(path) > 260:
        return False
    if root == 'localstate':
        return _safe_flat_name(path)
    return all(_safe_flat_name(segment) for segment in path.split('/'))


def parse_coverage(text: str, expect_build: str | None = None) -> dict | None:
    """The DLL's ParseCoverage (offline-proxy OfflineMod.win32/content/cp_core.cpp), rule for rule.

    None when the body is not a coverage report at all: wrong protocol, no end line, an unknown or
    repeated field, an unknown row (the first Session 16 broker's A rows), a malformed set or
    problem row, a bad number, a missing header field.  Otherwise the report's fields plus 'sets',
    'P', 'U' and 'complete' -- True only when everything the report states PROVES it (both startup
    sets present, non-empty and fully owned, no row, no reason, no unknown list, a loaded manifest
    and policy, enabled, ready, this client's build) -- 'proved', with 'why' when it is not; the
    field 'complete' is only what the report says.  A bare "complete 1" is never enough (QC
    2026-10-20).  Both readers judge the shared corpus
    offline-proxy/tests/fixtures/coverage_reports.txt alike."""
    lines = text.split('\n')
    if text.endswith('\n'):
        lines.pop()
    lines = [l[:-1] if l.endswith('\r') else l for l in lines]
    if len(lines) < 3 or lines[0] != 'bfcp 2' or lines[1] != 'coverage 1' or lines[-1] != 'end':
        return None
    out = {'sets': {}, 'P': [], 'U': [], 'I': []}
    seen = set()
    rows = collections.Counter()
    for line in lines[2:-1]:
        if line.startswith('P\t'):
            p = line.split('\t')
            if len(p) != 4 or p[2] not in COVERAGE_CLASSES or not p[1] or not p[3]:
                return None
            rows[(p[1], p[2])] += 1
            out['P'].append(p[1:])
            continue
        if line.startswith('U\t'):
            out['U'].append(line[2:])
            continue
        if line.startswith('I\t'):
            # a client file the manifest was derived from (Session 18)
            p = line.split('\t')
            if (len(p) != 4 or p[1] not in ('localstate', 'package') or not _safe_input_path(p[1], p[2])
                    or not re.fullmatch(r'sha256:[0-9a-f]{64}', p[3])
                    or any(r == p[1] and q.lower() == p[2].lower() for r, q, _d in out['I'])):
                return None
            out['I'].append((p[1], p[2], p[3]))
            continue
        if line.startswith('set '):
            w = line.split()
            if len(w) != 2 + 2 * len(COVERAGE_SET_FIELDS) or not w[1] or w[1] in out['sets']:
                return None
            counts = {}
            for k, field in enumerate(COVERAGE_SET_FIELDS):
                if w[2 + 2 * k] != field or not _u64(w[3 + 2 * k]):
                    return None
                counts[field] = int(w[3 + 2 * k])
            out['sets'][w[1]] = counts
            continue
        key, _, value = line.partition(' ')
        if key in seen or key not in COVERAGE_FIELDS:
            return None
        seen.add(key)
        if key in ('complete', 'enabled', 'ready', 'build_match') and value not in ('0', '1'):
            return None
        if key in ('generation', 'unknown', 'excluded', 'inputs') and not _u64(value):
            return None
        if key == 'epoch' and not value:
            return None
        out[key] = value
    if any(k not in seen for k in COVERAGE_HEADER):
        return None

    contradiction = ''
    for name, s in out['sets'].items():
        if sum(s[f] for f in ('ok',) + COVERAGE_CLASSES) != s['required']:
            contradiction = f'set {name} counts do not add up to its required names'
    for (name, cls), n in rows.items():
        if n > out['sets'].get(name, {}).get(cls, 0):
            contradiction = f'problem rows for set {name} {cls} exceed its count'
    if len(out['U']) != int(out['unknown']):
        contradiction = 'the unknown count and its rows disagree'
    if len(out['I']) != int(out['inputs']):
        contradiction = 'the inputs count and its rows disagree'

    sets = out['sets']
    if contradiction:
        why = 'the report contradicts itself: ' + contradiction
    elif out['complete'] != '1':
        why = out.get('reason') or 'the report says incomplete'
    elif out['enabled'] != '1' or out['ready'] != '1':
        why = 'the report says complete but the broker is not enabled and ready'
    elif out['build_match'] != '1':
        why = 'the report says complete but not for this client build'
    elif 'manifest_error' in out or not out['manifest'].startswith('sha256:') or not out['policy'].startswith('sha256:'):
        why = 'the report says complete without a loaded manifest and policy'
    elif out['unknown'] != '0' or out['U']:
        why = 'the report says complete with unknown startup lists'
    elif set(sets) != {'login', 'background'}:
        why = 'the report says complete without exactly the login and background sets'
    elif out['P']:
        why = 'the report says complete with problem rows'
    elif not out['I']:
        why = 'the report says complete without naming the client files its manifest was derived from'
    elif out.get('reason'):
        why = 'the report says complete and gives a reason'
    elif expect_build and (out['client_build'] != expect_build or out['manifest_build'] != expect_build):
        why = 'the report says complete for another client build'
    elif any(s['required'] == 0 or s['ok'] != s['required'] for s in sets.values()):
        why = 'the report says complete but a set is empty or not fully owned'
    else:
        why = ''
    out['proved'] = not why
    out['why'] = why
    return out


def cmd_coverage(args) -> int:
    """Read-only: the running harness broker's startup coverage for the layout EXE."""
    port, label = layout_port()
    if port is None:
        print(f'refusing: cannot tell which port the test package uses: {label}')
        return 1
    build = layout_build()
    if build is None:
        print(f'refusing: no client EXE in the layout {WORK / "package"} (run: layout)')
        return 1
    status, text = fetch_coverage(port, build)
    report = parse_coverage(text, build) if status == 200 else None
    if report is None:
        print(f'NOT READABLE: 127.0.0.1:{port} answered the coverage report with HTTP {status} {text[:80]!r}'
              ' -- the DLL reads this as not ready (no harness server, an older broker, or not a report)')
        return 1
    # The DLL's own rule (parse_coverage mirrors cp_core.cpp): proved, never just claimed.
    verdict = 'complete' if report['proved'] else 'incomplete'
    print(f'startup coverage for build {build} on 127.0.0.1:{port}: {verdict}'
          + ('' if report['proved'] else f' -- {report["why"]}'))
    print(f'  the report itself says complete {report.get("complete")}; epoch {report.get("epoch")}, generation'
          f' {report.get("generation")}, policy {report.get("policy", "")[:23]}, manifest {report.get("manifest", "")[:23]},'
          f' build match {report.get("build_match")}')
    for name, counts in report['sets'].items():
        print(f'  set {name}: ' + ', '.join(f'{k} {v}' for k, v in counts.items()))
    for row in report['U']:
        print('  unknown startup list:', row)
    for row in report['P']:
        print('  problem:', ' '.join(row))
    for root, path, digest in report['I']:
        print(f'  client file the manifest was derived from: {root} {path} {digest[:23]}')
    if verdict == 'complete':
        print('  -> once its provider is ACTIVE on this generation and its own copies of the client files above match'
              ' (a Session 18 DLL hashes them), the DLL leaves the LocalState census out and, unless an asset is'
              ' queued for background download, answers the background-download prompt "No" for the session')
    else:
        print('  -> the DLL builds the census and leaves the background-download prompt to the player')
    if args.expect and args.expect != verdict:
        print(f'EXPECTED {args.expect}, got {verdict}')
        return 1
    return 0


def cmd_server(args) -> int:
    if getattr(args, 'disposable', False) and not getattr(args, 'policy', None):
        print('refusing: --disposable needs --policy (the 21-name fixture root is already a copy: use edit)')
        return 1
    manifest_file = getattr(args, 'manifest', None)
    if manifest_file and not getattr(args, 'policy', None):
        print('refusing: --manifest needs --policy (startup coverage is judged against a generated policy)')
        return 1
    port, label = layout_port()
    if port is None:
        print(f'refusing: cannot tell which port the test package uses: {label}')
        return 1
    if manifest_file:
        why = manifest_problem(manifest_file)
        if why:
            print(f'refusing: {why}')
            return 1
    why = audio_guard(getattr(args, 'policy', None))
    if why:
        print(f'refusing: {why}')
        return 1
    if args.port and args.port != port:
        print(f'refusing: --port {args.port} but the layout DLL ({label}) talks to {port}')
        return 1
    busy = IsolatedServer._listeners(port)
    if busy:
        print(f'refusing: 127.0.0.1:{port} is in use by pid(s) {sorted(busy)}; this harness never shares its port'
              + (' (9960 is the live server: see the switch-over procedure)' if port == LIVE_PORT else ''))
        return 1
    procs = processes()
    if port == LIVE_PORT:
        running = [p for p in procs if p[1].lower() == 'gimuserverw.exe'] + installed_client(procs)
        if running:
            print('refusing: the layout DLL uses the live port 9960, which the installed client also uses, so a'
                  ' running server or installed client could reach the other\'s server and save.  Close these:')
            for p in running:
                print('  ', *p)
            return 1
    print(f'test package DLL: {label}; port {port}'
          + ('' if port == LIVE_PORT else ' (isolated: the normal server and the installed client may keep running)'))
    if port != LIVE_PORT and IsolatedServer._listeners(LIVE_PORT):
        # The copy below reads the live save while the normal server runs.  A
        # server without the Session 9 busy timeout (standalone_frontend/main.cpp)
        # loses the action whose COMMIT overlaps that read and then fails every
        # request until restarted (scripts/test_db_commit_contention_wire.py).
        print('caution: copying the live save while the normal server runs -- keep the installed client idle'
              ' for this step unless that server has the Session 9 busy-timeout fix')
    exe = Path(args.exe).resolve()
    print(f'server candidate {exe} sha256 {sha256(exe)}')
    mst_root = Path(args.mst_root).resolve() if getattr(args, 'mst_root', None) else None
    if mst_root is not None:
        # A test copy of the master data (scripts/make_gate_test_mst.py): read
        # by this server only; the shipped deploy/mst is what it replaces here.
        if not (mst_root / 'gacha_effect_mst.json').is_file():
            print(f'refusing: --mst-root {mst_root} holds no gacha_effect_mst.json')
            return 1
        print(f'mst_root {mst_root} (gacha_effect_mst.json sha256 {sha256(mst_root / "gacha_effect_mst.json")})')
    system = WORK / 'system'
    if system.exists():
        shutil.rmtree(system)
    shutil.copytree(ROOT / 'deploy' / 'system', system)
    policy_file = getattr(args, 'policy', None)
    save_name = getattr(args, 'save', None)
    if policy_file:
        # A generated policy (scripts/gen_unit_family_policy.py) reads the real
        # game_content read-only -- the broker never writes -- so `edit` (which
        # changes the fixture copy) does not apply to it.
        policy = json.loads(Path(policy_file).read_text(encoding='utf-8'))
        expected_owned = None
        if getattr(args, 'disposable', False):
            policy['root'] = str(build_disposable(policy, policy_file))
        print(f'policy {policy_file}: {len(policy.get("scope", []))} scope entries, '
              f'{len(policy.get("bundles", {}))} bundles, root {policy.get("root") or CONTENT}')
    else:
        policy = load_policy()
        policy['root'] = str(WORK / 'broker_root')
        expected_owned = len(owned_targets())
    policy['enabled'] = args.broker == 'on'
    (system / 'content_provider.json').write_text(json.dumps(policy, indent=1), encoding='utf-8')
    startup = system / 'content_startup.json'
    if manifest_file:
        shutil.copyfile(manifest_file, startup)
        print(f'startup manifest {manifest_file} (sha256 {sha256(startup)}) installed as {startup}')
    else:
        if startup.exists():
            startup.unlink()
        print('no startup manifest: the broker reports coverage incomplete ("no startup manifest"), so a'
              ' Session 16 DLL keeps the game\'s census and background-download prompt')
    if save_name:
        # A PERSISTENT fixture save: seeded once (from --seed or the live save),
        # then reused on every start, so progress survives a harness restart.
        if re.search(r'[\\/:]', save_name) or save_name in ('', '.', '..'):
            print(f'refusing: --save takes a plain name, not {save_name!r}')
            return 1
        seed = Path(args.seed) if getattr(args, 'seed', None) else LIVE_SAVE
        saves = WORK / 'saves' / save_name
        existed = (saves / 'gme.sqlite').exists()
        fx = Fixture.create(saves, port=port, source=seed, keep=True, mst_root=mst_root,
                            document_root=ROOT / 'deploy' / 'game_content', system_root=system)
        where = f'persistent save {fx.db_path} ' + ('(kept from earlier runs)' if existed else f'(seeded from {seed})')
    else:
        fx = Fixture.create(WORK / 'server', port=port, document_root=ROOT / 'deploy' / 'game_content',
                            system_root=system, mst_root=mst_root)
        where = f'save copied from {LIVE_SAVE} to {fx.db_path} (recopied every start: not a persistence test)'
    print(f'{where}; broker {args.broker}; waiting for readiness (up to {args.timeout:.0f} s)...', flush=True)
    server = IsolatedServer(exe, fx, checks=[fps_cap_check(fx), broker_check(args.broker, expected_owned)],
                            allow_live_port=port == LIVE_PORT, timeout=args.timeout)
    return serve(server, broker=args.broker)


def cmd_edit(args) -> int:
    path = WORK / 'broker_root' / 'sam' / 'MapVillage' / 'MapVillage.sam'
    data = bytearray(path.read_bytes())
    magic, version, rate, x, y, w, h = SAM_HEADER.unpack_from(data, 0)
    assert magic == b'MAS.', magic
    before = len(data)
    SAM_HEADER.pack_into(data, 0, magic, version, rate, x, y, w, args.height * 20)
    tmp = path.with_suffix('.sam.tmp')
    tmp.write_bytes(data)
    os.replace(tmp, path)                     # atomic publication
    print(f'MapVillage.sam height {h // 20} -> {args.height} px, size {before} -> {len(data)} (same size)')
    return 0


def cmd_snapshot(args) -> int:
    target = test_localstate()
    names = observation_targets(args)
    every = getattr(args, 'all', False)
    print(f'observing {len(names)} configured names' + (f' from {args.policy}' if getattr(args, 'policy', None) else ' (baseline policy)')
          + (' and every other asset file (--all)' if every else ''))
    if not target.exists():
        print(f'{target} does not exist: no snapshot (register the package, run state, then snapshot)')
        return 1
    out = {}
    for entry in os.scandir(target):
        if not entry.is_file():
            continue
        owned = entry.name.lower() in names
        if owned or (every and not entry.name.lower().endswith(STATE_SUFFIXES)):
            out[entry.name] = {'size': entry.stat().st_size, 'mtime': entry.stat().st_mtime,
                               'sha256': hashlib.sha256(Path(entry.path).read_bytes()).hexdigest()}
            if every:
                out[entry.name]['owned'] = owned
    snap = WORK / f'snapshot_{args.name}.json'
    snap.write_text(json.dumps(out, indent=1), encoding='utf-8')
    owned_count = sum(1 for k in out if k.lower() in names)
    print(f'{snap}: {owned_count} owned name(s) present in the test LocalState'
          + (f'; {len(out) - owned_count} other asset file(s)' if every else ''))
    for k, v in sorted(out.items()):
        print(f'  {k} {v["size"]} {v["sha256"][:16]}' + ('' if k.lower() in names or not every else '  (not owned)'))
    return 0


def asset_census(folder: Path) -> dict:
    """{flat name: size} of the asset files in a (flat) LocalState; state files excluded."""
    out = {}
    if folder.exists():
        for entry in os.scandir(folder):
            if entry.is_file() and not entry.name.lower().endswith(STATE_SUFFIXES):
                out[entry.name.lower()] = entry.stat().st_size
    return out


def cmd_progress(args) -> int:
    """How far the test package's legacy asset download has got.  Read-only.

    Phase B serves 21 names from the broker; every other asset still comes
    through the game's own download queue into the test LocalState.  The
    installed client's LocalState is a comparison, not a completion target:
    inventory, archive unlocks, visited screens and background-DLC choice affect
    downloads. Take a baseline after the current download finishes; later
    screens may still download assets."""
    reference = asset_census(PACKAGES / LIVE_PFN / 'LocalState')
    test = asset_census(test_localstate())
    owned = set(observation_targets(args))
    have = set(test) & set(reference)
    ref_bytes = sum(reference.values())
    got_bytes = sum(test[n] for n in have)
    print(f'installed client LocalState (reference): {len(reference):,} asset files, {ref_bytes / 1e9:.2f} GB;'
          f' {len(owned & set(reference))} of the {len(owned)} provider-owned names among them')
    if not test_localstate().exists():
        print('test LocalState does not exist yet (launch the test package once)')
        return 1
    pct = 100.0 * got_bytes / ref_bytes if ref_bytes else 0.0
    print(f'test package LocalState: {len(test):,} asset files, {sum(test.values()) / 1e9:.2f} GB;'
          f' {len(have):,} of the reference names ({pct:.1f}% of its bytes)')
    print('Reference overlap is NOT download completion. Save contents, visited screens and'
          ' the background-DLC choice affect which assets are downloaded.')
    print(f'provider-owned names written to the test LocalState: {len(owned & set(test))}'
          ' (compare snapshots: they must not change while the provider serves them)')
    return 0


def cmd_check(args) -> int:
    """Read-only pre-flight: everything the maintainer's steps need, and what is still missing."""
    import winreg
    need = []

    def report(ok, text):
        print(('ok    ' if ok else 'NEED  ') + text)
        if not ok:
            need.append(text)

    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            r'SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock') as key:
            dev = winreg.QueryValueEx(key, 'AllowDevelopmentWithoutDevLicense')[0] == 1
    except OSError:
        dev = False
    report(dev, 'Windows Developer Mode (needed to register an unsigned loose layout)')
    layout = WORK / 'package'
    manifest = (layout / 'AppxManifest.xml').read_text(encoding='utf-8') if (layout / 'AppxManifest.xml').exists() else ''
    report(f'Name="{TEST_NAME}"' in manifest and f'Publisher="{LIVE_PUBLISHER}"' in manifest,
           f'layout {layout} with identity {TEST_NAME} / {LIVE_PUBLISHER} (run: layout)')
    port, label = layout_port()
    report(port is not None, f'layout libcurl.dll: {label}' + (f'; port {port}' if port else ''))
    pfn = registered_pfn()
    report(pfn == TEST_PFN, f'test package registered as {TEST_PFN} (found: {pfn or "none"})')
    exempt = subprocess.run(['CheckNetIsolation.exe', 'LoopbackExempt', '-s'], capture_output=True, text=True).stdout
    report(TEST_PFN.lower() in exempt.lower(), f'loopback exemption for {TEST_PFN} (elevated CheckNetIsolation)')
    report((PACKAGES / TEST_PFN / 'LocalState').exists(), 'test LocalState exists (launch the test package once)')
    if getattr(args, 'policy', None):
        policy = json.loads(Path(args.policy).read_text(encoding='utf-8'))
        root = Path(policy.get('root') or CONTENT)
        targets = owned_targets(policy)
        missing = [rel for rel in targets.values() if not (root / rel).is_file()]
        report(bool(targets) and not missing, f'policy {args.policy}: {len(targets)} configured names, {len(missing)} missing files under {root}')
    else:
        root = WORK / 'broker_root'
        count = sum(1 for p in root.rglob('*') if p.is_file()) if root.exists() else 0
        report(count == len(owned_targets()), f'broker fixture root {root}: {count} of {len(owned_targets())} owned files (run: fixture)')
    if getattr(args, 'manifest', None):
        why = manifest_problem(args.manifest)
        report(why is None, f'startup manifest {args.manifest} for the layout EXE (build {layout_build() or "?"})'
               + (f': {why}' if why else f' sha256 {sha256(args.manifest)}'))
    exe = Path(args.exe)
    report(exe.is_file(), f'server candidate {exe}' + (f' sha256 {sha256(exe)}' if exe.is_file() else ''))
    procs = processes()
    servers = [p for p in procs if p[1].lower() == 'gimuserverw.exe']
    if port is not None:
        busy = IsolatedServer._listeners(port)
        report(not busy, f'127.0.0.1:{port} free for the harness server'
               + (f' (in use by pid(s) {sorted(busy)})' if busy else ''))
    if port == LIVE_PORT:
        report(not servers, 'no gimuserverw running (switch-over: stop the normal server first)'
               + ''.join(f'\n        pid {p[0]} {p[2]}' for p in servers))
        report(not installed_client(procs), 'the installed client is closed')
    elif port is not None:
        live = IsolatedServer._listeners(LIVE_PORT)
        print(f'info  isolated port: the normal server {"is" if live else "is not"} listening on 9960 and'
              f' the installed client is {"running" if installed_client(procs) else "closed"}; either may stay so')
    print(f'{len(need)} item(s) outstanding')
    return 1 if need else 0


def cmd_collect(args) -> int:
    # Each collect is also kept under collected/<time>: the copies in WORK are
    # replaced by the next run's, and the package log itself rotates at 8 MB.
    temp = PACKAGES / TEST_PFN / 'AC' / 'Temp'
    keep = WORK / 'collected' / time.strftime('%Y%m%d-%H%M%S')
    base = keep
    copied = 0
    for name in ('offlinemod_content.log', 'offlinemod_content.log.1'):
        if (temp / name).exists():
            if copied == 0:
                suffix = 0
                while True:
                    try:
                        keep.mkdir(parents=True, exist_ok=False)
                        break
                    except FileExistsError:
                        suffix += 1
                        keep = base.with_name(f'{base.name}-{suffix}')
            shutil.copy2(temp / name, WORK / name)
            shutil.copy2(temp / name, keep / name)
            copied += 1
    print(f'collected {copied} log file(s) into {WORK}' + (f' (this collect is also kept in {keep})' if copied else ''))
    return 0 if copied else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--identity', choices=sorted(IDENTITIES), default='test',
                    help="test package: 'test' (baseline), 'c1' (No branch), 'c1yes' (fresh Yes branch) or"
                         " 'c16' (Session 16 cold package)")
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('layout'); p.add_argument('--appx', required=True); p.add_argument('--dll', required=True)
    p.add_argument('--port', type=int, help='OFFLINEMOD_PORT of a DLL that is not in KNOWN_DLLS')
    p = sub.add_parser('dll'); p.add_argument('--file', required=True, help='a KNOWN_DLLS isolated-port build')
    p = sub.add_parser('state'); p.add_argument('--stale-town', action='store_true')
    p.add_argument('--appx', default=str(ROOT.parent / 'BraveFrontierAppxClient'))
    sub.add_parser('fixture')
    p = sub.add_parser('server'); p.add_argument('--exe', default=str(DEFAULT_SERVER))
    p.add_argument('--broker', choices=('on', 'off'), default='on')
    p.add_argument('--port', type=int, help='must match the layout DLL; normally omitted')
    p.add_argument('--timeout', type=float, default=180.0)
    p.add_argument('--save', help='persistent fixture save <work>/saves/NAME: seeded once, kept across restarts')
    p.add_argument('--seed', help='with --save: seed a NEW fixture save from this file instead of the live save')
    p.add_argument('--policy', help='serve this generated policy (e.g. unit families) instead of the 21-name fixture')
    p.add_argument('--disposable', action='store_true',
                   help='with --policy: serve a hard-link mirror of its files that swap/park/restore may change')
    p.add_argument('--mst-root', help='serve this test copy of deploy/mst (e.g. scripts/make_gate_test_mst.py)')
    p.add_argument('--manifest', help='with --policy: the startup manifest (content_startup.json) the broker reports on')
    p = sub.add_parser('coverage')
    p.add_argument('--expect', choices=('complete', 'incomplete'), help='exit 1 unless the verdict is this one')
    p = sub.add_parser('swap'); p.add_argument('name'); p.add_argument('--like', required=True)
    p = sub.add_parser('park'); p.add_argument('name')
    p = sub.add_parser('restore'); p.add_argument('name', nargs='?')
    sub.add_parser('disposable')
    p = sub.add_parser('progress'); p.add_argument('--policy', help='generated policy whose configured names to observe')
    p = sub.add_parser('edit'); p.add_argument('--height', type=int, choices=(985, 1161), required=True)
    p = sub.add_parser('snapshot'); p.add_argument('name')
    p.add_argument('--policy', help='generated policy whose configured names to observe')
    p.add_argument('--all', action='store_true', help='also record every other asset file (what the game downloaded)')
    sub.add_parser('collect')
    p = sub.add_parser('check'); p.add_argument('--exe', default=str(DEFAULT_SERVER))
    p.add_argument('--policy', help='generated policy to check instead of the 21-name fixture')
    p.add_argument('--manifest', help='startup manifest to check against the layout EXE build')
    args = ap.parse_args()
    select_identity(args.identity)
    WORK.mkdir(parents=True, exist_ok=True)
    return {'layout': cmd_layout, 'dll': cmd_dll, 'state': cmd_state, 'fixture': cmd_fixture, 'server': cmd_server,
            'progress': cmd_progress, 'edit': cmd_edit, 'snapshot': cmd_snapshot, 'collect': cmd_collect,
            'check': cmd_check, 'swap': cmd_swap, 'park': cmd_park, 'restore': cmd_restore,
            'disposable': cmd_disposable, 'coverage': cmd_coverage}[args.cmd](args)


if __name__ == '__main__':
    raise SystemExit(main())
