"""Startup coverage for the offline client (Sessions 16-18): what the client's login and
background-download paths ask for, a family policy that owns it, and what is left.

    python scripts/startup_coverage.py manifest --out MANIFEST.json [--report REPORT.json]
    python scripts/startup_coverage.py policy --base C6.json --out C7.json [--report REPORT.json]
    python scripts/startup_coverage.py evaluate --policy POLICY.json --manifest MANIFEST.json [--out REPORT.json]

Read only, except for the files named by --out / --report (never under deploy/).

WHAT THE CLIENT ASKS FOR AT STARTUP (x86 BraveFrontier.Windows.exe fb4f3c25 RVAs; arm64
libgame.so for names; docs/OFFLINE_STARTUP_TRANSITION_PLAN.md sections 8-9):

login       the priority downloads of a first login: the Appx resourceMap scene "999999"
            (DLCManagerComplementor::loadBundleResourceMap 0x16F7F0; the Session 11 cold
            trace's LoginScene -> ConfirmScene window was 1,571 of its names plus 701
            SuperAnim pages) and the dependencies the client derives from them itself:
            FileLoader::processSamAndCutSceneFile 0x106B10 reads a downloaded .sam's image
            table (SuperAnim::getSamSupportingFiles 0x62CBA0, short header layout) and a
            downloaded script's "*,type=PARAM" lines (param= split on ':', tokens ending in
            .png/.jpg/.mp3/.plist/.sam), queueing every page / asset it does not have.
background  LoginScene::createDownloadCheckList 0x8DF880 (arm64 0x1745184): every
            F_RESOURCE_MST path ("system<i>", 48,500 flat names) plus per-MST categories
            whose names the client builds in LoginScene::updateEvent 0x8E2960 with the
            GameScene request helpers.  Missing names go to FileLoader's MAIN list
            (addLoadFile 0x634FA0), which only the background-download answer "Yes" works
            through.

The resourceMap is read the way the client holds it: loadBundleResourceMap drops every token of
the client's own exclusion table (BUNDLE_EXCLUSIONS, 203 slots at VA 0x184DF00), so no scene --
the login bundle included -- ever downloads one of those names.

Every dependency a file names stays required whether or not it exists (Content.closure).
A category in DERIVED has its exact names: the fields of the rows the client holds, read the way
LoginScene::updateEvent builds each request (Session 18; see DERIVED).  Every other category is
only ENUMERATED from the files its folders hold today -- that cannot show a file the MST ids need
but nobody has -- so each is an UNKNOWN list and the report stays incomplete until its exact names
are derived from the ids (QC 2026-10-20).  Their existing files are still required.

The manifest names the inputs it was derived from (inputs.client_files: the installed client's
master tables and resourceMap, with their SHA-256): a client holding other tables needs another
manifest.  scripts/check_client_inputs.py compares a client with them (read only).  It also names
the server's response tables its derived lists read (inputs.server_files: response_tables() under
the server's mst_root, with their SHA-256).  The broker binds those to the tables its own responses
serve -- the bytes ServerCache parsed at boot, which nothing replaces while the server runs -- and
never calls a manifest complete that names other bytes, another table, or none (server_binding;
Session 18 correction, docs/CLAUDE_SESSION_18_CORRECTION.md).

Names the client never needs from the provider are EXCLUDED with their documented reason
(client state, the text-dictionary channel, names its own lookup reports present).  Names that
exist in no source at all (game_content, the Appx package, the APK assets) stay required and are
noted as ABSENT: absence proves a name cannot be served, not that startup does without it, so
the broker counts it as a problem (incomplete) until client evidence excludes it.

The manifest is what the server's broker reads (<system_root>/content_startup.json): the broker
checks every name against its live index and reports, per generation, whether startup coverage
is complete (GET /offline_mod/content/v1/coverage, gimuserver/content/ContentBroker.cpp).
"""
from __future__ import annotations

import argparse
import base64
import collections
import dataclasses
import datetime
import hashlib
import json
import os
import re
import struct
import sys
from dataclasses import dataclass
from fnmatch import fnmatch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_effect_family_policy import Tree, app_resources, parse_sam, reader_view, safe_flat_name  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'
MST_DIR = ROOT / 'deploy' / 'game_content' / 'mst'
RESPONSE_MST = ROOT / 'deploy' / 'mst'          # the tables the server sends in its responses
# Where the live server looks for translated area banners at boot (ServerCache.cpp kBannerDir,
# "./game_content/content/dungeon/" from its working folder, deploy/): served_area_rows.
BANNER_DIR = ROOT / 'deploy' / 'game_content' / 'content' / 'dungeon'
APPX = ROOT.parent / 'BraveFrontierAppxClient'
APK_ASSETS = ROOT.parent / 'BraveFrontier-APK' / 'assets'
FORMAT = 'bfcp-startup 1'

# Version-channel tables: the client's file key and AES-ECB encode key for each (recovered from
# libgame.so's DownloadMstFileList::add call sites; the MST download map lists all 133).
TABLE_KEYS = {
    'F_RESOURCE_MST': ('hdt2N6mr', 'z5dp9GX8'),
    'F_RESOURCE_BASE_MST': ('a4M4dh1i', 'kIj2n6s5'),
    'F_MEDAL_MST': ('DX7KZ0Tf', 'UH1ruJ36'),
    'F_STAMP_MST': ('W2j4E3te', 'NFJ74Wmg'),
    'F_STAMP_SET_MST': ('KuCe7t3p', 'em68GxrB'),
    'F_RAID_POINT_MST': ('79MTkNus', '76rVpcGL'),
    'F_GRAND_MISSION_MST': ('Yg4n5c7G', 'L60J1vWU'),
    'F_CHLNG_MISSION_MST': ('8GhcfX4Q', 'rvIa85RA'),
    'F_RAID_MAP_MST': ('39mGIANe', 'HMib7V50'),
    'F_GRAND_MISSION_MAP_MST': ('fSsA7Rg5', 'tdk6VX4P'),
    'F_ITEM_MST': ('83JWTCGy', 'houmK14R'),
    'F_DUNGEON_MST': ('B57MJYoF', 'w5W8RPSG'),
}

# createDownloadCheckList's categories as folder families: (key prefix(es), the MST it iterates,
# the helper / path literal that names the folder).  Every servable file under the folders is
# required (a superset of the exact names the helper builds from the MST ids).
#
# Session 19: item and area are DERIVED (below); these four stay enumerated -- unknown lists.
CATEGORIES = [
    ('unit, initial units', 'UnitMst::getAllKeys, DefineMst::getInitialUnit',
     'GameScene::requestUnitFiles (arm64 0x160AF90): unit/img, unit/cgg, unit/cgs; SuperAnim units unit_sam/',
     ('unit/img', 'unit/cgg', 'unit/cgs', 'unit_sam')),
    # Session 19: item/ joins this stand-in.  On x86 the weapon branch 0x7F6CE0 asks for "/item/" + a
    # SummonerArm field (0x7F6D50) and summoner_img 0x7F2690 for /unit/img/, /unit/cgg/, /unit/cgs/
    # (0x7F27E0); until item was derived, the item stand-in's item/ covered the weapon art by accident, and
    # the exact item rule leaves out the item/ files no F_ITEM_MST row names (38, weapon_thum_* among them).
    ('weapon, summoner_img', 'SummonerArmMst, SummonerImageMst',
     'requestSummonerWeaponFiles / requestSummonerUnitFiles (arm64 0x161DEAC, 0x160EB2C): summoner parts in unit/; '
     'x86 weapon 0x7F6CE0 asks for /item/ (0x7F6D50), summoner_img 0x7F2690 for unit/img, cgg, cgs (0x7F27E0)',
     ('unit/img', 'unit/cgg', 'unit/cgs', 'item')),
    ('gacha', 'GachaInfo bg/btn/door/contents banner', '"/gacha/" (x86 0x8E39C8)', ('gacha',)),
    # sound/: F_EFFECT_GROUP_MST xW6TKu9G names sounds (se_voice_b1_1.mp3, bf236_se_debility.mp3, ...),
    # which the C16 client queued at startup; the raid BGM entry's folder used to cover them.
    ('effect, effect_group', 'EffectMst, EffectGroupMst',
     'GameScene::requestEffectMst / requestEffectGroupMst: SuperAnim effects and particles, effect group sounds',
     ('effect', 'sam', 'sound')),
]

# Session 19: the end of LoginScene::updateEvent, AFTER createDownloadCheckList, requests more on the
# priority list before the login window's TransitionalDLCScene downloads it (arm64 0x174BDF8-0x174BF44:
# GameScene::requestMissionFiles for each PermitPlace place, every DungeonMst row's getImgIcon by dungeon
# type, VortexArenaMst art; the summoner skill icons and Home bundle art the client queued there).  It is
# not one of the check list's categories and is not derived (x86 not traced): an UNKNOWN list.  Until
# Session 19 the area category enumerated dungeon/, which covered its dungeon icons by accident; deriving
# area exactly exposed it.  Client evidence: in the C16 provider-off control (collected 20261021-210903,
# pid 13488) the client priority-queued, between its check list and TransitionalDLCScene #20 (67,463-68,170
# ms), 34 dungeon/ icons and these 12 names that no other list requires -- and nothing else.
LOGIN_END_EVIDENCE = (
    'freeenergy.png', 'freeenergy_countdown.png', 'home_locked_arena.png', 'home_locked_raid.png', 'home_win_coopraid.png',
    'su_skill_icon_01_09.png', 'su_skill_icon_01_10.png', 'su_skill_icon_06_06.png', 'su_skill_icon_07_01.png',
    'su_skill_icon_14_01.png', 'su_skill_icon_15_01.png', 'su_skill_icon_16_01.png')
CATEGORIES.append(
    ('login-end preloads', 'DungeonMst icons by dungeon type, PermitPlace places, VortexArenaMst, summoner skill icons',
     'LoginScene::updateEvent after createDownloadCheckList (arm64 0x174BDF8-0x174BF44; x86 not traced): every '
     'existing dungeon/ file, and the 12 other names the C16 provider-off client queued there',
     ('dungeon',), LOGIN_END_EVIDENCE))


# createDownloadCheckList categories whose EXACT names are derived (Session 18).  The check list
# holds one entry per row of the list (no filter but the ones a rule names): <key><row id> with the
# id as its value, or, for the lists marked below, <key><id or index> with the name itself as its
# value.  LoginScene::updateEvent 0x8E2960 tests each key by substring in the order unit, item,
# weapon, summoner_img, area, gacha_effect, gacha, system, effect_group, effect, dungeonkey,
# stampset, stamp, campaignQuest, banner, general_event, sp_challenge_title, raidMap,
# raid_point_bgm, raid_point_img, raid_boss_bgm, medal, campaignMap, sgXbbSkill (the first match
# wins), looks the row up by its id where it stored one, and asks for <folder> + <field> through
# 0x63E120: an empty field asks for nothing (0x63E173), and the name is the part after the last '/'
# (0x63E270 -> 0x235C30) that existsLocalFile 0x237EB0 checks before addLoadFile 0x634FA0.  A field
# is a column the table's parser stores as it is, or one token of a column it splits with strtok on
# ',' (0x236740: an empty or " " value has no token).  Which rows a list holds is decided by who
# fills it: a response parser replaces its list (removeAllObjects), DataMstManager loads the
# client's own LocalState copy at boot (getLoadMstsList 0x53E2A0, loadMst 0x53D340; Session 19: 0x540540
# is checkCorruptionsOfMsts, which then requires the lists non-empty).  tools/bin/x86_startup_rules_check.py
# re-reads every address cited below from the binary.  x86 BraveFrontier.Windows.exe fb4f3c25.

@dataclass(frozen=True)
class Field:
    column: str            # the MST column the table's parser reads
    token: int | None      # None: the whole value; n: token n of its ',' split
    folder: str            # the folder literal updateEvent prefixes
    evidence: str          # parser store -> getter -> request (x86 RVAs)
    guarded: bool = True   # False: the parser takes this token without checking that it exists
    sound: bool = False    # a sound column: 0x63E1C0 skips a .mp3 / .wav that 0x238870 finds (a
                           # CCFileUtils check by basename), so requiring every one is a superset
    empty_asks: bool = False  # True: an empty value is still requested (folder only, no 0x63E120 check)
    tiles: bool = False    # a map image list "name:x:y,...": split on ',' then ':' with 0x65F510 (empty pieces
                           # kept); a piece of exactly three parts names its first, when that is not empty
    folder_by: tuple | None = None  # (column, {n: folder}, default): the folder the request uses depends on the
                                    # row's int column (stream >> int); the flat name the client checks does not
    computed: str | None = None     # a name the client builds from the row (COMPUTED[computed]), not a column


@dataclass(frozen=True)
class Join:
    """A second table a list's request walks for each of its rows (Session 19: an area's dungeons)."""
    source: tuple          # the joined rows' source, as DerivedList.source
    column: str            # a joined row belongs to a list row when this column, as a string, ...
    parent: str            # ... equals this column of the list row
    fields: tuple          # what is requested for each joined row
    evidence: str


@dataclass(frozen=True)
class DerivedList:
    key: str               # the check-list key and updateEvent branch
    table: str             # F_*: a version-channel table the client keeps; M_*: one a server response carries
    source: tuple          # ('version', TABLE), ('response', deploy/mst file, response key), ('served', response
                           # key): a table the server synthesizes at boot (SERVED_SOURCES), or ('none', why)
    fields: tuple
    gate: str | None       # the feature byte the check-list loop sits under: derived as open, a superset
    evidence: str
    where: tuple | None = None   # (column, n): only rows whose column reads as the integer n
    also: tuple = ()             # further sources that can replace the list within a session: required too
    join: tuple = ()             # Join: tables the request walks for each row
    unique: str | None = None    # the id column the client keys the rows by: an id twice is a problem (which
                                 # row it keeps is not modelled), and the list stays unknown


DERIVED = (
    # Session 19.  Both helpers are reached from updateEvent with the check-list VALUE, the row id; the
    # check list holds one entry per row of the list, key "<key>"+id (0xE9960, the row's +0x14).
    DerivedList(
        'item', 'F_ITEM_MST', ('version', 'F_ITEM_MST'),
        (Field('Mt3Y0bo5', None, '/item/', 'thumbnail: parser 0x4EFCF0 stores it at +0x64 (0x4EFFD6); '
               'GameScene::requestItemFiles 0x7F6FA0 looks the row up by id (ItemMstList 0x5E4F90, map +0x14) and '
               'asks for "/item/" (0x7F7053) + getter 0xF1B80 (+0x64, ItemMst::getItemImg) through 0x63E120'),
         Field('computed:item_frame', None, '/common/', 'frame: ItemMst::getItemFrameName 0x5E4D20 -> 0x5E4D40 '
               '(item_frame_name) at 0x7F70CF; asked for under "/common/" (0x7F70E1) when not empty (0x7F70DB) -- '
               'it never is: every branch appends ".png" (0x5E4F41)', computed='item_frame')),
        None,
        'check list 0x8E0CBD-0x8E0E55 (every ItemMstList row: the array 0x5E5170 builds from the map, key '
        '"item"+id); updateEvent 0x8E376C ("item", tested second) -> 0x7F6FA0.  No server response carries '
        '2C7LDzYk, so the rows are what DataMstManager::loadItemMst 0x54E310 loads from the client\'s own '
        'F_ITEM_MST (ItemMstResponse vtable 0x18F39C0, readParam 0x4EFCF0)', unique='kixHbe54'),
    DerivedList(
        'area', 'M_AREA_MST', ('served', '3SG2wX0R'),
        (Field('b5VveUd8', None, '/area/', 'img: parser 0x4CBC80 stores it at +0x4C (0x4CBF1C); '
               'GameScene::requestAreaFiles 0x7F7140 (AreaMstList 0x5C61E0, getAreaMstWithAreaID 0x5C6360) asks for '
               '"/area/" (0x7F71FC) + getter 0xF4FF0 (+0x4C, AreaMst::getImg) when not empty'),
         Field('H0UB8pqm', None, '/area/', 'plate_img: stored at +0x64 (0x4CBFA0); when getter 0xF1B80 (+0x64, '
               'AreaMst::getAreaNameImg) is not empty, "/dungeon/" (0x7F7326) for area types 1 and 3 (+0x44 at '
               '0x7F72BF-0x7F72CA, 3v1qg7Uj stored at 0x4CBE95), else "/area/" (0x7F72CC)',
               folder_by=('3v1qg7Uj', {1: '/dungeon/', 3: '/dungeon/'}, '/area/'))),
        None,
        'check list 0x8E0B4B-0x8E0CB7 (every AreaMstList row, key "area"+id); updateEvent 0x8E37F9 ("area") -> '
        '0x7F7140, then the area\'s dungeons (join).  UserInfo\'s 3SG2wX0R REPLACES the list (AreaMstResponse '
        'readParam 0x4CBC80 releases every row at row 0 / field 0, 0x4CBCDC-0x4CBD08) after loadAreaMst 0x544C30 '
        'loaded the client\'s own F_AREA_MST, so the rows are the curated table the server synthesizes at boot '
        '(served_area_rows; ServerCache::clientAreaMst), bound by its served projection digest',
        join=(Join(('version', 'F_DUNGEON_MST'), 'VjCY7rX4', 'VjCY7rX4',
                   (Field('21VKZo0E', None, '/dungeon/', 'room_asset: DungeonMst parser 0x4E3230 stores it at +0xE4 '
                          '(0x4E35DC); updateEvent asks for "/dungeon/" (0x8E3897) + getter 0x399A90 (+0xE4, '
                          'DungeonMst::getImgIcon) through 0x63E120 for each dungeon of the list 0x5DA320 returns'),),
                   'DungeonMstList::getDungeonListWithAreaID 0x5DA320 (DungeonMstList 0x5DA030): every row whose area '
                   'id (getter 0xEE4C0, +0x5C; VjCY7rX4 stored at 0x4E34D3) equals the area\'s, as strings, AND that '
                   'PermitPlaceInfoList::isPermitDungeon accepts (singleton 0x1997730, 0x582D80).  The permit list '
                   'is the player\'s own (progression, the Vortex day), never a startup snapshot: it can only narrow '
                   'the request, so every dungeon of the area is required -- a superset over every permit state.  '
                   'No response carries 01pzrTCm: the rows are loadDungeonMst 0x544D40\'s, the client\'s own '
                   'F_DUNGEON_MST'),), unique='VjCY7rX4'),
    DerivedList(
        'dungeonkey', 'M_DUNGEON_KEY_MST', ('response', 'dungeon_key_mst.json', '4NG79sX1'),
        (Field('M2cv6dum', 0, '/dungeon_key/', 'parser 0x4E2970 splits it (0x4E2C47) and stores token 0 at '
               '+0x74 without a size check (0x4E2C8C-0x4E2CEA); 0x8E3C89 getter 0xEE2C0 (+0x74)', guarded=False),
         Field('M2cv6dum', 1, '/dungeon_key/', 'token 1 only when there are two (0x4E2D23), through vtable '
               '+0x18 (0xEC050 -> +0x14); 0x8E3CE8 vtable +0x14 (0xE9960, +0x14)'),
         Field('VX0j1fni', None, '/dungeon/', 'stored at +0x8C (0x4E2DCC); 0x8E3D42 getter 0xEE300 (+0x8C)'),
         Field('9unNZ6b0', None, '/dungeon/', 'stored at +0xA4 (0x4E2E5C); 0x8E3D9C getter 0xEE340 (+0xA4)')),
        None,
        'check list 0x8E18C0-0x8E19C9 (every DungeonKeyMstList 0x5D8CC0 row, key "dungeonkey"+id); updateEvent '
        '0x8E3C10-0x8E3DC6.  The rows are the Initialize response\'s 4NG79sX1, which the server sends from '
        'deploy/mst/dungeon_key_mst.json as they are (ServerCache m_initrsp.dungeon_keys; plain strings)'),
    DerivedList(
        'stampset', 'F_STAMP_SET_MST', ('version', 'F_STAMP_SET_MST'),
        (Field('M2cv6dum', None, '/stamp/', 'parser 0x511A90 stores it at +0x30 (0x511C18); 0x8E3E46 getter '
               '0xF17A0 (+0x30)'),),
        'FUNC_RAID',
        'check list 0x8E23B0-0x8E246A (every StampSetMstList 0x5FB1D0 row, key "stampset"+id) inside the '
        'FUNC_RAID block (0x8E2073: byte 0x184AA65, initially 1); updateEvent 0x8E3DCD-0x8E3E76'),
    DerivedList(
        'stamp', 'F_STAMP_MST', ('version', 'F_STAMP_MST'),
        (Field('nGVc2zS1', None, '/stamp/', 'parser 0x511840 stores it at +0x1C (0x51197E); 0x8E3EF6 getter '
               '0x109CC0 (+0x1C)'),),
        'FUNC_RAID',
        'check list 0x8E24A0-0x8E255A (every StampMstList 0x5FA7C0 row, key "stamp"+id) inside the FUNC_RAID '
        'block; updateEvent 0x8E3E7D-0x8E3F26, tested after "stampset", so no stampset key lands there'),
    DerivedList(
        'medal', 'F_MEDAL_MST', ('version', 'F_MEDAL_MST'),
        (Field('M2cv6dum', None, '/medal/', 'parser 0x4F1220 passes it to vtable +0x28 (0x4F1369; 0xF00F0 -> '
               '+0x44); 0x8E4444 vtable +0x24 (0xEE480, +0x44)'),),
        None,
        'check list 0x8E19F0-0x8E1B03 (every MedalMstList 0x5E6B50 row, key "medal"+id); updateEvent '
        '0x8E43C9-0x8E446D'),
    # The next lists store the NAME as the check-list value, which updateEvent requests under its folder.
    DerivedList(
        'campaignQuest', 'F_GRAND_MISSION_MST', ('version', 'F_GRAND_MISSION_MST'),
        (Field('yTPw0BD7', None, '/grand/', 'frame image: parser 0x4D3660 stores it at +0x48 (0x4D3A49); the value '
               'is getter 0xF5E90 (+0x48) at 0x8E1C97'),
         Field('M2cv6dum', None, '/grand/', 'thumbnail: stored at +0x60 (0x4D3AC9); the value is getter 0x1D48D0 '
               '(+0x60) at 0x8E1D2F, keyed "campaignQuest"+id+"thum"')),
        'FUNCK_CAMPAIGN',
        'check list 0x8E1C76-0x8E1DDF (every non-null CampaignMissionMstList 0x5CDE70 row, key "campaignQuest"+id) '
        'inside the FUNCK_CAMPAIGN block (0x8E1C43: byte 0x184AA38); updateEvent 0x8E3F50 asks for "/grand/" + '
        'value.  At login the list holds the client\'s own F_GRAND_MISSION_MST (no login response carries '
        'as8G75gK; the C16 client queued all 52 names); CampaignMissionGet and CampaignStart replace it with '
        'deploy/mst/grand_mission_mst.json within a session, so those rows are required too',
        also=(('response', 'grand_mission_mst.json', 'as8G75gK'),)),
    DerivedList(
        'banner', 'M_BANNER_INFO_MST', ('response', 'banner_info_mst.json', 'Pk5F1vhx'),
        (Field('1gDkL6XR', None, '/banner/', 'parser 0x4CDEA0 stores it at +0x78 (0x4CE1E9); the value is getter '
               '0xF50F0 (+0x78) at 0x8E1B9A; updateEvent 0x8E3FA4 asks for "/banner/" + value'),),
        'FUNC_BANNER_MST',
        'check list 0x8E1B30-0x8E1C3D (every BannerInfoMstList 0x5C78F0 row, key "banner"+id) inside the '
        'FUNC_BANNER_MST block (0x8E1B03: byte 0x184AA67).  The list holds the Initialize response\'s Pk5F1vhx '
        '(ServerCache m_initrsp.banner_info from deploy/mst/banner_info_mst.json), not the client\'s own '
        'F_BANNER_INFO_MST: the C16 client queued exactly its 4 names and none of the 28 other rows its '
        'F_BANNER_INFO_MST copy holds, 11 of which no source has (provider-on runs queue such a name)'),
    DerivedList(
        'sp_challenge_title', 'F_CHLNG_MISSION_MST', ('version', 'F_CHLNG_MISSION_MST'),
        (Field('jsRoN50z', None, '/dungeon/', 'parser 0x5104C0 stores it at +0x6C (0x510727); updateEvent 0x8E4048 '
               'looks the row up and asks for "/dungeon/" + getter 0xF01D0 (+0x6C) at 0x8E4066'),),
        None,
        'check list 0x8E1F27-0x8E206D (every SPChallengeMisMstList 0x5F9E70 row whose +0x6C is not empty, '
        '0x8E1F53, key "sp_challenge_title"+index, value the row id).  The server never sends d8DM0Fvx, so the '
        'list is what DataMstManager loads at boot from the client\'s F_CHLNG_MISSION_MST (0x541444)'),
    DerivedList(
        'raidMap', 'F_RAID_MAP_MST', ('version', 'F_RAID_MAP_MST'),
        (Field('b5VveUd8', None, '/raid/raid_map/', 'map images: parser 0x4FE520 stores it at +0x44; updateEvent '
               '0x8E40C0-0x8E42C6 looks the row up (0x5CAFD0), splits getter 0xEE480 (+0x44) on \',\' (0x8E414D) '
               'and each piece on \':\' (0x8E41C6), and asks for part 0 of a piece with 3 parts (0x8E41F7, '
               'non-empty 0x8E421D)', tiles=True),),
        'FUNC_RAID',
        'check list 0x8E20C0-0x8E21C9 (every row of the copy 0x5F3620 makes of RaidMapMstList 0x5F34A0, key '
        '"raidMap"+id) inside the FUNC_RAID block.  The server never sends K3H0uzhE, so the list is the '
        'client\'s F_RAID_MAP_MST (boot load 0x54168A)'),
    DerivedList(
        'campaignMap', 'F_GRAND_MISSION_MAP_MST', ('version', 'F_GRAND_MISSION_MAP_MST'),
        (Field('b5VveUd8', None, '/raid/raid_map/', 'map images: parser 0x4D0810 stores it at +0x44; updateEvent '
               '0x8E4478-0x8E4672 splits getter 0xEE480 (+0x44) on \',\' (0x8E4505) and \':\' (0x8E4577) and asks '
               'for part 0 of a piece with 3 parts (0x8E45A8, non-empty 0x8E45CE)', tiles=True),),
        'FUNCK_CAMPAIGN',
        'check list 0x8E25A0-0x8E26A2 (every row of the copy 0x5CB100 makes of CampaignMapMstList 0x5CAE50, key '
        '"campaignMap"+id) inside the FUNCK_CAMPAIGN block (0x8E2566).  At login the list is the client\'s '
        'F_GRAND_MISSION_MAP_MST (boot load 0x53F8E6; the server stages it, so the client holds the staged '
        'version); CampaignMissionGet and CampaignStart replace it with deploy/mst/grand_mission_map_mst.json',
        also=(('response', 'grand_mission_map_mst.json', 'iyCP4N1h'),)),
    DerivedList(
        'raid_point_img', 'F_RAID_POINT_MST', ('version', 'F_RAID_POINT_MST'),
        (Field('L8iA9M6c', None, '/dungeon/', 'parser 0x4FECE0 stores it at +0x58 (0x4FF04C); the value is getter '
               '0x179B20 (+0x58) at 0x8E2267; updateEvent 0x8E4337 asks for "/dungeon/" + value'),),
        'FUNC_RAID',
        'check list 0x8E21F1-0x8E22C9 (every RaidMapPointMstList 0x5F3BF0 row, key "raid_point_img"+index, '
        'added by 0x8E28A0) inside the FUNC_RAID block.  The server sends no raid table (ServerCache loads '
        'raid_point_mst.json but no handler sends it), so the rows are the client\'s own F_RAID_POINT_MST'),
    DerivedList(
        'raid_point_bgm', 'F_RAID_POINT_MST', ('version', 'F_RAID_POINT_MST'),
        (Field('3sRN9BPS', None, '/sound/', 'parser 0x4FECE0 stores it at +0x88 (0x4FF149); the value is getter '
               '0xF0290 (+0x88) at 0x8E220E; updateEvent 0x8E42E3 asks for "/sound/" + value', sound=True),),
        'FUNC_RAID',
        'check list 0x8E21F1-0x8E22C9 (every RaidMapPointMstList row, key "raid_point_bgm"+index, added by '
        '0x8E28A0) inside the FUNC_RAID block'),
    DerivedList(
        'general_event', 'M_GENERAL_EVENT_MST', ('response', 'general_event_mst.json', 'Md9gG3c0'),
        (Field('t5R47iwj', 0, '/sound/', 'parser 0x4EDCA0 stores it at +0x34 (0x4EDE89); getResouceFiles 0x5E2270 '
               'asks 0x5E1EB0 for token 0 of its \',\' split (0x5E1F45; past the end an empty string, 0x5E1F9C) '
               'and lists "/sound/" + it (0x5E2338); the check list stores that path as the value and '
               'updateEvent 0x8E4004 requests it through 0x63E1C0 -- no empty check', sound=True, empty_asks=True),),
        None,
        'check list 0x8E1E30-0x8E1ED8 (every path GeneralEventMstList 0x5E2010 -> getResouceFiles 0x5E2270 '
        'returns, key "general_event"+index): only rows whose b4D63uav (+0x2C, parser 0x4EDE05, read with a '
        'stream >> int) is 1 (0x5E2317).  The rows are the UserInfo response\'s Md9gG3c0 (ServerCache '
        'm_userrsp.general_event from deploy/mst/general_event_mst.json; GeneralEventMstResponse replaces '
        'the list)', where=('b4D63uav', 1)),
    DerivedList(
        'sgXbbSkill', 'M_DBB_MST', ('response', 'dbb_mst.json', 'fTV3A0By'),
        (Field('QqfI9mM4', None, '/xbb/', 'dbb cgg: parser 0xDECF0 stores it at +0x44 (0xDEE9A); 0x8E4719 getter '
               '0xEE480 (+0x44)'),
         Field('0QxL2is7', None, '/xbb/', 'dbb cgs: stored at +0x5C (0xDEE27); 0x8E4798 getter 0xEE4C0 (+0x5C)'),
         Field('2EF0d6ue', None, '/xbb/', 'dbb anime sprite sheet: stored at +0x2C (0xDEF1A); 0x8E481D getter '
               '0xEE440 (+0x2C)')),
        None,
        'check list 0x8E26E0-0x8E287B: for every DbbMstList 0xEE540 row, the ids getAllKeys 0xEED20 returns, '
        'each found (0xEEAB0) keyed "sgXbbSkill"+id -- every row once; updateEvent 0x8E467E-0x8E4847 asks for '
        'each non-empty field (0x8E46E4, 0x8E4763, 0x8E47E8) under "/xbb/".  The rows are the UserInfo '
        'response\'s fTV3A0By, sent on every UserInfo (ServerCache dbbMst; DbbMstResponse replaces the list)'),
    DerivedList(
        'raid_boss_bgm', 'F_RAID_BOSS_MST', ('none', 'no check-list key reaches the branch'),
        (),
        'FUNC_RAID',
        'EMPTY in this build: the check list keys each RaidBossMstList 0x5F22B0 row "raidBossBgm"+id (0x8E2334, '
        'value nDf28LtU at +0x74), but updateEvent tests "raid_boss_bgm" (0x8E4375, the literal\'s only use) and '
        'no other branch matches; an unmatched entry goes to the common exit 0x8E484C.  The two writers of the '
        'check-list dictionary are createDownloadCheckList and its helper 0x8E28A0'),
)


# The client's own resourceMap exclusions.  DLCManagerComplementor::loadBundleResourceMap 0x16F7F0
# splits the file on "\n" (0x168BC44), a line on ':' (0x168A218; a line needs two parts, 0x16FA1E)
# and the part after it on ',' (0x168A28C), and keeps a token only if it is not "\r" (0x168F604)
# and equals none of the 203 std::string slots of the table at VA 0x184DF00 (the loop
# 0x16FA90-0x16FB1C compares size, then bytes: exact case).  Static initializers from 0x6B10 fill
# the table; tools/bin/x86_bundle_exclusions.py reads them back.  A dropped name is not in the
# client's in-memory map, so no scene download asks for it (downloadBundleResourcesViaSceneId
# 0x16FE20 -> downloadBundlePriority 0x660870 -> addLoadFilePriority, no other filter).  Slot
# order; two names fill two slots each.  Recovered from this client build only.
BUNDLE_EXCLUSIONS_BUILD = '6253F827-01756000-00ED273D'
BUNDLE_EXCLUSIONS_VA = 0x184DF00
BUNDLE_EXCLUSIONS = tuple('''
    BattleSpark.png BattleCritical.png BattleCritical_cgg.csv BattleCritical_cgs.csv colosseum_base.jpg
    battle_edit_menu_icon.png battle_skill_menu_icon.png fish_park_back_ground.jpg item_frame_
    layout_ChallengeSelect.csv layout_ChallengeSelect.csv icon_reality.png home_leader_mark.png
    header_arena_light.png arena_vs.png item_frame1.png layout_FrontierGateTop.csv
    layout_battle_side_menu.csv layout_multi_unitDeatilVirtuallyInfo.csv leader_skill_label.png
    minigame_top10.png multi_room_orb_01.png multi_top_header.png page_feed_label.png scrl_bar_base.png
    multi_btl_friend_ber.png multi_btl_boss_ber.png multi_battle_plate3.png multi_unit_frame_on_btn.png
    multi_room_orb_00.png mission_result_lvup.png ConvCircleWhite.sam battle_next_btn.png Cutin.sam
    CutinAdd.sam CutinRight.sam CutinRightAdd.sam guild_guardian_select_skill%d_btn_1.png
    guild_guardian_select_skill%d_btn_2.png progress_base.png progress_active.png
    mission_result_get_unit.png colosseum.png main_l_r_btn1_friend_introduction.png
    main_l_r_btn2_friend_introduction.png ConvertType.sam orb4_multi.png trial_label_deck_name_party
    multi_battle_ready.png multi_guild_information_frame_c.png multi_guild_information_frame_b.png
    multi_guild_info_ multi_guild_information_frame_t.png multi_top_header_close.png multi_teammake.txt
    multi_tutorial.txt map_dlc_btn_label1.png map_dlc_btn_label2.png event_token_%04d_item_icon.png
    speardude_3.sam multi_subdue_rank_thum.png multi_subdue_point_thum.png multi_contribution_point_thum.png
    multi_battle_plate1.png battle_auto_btn1_press.png home_win_raid_trial.png BattleUnitGetText.png
    omni_paradigm_0%d.png downloading_icon.sam sub_s_btn1_darken_overlay.png ca_back_button_off.png
    ca_back_button_on.png ca_battle_ui_sg.png ca_claims_current_rank.png ca_claims_total_ranking_point.png
    ca_bg_transition.png ca_claims_get_cap_frame.png ca_close_btn_on.png ca_close_btn_off.png
    ca_current_rank.png ca_daily_ranking_btn_on.png ca_daily_ranking_btn_off.png ca_dailyrank_title.png
    ca_drop_coin_rainbow.png ca_day_ending.png ca_drop_coin_gold.png ca_drop_coin_silver.png
    ca_footer_arenahome_off.png ca_footer_arenahome_on.png ca_footer_base.png ca_footer_gacha.png
    ca_footer_gacha_on.png ca_footer_help_on.png ca_footer_help.png ca_footer_ranking.png
    ca_footer_ranking_on.png ca_footer_shop.png ca_header_notimer.png ca_header.png ca_home_base.png
    ca_legendary_icon.png ca_footer_shop_on.png ca_myrewards_btn_off.png ca_myrewards_btn_on.png
    ca_next_opponent.png ca_opponent_view.png ca_prepare_button_closed.png ca_opponent_panel1.png
    ca_prepare_button_on.png ca_prepare_button_off.png ca_ranking_backing_legendary.png
    ca_ranking_backing_rookie.png ca_ranking_playerhighlight.png ca_ranking_tab_base.png
    ca_ranking_backing_summoner.png ca_ranking_panel.png ca_ranking_tab_frienddeselect.png
    ca_ranking_tab_friendselect.png ca_ranking_tab_framefooter.png ca_ranking_tab_framemiddle.png
    ca_ranking_tab_rivaldeselect.png ca_ranking_tab_rivalselect.png ca_ranking_tab_topdeselect.png
    ca_ranking_tab_topselect.png ca_rewards_daily_btn_off.png ca_rewards_framefooter.png
    ca_rewards_daily_btn_on.png ca_rewards_leagues.png ca_rewards_framemiddle.png ca_rewards_stats.png
    ca_rewards_weekly_btn_off.png ca_rewards_weekly_btn_on.png ca_rule_box.png ca_rookie_icon.png
    ca_summon_btn_off.png ca_summon_btn_on.png ca_summoner_icon.png ca_tier_bar_frame.png ca_tier_bar.png
    ca_tier_next.png ca_total_point.png ca_viewrewards_btn_on.png ca_viewrewards_btn_off.png
    ca_viewunit_btn_off.png ca_weekly_ranking_btn_off.png ca_week_ending.png ca_weekly_ranking_btn_on.png
    ca_weeklyrank_title.png ca_viewunit_btn_on.png frontiergate_op.txt grand_op.txt grand_op2.txt
    raid_01_1.txt raid_01_2.txt raid_02_2.txt raid_02_1.txt 1.png 2.png multi_break_percentage_frame.png
    badge_99plus.png sub_sss_btn_orb_recovery_label2.png sub_sss_btn_orb_recovery_label1.png bonds_spark.png
    ripple.sam guild_event_deck_name_squad trial_label_deck_name_party multi_battle_battle.png
    iphx_footer.png iphx_header.png Loading.sam title2_logo.png gacha_banner_light.png dia_icon.png
    header_back_img.png sub_m_btn_taking_screenshots_label1.png sub_m_btn_taking_screenshots_label2.png
    sub_m_btn_mail_label1.png sub_m_btn_mail_label2.png sale_icon.png header_base_1_img.png
    time_list_frame_light.png time_frame.png navi_chara_50_1_full.png ca_news_board.png
    ca_challenge_button_off.png ca_challenge_button_on.png ca_sort_button_off.png ca_sort_button_on.png
    ca_add_unit_on.png ca_bravebonus_max.png ca_add_unit_off.png ca_bravebonus.png ca_shop_tab_gold.png
    ca_shop_tab_silver.png ca_firststrike.png ca_opponent_panel2.png Bonus-Ad-Spin_A.png Bonus-Ad-Spin_B.png
    Daily-Login-Slot-Machine_BASE.png Daily-Login-Slot-Machine_WHEEL.png Daily-Login-Slot-Machine_TOP.png
    Daily-Free-Spin_A.png Daily-Free-Spin_B.png
'''.split())

# Extensions a provider policy may own once its reader is proven (CONTENT_PROVIDER_WINDOWS_CONTRACT.md
# section 3; every EXE file read is getFileData, the audio reader's fopen_s, or a non-asset fopen).
OWNABLE = ('.png', '.jpg', '.sam', '.plist', '.csv', '.mp3', '.txt', '.fnt', '.gif', '.css', '.cgg', '.cgs')
# Client state the proxy never owns, whatever an index says (cp_core.cpp IsProtectedStateName).
STATE = re.compile(r'(\.dat|\.xml|\.ini|\.tmp|\.json|\.zip)$|^thumbs\.db$|^\.ds_', re.I)
SCRIPT_TOKEN = ('.png', '.jpg', '.mp3', '.plist', '.sam')


# ----------------------------------------------------------------------------- inputs ----

def sha256_file(path: Path) -> str:
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def client_build(exe: Path = APPX / 'BraveFrontier.Windows.exe') -> str:
    """TimeDateStamp-SizeOfImage-AddressOfEntryPoint, as the proxy's kSupportedModules[0] checks them."""
    with open(exe, 'rb') as f:
        head = f.read(4096)
    e_lfanew = struct.unpack_from('<I', head, 0x3C)[0]
    stamp = struct.unpack_from('<I', head, e_lfanew + 8)[0]
    entry = struct.unpack_from('<I', head, e_lfanew + 24 + 16)[0]
    size = struct.unpack_from('<I', head, e_lfanew + 24 + 56)[0]
    return f'{stamp:08X}-{size:08X}-{entry:08X}'


def decrypt_part(raw: bytes, key: str) -> list:
    from Crypto.Cipher import AES
    material = key.encode()[:16].ljust(16, b'\0')
    data = AES.new(material, AES.MODE_ECB).decrypt(base64.b64decode(raw))
    return json.loads(data[:-data[-1]].decode('utf-8'))


def load_version_table(table: str, required: bool = True) -> tuple[int, list, list[str]] | None:
    """(version, rows, part paths) of the newest copy the server can announce: a staged
    content/<TABLE>/ (what the client downloads when it is newer) or deploy/game_content/mst.
    Without any copy: an error, or None when not required."""
    file_key, enc_key = TABLE_KEYS[table]
    versions = collections.defaultdict(list)
    for folder in (CONTENT / table, MST_DIR):
        if not folder.is_dir():
            continue
        for p in folder.iterdir():
            m = re.fullmatch(rf'Ver(\d+)_{re.escape(file_key)}(?:_(\d+))?\.dat', p.name)
            if m:
                versions[int(m.group(1))].append((int(m.group(2) or 0), p))
    if not versions:
        if not required:
            return None
        raise SystemExit(f'{table}: no Ver*_{file_key}*.dat under {CONTENT / table} or {MST_DIR}')
    v = max(versions)
    parts = sorted(versions[v])
    rows = []
    for _n, p in parts:
        rows += decrypt_part(p.read_bytes(), enc_key)
    return v, rows, [str(p) for _n, p in parts]


def bundle_map(appx: Path = APPX, exclusions=BUNDLE_EXCLUSIONS) -> tuple[dict[str, list[str]], dict[str, list[str]]]:
    """(scene -> the names the client keeps, scene -> the names its exclusion table drops), read the
    way loadBundleResourceMap 0x16F7F0 reads the package's resourceMap (see BUNDLE_EXCLUSIONS): the
    raw bytes split on newline, each line's scene and the list after its ':', a token dropped when it
    is a carriage return or an excluded name (exact case).  An empty token names no file."""
    drop = set(exclusions)
    kept, dropped = {}, {}
    raw = (appx / 'Assets' / 'Resources' / 'resourceMap').read_bytes().decode('utf-8', errors='replace')
    for line in raw.split('\n'):
        parts = line.split(':')
        if len(parts) < 2:
            continue
        names = [t for t in parts[1].split(',') if t and t != '\r']
        kept[parts[0]] = [t for t in names if t not in drop]
        dropped[parts[0]] = [t for t in names if t in drop]
    return kept, dropped


def resource_map(appx: Path = APPX) -> dict[str, list[str]]:
    """scene -> the names the client downloads for it (bundle_map's kept names)."""
    return bundle_map(appx)[0]


def package_names() -> set[str]:
    """Flat names any local client package ships (Appx Assets, APK assets): never 'absent'."""
    names = set()
    for root in (APPX / 'Assets', APK_ASSETS):
        if root.is_dir():
            for _dp, _dn, files in os.walk(root):
                names.update(f.lower() for f in files)
    return names


def script_params(data: bytes) -> list[str]:
    """Flat names a script's PARAM lines name, as processSamAndCutSceneFile (0x106B10) finds them."""
    out = []
    for line in data.decode('utf-8', errors='replace').splitlines():
        if 'type=PARAM' not in line:
            continue
        m = re.search(r'param=([^,]*)', line)
        if not m:
            continue
        for token in m.group(1).split(':'):
            token = token.strip()
            if token.lower().endswith(SCRIPT_TOKEN):
                out.append(token.replace('\\', '/').rsplit('/', 1)[-1])
    return out


def fnt_pages(data: bytes) -> list[str]:
    return re.findall(r'page\s+id=\d+\s+file="([^"]+)"', data.decode('utf-8', errors='replace'))


def particle_texture(data: bytes) -> tuple[bool, str | None, bool]:
    """(is a particle system, textureFileName, has embedded textureImageData)."""
    text = reader_view('x.plist', data).decode('utf-8', errors='replace')
    is_particle = '<key>maxParticles</key>' in text or '<key>emitterType</key>' in text
    m = re.search(r'<key>textureFileName</key>\s*<string>([^<]*)</string>', text)
    return is_particle, (m.group(1).strip() or None) if m else None, '<key>textureImageData</key>' in text


# ----------------------------------------------------------------------- dependencies ----

class Content:
    """The served tree, by flat basename, with the dependency rules the client applies."""

    def __init__(self, content: Path = CONTENT, aliases: dict | None = None):
        self.content = content
        self.tree = Tree(content, aliases or {})

    def resolve(self, name: str):
        return self.tree.resolve(name)

    def read(self, rel: str) -> bytes:
        return (self.content / rel).read_bytes()

    def deps(self, name: str) -> tuple[list[str], str | None]:
        """(flat names this file makes the client fetch or read with it, problem or None).

        Every name a file NAMES is returned whether or not it exists: a missing dependency must stay
        a required name -- and a problem in the closure -- never vanish from it (QC 2026-10-20: a
        sprite plist without its texture used to yield neither)."""
        rel, how = self.resolve(name)
        if rel is None:
            return [], None
        low = name.lower()
        if low.endswith('.sam'):
            layout, pages = parse_sam(self.read(rel))
            if pages is None:
                return [], f'unparseable SuperAnim ({layout})'
            return pages, None
        if low.endswith('.txt'):
            return script_params(self.read(rel)), None
        if low.endswith('.fnt'):
            return fnt_pages(self.read(rel)), None
        if low.endswith('.plist'):
            particle, texture, embedded = particle_texture(self.read(rel))
            if particle:
                # cocos2d-x CCParticleSystem::initWithDictionary tries the named texture first
                # (CCTextureCache::addImage) and decodes the embedded textureImageData only when that
                # file cannot be read; nothing downloads a particle's texture by itself.  So WITHOUT
                # embedded data the named file is required whether it exists or not (a missing one is
                # a gap), and WITH embedded data the named file is a dependency when some copy exists
                # (it is what the game draws) and nothing when none does (the embedded copy is drawn):
                # 1,077 such names exist in no source, every one with its plist's embedded texture.
                if texture and (not embedded or texture.lower() in self.tree.where):
                    return [texture], None
                if texture or embedded:
                    return [], None
                return [], 'particle system names no texture and embeds none'
            # A sprite-frame plist: the game names the texture itself (addSpriteFramesWithFile with
            # an explicit texture; the metadata of e.g. footer_btn.plist says "test.png").  By the
            # client's convention the texture is <name>.png beside it -- required whether it exists
            # or not.
            stem = name[:-len('.plist')]
            return [stem + '.png'], None
        return [], None

    def closure(self, names) -> tuple[set[str], dict[str, str]]:
        """Every name reachable through deps -- names that do not resolve included -- and the
        problems met (name -> why): a name with no single servable copy ('missing', a collision)
        is one, so a family that needs it can never look complete."""
        seen, problems = set(), {}
        todo = [n for n in names]
        while todo:
            n = todo.pop()
            key = n.lower()
            if key in seen:
                continue
            seen.add(key)
            rel, how = self.resolve(n)
            if rel is None:
                problems[key] = how
                continue
            more, why = self.deps(n)
            if why:
                problems[key] = why
            todo.extend(more)
        return seen, problems


# ---------------------------------------------------------------------------- manifest ----

def stream_int(value) -> int:
    """A column as the parser's stream >> int reads it (0x65E860): leading blanks skipped, an optional
    sign and the digits that follow; 0 when nothing parses."""
    m = re.match(r'\s*([+-]?\d+)', '' if value is None else str(value))
    return int(m.group(1)) if m else 0


def split_tokens(value) -> list[str]:
    """The client's ',' split (0x236740): no token for an empty or " " value (0x2367B7, 0x2367CE),
    else strtok's tokens -- an empty one is skipped."""
    value = '' if value is None else str(value)
    if value in ('', ' '):
        return []
    return [t for t in value.split(',') if t]


def item_frame_name(row: dict) -> str:
    """ItemMst::getItemFrameName (x86 0x5E4D20 -> 0x5E4D40) for one F_ITEM_MST row: the frame image
    requestItemFiles asks for under "/common/".  The int columns read as stream >> int (0x65E860):
    h0K7wjeH item type (+0x5C), ZKQ1X69a (+0x174), rNmJkH6K (+0x180)."""
    kind = stream_int(row.get('h0K7wjeH'))
    if stream_int(row.get('rNmJkH6K')) == 1 and kind == 1:              # 0x5E4DA1, 0x5E4DB4
        return 'item_frame_raid.png'
    if stream_int(row.get('ZKQ1X69a')) == 1 and kind not in (3, 5):     # 0x5E4DCC-0x5E4DDA
        return 'item_frame_5.png' if kind == 2 else 'item_frame_raid.png'
    if kind in (888001, 888002):                                         # 0x5E4E04 / 0x5E4E0F -> 0x5E4EB6
        return 'item_frame_4.png'
    if kind < 5:                                                         # 0x5E4E1A: "item_frame_" + kind
        return f'item_frame_{kind}.png'
    if kind == 5:                                                        # 0x5E4E56: "item_frame_" + 6
        return 'item_frame_6.png'
    if kind == 6:                                                        # 0x5E4E9C
        return 'item_frame_7.png'
    return '.png'                                                        # nothing assigned; ".png" appended


COMPUTED = {'item_frame': item_frame_name}


def _wrapped_rows(path: Path, key: str) -> list | None:
    data = json.loads(path.read_text(encoding='utf-8')) if path.is_file() else None
    rows = data.get(key) if isinstance(data, dict) else None
    return rows if isinstance(rows, list) else None


def displayable_name(name) -> bool:
    """ServerCache.cpp IsDisplayableName: no byte above 0x7F and no run of three '?'."""
    run = 0
    for b in ('' if name is None else str(name)).encode('utf-8'):
        if b > 0x7F:
            return False
        run = run + 1 if b == ord('?') else 0
        if run >= 3:
            return False
    return True


def _stoi(value) -> int | None:
    """std::stoi: leading blanks, a sign, the digits that follow; None where it throws."""
    m = re.match(r'\s*([+-]?\d+)', '' if value is None else str(value))
    return int(m.group(1)) if m else None


VORTEX_LAND, VORTEX_AREA_MAX = 99, 200000          # ServerCache.cpp kVortexLandId, kVortexAreaIdMax
PARADE_GARDEN, PARADE_GARDEN_ORDER = 100600, 99999  # kParadeGardenAreaId, kParadeGardenDispOrder


def served_area_rows(response_root: Path = RESPONSE_MST, banner_dir: Path | None = BANNER_DIR) -> list[dict] | None:
    """The area table UserInfo sends at 3SG2wX0R: ServerCache::Setup's clientAreaMst, ported exactly from
    ServerCache.cpp ("Which of the 88 Vortex areas are worth a tile", "The area table the client is
    served").  From the server's own area_mst.json and dungeon_mst.json (under its mst_root): every row in
    file order except the Vortex areas (land 99, id below 200000) that are not a parent category and have
    no dungeon, a name that is not displayable (theirs or a dungeon's), or a dungeon banner set already
    seen (in (display_order, area_id) order); Parade Garden floated to 99999; a plate_img "<x>.png" whose
    "<x>_EN.png" exists in the server's banner folder (LocalisedBanner, probed once at boot) replaced by it.
    The folder is relative to the server's working folder: a server run from a fixture folder (every
    isolated test server, the client harness) finds none -- `banner_dir` None -- and serves the original
    plates (the C16 provider-off client queued sp_quest_banner_frog.png, never the _EN copy), while the
    live server in deploy/ serves the translated ones.  Rows carry the values as served (ids and types as
    decimal strings).  None without either table."""
    areas = _wrapped_rows(Path(response_root) / 'area_mst.json', '3SG2wX0R')
    dungeons = _wrapped_rows(Path(response_root) / 'dungeon_mst.json', '01pzrTCm')
    if areas is None or dungeons is None:
        return None
    vortex_dungeons = collections.defaultdict(list)
    for d in dungeons:
        if int(d['9C64Qwe0']) == VORTEX_LAND and int(d['VjCY7rX4']) < VORTEX_AREA_MAX:
            vortex_dungeons[int(d['VjCY7rX4'])].append(d)
    vortex = sorted((a for a in areas if int(a['9C64Qwe0']) == VORTEX_LAND and int(a['VjCY7rX4']) < VORTEX_AREA_MAX),
                    key=lambda a: (int(a['XuJL4pc5']), int(a['VjCY7rX4'])))
    parents = set()
    for a in areas:
        parent = a.get('sHMai2ZG')
        if parent in (None, '', '0'):
            continue
        n = _stoi(parent)
        if n is not None:
            parents.add(n)
    hidden, seen = set(), set()
    for a in vortex:
        area_id = int(a['VjCY7rX4'])
        if area_id in parents:
            continue
        ds = vortex_dungeons.get(area_id)
        if not ds:
            hidden.add(area_id)
            continue
        if not (displayable_name(a.get('V84mzqoX')) and all(displayable_name(d.get('bWsLFP96')) for d in ds)):
            hidden.add(area_id)
            continue
        banners = tuple(sorted(str(d.get('21VKZo0E') or '') for d in ds))
        if banners in seen:
            hidden.add(area_id)
            continue
        seen.add(banners)
    out = []
    for a in areas:
        area_id = int(a['VjCY7rX4'])
        if area_id in hidden:
            continue
        row = dict(a)
        row['VjCY7rX4'], row['3v1qg7Uj'] = str(area_id), str(int(a['3v1qg7Uj']))
        row['b5VveUd8'] = str(a.get('b5VveUd8') or '')
        plate = str(a.get('H0UB8pqm') or '')
        if plate.endswith('.png') and banner_dir is not None and (Path(banner_dir) / (plate[:-4] + '_EN.png')).exists():
            plate = plate[:-4] + '_EN.png'
        row['H0UB8pqm'] = plate
        if area_id == PARADE_GARDEN:
            row['XuJL4pc5'] = str(PARADE_GARDEN_ORDER)
        out.append(row)
    return out


# A response the server synthesizes at boot: key -> (the rows' builder, the columns the client's
# request reads, in projection order).  ServerCache records the SHA-256 of served_projection over the
# rows it serves (StartupProjection in ServerCache.cpp, the same bytes): the manifest binds to that,
# never to the files the rows were built from.
SERVED_SOURCES = {'3SG2wX0R': (served_area_rows, ('VjCY7rX4', '3v1qg7Uj', 'b5VveUd8', 'H0UB8pqm'))}


def _projection_text(value) -> str:
    return ('' if value is None else str(value)).replace('\\', '\\\\').replace('\t', '\\t').replace(
        '\n', '\\n').replace('\r', '\\r')


def served_projection(key: str, rows: list[dict]) -> bytes:
    """The bytes the server hashes for a synthesized response: a header line "<key> <col>,<col>,..." and
    one tab-separated line per served row, in served order, each value as served ('\\', tab, CR and LF
    escaped)."""
    columns = SERVED_SOURCES[key][1]
    lines = [f'{key} {",".join(columns)}'] + ['\t'.join(_projection_text(r.get(c)) for c in columns) for r in rows]
    return ('\n'.join(lines) + '\n').encode('utf-8')


def _field_names(f: Field, row: dict, where: str, names: list, problems: list) -> None:
    """The names one field of one row asks for (derived_names), appended to `names` as (name, field) --
    a field whose folder depends on the row comes back with the folder that row uses."""
    if f.folder_by:
        column, folders, default = f.folder_by
        f = dataclasses.replace(f, folder=folders.get(stream_int(row.get(column)), default), folder_by=None)
    value = COMPUTED[f.computed](row) if f.computed else row.get(f.column)
    if f.tiles:
        for piece in ('' if value is None else str(value)).split(','):
            parts = piece.split(':')
            if len(parts) == 3 and parts[0]:
                names.append((parts[0].rsplit('/', 1)[-1], f))
        return
    if f.token is None:
        v = '' if value is None else str(value)
    else:
        tokens = split_tokens(value)
        if f.token >= len(tokens):
            if not f.guarded:
                problems.append(f'{where}: {f.column} has no token {f.token}, which the parser reads unchecked')
            elif f.empty_asks:
                problems.append(f'{where}: {f.column} has no token {f.token}: the client asks for {f.folder} '
                                'itself, a request no name can stand for')
            return
        v = tokens[f.token]
    if not v:
        if f.empty_asks:
            problems.append(f'{where}: {f.column} is empty: the client asks for {f.folder} itself')
        return
    name = v.rsplit('/', 1)[-1]
    if not name:
        problems.append(f'{where}: {f.column} names a folder ({v!r}), not a file')
    elif ('.mp3' in name.lower() or '.wav' in name.lower()) and not f.sound:
        problems.append(f'{where}: {f.column} names a sound ({name}), which 0x238870 checks first')
    else:
        names.append((name, f))


def derived_names(rule: DerivedList, rows: list, joined: dict | None = None) -> tuple[list[tuple[str, Field]], list[str]]:
    """([(flat name, the field naming it)], problems) for one DERIVED list over the rows the client holds.

    A row without the column keeps the field's default, an empty string: nothing is asked for.  A
    problem is a row whose request the rule cannot know -- a token the parser takes unchecked that
    is not there (undefined behaviour in the client), a field that ends in '/', a sound (0x63E1C0
    checks a .mp3 / .wav with 0x238870 before queueing it; not modelled) -- and keeps the list unknown.
    `joined` holds each Join's rows by index; a joined row asks for its fields once per list row it
    belongs to."""
    names, problems = [], []
    ids = collections.Counter(str(r.get(rule.unique)) for r in rows if isinstance(r, dict)) if rule.unique else {}
    for i, row in enumerate(rows):
        if not isinstance(row, dict):
            problems.append(f'row {i} is not an object')
            continue
        if rule.where and stream_int(row.get(rule.where[0])) != rule.where[1]:
            continue
        if rule.unique and ids[str(row.get(rule.unique))] > 1:
            problems.append(f'row {i}: id {row.get(rule.unique)!r} appears {ids[str(row.get(rule.unique))]} times; '
                            'which row the client keeps is not modelled')
        for f in rule.fields:
            _field_names(f, row, f'row {i}', names, problems)
        for j, join in enumerate(rule.join):
            link = '' if row.get(join.parent) is None else str(row.get(join.parent))
            for k, jrow in enumerate((joined or {}).get(j) or []):
                if not isinstance(jrow, dict):
                    problems.append(f'joined row {k} is not an object')
                    continue
                if ('' if jrow.get(join.column) is None else str(jrow.get(join.column))) != link:
                    continue
                for f in join.fields:
                    _field_names(f, jrow, f'row {i} joined row {k}', names, problems)
    return names, problems


def field_text(f: Field) -> str:
    return f'{f.column}{"" if f.token is None else f"[{f.token}]"} -> {f.folder}'


def build_manifest(content: Content, *, scene: list[str], res_paths: list[str], packaged: set[str],
                   categories=CATEGORIES, derived=(), scene_dropped=(), build: str,
                   inputs: dict | None = None) -> tuple[dict, dict]:
    """(manifest, report) from explicit inputs -- testable on any tree.

    Required names come from the client's own lists (`scene` = resourceMap 999999 as the client
    keeps it, `res_paths` = F_RESOURCE_MST, `derived` = [(DerivedList, the rows the client holds or
    None)]) and everything those files name, existing or not (Content.closure).  A list whose names
    are only ENUMERATED from existing files -- the other createDownloadCheckList categories --
    cannot show what is missing from it, so each is an `unknown` list: the report stays incomplete
    until its exact names are derived (QC 2026-10-20).  Its existing files are still required.  So
    is a derived list without its table, or with a row the rule cannot read.
    A required name in no source is kept, noted in `absent`: a gap, not an exemption.
    `scene_dropped` are the 999999 names the client's exclusion table drops: excluded with that
    reason unless another list requires them."""
    provenance = collections.defaultdict(set)   # lower name -> {source, ...}
    login, background = set(), set()
    unknown = []

    login_closure, login_problems = content.closure(scene)
    for n in scene:
        provenance[n.lower()].add('resourceMap 999999')
    for n in login_closure - {s.lower() for s in scene}:
        provenance[n].add('dependency of the 999999 bundle (SAM page / script param / font page / plist texture)')
    login |= login_closure

    for p in res_paths:
        name = p.rsplit('/', 1)[-1]
        background.add(name.lower())
        provenance[name.lower()].add('F_RESOURCE_MST')
    res_closure, res_problems = content.closure([p.rsplit('/', 1)[-1] for p in res_paths])
    for n in res_closure - {p.rsplit('/', 1)[-1].lower() for p in res_paths}:
        provenance[n].add('dependency of an F_RESOURCE_MST file')
    background |= res_closure

    category_report = []
    where = content.tree.where
    for keys, mst, helper, folders, *observed in categories:
        names = set()
        for name, rels in where.items():
            if any(r.rsplit('/', 1)[0] == f or r.startswith(f + '/') for r in rels for f in folders):
                names.add(name)
        # Names client evidence shows the list asks for (a lower bound, never exhaustive): required too.
        names |= {n.lower() for n in (observed[0] if observed else ())}
        closure, problems = content.closure(names)
        for n in closure:
            provenance[n].add(f'category {keys}')
        background |= closure
        res_problems.update(problems)
        unknown.append(f'category {keys} ({mst}; {helper}): names enumerated from the existing files of '
                       f'{", ".join(folders)}' + (f' plus {len(observed[0])} the client queued' if observed else '')
                       + ', not derived from the MST ids -- a missing file cannot show')
        category_report.append({'keys': keys, 'mst': mst, 'helper': helper, 'folders': list(folders),
                                'names': len(closure)})

    derived_report = []
    for entry in derived:
        rule, rows, joined = (tuple(entry) + (None,))[:3]
        joined = joined or {}
        if rows is None or any(joined.get(j) is None for j in range(len(rule.join))):
            unknown.append(f'category {rule.key} ({rule.table}): derived from the client\'s rows, but no copy of '
                           'the table' + (' or of a table it walks' if rule.join else '') + ' was found')
            derived_report.append({'key': rule.key, 'table': rule.table, 'rows': None})
            continue
        named, why = derived_names(rule, rows, joined)
        for name, f in named:
            provenance[name.lower()].add(f'category {rule.key} ({rule.table} {field_text(f)})')
        closure, problems = content.closure([name for name, _f in named])
        for n in closure - {name.lower() for name, _f in named}:
            provenance[n].add(f'dependency of a category {rule.key} file')
        background |= closure
        res_problems.update(problems)
        if why:
            unknown.append(f'category {rule.key} ({rule.table}): {len(why)} row(s) whose request the client\'s '
                           f'rule cannot give, e.g. {why[0]}')
        derived_report.append({'key': rule.key, 'table': rule.table, 'rows': len(rows),
                               'fields': [field_text(f) for f in rule.fields], 'gate': rule.gate,
                               'names': len({name.lower() for name, _f in named}), 'closure': len(closure),
                               'problems': why})

    excluded, absent = {}, {}
    for name in sorted(login | background):
        if name.startswith('.ds_') or name == 'thumbs.db':
            excluded[name] = ('the client reports it present itself: the downloaded-name lookup 0xE417E0 '
                              'answers yes for Thumbs.db and .DS_* (0xE41814, 0xE4182D)')
        elif name.endswith('.zip'):
            excluded[name] = ('text-dictionary channel: LoginScene::requestTextDictionary downloads and the ZIP '
                              'extractor 0x391EE0 unpacks it during FILE_CHECK (client state; never owned)')
        elif STATE.search(name):
            excluded[name] = ('client-state extension the proxy never owns (cp_core.cpp IsProtectedStateName); '
                              'an F_RESOURCE_MST entry with no reader in the client')
        elif name not in where:
            absent[name] = ('shipped in the app package, absent from game_content' if name in packaged
                            else 'absent from game_content, the Appx package and the APK assets')
    login -= set(excluded)
    background -= set(excluded)
    # Not asked for by the login bundle at all (bundle_map): recorded with the reason, unless another
    # list requires the name -- an exclusion never hides a required name.
    dropped_all = {n.lower() for n in scene_dropped}
    required_elsewhere = sorted(dropped_all & (login | background))
    dropped = sorted(dropped_all - login - background - set(excluded))
    for name in dropped:
        excluded[name] = ('the client drops it from resourceMap scene 999999 itself: its exclusion table (VA '
                          '0x184DF00) in loadBundleResourceMap 0x16F7F0, so no scene download asks for it')

    manifest = {
        'format': FORMAT,
        'build': build,
        'generated': datetime.datetime.now().astimezone().isoformat(timespec='seconds'),
        'inputs': dict(inputs or {}, content_files=sum(len(v) for v in where.values()),
                       generator='scripts/startup_coverage.py'),
        'sets': {'login': sorted(login), 'background': sorted(background)},
        'unknown': unknown,
        'excluded': excluded,
        'absent': {n: why for n, why in absent.items() if n in login or n in background},
    }
    report = {
        'build': build,
        'login': len(login), 'background': len(background),
        'excluded': len(excluded), 'absent': len(manifest['absent']), 'unknown': unknown,
        'categories': category_report,
        'derived': derived_report,
        'login_bundle_dropped': {'names': len(dropped_all), 'excluded': len(dropped),
                                 'required_elsewhere': required_elsewhere},
        'dependency_problems': {**login_problems, **res_problems},
        'provenance': {n: sorted(v) for n, v in sorted(provenance.items())},
    }
    return manifest, report


def version_identity(table: str, loaded) -> dict:
    version, rows, parts = loaded
    return {'version': version, 'file_key': TABLE_KEYS[table][0], 'rows': len(rows),
            'copy': 'staged' if Path(parts[0]).parent.name == table else 'game_content/mst',
            'parts': [Path(p).name for p in parts]}


def source_rows(source: tuple, response_root: Path = RESPONSE_MST,
                banner_dir: Path | None = BANNER_DIR) -> tuple[list | None, list[dict], dict]:
    """(rows -- None without a copy --, the files they are read from as inputs entries, identity) of
    one DerivedList source.  A version table is the copy the server can announce, which the installed
    client keeps byte for byte under LocalState (inputs.client_files says which); a response table is
    what the server sends from its mst_root, `response_root` (inputs.server_files); a served table is the
    one the server synthesizes at boot from its mst_root (and `banner_dir`), bound by the digest of its
    served projection (root "served", the response key)."""
    if source[0] == 'none':
        return [], [], {'empty': source[1]}
    if source[0] == 'version':
        loaded = load_version_table(source[1], required=False)
        if loaded is None:
            return None, [], {'missing': True}
        files = [{'root': 'localstate', 'path': Path(p).name, 'sha256': sha256_file(p)} for p in loaded[2]]
        return loaded[1], files, version_identity(source[1], loaded)
    if source[0] == 'served':
        rows = SERVED_SOURCES[source[1]][0](response_root, banner_dir)
        if rows is None:
            return None, [], {'missing': True}
        digest = hashlib.sha256(served_projection(source[1], rows)).hexdigest()
        return rows, [{'root': 'served', 'path': source[1], 'sha256': digest}], {'served': source[1], 'rows': len(rows)}
    path = Path(response_root) / source[1]
    data = json.loads(path.read_text(encoding='utf-8')) if path.is_file() else None
    rows = data.get(source[2]) if isinstance(data, dict) else None
    if not isinstance(rows, list):
        return None, [], {'missing': True}
    return (rows, [{'root': 'mst', 'path': source[1], 'sha256': sha256_file(path)}],
            {'response_key': source[2], 'rows': len(rows)})


def derived_rows(rule: DerivedList, response_root: Path = RESPONSE_MST,
                 banner_dir: Path | None = BANNER_DIR) -> tuple[list | None, list[dict], dict]:
    """(the rows a DERIVED list can hold -- its source's, then each `also` source's; None when any is
    missing --, their files as inputs entries, the table's identity)."""
    rows, files, identity = source_rows(rule.source, response_root, banner_dir)
    for extra in rule.also:
        more, more_files, more_identity = source_rows(extra, response_root, banner_dir)
        identity = dict(identity, also=identity.get('also', []) + [more_identity])
        files = files + more_files
        rows = None if rows is None or more is None else rows + more
    return rows, files, identity


def joined_rows(rule: DerivedList, response_root: Path = RESPONSE_MST,
                banner_dir: Path | None = BANNER_DIR) -> tuple[dict, list[dict], list[dict]]:
    """({join index: rows or None}, their files as inputs entries, their identities) for a list's joins."""
    joined, files, identities = {}, [], []
    for j, join in enumerate(rule.join):
        rows, more, identity = source_rows(join.source, response_root, banner_dir)
        joined[j], files, identities = rows, files + more, identities + [identity]
    return joined, files, identities


def response_tables() -> list[str]:
    """The server tables a DERIVED list reads from a response (its source or an `also` source), in DERIVED
    order -- the files server_sources names.  The server's own list of them is ServerCache::startupTables
    (gimuserver/drogon/ServerCache.cpp); the two are compared live by scripts/test_content_server_tables.py."""
    files = []
    for rule in DERIVED:
        for source in (rule.source,) + rule.also:
            if source[0] == 'response' and source[1] not in files:
                files.append(source[1])
    return files


#: inputs.server_files roots: a file under the server's mst_root, or a response it synthesizes at boot.
SERVER_ROOTS = ('mst', 'served')


def server_input_name(root: str, path: str) -> str:
    """How the broker names a server input in a reason and in served: a file by its name, a served
    projection as "served:<response key>"."""
    return path if root == 'mst' else f'{root}:{path}'


def server_sources() -> list[str]:
    """Every server input a manifest must name, as server_input_name gives it: the response tables, then
    the synthesized responses (Session 19: served:3SG2wX0R, the area table)."""
    served = []
    for rule in DERIVED:
        for source in (rule.source,) + rule.also:
            if source[0] == 'served' and server_input_name('served', source[1]) not in served:
                served.append(server_input_name('served', source[1]))
    return response_tables() + served


def server_file_entries(served: dict[str, str]) -> list[dict]:
    """inputs.server_files entries naming exactly `served` (server_input_name -> sha256)."""
    out = []
    for name, digest in served.items():
        root, _sep, path = name.partition(':') if ':' in name else ('mst', '', name)
        out.append({'root': root, 'path': path, 'sha256': digest})
    return out


def recorded_client_parts(tables) -> list[str]:
    """The LocalState parts inputs.tables records a derivation read (a version table's own `parts`, its
    `also` sources' and its joined tables'): each must be bound in inputs.client_files."""
    out = []

    def walk(identity):
        if isinstance(identity, dict):
            out.extend(p for p in identity.get('parts') or [] if isinstance(p, str))
            for key in ('also', 'joined'):
                for more in identity.get(key) or []:
                    walk(more)
    for identity in (tables or {}).values() if isinstance(tables, dict) else []:
        walk(identity)
    return out


def served_tables(mst_root: Path = RESPONSE_MST, banner_dir: Path | None = BANNER_DIR) -> dict[str, str]:
    """{server_input_name: sha256} of what a server started now over `mst_root` (and probing `banner_dir`)
    serves: ServerCache parses each response table once at boot and synthesizes the served tables then, and
    keeps both for the life of the process, so these are the bytes a broker started now binds the manifest
    to.  A table the folder lacks is left out (such a server does not start)."""
    out = {}
    for name in response_tables():
        path = Path(mst_root) / name
        if path.is_file():
            out[name] = sha256_file(path)
    for key, (build, _columns) in SERVED_SOURCES.items():
        rows = build(mst_root, banner_dir)
        if rows is not None:
            out[server_input_name('served', key)] = hashlib.sha256(served_projection(key, rows)).hexdigest()
    return out


def server_binding(manifest: dict, served: dict[str, str]) -> str:
    """Why `manifest` is not bound to the response tables a server serves (`served`: server_input_name ->
    sha256, from served_tables or the broker's /status T rows); '' when it is.  ContentBroker's
    bindServerTables, word for word: every inputs.server_files entry must name a table the server serves to
    a startup list, with the digest of the bytes it serves, and every such table must be named.  A manifest
    naming none is unbound -- an old manifest is never trusted as complete."""
    if not served:
        return ('the server recorded none of the response tables the startup lists read: there is nothing to '
                'bind the manifest to')
    files = (manifest.get('inputs') or {}).get('server_files') or []
    if not files:
        return ("the manifest names none of the server's response tables (inputs.server_files): it cannot be "
                "bound to what this server's responses serve")
    by_lower = {name.lower(): (name, digest) for name, digest in served.items()}
    named, items = set(), []
    for f in files:
        shown = server_input_name(f['root'], f['path'])
        low = shown.lower()
        named.add(low)
        if low not in by_lower:
            items.append((low, f'{shown} is not a table any response serves to a startup list'))
        elif by_lower[low][1] != f['sha256']:
            items.append((low, f'{shown}: derived from sha256:{f["sha256"][:16]}..., served '
                               f'sha256:{by_lower[low][1][:16]}...'))
    for low, (name, digest) in by_lower.items():
        if low not in named:
            items.append((low, f'{name}: not named (served sha256:{digest[:16]}...)'))
    if not items:
        return ''
    return ('the manifest does not match the response tables this server serves: '
            + '; '.join(text for _low, text in sorted(items)))


def exclusions_identity() -> dict:
    return {'build': BUNDLE_EXCLUSIONS_BUILD, 'table_va': f'{BUNDLE_EXCLUSIONS_VA:#X}', 'slots': len(BUNDLE_EXCLUSIONS),
            'names': len(set(BUNDLE_EXCLUSIONS)),
            'sha256': hashlib.sha256('\n'.join(BUNDLE_EXCLUSIONS).encode()).hexdigest()}


def derive_manifest(content: Content, response_root: Path = RESPONSE_MST,
                    banner_dir: Path | None = BANNER_DIR) -> tuple[dict, dict]:
    """(manifest, report) for the supported client -- see the module docstring and build_manifest.
    `response_root` is the mst_root of the server the manifest is for (its response tables), `banner_dir`
    the folder that server probes for translated area banners (served_area_rows)."""
    build = client_build()
    if build != BUNDLE_EXCLUSIONS_BUILD:
        # Every RVA above, and the exclusion table, were read from this build.
        raise SystemExit(f'client build {build} is not {BUNDLE_EXCLUSIONS_BUILD}, the build the startup rules were '
                         'recovered from')
    kept, dropped = bundle_map()
    res = load_version_table('F_RESOURCE_MST')
    client_files = [{'root': 'package', 'path': 'Assets/Resources/resourceMap',
                     'sha256': sha256_file(APPX / 'Assets' / 'Resources' / 'resourceMap')}]
    client_files += [{'root': 'localstate', 'path': Path(p).name, 'sha256': sha256_file(p)} for p in res[2]]
    server_files, tables, derived = [], {'F_RESOURCE_MST': version_identity('F_RESOURCE_MST', res)}, []
    for rule in DERIVED:
        rows, files, identity = derived_rows(rule, response_root, banner_dir)
        joined, more, joined_identity = joined_rows(rule, response_root, banner_dir)
        derived.append((rule, rows, joined))
        tables[rule.table] = dict(identity, joined=joined_identity) if rule.join else identity
        for f in files + more:
            # One entry per file: two lists may read the same table (raid_point_img and _bgm).
            listed = server_files if f['root'] in SERVER_ROOTS else client_files
            if not any(g['root'] == f['root'] and g['path'].lower() == f['path'].lower() for g in listed):
                listed.append(f)
    return build_manifest(
        content, scene=kept.get('999999', []), res_paths=[r['ZwstRU92'] for r in res[1]],
        packaged=package_names(), derived=derived, scene_dropped=dropped.get('999999', []), build=build,
        inputs={'client_files': client_files, 'server_files': server_files, 'tables': tables,
                'bundle_exclusions': exclusions_identity()})


# The strict schema the broker's loadManifest (gimuserver/content/ContentBroker.cpp) applies; the
# offline evaluation must refuse exactly what the server refuses.
MANIFEST_KEYS = {'format', 'build', 'sets', 'unknown', 'excluded', 'absent', 'inputs', 'generated'}
STARTUP_SETS = ('background', 'login')


def protocol_text(s) -> bool:
    """ContentBroker's safeProtocolText: a non-empty string without control characters."""
    return isinstance(s, str) and bool(s) and not any(ord(c) < 0x20 or ord(c) == 0x7F for c in s)


def manifest_flat_name(s) -> bool:
    return protocol_text(s) and '/' not in s and '\\' not in s


INPUT_ROOTS = ('localstate', 'package')


def safe_input_path(root: str, path) -> bool:
    """An inputs.client_files / server_files path as the broker's loadManifest and the DLL's SafeInputPath
    accept it: '/'-separated names, each 1-255 characters, not "." or "..", no control character or
    \\ : * ? " < > |, not ending in "." or " "; 260 characters at most; a LocalState input and a server
    table (a file directly under mst_root) are a single name."""
    if not isinstance(path, str) or not path or len(path) > 260 or (root in ('localstate',) + SERVER_ROOTS and '/' in path):
        return False
    return all(seg and len(seg) <= 255 and seg not in ('.', '..') and not seg.endswith(('.', ' '))
               and not any(ord(c) < 0x20 or ord(c) == 0x7F or c in '\\:*?"<>|' for c in seg)
               for seg in path.split('/'))


def validate_manifest(manifest) -> list[str]:
    """Why the broker would refuse this manifest (empty: it would load it)."""
    if not isinstance(manifest, dict):
        return ['not a JSON object']
    errors = [f'unknown key {k}' for k in manifest if k not in MANIFEST_KEYS]
    if manifest.get('format') != FORMAT:
        errors.append(f'format is not "{FORMAT}"')
    if not protocol_text(manifest.get('build')):
        errors.append('names no client build')
    sets = manifest.get('sets')
    lowered = {}
    if not isinstance(sets, dict):
        errors.append('sets is not an object')
    else:
        errors += [f'unknown set {s}' for s in sets if s not in STARTUP_SETS]
        for s in STARTUP_SETS:
            names = sets.get(s)
            if not isinstance(names, list) or not names:
                errors.append(f'lacks the required non-empty set {s}')
                continue
            if not all(manifest_flat_name(n) for n in names):
                errors.append(f'set {s} holds a non-flat name')
                continue
            low = [n.lower() for n in names]
            if len(set(low)) != len(low):
                errors.append(f'set {s} names a file twice')
            lowered[s] = set(low)
    unknown = manifest.get('unknown')
    if unknown is not None and (not isinstance(unknown, list) or not all(protocol_text(u) for u in unknown)):
        errors.append('unknown is not a list of descriptions')
    for key in ('excluded', 'absent'):
        entries = manifest.get(key)
        if entries is None:
            continue
        if not isinstance(entries, dict) or not all(manifest_flat_name(n) and isinstance(why, str) and why
                                                     for n, why in entries.items()):
            errors.append(f'{key} entry is malformed (a flat name and its reason)')
    required = set().union(*lowered.values()) if lowered else set()
    for name in (manifest.get('excluded') or {}) if isinstance(manifest.get('excluded'), dict) else []:
        if name.lower() in required:
            errors.append(f'both requires and excludes {name}')
    for name in (manifest.get('absent') or {}) if isinstance(manifest.get('absent'), dict) else []:
        if lowered and name.lower() not in required:
            errors.append(f'notes {name} as absent, but no set requires it')
    if manifest.get('inputs') is not None and not isinstance(manifest.get('inputs'), dict):
        errors.append('inputs is not an object')
    files = manifest['inputs'].get('client_files') if isinstance(manifest.get('inputs'), dict) else None
    if files is not None:
        if not isinstance(files, list):
            errors.append('inputs.client_files is not an array')
        else:
            seen = set()
            for f in files:
                ok = (isinstance(f, dict) and set(f) == {'root', 'path', 'sha256'}
                      and all(isinstance(v, str) for v in f.values()) and f['root'] in INPUT_ROOTS
                      and safe_input_path(f['root'], f['path']) and re.fullmatch(r'[0-9a-f]{64}', f['sha256']))
                key = (f['root'], f['path'].lower()) if ok else None
                if not ok or key in seen:
                    errors.append('inputs.client_files entry is malformed or repeated')
                    break
                seen.add(key)
    # inputs.server_files: the server's response tables, each a file directly under its mst_root.
    files = manifest['inputs'].get('server_files') if isinstance(manifest.get('inputs'), dict) else None
    if files is not None:
        if not isinstance(files, list):
            errors.append('inputs.server_files is not an array')
        else:
            seen = set()
            for f in files:
                ok = (isinstance(f, dict) and set(f) == {'root', 'path', 'sha256'}
                      and all(isinstance(v, str) for v in f.values()) and f['root'] in SERVER_ROOTS
                      and safe_input_path(f['root'], f['path']) and re.fullmatch(r'[0-9a-f]{64}', f['sha256']))
                key = server_input_name(f['root'], f['path']).lower() if ok else None
                if not ok or key in seen:
                    errors.append('inputs.server_files entry is malformed or repeated')
                    break
                seen.add(key)
    # Session 19: a client table the derivation recorded reading must be bound (an omitted binding).
    inputs = manifest.get('inputs') if isinstance(manifest.get('inputs'), dict) else {}
    tables = inputs.get('tables')
    if tables is not None and not isinstance(tables, dict):
        errors.append('inputs.tables is not an object')
    bound = {f['path'].lower() for f in inputs.get('client_files') or []
             if isinstance(f, dict) and f.get('root') == 'localstate' and isinstance(f.get('path'), str)}
    for part in recorded_client_parts(tables):
        if part.lower() not in bound:
            errors.append(f'inputs.tables records reading {part}, which inputs.client_files does not bind')
            break
    return errors


# ------------------------------------------------------------------- policy evaluation ----

def owned_names(policy: dict, content: Content) -> dict[str, str]:
    """lower name -> served relative path, as ContentBroker::recomputeOwned resolves the policy:
    in scope (exact path or glob), an allowed extension, not excluded, one copy (or identical copies,
    or an alias), and every member of every bundle it belongs to servable (fixpoint)."""
    exact = {p.lower() for p in policy.get('scope', []) if '*' not in p and '?' not in p}
    globs = [p for p in policy.get('scope', []) if '*' in p or '?' in p]
    exts = {e.lower() for e in policy.get('extensions', [])}
    excludes = policy.get('exclude', [])
    aliases = {k.lower(): v for k, v in policy.get('aliases', {}).items()}

    def in_scope(rel):
        low = rel.lower()
        return low in exact or any(fnmatch(low, g.lower()) for g in globs)

    owned = {}
    for name, rels in content.tree.where.items():
        if os.path.splitext(name)[1] not in exts or any(fnmatch(name, x.lower()) for x in excludes):
            continue
        scoped = [r for r in rels if in_scope(r)]
        if name in aliases:
            target = aliases[name]
            if target in rels and in_scope(target):
                owned[name] = target
            continue
        if not scoped:
            continue
        rel, how = content.resolve(name)
        if rel is None:
            continue
        owned[name] = scoped[0]
    bundles = {b: [m.lower() for m in ms] for b, ms in policy.get('bundles', {}).items()}
    withdrew = True
    while withdrew:
        withdrew = False
        for members in bundles.values():
            if any(m in owned for m in members) and not all(m in owned for m in members):
                for m in members:
                    if owned.pop(m, None) is not None:
                        withdrew = True
    return owned


def evaluate(policy: dict, manifest: dict, content: Content, served: dict[str, str] | None = None) -> dict:
    """The broker's coverage verdict, offline: the same schema checks (validate_manifest), the same
    classes in the same order (ContentBroker::coverageText) -- owned; ambiguous (a collision);
    unavailable (a bundle member the policy declares but cannot serve); unowned (in the tree, not
    owned); absent (in no source: a gap like the others); missing -- and incomplete while any list
    is unknown, or while the manifest is not bound to the response tables the server serves
    (`served`, file -> sha256; served_tables() of deploy/mst unless given)."""
    errors = validate_manifest(manifest)
    out = {'manifest_errors': errors, 'unknown': list(manifest.get('unknown') or []) if not errors else [],
           'sets': {}}
    if errors:
        out.update(owned=0, complete=False)
        return out
    owned = owned_names(policy, content)
    declared = {m.lower() for ms in policy.get('bundles', {}).values() for m in ms}
    where = content.tree.where
    absent = {n.lower() for n in manifest.get('absent', {})}
    out['owned'] = len(owned)
    # The broker never calls a manifest complete that names none of the client's own files
    # (inputs.client_files): nothing would tie it to the tables a client holds.
    out['inputs'] = len((manifest.get('inputs') or {}).get('client_files') or [])
    out['server_binding'] = server_binding(manifest, served_tables() if served is None else served)
    complete = not out['server_binding'] and not out['unknown'] and out['inputs'] > 0
    for set_name, names in manifest['sets'].items():
        classes = collections.defaultdict(list)
        for n in (x.lower() for x in names):
            if n in owned:
                classes['ok'].append(n)
            elif n in where and content.resolve(n)[0] is None:
                classes['ambiguous'].append(n)
            elif n in declared:
                classes['unavailable'].append(n)
            elif n in where:
                classes['unowned'].append(n)
            elif n in absent:
                classes['absent'].append(n)
            else:
                classes['missing'].append(n)
        problems = {k: v for k, v in classes.items() if k != 'ok' and v}
        complete = complete and not problems
        out['sets'][set_name] = {k: len(v) for k, v in classes.items()} | {
            'problems': {k: sorted(v) for k, v in problems.items()}}
    out['complete'] = complete
    return out


# --------------------------------------------------------------------- the C7 policy ----

# Character animations: a frame layout (cgg / cgs CSV) is only served with its texture, as Phase C1
# did per unit.  key = the id the client builds all of an animation's names from.
ANIMATION_KEYS = [
    re.compile(r'^monster_(?:anime|cgg|[a-z]+_cgs(?:_ss)?)_(.+)\.(?:png|csv)$', re.I),   # monster/
    re.compile(r'^effect_(?:anime|cgg|[a-z]+_cgs)_(.+)\.(?:png|csv)$', re.I),            # effect/img, effect/cgg
    re.compile(r'^unit_[a-z_]*?(\d{8})(?:_[a-z0-9]+)?\.(?:png|csv)$', re.I),            # summoner parts (8-digit ids)
    re.compile(r'^(.+?)(?:_cgg|_cgs)?\.(?:png|csv)$', re.I),                             # <base>.png + <base>_cgg/_cgs.csv
]


def animation_key(name: str, rel: str) -> str | None:
    folder = rel.rsplit('/', 1)[0]
    if folder.startswith('monster/'):
        m = ANIMATION_KEYS[0].match(name)
        return f'monster:{m.group(1).lower()}' if m else None
    if folder in ('effect/img', 'effect/cgg', 'effect/cgs'):
        m = ANIMATION_KEYS[1].match(name)
        return f'effect:{m.group(1).lower()}' if m else None
    if folder in ('unit/img', 'unit/cgg', 'unit/cgs'):
        m = ANIMATION_KEYS[2].match(name)
        return f'summoner:{m.group(1)}' if m else None
    if folder == '_dlcbundle' and (name.lower().endswith(('_cgg.csv', '_cgs.csv')) or name.lower().endswith('.png')):
        m = ANIMATION_KEYS[3].match(name)
        return f'dlcanim:{m.group(1).lower()}' if m else None
    return None


def server_first_found(content: Path = CONTENT) -> dict[str, str]:
    """name (exact case) -> relative path, as BfWebController::findContentByName indexes the tree:
    std::filesystem::recursive_directory_iterator order (pre-order, the file system's own entry
    order), first wins.  That is the copy a request for a path that does not exist (the client's
    /content/_dlcbundle/<name> for a scene bundle) is answered with."""
    index = {}

    def walk(folder: Path):
        for entry in os.scandir(folder):
            if entry.is_dir(follow_symlinks=False):
                yield from walk(Path(entry.path))
            elif entry.is_file(follow_symlinks=False):
                yield entry
    for entry in walk(content):
        index.setdefault(entry.name, Path(entry.path).relative_to(content).as_posix())
    return index


def legacy_cache_copy(name: str, ls: Path | None, content: Content, candidates: list[str]) -> str | None:
    """The candidate a legacy client's LocalState copy of `name` is (raw or FileCrypt-encoded), if exactly one."""
    if ls is None or not ls.is_dir():
        return None
    path = ls / name
    if not path.is_file():
        return None
    cached = path.read_bytes()
    decoded = bytes((c - i * i) & 0xFF for i, c in enumerate(cached))
    matches = [r for r in candidates if content.read(r) in (cached, decoded)]
    return matches[0] if len(matches) == 1 else None


def resolve_collisions(content: Content, required: set[str], res_paths: list[str], rm: dict,
                       legacy_ls: Path | None) -> tuple[dict[str, str], dict]:
    """aliases {lower name: chosen rel} for required collided names, and the evidence for every decision."""
    res_rels = collections.defaultdict(set)
    for p in res_paths:
        rel = p.strip('/')
        res_rels[rel.rsplit('/', 1)[-1].lower()].add(rel.lower())
    rm_scenes = collections.defaultdict(list)
    for scene, files in rm.items():
        for f in files:
            rm_scenes[f.lower()].append(scene)
    first_found = None
    aliases, evidence = {}, {}
    for name in sorted(required):
        rel, how = content.resolve(name)
        if rel is not None or not how.startswith('collision'):
            continue
        rels = sorted(content.tree.where[name])
        why = []
        # Seasonal overlay folders (_Halloween2015, _Holiday2015, ...) are in no list the client
        # downloads from: F_RESOURCE_MST and the resourceMap name the plain folders only.
        plain = [r for r in rels if not (r.startswith('_') and not r.startswith('_dlcbundle/'))]
        if len(plain) < len(rels):
            why.append(f'seasonal overlay copies {sorted(set(rels) - set(plain))} are in no client list')
        cands = plain or rels
        listed = sorted(r for r in cands if r.lower() in res_rels.get(name, set()))
        chosen = None
        if len({content.tree.digest(r) for r in cands}) == 1:
            chosen = cands[0]
            why.append('the remaining copies are identical')
        elif len(listed) == 1:
            chosen = listed[0]
            why.append(f'F_RESOURCE_MST lists only {chosen}')
        if chosen is None and rm_scenes.get(name):
            if first_found is None:
                first_found = server_first_found(content.content)
            served = next((r for r in rels if r.lower() == f'_dlcbundle/{name}'), None)
            if served is None:
                served = next((v for k, v in first_found.items() if k.lower() == name), None)
            if served in cands and (not listed or served in listed):
                chosen = served
                why.append(f'resourceMap scene(s) {rm_scenes[name][:4]} request /content/_dlcbundle/{name}; '
                           f'the server answers with {served}')
        if chosen is None:
            cached = legacy_cache_copy(name, legacy_ls, content, cands)
            if cached is not None and (not listed or cached in listed):
                chosen = cached
                why.append(f'the legacy background-downloaded cache holds {cached} '
                           '(byte-identical, raw or FileCrypt-encoded)')
        if chosen is None:
            evidence[name] = {'copies': rels, 'chosen': None,
                              'why': why + ['no client evidence picks one copy: left legacy']}
            continue
        aliases[name] = chosen
        evidence[name] = {'copies': rels, 'chosen': chosen, 'why': why}
    return aliases, evidence


def generate_policy(base: dict, manifest: dict, content_root: Path = CONTENT,
                    legacy_ls: Path | None = None) -> tuple[dict, dict]:
    """The C7 candidate: the base policy plus every servable file whose reader is proven, each with
    what it pulls in (module docstring; docs/OFFLINE_CONTENT_PHASE_C7.md)."""
    policy = json.loads(json.dumps(base))
    plain = Content(content_root, {})
    rm, rm_dropped = bundle_map()
    _v, res_rows, _parts = load_version_table('F_RESOURCE_MST')
    res_paths = [r['ZwstRU92'] for r in res_rows]
    required = set()
    for names in manifest['sets'].values():
        required |= set(names)
    base_owned = {m.lower() for ms in policy['bundles'].values() for m in ms}

    aliases, collision_evidence = resolve_collisions(plain, required, res_paths, rm, legacy_ls)
    have = {k.lower() for k in policy['aliases']}
    for name, rel in aliases.items():
        if name not in have:
            policy['aliases'][rel.rsplit('/', 1)[-1]] = rel
    content = Content(content_root, dict(policy['aliases']))
    packaged = app_resources(APPX / 'Assets' / 'Resources')

    def ownable(name: str) -> tuple[str | None, str | None]:
        """(served rel, None) or (None, why not)."""
        low = name.lower()
        if not safe_flat_name(name):
            return None, 'not a flat name the client namespace can hold'
        if STATE.search(low):
            return None, 'client state the proxy never owns'
        if os.path.splitext(low)[1] not in OWNABLE:
            return None, 'no proven reader for this extension'
        rel, how = content.resolve(name)
        if rel is None:
            return None, how
        # C3 left a definition the app package ships with other bytes to the game: a client that
        # never downloads it draws the packaged one.  A name a startup list requests IS downloaded
        # by every stock client at its first login, which then reads the server's copy -- owning
        # it serves that copy from the first launch.
        if (low.endswith('.sam') and low in packaged and low not in required
                and packaged[low].read_bytes() != content.read(rel)):
            return None, 'the app package ships different bytes (a SuperAnim definition: left legacy as in C3)'
        return rel, None

    new_bundles, legacy, kinds = {}, {}, collections.Counter()
    claimed = set(base_owned)
    # What a family rule left legacy.  Step 3 never owns these one name at a time: a root whose
    # closure is incomplete would otherwise be declared ready merely because the root itself
    # exists (the C7 candidate of 2026-10-11 owned 35 such roots as s16_file_* bundles), and a
    # character animation is all or nothing (C1).  A name such a root reaches stays ownable on its
    # own merits -- a page or an image depends on nothing -- as C3 left the members of its legacy
    # definitions.
    refused = set()

    def add_bundle(kind: str, root: str, members: list[str], whole: bool = False) -> bool:
        rels, problems = [], []
        for m in members:
            rel, why = ownable(m)
            if rel is None:
                problems.append(f'{m}: {why}')
            else:
                rels.append(rel)
        if problems:
            legacy[root.lower()] = f'{kind}: ' + '; '.join(problems[:4])
            refused.add(root.lower())
            if whole:
                refused.update(m.lower() for m in members)
            return False
        bundle = f's16_{kind}_{root}'
        n = 2
        while bundle in policy['bundles'] or bundle in new_bundles:
            bundle = f's16_{kind}_{root}_{n}'
            n += 1
        new_bundles[bundle] = sorted({r.rsplit('/', 1)[-1] for r in rels})
        for r in rels:
            if r not in scope:
                scope.add(r)
                policy['scope'].append(r)
            claimed.add(r.rsplit('/', 1)[-1].lower())
        kinds[kind] += 1
        return True

    scope = set(policy['scope'])
    where = content.tree.where
    # 1. Files that pull others in -- definitions and their pages, scripts and their assets, fonts
    #    and their atlas pages, plists and their textures: one bundle per root with everything it
    #    reaches.  Bundles may share members; withdrawing one withdraws every bundle naming it.
    roots = sorted(n for n in where if n.endswith(('.sam', '.txt', '.fnt', '.plist')) and n not in base_owned)
    for name in roots:
        rel0, how = content.resolve(name)
        if rel0 is None:
            if name in required:
                legacy[name] = how
            continue
        members, problems = content.closure([rel0.rsplit('/', 1)[-1]])
        bad = {m: why for m, why in problems.items() if m in members}
        if bad:
            legacy[name] = '; '.join(f'{m}: {why}' for m, why in list(bad.items())[:3])
            refused.add(name)
            continue
        add_bundle(os.path.splitext(name)[1][1:], rel0.rsplit('/', 1)[-1],
                   sorted({(content.resolve(m)[0] or m).rsplit('/', 1)[-1] for m in members}))
    # 1b. Screen layouts with their resourceMap entries.  A layout CSV holds rectangles and element
    #     names only; what its screen draws is the screen's resourceMap entry (the scene's
    #     downloadFiles list).  A layout is owned together with every file of every entry naming it
    #     and what those files pull in, as one bundle; one whose entries name a file no source holds
    #     is UNRESOLVED and stays legacy -- never ready merely because the CSV exists.  A stem with
    #     no extension (item_frame_, the deck-name labels) is not a file: the client completes it at
    #     runtime.  A file only the app package ships is drawn from the package by the game's own
    #     path, as on a stock client.  Scene 999999 is the login bundle, a download list rather than
    #     a screen: a layout it alone lists is owned by itself in step 3.  An entry is what the client
    #     keeps of the resourceMap line (bundle_map): a name its exclusion table drops is never
    #     downloaded for the screen, so it is not part of the bundle (listed in the report).
    screens = collections.defaultdict(set)
    screens_dropped = collections.defaultdict(set)
    for scene, names in rm.items():
        if scene == '999999':
            continue
        for n in names:
            if n.lower().startswith('layout_') and n.lower().endswith('.csv'):
                screens[n.lower()].update(names)
                screens_dropped[n.lower()].update(rm_dropped.get(scene, []))
    layouts = {'bundled': 0, 'unresolved': {}, 'templates': {}, 'package_only': {},
               'login_bundle_only': 0,
               'entry_names_the_client_drops': {k: sorted(v) for k, v in sorted(screens_dropped.items()) if v}}
    for layout, entry in sorted(screens.items()):
        rel0 = content.resolve(layout)[0]
        if layout in base_owned or rel0 is None:
            continue
        files = sorted(n for n in entry if os.path.splitext(n)[1])
        stems = sorted(n for n in entry if not os.path.splitext(n)[1])
        missing = [n for n in files if n.lower() not in where and n.lower() not in packaged]
        members, problems = content.closure([n for n in files if n.lower() in where])
        bad = {m: why for m, why in problems.items() if m in members}
        if stems:
            layouts['templates'][layout] = stems
        if missing or bad:
            why = [f'{m}: in no source' for m in missing[:4]] + [f'{m}: {w}' for m, w in list(bad.items())[:2]]
            legacy[layout] = 'layout: ' + '; '.join(why)
            layouts['unresolved'][layout] = why
            refused.add(layout)
            continue
        only_packaged = [n for n in files if n.lower() not in where]
        if only_packaged:
            layouts['package_only'][layout] = only_packaged
        if add_bundle('layout', rel0.rsplit('/', 1)[-1],
                      sorted({(content.resolve(m)[0] or m).rsplit('/', 1)[-1] for m in members})):
            layouts['bundled'] += 1
        else:
            layouts['unresolved'][layout] = [legacy[layout]]
    layouts['login_bundle_only'] = len({n.lower() for n in rm.get('999999', [])
                                        if n.lower().startswith('layout_') and n.lower().endswith('.csv')}
                                       - set(screens))
    # 2. Character animations: frame layouts only together with their textures (as C1 per unit).
    #    Every group is bundled whole, even when an earlier bundle already holds some of its
    #    members (bundles may share members): skipping those would leave frame layouts in a
    #    bundle without their texture.
    groups = collections.defaultdict(list)
    for name in where:
        if name in refused:
            continue
        rel = content.resolve(name)[0]
        if rel is None:
            continue
        key = animation_key(rel.rsplit('/', 1)[-1], rel)
        if key and name.endswith(('.png', '.csv')):
            groups[key].append(rel.rsplit('/', 1)[-1])
    for key, members in sorted(groups.items()):
        if all(m.lower() in base_owned for m in members):
            continue                          # the base policy's own family (C1 units, ...)
        if len(members) > 1 and any(m.lower().endswith('.csv') for m in members):
            add_bundle('anim', key.replace(':', '_'), members, whole=True)
    # 3. Every other servable file: one name per bundle -- never a name a family rule refused.
    for name in sorted(where):
        if name in claimed or name in refused:
            continue
        rel, why = ownable((content.resolve(name)[0] or where[name][0]).rsplit('/', 1)[-1])
        if rel is None:
            if name in required:
                legacy[name] = why
            continue
        add_bundle('file', rel.rsplit('/', 1)[-1], [rel.rsplit('/', 1)[-1]])

    leaked = refused & {n.lower() for ms in new_bundles.values() for n in ms}
    if leaked:
        raise SystemExit(f'generator bug: {len(leaked)} refused name(s) owned, e.g. {sorted(leaked)[:5]}')
    policy['bundles'].update(new_bundles)
    for ext in OWNABLE:
        if ext not in policy['extensions']:
            policy['extensions'].append(ext)
    policy['enabled'] = False
    packaged_differ = sorted({n for ms in new_bundles.values() for n in ms
                              if n.lower() in packaged and content.resolve(n)[0]
                              and reader_view(n, packaged[n.lower()].read_bytes())
                              != reader_view(n, content.read(content.resolve(n)[0]))})
    report = {
        'base_names': len(base_owned),
        'new_bundles_by_kind': dict(kinds),
        'new_names': len({n.lower() for ms in new_bundles.values() for n in ms} - base_owned),
        'owned_names': len({n.lower() for ms in policy['bundles'].values() for n in ms}),
        'aliases_added': len([n for n in aliases if n not in have]),
        'collisions': collision_evidence,
        'legacy': dict(sorted(legacy.items())),
        'refused': sorted(refused),
        'layouts': layouts,
        'packaged_with_other_bytes_owned': packaged_differ,
    }
    return policy, report


# -------------------------------------------------------------------------------- CLI ----

def write_json(path: Path, data) -> None:
    path = Path(path).resolve()
    if (ROOT / 'deploy').resolve() in path.parents:
        raise SystemExit(f'refusing to write under deploy/: {path}')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=1), encoding='utf-8')


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('manifest', help='derive the startup sets (content_startup.json)')
    p.add_argument('--out', required=True, type=Path)
    p.add_argument('--report', type=Path)
    p.add_argument('--server-mst', type=Path, default=RESPONSE_MST,
                   help="the mst_root of the server the manifest is for: its response tables (read only)")
    p.add_argument('--banner-dir', type=Path, default=BANNER_DIR,
                   help="the folder that server probes for translated area banners (its ./game_content/content/dungeon)")
    p.add_argument('--no-banner-dir', action='store_true',
                   help='the server runs from a folder without game_content (an isolated test or harness server): '
                        'no banner is translated')
    p = sub.add_parser('policy', help='generate the C7 family policy from a base policy and a manifest')
    p.add_argument('--base', required=True, type=Path)
    p.add_argument('--manifest', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    p.add_argument('--report', type=Path)
    p.add_argument('--legacy-cache', type=Path,
                   help='a legacy client LocalState (read only) whose background-downloaded copies settle collisions')
    p = sub.add_parser('evaluate', help='coverage of a policy, resolved like the broker (offline)')
    p.add_argument('--policy', required=True, type=Path)
    p.add_argument('--manifest', required=True, type=Path)
    p.add_argument('--out', type=Path)
    p.add_argument('--server-mst', type=Path, default=RESPONSE_MST,
                   help="the mst_root of the server whose responses the manifest must be bound to (read only)")
    p.add_argument('--banner-dir', type=Path, default=BANNER_DIR,
                   help="the folder that server probes for translated area banners (its ./game_content/content/dungeon)")
    p.add_argument('--no-banner-dir', action='store_true',
                   help='the server runs from a folder without game_content: no banner is translated')
    args = ap.parse_args()
    if getattr(args, 'no_banner_dir', False):
        args.banner_dir = None

    if args.cmd == 'manifest':
        manifest, report = derive_manifest(Content(), args.server_mst, args.banner_dir)
        write_json(args.out, manifest)
        if args.report:
            write_json(args.report, report)
        print(f'{args.out}: build {manifest["build"]}; login {len(manifest["sets"]["login"])}, background '
              f'{len(manifest["sets"]["background"])}, excluded {len(manifest["excluded"])}, absent '
              f'{len(manifest["absent"])}; sha256 {sha256_file(args.out)}')
        return 0
    if args.cmd == 'policy':
        base = json.loads(args.base.read_text(encoding='utf-8'))
        manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
        policy, report = generate_policy(base, manifest, CONTENT, args.legacy_cache)
        write_json(args.out, policy)
        report.update({'base': str(args.base), 'base_sha256': sha256_file(args.base),
                       'policy': str(args.out), 'policy_sha256': sha256_file(args.out)})
        if args.report:
            write_json(args.report, report)
        print(f'{args.out}: {report["owned_names"]} owned names (+{report["new_names"]}), bundles by kind '
              f'{report["new_bundles_by_kind"]}, aliases +{report["aliases_added"]}, legacy {len(report["legacy"])}; '
              f'sha256 {report["policy_sha256"]}')
        return 0
    policy = json.loads(args.policy.read_text(encoding='utf-8'))
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    result = evaluate(policy, manifest, Content(CONTENT, policy.get('aliases', {})), served_tables(args.server_mst, args.banner_dir))
    if args.out:
        write_json(args.out, result)
    print(json.dumps({'complete': result['complete'], 'owned': result['owned'],
                      'manifest_errors': result['manifest_errors'],
                      'server_binding': result.get('server_binding') or 'bound', 'unknown': len(result['unknown']),
                      'sets': {k: {c: n for c, n in v.items() if c != 'problems'} for k, v in result['sets'].items()}},
                     indent=1))
    for set_name, v in result['sets'].items():
        for cls, names in v['problems'].items():
            print(f'  {set_name} {cls}: {len(names)}: {", ".join(names[:12])}')
    for u in result['unknown']:
        print(f'  unknown: {u[:140]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
