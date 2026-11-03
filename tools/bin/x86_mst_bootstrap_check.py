"""The x86 client's fresh-install master-table contract, re-read from the binary (Session 19).

    python tools/bin/x86_mst_bootstrap_check.py           the binary still says what scripts/qa_baselines/mst_bootstrap_x86.json says
    python tools/bin/x86_mst_bootstrap_check.py --write   regenerate it from the binary

BraveFrontier.Windows.exe, build 6253F827-01756000-00ED273D (the supported client).  What an empty
LocalState needs before Home, and what the client does when a master table is missing or short:

  version channel   VersionInfoResponse::readParam 0x52C900 reads the Initialize KeC10fuL rows (moWQ30GH
                    table, d2RFtP8T version, H6k1LIxC file count, 5kbnkTp0 record count) and, for every row
                    naming one of its 154 tables, calls DownloadMstFileList::add 0x5556D0(table, file key,
                    encode key, version, file count).  The keys are std::string globals filled by static
                    initializers (push len; push text; mov ecx, global; call 0xDDDC0).  GameScene::
                    requestMstFiles 0x7F48F0 then asks for "/" + "mst" + "/" + Ver<v>_<key>[_<n>].dat.
  response channel  getResponseObject 0x493770: 52 tags whose branch calls a DataMstManager save function
                    writing one fixed LocalState file (YDv9bJ3s -> EEPBM76L2G.dat, ...).
  login             LoginScene::updateEvent 0x8E2960 state 3: getLoadMstsList 0x53E2A0, loadMst 0x53D340 per
                    entry, checkCorruptionsOfMsts 0x540540.  false: REDOWNLOAD_MST_FILES (state 0xC); true:
                    DownloadMstFileList::saveVersion 0x558930, then state 4: setInitialUnits 0x8DF020;
                    false there: NETWORK_SERVER_ERROR -4000 and a CORRUPT_MST_ERROR report.
  corruption check  79 lists; an empty one -> deleteCorruptFile 0x542050 (only while corrupted_mst_check
                    0x184AA40 is set: clears the result bit, version "0", removes the files) or, for two
                    tables, resetFileVersion 0x542640 (version "0", removes the file; the result stands).
  record count      loadFile 0x543030: rows parsed != the announced record count, and that count not 0 ->
                    every part deleted and the table re-added to the download list as invalid.

Every fact names its RVA; out/qa/current/session19/re holds the disassembly the facts were read from.
Exit 0 when the binary matches the JSON and every rule holds; 1 otherwise.  Read only, except --write.
"""
from __future__ import annotations

import hashlib
import json
import re
import struct
import sys
from functools import lru_cache
from pathlib import Path

from capstone.x86 import X86_OP_IMM, X86_OP_MEM

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / 'scripts'))
import pe_x86  # noqa: E402
import startup_coverage as sc  # noqa: E402

FACTS = ROOT / 'scripts' / 'qa_baselines' / 'mst_bootstrap_x86.json'   # read by scripts/check_mst_bootstrap.py
ARM64_MAP = ROOT / 'tools' / 'mst_download_map.json'
FORMAT = 'bfcp-mst-bootstrap-x86 1'
BUILD = '6253F827-01756000-00ED273D'

READPARAM, ADD, ASSIGN = 0x52C900, 0x5556D0, 0xDDDC0
VERSION_LIST, VERSION_OBJECT = 0x5C0FD0, 0x5C2870       # VersionInfoList::shared, ::getObject(name)
FACTORY = 0x493770                                       # GameResponseParser::getResponseObject
CHECK, DELETE, RESET = 0x540540, 0x542050, 0x542640
DELETE_TABLE, RESET_TABLE, FILE_NAME = 0x5424E0, 0x542760, 0x554AA0
LOAD_LIST, LOAD_ONE, LOAD_FILE = 0x53E2A0, 0x53D340, 0x543030
LOAD_DOWNLOADED = 0x555A50                               # DownloadMstFileList::loadMsts (after a download)
SAVE_VERSION, INITIAL_UNITS, LOGIN, REQUEST = 0x558930, 0x8DF020, 0x8E2960, 0x7F48F0
GATE_BYTE, GATE_SETTER = 0x184AA40, 0x1EFEF0
SAVE_DEFAULT, REMOVE = 0x23FFD0, 'msvcr120_app.dll!remove'
# std::string construction and comparison helpers: never a list getter, a loader or a save function.
HELPERS = {0xDDEC0, 0xDE250, 0xDDDC0, 0xDEC10, 0xDDF20, 0xE0FC0, 0xE5990}
TABLE = re.compile(r'^[MF]_[A-Z0-9_]+$')
FILE = re.compile(r'^[0-9A-Z]{10}\.dat$')
TAG = re.compile(r'^[0-9A-Za-z]{8}$')


class Binary:
    def __init__(self, exe: str = pe_x86.EXE):
        self.path = Path(exe)
        self.img = pe_x86.Image(exe)
        self.mem = bytes(self.img.mem)
        self.base = self.img.base

    @lru_cache(maxsize=None)
    def insns(self, fn: int, limit: int = 8000) -> tuple:
        return tuple(self.img.insns(fn, limit))

    def at(self, rva: int):
        return next(self.img.md.disasm(self.mem[rva:rva + 16], rva))

    def literals(self, ins) -> list[str]:
        """The C strings an instruction's immediate or absolute-memory operands point at."""
        out = []
        for op in ins.operands:
            va = op.imm if op.type == X86_OP_IMM else (
                op.mem.disp if op.type == X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0 else None)
            if va is not None and va >= self.base:
                s = self.img.cstring(va - self.base)
                if s is not None:
                    out.append(s)
        return out

    @staticmethod
    def target(ins) -> int | None:
        if ins.mnemonic in ('call', 'jmp') and ins.operands and ins.operands[0].type == X86_OP_IMM:
            return ins.operands[0].imm
        return None

    def calls(self, fn: int, limit: int = 8000) -> set[int]:
        return {t for i in self.insns(fn, limit) if i.mnemonic == 'call' and (t := self.target(i)) is not None}

    def import_calls(self, fn: int) -> set[str]:
        out = set()
        for i in self.insns(fn):
            if i.mnemonic == 'call' and i.operands and i.operands[0].type == X86_OP_MEM:
                name = self.img.imports.get(i.operands[0].mem.disp - self.base)
                if name:
                    out.add(name)
        return out

    def byte(self, va: int) -> int:
        return self.mem[va - self.base]

    def dword(self, va: int) -> int:
        return struct.unpack_from('<I', self.mem, va - self.base)[0]

    @lru_cache(maxsize=None)
    def static_strings(self) -> dict[int, str]:
        """std::string globals the static initializers fill: push len; push text; mov ecx, global; call ASSIGN.
        A global assigned two different texts (a few UI globals are) is dropped rather than guessed."""
        out, clash = {}, set()
        for a, b in self.img.text:
            for m in re.finditer(rb'\x6a(.)\x68(.{4})\xb9(.{4})\xe8(.{4})', self.mem[a:b], re.S):
                at = a + m.start()
                if at + 17 + struct.unpack('<i', m.group(4))[0] != ASSIGN:
                    continue
                text = self.img.cstring(struct.unpack('<I', m.group(2))[0] - self.base, 120)
                if text is None or len(text) != m.group(1)[0]:
                    continue
                g = struct.unpack('<I', m.group(3))[0]
                if g in out and out[g] != text:
                    clash.add(g)
                out[g] = text
        return {g: t for g, t in out.items() if g not in clash}

    def vtable_of(self, fn: int) -> int | None:
        """The first vtable a constructor-like function stores: mov dword ptr [reg], <.data address>."""
        for i in self.insns(fn, 800):
            m = re.fullmatch(r'dword ptr \[e[a-z]{2}\], (0x[0-9a-f]+)', i.op_str)
            if i.mnemonic == 'mov' and m and 0x1840000 <= int(m.group(1), 16) < 0x19C3000:
                return int(m.group(1), 16)
        return None

    def slot(self, vtable: int, index: int) -> int:
        return self.dword(vtable + 4 * index) - self.base


# ---------------------------------------------------------------------------------------------------
# extraction

def version_channel(b: Binary) -> dict:
    """readParam's branches: each table-name literal opens one; the two key globals it pushes are, in push
    order, the encode key (add's 3rd argument) and the file key (its 2nd).  The later branches end in a
    jmp to one shared add (0x530FB9), so a branch is closed by the next table name, not by the call."""
    ins = b.insns(READPARAM, 20000)
    strings = b.static_strings()
    index = {i.address: k for k, i in enumerate(ins)}

    def reaches_add(t: int) -> bool:
        for i in ins[index.get(t, len(ins)):][:60]:
            if b.target(i) == ADD and i.mnemonic == 'call':
                return True
            if i.mnemonic.startswith('j') or i.mnemonic == 'ret':
                return False
        return False

    tails = {t for i in ins if i.mnemonic == 'jmp' and (t := b.target(i)) is not None and reaches_add(t)}
    fields, branches, flagged = [], [], []
    for i in ins:
        names = [s for s in b.literals(i) if TABLE.match(s)]
        if not branches:
            fields += [s for s in b.literals(i) if TAG.match(s) and s not in fields]
        if names and (not branches or branches[-1]['table'] != names[0]):
            branches.append({'table': names[0], 'site': i.address, 'globals': [], 'add': False})
        if not branches:
            continue
        t = b.target(i)
        if i.mnemonic == 'push' and i.operands[0].type == X86_OP_IMM and i.operands[0].imm in strings:
            branches[-1]['globals'].append(i.operands[0].imm)
        elif i.mnemonic == 'call' and t == ADD:
            branches[-1]['add'] = True
        elif i.mnemonic == 'jmp' and t in tails:
            branches[-1]['add'] = True
        elif i.mnemonic == 'call' and t == VERSION_OBJECT:
            flagged.append((branches[-1]['table'], i.address))
    tables = [{'table': br['table'], 'file_key': strings[br['globals'][1]], 'encode_key': strings[br['globals'][0]],
               'site': f'0x{br["site"]:X}'}
              for br in branches if br['add'] and len(br['globals']) == 2]
    ignored = [{'table': br['table'], 'site': f'0x{br["site"]:X}'}
               for br in branches if not br['add'] or len(br['globals']) != 2]
    return {'row_fields': fields[:4], 'tables': tables, 'ignored': ignored,
            'download_flag': [{'table': t, 'site': f'0x{s:X}'} for t, s in flagged]}


def response_channel(b: Binary) -> list[dict]:
    """getResponseObject's tag branches that call a save function: a callee that names exactly one fixed
    LocalState file and is not one of the list-wide functions (load list, dispatcher, check)."""
    listwide = {LOAD_LIST, LOAD_ONE, CHECK}
    out, tag = [], None
    for i in b.insns(FACTORY, 200000):
        if i.mnemonic == 'mov' and i.op_str.startswith('ecx, 0x'):
            names = [s for s in b.literals(i) if TAG.match(s)]
            if names:
                tag = names[0]
        t = b.target(i)
        if i.mnemonic != 'call' or t is None or t in HELPERS or t in listwide:
            continue
        files = {s for j in b.insns(t, 400) for s in b.literals(j) if FILE.match(s)}
        if len(files) == 1:
            out.append({'tag': tag, 'file': files.pop(), 'save': f'0x{t:X}', 'site': f'0x{i.address:X}'})
    return out


def dispatcher(b: Binary, fn: int) -> list[tuple[str, int]]:
    """(entry name, loader) -- each name literal compared, then the first loader it calls.  loadMst 0x53D340
    (the login load list) and loadMsts 0x555A50 (what a download just fetched) have this shape; the latter
    fetches its DataMstManager first (0x53CFA0), which is not a loader."""
    pairs, name = [], None
    for i in b.insns(fn, 8000):
        names = [s for s in b.literals(i) if TABLE.match(s) or FILE.match(s)]
        if names:
            name = names[0]
        t = b.target(i)
        if i.mnemonic == 'call' and t is not None and t not in HELPERS and t != 0x53CFA0 and name:
            pairs.append((name, t))
            name = None
    return pairs


def fills(b: Binary, loader: int, getter: int) -> bool:
    """The loader fills the list: its response object's readParam or read-finished handler (vtable slots 5
    and 6) calls the list getter, the object built inline or by a constructor the loader calls; or the
    loader calls the getter itself."""
    if getter in b.calls(loader, 1500):
        return True
    for maker in [loader] + sorted(b.calls(loader, 1500) - HELPERS - {LOAD_FILE}):
        vt = b.vtable_of(maker)
        if vt is not None and any(getter in b.calls(b.slot(vt, k), 4000) for k in (5, 6)):
            return True
    return False


def corruption_checks(b: Binary, versions: dict, responses: list[dict]) -> list[dict]:
    strings = b.static_strings()
    handlers = {DELETE: ('response', 'deleteCorruptFile'), DELETE_TABLE: ('version', 'deleteCorruptFile'),
                RESET_TABLE: ('version', 'resetFileVersion')}
    saved = {r['file']: r['tag'] for r in responses}
    paths = {'login': dispatcher(b, LOAD_ONE), 'download': dispatcher(b, LOAD_DOWNLOADED)}
    ins = b.insns(CHECK, 6000)
    out, getter, file, table, key = [], None, None, None, None
    for k, i in enumerate(ins):
        for s in b.literals(i):
            if TABLE.match(s):
                table = s
            elif FILE.match(s):
                file = s
        if i.mnemonic == 'push' and i.operands[0].type == X86_OP_IMM and i.operands[0].imm in strings:
            key = strings[i.operands[0].imm]
        t = b.target(i)
        if i.mnemonic != 'call' or t is None:
            continue
        if t in handlers:
            channel, handler = handlers[t]
            entry = {'table': table, 'handler': handler, 'site': f'0x{i.address:X}'}
            if channel == 'response':
                entry.update(channel='response', file=file, tag=saved.get(file), list=getter and f'0x{getter:X}')
            else:
                entry.update(channel='version', file_key=key, list=getter and f'0x{getter:X}')
            out.append(entry)
            table = file = key = getter = None
        elif t not in HELPERS and any(re.search(r'\[eax \+ 0x[0-9a-f]+\]', j.op_str) for j in ins[k + 1:k + 7]):
            # A list's shared() returns the singleton whose container member the test reads next, and it is
            # the LAST such call before the check's handler: the calls between are container helpers whose
            # result is not dereferenced (F_UNIT_CGS_MST: allKeys 0xDFD00, then a vector<string> destructor
            # 0xE10D0) or the gate of a conditional check (F_RESOURCE_BASE_MST: 0xE13190, before 0xE1AB70).
            getter = t
    # Who fills each list: the entries of either load path whose loader fills the list getter.
    for e in out:
        if e['table'] in ('M_DEFINE_MST', 'M_URL_MST'):
            # 0x541D0F-0x541D34: not a list -- both files go when any earlier check failed or either
            # table's loaded flag is clear (DefineMst 0x5D6E10 +0x23C, UrlMst 0x60BC30 +0x270).
            e['list'] = None
            e['test'] = 'loaded flags DefineMst+0x23C, UrlMst+0x270, or any earlier failure'
            e['filled_by'] = {'login': [e['file']]}
            continue
        e['test'] = 'the list holds no row'
        if e['table'] == 'F_RESOURCE_BASE_MST':
            # 0x541C9D-0x541CC2: checked only while byte 0x199734F and both flags +0x15 / +0x16 of the
            # object 0xE13190 returns are set.
            e['test'] = 'the list holds no row, checked only while 0x199734F, 0xE13190()+0x15 and +0x16 are set'
        getter = int(e['list'], 16)
        e['filled_by'] = {path: names for path, pairs in paths.items()
                          if (names := sorted({n for n, loader in pairs if fills(b, loader, getter)}))}
    return out


def load_list(b: Binary) -> list[str]:
    seen = []
    for i in b.insns(LOAD_LIST, 20000):
        for s in b.literals(i):
            if (TABLE.match(s) or FILE.match(s)) and s not in seen:
                seen.append(s)
    return seen


def resource_versions(b: Binary) -> list[str]:
    """The version rows VersionInfoResponse's slot-6 handler 0x531A00 acts on (its table-name literals, in
    order): content kinds whose cached files a version change purges (F_UNIT_IMG -> unit_*.png, ...)."""
    seen = []
    for i in b.insns(b.slot(0x18F5360, 6), 6000):
        for s in b.literals(i):
            if re.fullmatch(r'[MF]_[A-Z_]+', s) and s not in seen:
                seen.append(s)
    return seen


def extract(b: Binary) -> dict:
    versions = version_channel(b)
    responses = response_channel(b)
    return {
        'format': FORMAT,
        'build': sc.client_build(b.path),
        'exe_sha256': hashlib.sha256(b.path.read_bytes()).hexdigest(),
        'version_channel': versions,
        'request_path': '/mst/<Ver<version>_<file key>.dat | Ver<version>_<file key>_<part>.dat>',
        'resource_versions': resource_versions(b),
        'response_channel': responses,
        'corruption_checks': corruption_checks(b, versions, responses),
        'load_list': load_list(b),
    }


# ---------------------------------------------------------------------------------------------------
# the rules: instruction facts, each at its RVA

def op(b: Binary, rva: int, mnemonic: str, op_str: str) -> bool:
    i = b.at(rva)
    return i.mnemonic == mnemonic and i.op_str == op_str


def calls_at(b: Binary, rva: int, target: int) -> bool:
    i = b.at(rva)
    return i.mnemonic == 'call' and b.target(i) == target


def literal_at(b: Binary, rva: int) -> str | None:
    lits = b.literals(b.at(rva))
    return lits[0] if lits else None


def rules(b: Binary, facts: dict) -> list[tuple[str, bool, object]]:
    strings = b.static_strings()
    v = facts['version_channel']
    checks = facts['corruption_checks']
    out = []

    def rule(label, ok, detail=''):
        out.append((label, bool(ok), detail))

    rule(f'the client is build {BUILD}', facts['build'] == BUILD, facts['build'])
    rule('readParam 0x52C900 reads the row fields moWQ30GH, d2RFtP8T, H6k1LIxC, 5kbnkTp0 (in that order)',
         v['row_fields'] == ['moWQ30GH', 'd2RFtP8T', 'H6k1LIxC', '5kbnkTp0'], v['row_fields'])
    rule('readParam registers 154 distinct tables, each with an 8-character file key and encode key',
         len(v['tables']) == 154 and len({t['table'] for t in v['tables']}) == 154
         and all(TAG.match(t['file_key']) and TAG.match(t['encode_key']) for t in v['tables']), len(v['tables']))
    rule('readParam compares F_AI_MST at 0x52D9EC and drops the result (the F_LEADER_SKILL_MST compare at '
         '0x52D9FA overwrites al): an announced F_AI_MST row is never registered',
         [x['table'] for x in v['ignored']] == ['F_AI_MST'] and op(b, 0x52D9EC, 'mov', 'edx, 0x16b1124')
         and op(b, 0x52D9FA, 'mov', 'edx, 0x16b1ad4') and op(b, 0x52DA0C, 'test', 'al, al'), v['ignored'])
    rule('after registering F_RESOURCE_MST, F_ITEM_MST, F_RECIPE_MST, F_EFFECT_MST and F_EFFECT_GROUP_MST it '
         'sets their VersionInfo +0x3C flag (e.g. 0x52D9DF getObject, 0x52D9E8 mov byte ptr [eax + 0x3c], 1)',
         [x['table'] for x in v['download_flag']] == ['F_RESOURCE_MST', 'F_ITEM_MST', 'F_RECIPE_MST',
                                                      'F_EFFECT_MST', 'F_EFFECT_GROUP_MST']
         and calls_at(b, 0x52D9DF, VERSION_OBJECT) and op(b, 0x52D9E8, 'mov', 'byte ptr [eax + 0x3c], 1'),
         v['download_flag'])
    rule('the F_UNIT_MST branch passes add the globals 0x186B850 (file key) and 0x186B868 (encode key), '
         'pushed at 0x52CBC5 and 0x52CB89', op(b, 0x52CB89, 'push', '0x186b868') and op(b, 0x52CBC5, 'push', '0x186b850')
         and strings.get(0x186B850) == '2r9cNSdt' and strings.get(0x186B868) == '7nL1WTUb'
         and calls_at(b, 0x52CC25, ADD), (strings.get(0x186B850), strings.get(0x186B868)))
    # the request
    rule('requestMstFiles 0x7F48F0 asks for "/" + "mst" + "/" + the part name: getDownloadFilename 0x5550C0 / '
         '0x5551A0 (0x7F4AA9, 0x7F4AB6), "/" 0x168A67C, the global 0x188D000 "mst", requestFile 0x63E1C0',
         calls_at(b, 0x7F4AA9, 0x5550C0) and calls_at(b, 0x7F4AB6, 0x5551A0) and op(b, 0x7F4B68, 'push', '0x168a67c')
         and op(b, 0x7F4B76, 'push', '0x188d000') and strings.get(0x188D000) == 'mst'
         and calls_at(b, 0x7F4BF9, 0x63E1C0) and b.mem[0x168A67C - b.base:0x168A67E - b.base] == b'/\0')
    rule('part names: getMstFileName 0x554C80 builds "Ver" (0x186C420) + version + "_" + key + "_" + part '
         '+ ".dat" (0x186C408)', op(b, 0x554CE5, 'push', '0x186c420') and strings.get(0x186C420) == 'Ver'
         and op(b, 0x554D2C, 'push', '0x168c14c') and op(b, 0x554D4D, 'push', '0x168c14c')
         and op(b, 0x554D86, 'push', '0x186c408') and strings.get(0x186C408) == '.dat'
         and calls_at(b, 0x554CFD, 0x65DEE0) and calls_at(b, 0x554D5B, 0x65E2E0))
    # the response channel
    resp = facts['response_channel']
    rule('getResponseObject 0x493770: 52 tags call a save function writing one fixed LocalState file, no file '
         'written by two tags', len(resp) == 52 and len({r['file'] for r in resp}) == 52 and all(r['tag'] for r in resp),
         len(resp))
    rule('e.g. YDv9bJ3s -> save 0x544810 -> EEPBM76L2G.dat (0x493AB8), VkoZ5t3K -> 0x545570 -> ZSPFQ9CFY4.dat '
         '(0x493E36), 4NG79sX1 -> 0x54AAF0 -> ZX8TREMHH4.dat (0x494CFE)',
         {('YDv9bJ3s', 'EEPBM76L2G.dat', '0x544810'), ('VkoZ5t3K', 'ZSPFQ9CFY4.dat', '0x545570'),
          ('4NG79sX1', 'ZX8TREMHH4.dat', '0x54AAF0')} <= {(r['tag'], r['file'], r['save']) for r in resp}
         and calls_at(b, 0x493AB8, 0x544810) and calls_at(b, 0x493E36, 0x545570) and calls_at(b, 0x494CFE, 0x54AAF0))
    # the corruption check
    rule('checkCorruptionsOfMsts 0x540540 checks 79 tables: 32 named for a response file and 47 version tables',
         len(checks) == 79 and sum(c['channel'] == 'response' for c in checks) == 32,
         (len(checks), sum(c['channel'] == 'response' for c in checks)))
    soft = [c['table'] for c in checks if c['handler'] == 'resetFileVersion']
    rule('two are soft (resetFileVersion through 0x542760): F_NPC_MESSAGE_MST and F_RELOAD_FILE_MST',
         soft == ['F_NPC_MESSAGE_MST', 'F_RELOAD_FILE_MST'], soft)
    registry = {t['table']: t for t in v['tables']}
    rule('every version check passes the same file key readParam registers for that table',
         all(c['table'] in registry and registry[c['table']]['file_key'] == c['file_key']
             for c in checks if c['channel'] == 'version'),
         [c['table'] for c in checks if c['channel'] == 'version'
          and registry.get(c['table'], {}).get('file_key') != c['file_key']])
    rule('every response check but three names a file a response tag saves; M_AREA_MST, M_DUNGEON_MST and '
         'M_LAND_MST name files nothing writes (in-memory lists)',
         sorted(c['table'] for c in checks if c['channel'] == 'response' and not c['tag'])
         == ['M_AREA_MST', 'M_DUNGEON_MST', 'M_LAND_MST'])
    filled = {c['table']: c['filled_by'].get('login') for c in checks}
    rule('those three lists are filled by loading the client\'s own F_AREA_MST (loader 0x544C30), F_DUNGEON_MST '
         '(0x544D40) and F_LAND_MST (0x5480B0): their response readParam calls the checked list getter',
         filled.get('M_AREA_MST') == ['F_AREA_MST'] and filled.get('M_DUNGEON_MST') == ['F_DUNGEON_MST']
         and filled.get('M_LAND_MST') == ['F_LAND_MST'],
         {k: filled.get(k) for k in ('M_AREA_MST', 'M_DUNGEON_MST', 'M_LAND_MST')})
    rule('every checked list has a filler on a load path that runs before the check: the login load list '
         '(loadMst 0x53D340) or, for F_RESOURCE_BASE_MST only, loadMsts 0x555A50 after a download (0x554DE0)',
         all(c['filled_by'].get('login') or c['table'] == 'F_RESOURCE_BASE_MST' for c in checks)
         and next(c for c in checks if c['table'] == 'F_RESOURCE_BASE_MST')['filled_by'] == {
             'download': ['F_RESOURCE_BASE_MST']} and calls_at(b, 0x558876, 0x554DE0),
         [c['table'] for c in checks if not c['filled_by'].get('login') and c['table'] != 'F_RESOURCE_BASE_MST'])
    rule('F_RESOURCE_BASE_MST is checked conditionally: byte 0x199734F (0x541C9D), then flags +0x15 and +0x16 of '
         'the object 0xE13190 returns (0x541CA6-0x541CC2), before its list 0xE1AB70 (0x541CC4)',
         op(b, 0x541C9D, 'cmp', 'byte ptr [0x199734f], 0') and calls_at(b, 0x541CA6, 0xE13190)
         and op(b, 0x541CAB, 'mov', 'al, byte ptr [eax + 0x15]') and op(b, 0x541CBD, 'mov', 'al, byte ptr [eax + 0x16]')
         and calls_at(b, 0x541CC4, 0xE1AB70)
         and next(c for c in checks if c['table'] == 'F_RESOURCE_BASE_MST')['list'] == '0xE1AB70')
    recipe = next(c for c in checks if c['table'] == 'M_RECIPE_MST')
    rule('M_RECIPE_MST: the Initialize tag 8f0bCciN saves 7DKCJFK1J7.dat, but the login fills the recipe list '
         'from the client\'s own F_RECIPE_MST (the saved file is not on the load list)',
         recipe['tag'] == '8f0bCciN' and recipe['filled_by'].get('login') == ['F_RECIPE_MST']
         and '7DKCJFK1J7.dat' not in facts['load_list'], recipe['filled_by'])
    rule('M_DEFINE_MST and M_URL_MST (0x541D0F-0x541D9D): both files deleted when any earlier check failed or '
         'DefineMst +0x23C / UrlMst +0x270 (the loaded flags) is clear',
         op(b, 0x541D0F, 'test', 'byte ptr [esi + 0xd4], 1') and calls_at(b, 0x541D18, 0x5D6E10)
         and op(b, 0x541D1D, 'mov', 'al, byte ptr [eax + 0x23c]') and calls_at(b, 0x541D27, 0x60BC30)
         and op(b, 0x541D2C, 'mov', 'al, byte ptr [eax + 0x270]') and calls_at(b, 0x541D67, DELETE)
         and calls_at(b, 0x541D9D, DELETE))
    rule('the check returns its result bit: 0x542026 mov al, byte ptr [esi + 0xd4]; 0x54202C and al, 1',
         op(b, 0x542026, 'mov', 'al, byte ptr [esi + 0xd4]') and op(b, 0x54202C, 'and', 'al, 1'))
    rule('deleteCorruptFile 0x542050 acts only while byte 0x184AA40 is set (0x5420D8), clears the result bit '
         '(0x5420E5), saves version "0" under the table name (0x54220D "0", 0x542245) and removes the files '
         '(every part the download list names, else the file given)',
         op(b, 0x5420D8, 'cmp', 'byte ptr [0x184aa40], 0') and op(b, 0x5420E5, 'and', 'byte ptr [esi + 0xd4], 0xfe')
         and op(b, 0x54220D, 'push', '0x168a290') and calls_at(b, 0x542245, SAVE_DEFAULT)
         and REMOVE in b.import_calls(DELETE) and b.mem[0x168A290 - b.base:0x168A292 - b.base] == b'0\0')
    rule('resetFileVersion 0x542640 saves version "0" (0x542688, 0x5426C0) and removes the file (0x5426DC) -- '
         'no gate and no result bit',
         op(b, 0x542688, 'push', '0x168a290') and calls_at(b, 0x5426C0, SAVE_DEFAULT) and REMOVE in b.import_calls(RESET)
         and not any('0x184aa40' in i.op_str or '0xd4]' in i.op_str for i in b.insns(RESET, 400)))
    rule('the wrappers: 0x5424E0 / 0x542760 name the table\'s file (0x554AA0 at 0x542567 / 0x5427E5) and hand '
         'it to deleteCorruptFile (0x5425C3) / resetFileVersion (0x54283F)',
         calls_at(b, 0x542567, FILE_NAME) and calls_at(b, 0x5425C3, DELETE)
         and calls_at(b, 0x5427E5, FILE_NAME) and calls_at(b, 0x54283F, RESET))
    rule('the gate: FeatureCheck key corrupted_mst_check (0x1F0419) sets byte 0x184AA40 (0x1F0435); the '
         'image\'s initial value is 1',
         literal_at(b, 0x1F0419) == 'corrupted_mst_check' and op(b, 0x1F0435, 'sete', 'byte ptr [0x184aa40]')
         and b.byte(GATE_BYTE) == 1)
    # the record count
    rule('loadFile 0x543030: rows parsed (summed per part at 0x5432F8) == the version list\'s record count '
         '(0x54339C) or that count is 0 (0x5433AA) -> loaded; otherwise every existing part deleted '
         '(0x543512 exists, 0x54351E delete), the table added to the download list (0x543603) and marked '
         'invalid (0x543624 push 1, 0x543626)',
         op(b, 0x5432F8, 'add', 'esi, dword ptr [ebp - 0x70]') and op(b, 0x54339C, 'call', 'eax')
         and op(b, 0x54339E, 'cmp', 'dword ptr [ebp - 0x8c], eax') and op(b, 0x5433A4, 'je', '0x543629')
         and op(b, 0x5433AA, 'test', 'eax, eax') and op(b, 0x5433AC, 'je', '0x543629')
         and calls_at(b, 0x543512, 0x237EB0) and calls_at(b, 0x54351E, 0x238160) and calls_at(b, 0x543603, ADD)
         and op(b, 0x543624, 'push', '1') and op(b, 0x543626, 'call', 'dword ptr [edx + 0x20]'))
    rule('loadFile takes one part when the version list does not know the table (0x54308B)',
         op(b, 0x54308B, 'mov', 'dword ptr [ebp - 0x7c], 1'))
    # the login order
    rule('LoginScene state 3: the load list 0x53E2A0 (0x8E2AC2), loadMst per entry (0x8E2B3D, 0x8E2BA6), then '
         'the check (0x8E2BBD)', calls_at(b, 0x8E2AC2, LOAD_LIST) and calls_at(b, 0x8E2B3D, LOAD_ONE)
         and calls_at(b, 0x8E2BA6, LOAD_ONE) and calls_at(b, 0x8E2BBD, CHECK))
    rule('check false: REDOWNLOAD_MST_FILES_CHECK_COUNTER "1" (0x8E2C05, 0x8E2C1F, 0x8E2C34) and the '
         'REDOWNLOAD_MST_FILES notice (0x8E2C45), state 0xC (0x8E2C4B)',
         op(b, 0x8E2C05, 'push', '0x1689714') and literal_at(b, 0x8E2C1F) == 'REDOWNLOAD_MST_FILES_CHECK_COUNTER'
         and calls_at(b, 0x8E2C34, SAVE_DEFAULT) and literal_at(b, 0x8E2C45) == 'REDOWNLOAD_MST_FILES'
         and op(b, 0x8E2C4B, 'mov', 'dword ptr [edi + 0x214], 0xc') and b.mem[0x1689714 - b.base:0x1689716 - b.base] == b'1\0')
    rule('check true: the counter cleared (0x8E2C8B ""), DownloadMstFileList::saveVersion 0x558930 (0x8E2CC9), '
         'state 4 (0x8E2CCE) -- versions are saved only after every list loaded',
         op(b, 0x8E2C8B, 'push', '0x168987d') and calls_at(b, 0x8E2CC2, 0x555560) and calls_at(b, 0x8E2CC9, SAVE_VERSION)
         and op(b, 0x8E2CCE, 'mov', 'dword ptr [edi + 0x214], 4') and b.byte(0x168987D) == 0)
    rule('saveVersion 0x558930 saves each listed version (0x5589D4), deletes lower-version files (0x5589EF '
         '0x5588E0) and drops the valid entries (0x5589F6 0x5558C0)',
         calls_at(b, 0x5589D4, SAVE_DEFAULT) and calls_at(b, 0x5589EF, 0x5588E0) and calls_at(b, 0x5589F6, 0x5558C0))
    rule('state 4: setInitialUnits 0x8DF020 (0x8E2D02); false -> NETWORK_SERVER_ERROR (0x8E2D18) code -4000 '
         '(0x8E2D38), state 0xC, reported as CORRUPT_MST_ERROR / DEFINE_MST_INITIAL_UNIT (0x8E2DB2, 0x8E2DAD)',
         calls_at(b, 0x8E2D02, INITIAL_UNITS) and literal_at(b, 0x8E2D18) == 'NETWORK_SERVER_ERROR'
         and op(b, 0x8E2D38, 'push', '0xfffff060') and op(b, 0x8E2D1E, 'mov', 'dword ptr [edi + 0x214], 0xc')
         and literal_at(b, 0x8E2DB2) == 'CORRUPT_MST_ERROR' and literal_at(b, 0x8E2DAD) == 'DEFINE_MST_INITIAL_UNIT')
    rule('setInitialUnits reads DefineMst (0x5D7840 at 0x8DF0AB), splits on "," (0x8DF0C9), "-" (0x8DF18C), '
         '":" (0x8DF1FE) and "@" (0x8DF2E5), and succeeds only when each element 1..4 (0x8DF6FF, 0x8DF7EA) has '
         'an initial unit (0x8DF77D) with a non-empty unit id (0x8DF7A1)',
         calls_at(b, 0x8DF0AB, 0x5D7840) and op(b, 0x8DF0C9, 'push', '0x168a28c') and op(b, 0x8DF18C, 'push', '0x16900ac')
         and op(b, 0x8DF1FE, 'push', '0x168a218') and op(b, 0x8DF2E5, 'push', '0x1689a94')
         and [b.mem[a - b.base:a - b.base + 2] for a in (0x168A28C, 0x16900AC, 0x168A218, 0x1689A94)]
         == [b',\0', b'-\0', b':\0', b'@\0']
         and op(b, 0x8DF6FF, 'mov', 'edi, 1') and op(b, 0x8DF77D, 'cmp', 'dword ptr [eax + 0x14], edi')
         and op(b, 0x8DF7A1, 'cmp', 'dword ptr [eax + 0x10], 0') and op(b, 0x8DF7EA, 'cmp', 'edi, 4'))
    rule('every login filler is on the load list 0x53E2A0, so state 3 loads it before the check',
         all(f in facts['load_list'] for c in checks for f in c['filled_by'].get('login', [])),
         sorted({f for c in checks for f in c['filled_by'].get('login', []) if f not in facts['load_list']}))
    # version changes of content, and the text dictionaries (not master tables, but delivered at FILE_CHECK)
    rule('VersionInfoResponse vtable 0x18F5360 slot 6 is 0x531A00, which acts on 16 content-kind version rows '
         '(F_UNIT_IMG ... F_PARTICLE, M_GACHA_EFFECT_MST, M_RESOURCE_MST, M_EFFECT_MST, M_EFFECT_GROUP_MST), '
         'e.g. F_UNIT_IMG with "unit_" + ".png" (0x531C9D, 0x531CC4)',
         b.slot(0x18F5360, 6) == 0x531A00 and facts['resource_versions'] == [
             'F_UNIT_IMG', 'F_UNIT_CGG', 'F_UNIT_CGS', 'F_MONSTER_IMG', 'F_MONSTER_CGG', 'F_MONSTER_CGS',
             'F_SYSTEM_RES', 'F_GACHA_BG_IMG', 'F_EVENT_FILE', 'F_SAM', 'F_SOUND', 'F_PARTICLE',
             'M_GACHA_EFFECT_MST', 'M_RESOURCE_MST', 'M_EFFECT_MST', 'M_EFFECT_GROUP_MST']
         and op(b, 0x531CC4, 'mov', 'edi, 0x16b1d44'), facts['resource_versions'])
    rule('text dictionaries: createDownloadCheckList 0x8DF880 calls 0x3918E0 (0x8E06DB), which builds '
         '"sgtext_" + ... + ".csv" (0x39192C, 0x391984) over F_RESOURCE_MST\'s list (0x391A56 0x5F7890); '
         'LoginScene::updateEvent extracts the ZIPs through 0x392DA0 (0x8E5069, 0x8E5194): ".zip" '
         '(0x392E30), existsLocalFile (0x392FE1), the extractor 0x393560 (0x393064)',
         calls_at(b, 0x8E06DB, 0x3918E0) and op(b, 0x39192C, 'push', '0x16a6c98') and op(b, 0x391984, 'push', '0x16a6ca0')
         and calls_at(b, 0x391A56, 0x5F7890) and calls_at(b, 0x8E5069, 0x392DA0) and calls_at(b, 0x8E5194, 0x392DA0)
         and op(b, 0x392E30, 'push', '0x16a6ca8') and calls_at(b, 0x392FE1, 0x237EB0) and calls_at(b, 0x393064, 0x393560))
    # arm64 corroboration (tools/mst_download_map.json, from libgame.so's readParam: 133 entries for its 154
    # add calls).  Its F_MULTI_GUILD_NPC_MESSAGE_MST entry runs 28 strings into the next branches (it opens
    # with we5dRx18, x86's F_MULTI_MR_MST file key): the x86 branch 0x530D33 is the authority there, and no
    # file under either key exists to decrypt.
    if ARM64_MAP.is_file():
        entries = json.loads(ARM64_MAP.read_text(encoding='utf-8'))
        arm = {e['table']: tuple(e['strings'][:2]) for e in entries if len(e['strings']) == 2}
        merged = sorted(e['table'] for e in entries if len(e['strings']) != 2)
        x86 = {t['table']: (t['file_key'], t['encode_key']) for t in v['tables']}
        rule('arm64 corroboration: every two-string entry of tools/mst_download_map.json is registered on x86 '
             'with the same file key and encode key; the one merged entry is F_MULTI_GUILD_NPC_MESSAGE_MST; '
             'the 21 x86 tables the map lacks are F_MULTI_*',
             all(x86.get(t) == k for t, k in arm.items()) and merged == ['F_MULTI_GUILD_NPC_MESSAGE_MST']
             and x86['F_MULTI_GUILD_NPC_MESSAGE_MST'] == ('Y70NXZUz', 'zafH9rL1')
             and len(set(x86) - set(arm) - set(merged)) == 21
             and all(t.startswith('F_MULTI_') for t in set(x86) - set(arm) - set(merged)),
             {'differ': sorted(t for t, k in arm.items() if x86.get(t) != k), 'merged': merged,
              'x86 only': len(set(x86) - set(arm) - set(merged))})
    return out


def main() -> int:
    write = '--write' in sys.argv[1:]
    b = Binary()
    fresh = extract(b)
    if write:
        FACTS.write_text(json.dumps(fresh, indent=1) + '\n', encoding='utf-8')
        print(f'wrote {FACTS.relative_to(ROOT)}')
    stored = json.loads(FACTS.read_text(encoding='utf-8')) if FACTS.is_file() else None
    results = []

    def check(label, ok, detail=''):
        results.append(bool(ok))
        print(f'{"PASS" if ok else "FAIL"}  {label}' + ('' if ok else f'  -- {detail}'))

    check(f'{FACTS.name} matches a fresh extraction from {b.path.name}', stored == fresh,
          '' if stored is None else [k for k in fresh if stored.get(k) != fresh[k]])
    for label, ok, detail in rules(b, fresh):
        check(label, ok, detail)
    print(f'x86_mst_bootstrap_check: {sum(results)} passed, {len(results) - sum(results)} failed')
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
