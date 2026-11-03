"""Re-reads from the x86 client every fact scripts/startup_coverage.py's startup rules cite.

    python tools/bin/x86_startup_rules_check.py

For each DERIVED list (BraveFrontier.Windows.exe fb4f3c25, build 6253F827-01756000-00ED273D):
  * the check-list key literal in createDownloadCheckList 0x8DF880 and the branch literal in
    LoginScene::updateEvent 0x8E2960, and that no branch tested earlier matches the key;
  * per field: the folder literal pushed in the branch, the getter's field offset, and the
    table parser's store of the column into that same field (directly, through a virtual
    setter, or as a token of the ',' split 0x236740);
  * the feature-gate bytes the check-list loops sit under;
  * the extra facts of raid_boss_bgm (a key no branch tests) and general_event (the type filter);
  * the resourceMap exclusion table (tools/bin/x86_bundle_exclusions.py).
Every DERIVED field must be covered by a fact here and every fact must match a DERIVED field.
Exit 0 when everything holds; 1 otherwise.  Read only.
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1] / 'scripts'))
import pe_x86  # noqa: E402
import x86_bundle_exclusions  # noqa: E402
import startup_coverage as sc  # noqa: E402

BUILDER, UPDATE = 0x8DF880, 0x8E2960
SPLIT = 0x236740
# The order updateEvent tests the keys in (each a substring find; the first match wins).
DISPATCH = ['unit', 'item', 'weapon', 'summoner_img', 'area', 'gacha_effect', 'gacha', 'system', 'effect_group',
            'effect', 'dungeonkey', 'stampset', 'stamp', 'campaignQuest', 'banner', 'general_event',
            'sp_challenge_title', 'raidMap', 'raid_point_bgm', 'raid_point_img', 'raid_boss_bgm', 'medal',
            'campaignMap', 'sgXbbSkill']

# key: builder key site, branch site, gate (byte VA, the builder's compare site) or None, parser,
# fields [(column, token, folder push site, folder, getter, field offset, store)].  store: the field
# offset again (a direct store), ('split', token) or ('virtual', slot offset, setter fn).
FACTS = {
    'dungeonkey': dict(builder=(0x8E18E4, 'dungeonkey'), branch=(0x8E3C10, 'dungeonkey'), gate=None,
                       parser=0x4E2970, fields=[
                           ('M2cv6dum', 0, 0x8E3C66, '/dungeon_key/', 0xEE2C0, 0x74, ('split', 0)),
                           ('M2cv6dum', 1, 0x8E3CC6, '/dungeon_key/', 0xE9960, 0x14, ('virtual', 0x18, 0xEC050)),
                           ('VX0j1fni', None, 0x8E3D22, '/dungeon/', 0xEE300, 0x8C, 0x8C),
                           ('9unNZ6b0', None, 0x8E3D7C, '/dungeon/', 0xEE340, 0xA4, 0xA4)]),
    'stampset': dict(builder=(0x8E23D9, 'stampset'), branch=(0x8E3DCD, 'stampset'), gate=(0x184AA65, 0x8E2073),
                     parser=0x511A90, fields=[('M2cv6dum', None, 0x8E3E23, '/stamp/', 0xF17A0, 0x30, 0x30)]),
    'stamp': dict(builder=(0x8E24C9, 'stamp'), branch=(0x8E3E7D, 'stamp'), gate=(0x184AA65, 0x8E2073),
                  parser=0x511840, fields=[('nGVc2zS1', None, 0x8E3ED3, '/stamp/', 0x109CC0, 0x1C, 0x1C)]),
    'medal': dict(builder=(0x8E1A16, 'medal'), branch=(0x8E43C9, 'medal'), gate=None,
                  parser=0x4F1220, fields=[('M2cv6dum', None, 0x8E441F, '/medal/', 0xEE480, 0x44,
                                            ('virtual', 0x28, 0xF00F0))]),
    'campaignQuest': dict(builder=(0x8E1CB6, 'campaignQuest'), branch=(0x8E3F2D, 'campaignQuest'),
                          gate=(0x184AA38, 0x8E1C43), parser=0x4D3660, fields=[
                              ('yTPw0BD7', None, 0x8E3F50, '/grand/', 0xF5E90, 0x48, 0x48),
                              ('M2cv6dum', None, 0x8E3F50, '/grand/', 0x1D48D0, 0x60, 0x60)]),
    'banner': dict(builder=(0x8E1B58, 'banner'), branch=(0x8E3F8E, 'banner'), gate=(0x184AA67, 0x8E1B03),
                   parser=0x4CDEA0, fields=[('1gDkL6XR', None, 0x8E3FA4, '/banner/', 0xF50F0, 0x78, 0x78)]),
    'sp_challenge_title': dict(builder=(0x8E1F89, 'sp_challenge_title'), branch=(0x8E4010, 'sp_challenge_title'),
                               gate=None, parser=0x5104C0,
                               fields=[('jsRoN50z', None, 0x8E4066, '/dungeon/', 0xF01D0, 0x6C, 0x6C)]),
    'raidMap': dict(builder=(0x8E20E6, 'raidMap'), branch=(0x8E40C0, 'raidMap'), gate=(0x184AA65, 0x8E2073),
                    parser=0x4FE520, fields=[('b5VveUd8', None, 0x8E4226, '/raid/raid_map/', 0xEE480, 0x44, 0x44)]),
    'campaignMap': dict(builder=(0x8E25C6, 'campaignMap'), branch=(0x8E4478, 'campaignMap'),
                        gate=(0x184AA38, 0x8E2566), parser=0x4D0810,
                        fields=[('b5VveUd8', None, 0x8E45D7, '/raid/raid_map/', 0xEE480, 0x44, 0x44)]),
    'raid_point_img': dict(builder=(0x8E228C, 'raid_point_img'), branch=(0x8E4321, 'raid_point_img'),
                           gate=(0x184AA65, 0x8E2073), parser=0x4FECE0,
                           fields=[('L8iA9M6c', None, 0x8E4337, '/dungeon/', 0x179B20, 0x58, 0x58)]),
    'raid_point_bgm': dict(builder=(0x8E2231, 'raid_point_bgm'), branch=(0x8E42CD, 'raid_point_bgm'),
                           gate=(0x184AA65, 0x8E2073), parser=0x4FECE0,
                           fields=[('3sRN9BPS', None, 0x8E42E3, '/sound/', 0xF0290, 0x88, 0x88)]),
    'general_event': dict(builder=(0x8E1E3B, 'general_event'), branch=(0x8E3FE2, 'general_event'), gate=None,
                          parser=0x4EDCA0, fields=[('t5R47iwj', 0, 0x5E2338, '/sound/', None, 0x34, 0x34)]),
    'sgXbbSkill': dict(builder=(0x8E2767, 'sgXbbSkill'), branch=(0x8E467E, 'sgXbbSkill'), gate=None,
                       parser=0xDECF0, fields=[
                           ('QqfI9mM4', None, 0x8E46F9, '/xbb/', 0xEE480, 0x44, 0x44),
                           ('0QxL2is7', None, 0x8E4778, '/xbb/', 0xEE4C0, 0x5C, 0x5C),
                           ('2EF0d6ue', None, 0x8E47FD, '/xbb/', 0xEE440, 0x2C, 0x2C)]),
    'raid_boss_bgm': dict(builder=(0x8E2334, 'raidBossBgm'), branch=(0x8E4375, 'raid_boss_bgm'),
                          gate=(0x184AA65, 0x8E2073), parser=None, fields=[]),
    # Session 19.  A field may carry an 8th element, the parser of ANOTHER table (a joined row); a computed
    # field has store ('computed',) and its rule is checked in section 6.
    'item': dict(builder=(0x8E0D41, 'item'), branch=(0x8E3770, 'item'), gate=None, parser=0x4EFCF0, fields=[
        ('Mt3Y0bo5', None, 0x7F7053, '/item/', 0xF1B80, 0x64, 0x64),
        ('computed:item_frame', None, 0x7F70E1, '/common/', None, None, ('computed',))]),
    'area': dict(builder=(0x8E0B9B, 'area'), branch=(0x8E37FB, 'area'), gate=None, parser=0x4CBC80, fields=[
        ('b5VveUd8', None, 0x7F71FC, '/area/', 0xF4FF0, 0x4C, 0x4C),
        ('H0UB8pqm', None, 0x7F72CC, '/area/', 0xF1B80, 0x64, 0x64),
        ('21VKZo0E', None, 0x8E3897, '/dungeon/', 0x399A90, 0xE4, 0xE4, 0x4E3230)]),
}

img = pe_x86.image('exe')
base, mem = img.base, img.mem
results = []


def check(label, ok, detail=''):
    results.append(ok)
    print(f'{"PASS" if ok else "FAIL"}  {label}' + ('' if ok else f'  -- {detail}'))


def insn(rva):
    return next(img.md.disasm(bytes(mem[rva:rva + 16]), rva, 1), None)


def literal_at(rva):
    ins = insn(rva)
    if ins is None:
        return None
    for op in ins.operands:
        if op.type == 2:
            v = op.imm & 0xFFFFFFFF
            return img.cstring(v - base) if v > base else None
    return None


def getter_offset(fn):
    for ins in img.md.disasm(bytes(mem[fn:fn + 0x30]), fn):
        if ins.mnemonic == 'add' and ins.op_str.startswith('ecx, 0x'):
            return int(ins.op_str.split(',')[1], 16)
    return None


def literal_refs(text):
    raw = text.encode() + b'\0'
    out, i = [], mem.find(raw)
    while i >= 0:
        pat = (base + i).to_bytes(4, 'little')
        for lo, hi in img.text:
            seg = bytes(mem[lo:hi])
            j = seg.find(pat)
            while j >= 0:
                out.append(img.func_start(lo + j - 1))
                j = seg.find(pat, j + 1)
        i = mem.find(raw, i + 1)
    return out


def column_window(parser, column):
    """The parser's instructions from the column's literal to the next column literal."""
    seq = list(img.md.disasm(bytes(mem[parser:parser + 0x2400]), parser))
    start = None
    for i, ins in enumerate(seq):
        if ins.mnemonic == 'mov' and ins.op_str.startswith(('ecx, 0x', 'eax, 0x', 'edx, 0x')):
            v = int(ins.op_str.split(',')[1], 16)
            s = img.cstring(v - base) if v > base else None
            if s == column and start is None:
                start = i
            elif start is not None and s and len(s) == 8 and s.isalnum():
                return seq[start:i]
    return seq[start:start + 120] if start is not None else []


def store_holds(parser, column, store, offset):
    window = column_window(parser, column)
    if not window:
        return False, f'{column} not compared in parser {parser:#x}'
    adds = {int(i.op_str.split(',')[1], 16) for i in window if i.mnemonic == 'add' and i.op_str.startswith('ecx, 0x')}
    movs = {i.op_str for i in window if i.mnemonic == 'mov' and i.op_str.startswith('dword ptr [ecx + 0x')}
    calls = [i.op_str for i in window if i.mnemonic == 'call']
    if isinstance(store, int):
        ok = store in adds or any(m.startswith(f'dword ptr [ecx + {store:#x}]') for m in movs)
        return ok, f'stores {sorted(map(hex, adds))} {sorted(movs)}'
    if store[0] == 'split':
        ok = hex(SPLIT) in calls and (store[1] != 0 or offset in adds)
        return ok, f'calls {calls[:6]}, stores {sorted(map(hex, adds))}'
    if store[0] == 'virtual':
        slot, setter = store[1], store[2]
        ok = any(c.endswith(f'+ {slot:#x}]') for c in calls) and getter_offset(setter) == offset
        return ok, f'calls {calls[:6]}; setter {setter:#x} field {getter_offset(setter)}'
    return False, f'unknown store {store}'


# 0. the branch order, read from updateEvent: the literal pushed before each substring find
seen = []
seq = list(img.md.disasm(bytes(mem[UPDATE:UPDATE + 0x2400]), UPDATE))
for i, ins in enumerate(seq):
    if ins.mnemonic == 'call' and ins.op_str in ('0xf0ff0', '0xf1140'):
        for back in seq[max(0, i - 4):i][::-1]:
            if back.mnemonic == 'push' and back.op_str.startswith('0x'):
                s = img.cstring(int(back.op_str, 16) - base) if int(back.op_str, 16) > base else None
                if s and s not in seen and s.replace('_', '').isalnum():
                    seen.append(s)
                break
check('updateEvent tests the keys in the order DISPATCH lists', seen == DISPATCH, seen)

# 1. every DERIVED field has a fact, and every fact a DERIVED field
derived = {r.key: r for r in sc.DERIVED}
check('every DERIVED list has facts and every fact set a DERIVED list', set(derived) == set(FACTS),
      sorted(set(derived) ^ set(FACTS)))
for key, rule in derived.items():
    facts = FACTS.get(key)
    if not facts:
        continue
    want = sorted((f.column, f.token, f.folder) for f in rule.fields + tuple(f for j in rule.join for f in j.fields))
    have = sorted((fact[0], fact[1], fact[3]) for fact in facts['fields'])
    check(f'{key}: the rule\'s fields (its joins\' too) are the facts\' fields', want == have, f'{want} vs {have}')

# 2. literals, gates, dispatch order
for key, facts in FACTS.items():
    site, text = facts['builder']
    check(f'{key}: check-list key "{text}" at {site:#x}', literal_at(site) == text, literal_at(site))
    site, text = facts['branch']
    check(f'{key}: updateEvent branch "{text}" at {site:#x}, the list\'s own branch',
          literal_at(site) == text and text == key, literal_at(site))
    builder_key = facts['builder'][1]
    first = next((d for d in DISPATCH if d in builder_key), None)
    expect = facts['branch'][1] if facts['branch'][1] in builder_key else None
    check(f'{key}: the first branch whose literal the key "{builder_key}<id>" holds is '
          f'{expect or "none (the entry is never requested)"}', first == expect, first)
    if facts['gate']:
        va, at = facts['gate']
        ins = insn(at)
        check(f'{key}: check-list loop under the feature byte {va:#x} ({at:#x})',
              ins is not None and ins.mnemonic == 'cmp' and f'{va:#x}' in ins.op_str, ins and ins.op_str)
    for column, token, fsite, folder, getter, offset, store, *parser in facts['fields']:
        check(f'{key}: {column}{"" if token is None else f"[{token}]"} folder "{folder}" at {fsite:#x}',
              literal_at(fsite) == folder, literal_at(fsite))
        if getter is not None:
            check(f'{key}: getter {getter:#x} reads +{offset:#x}', getter_offset(getter) == offset, getter_offset(getter))
        if store == ('computed',):
            continue
        table_parser = parser[0] if parser else facts['parser']
        ok, why = store_holds(table_parser, column, store, offset)
        check(f'{key}: parser {table_parser:#x} stores {column} into +{offset:#x}', ok, why)

# 3. raid_boss_bgm: no branch tests the key the check list uses
check('raid_boss_bgm: "raid_boss_bgm" is used by updateEvent only',
      set(literal_refs('raid_boss_bgm')) == {UPDATE}, sorted(map(hex, set(literal_refs('raid_boss_bgm')))))
check('raid_boss_bgm: "raidBossBgm" is used by the check list only',
      set(literal_refs('raidBossBgm')) == {BUILDER}, sorted(map(hex, set(literal_refs('raidBossBgm')))))

# 4. general_event: getResouceFiles keeps rows whose +0x2C is 1, which the parser fills from b4D63uav
ins = insn(0x5E2317)
check('general_event: getResouceFiles 0x5E2317 compares the row\'s +0x2C with 1',
      ins is not None and ins.mnemonic == 'cmp' and ins.op_str == 'dword ptr [ecx + 0x2c], 1', ins and ins.op_str)
ok, why = store_holds(0x4EDCA0, 'b4D63uav', 0x2C, 0x2C)
check('general_event: parser 0x4EDCA0 stores b4D63uav into +0x2C', ok, why)
ins = insn(0x5E1F3E)
check('general_event: 0x5E1EB0 splits the row\'s +0x34', ins is not None and ins.op_str == 'edx, [esi + 0x34]'
      and insn(0x5E1F45).op_str == hex(SPLIT), ins and ins.op_str)

# 5. the resourceMap exclusion table
names, _sites = x86_bundle_exclusions.read_table(img)
check(f'resourceMap exclusion table at {x86_bundle_exclusions.TABLE:#x}: {len(names)} slots equal BUNDLE_EXCLUSIONS',
      list(sc.BUNDLE_EXCLUSIONS) == names, f'{sum(1 for a, b in zip(sc.BUNDLE_EXCLUSIONS, names) if a != b)} differ')

# 6. Session 19: item and area -- the request helpers, the frame name, the area type, the dungeon join and
#    its permit filter, who fills each list.
def calls(site, target):
    ins = insn(site)
    return ins is not None and ins.mnemonic == 'call' and ins.op_str == hex(target)


def op_is(site, mnemonic, op_str):
    ins = insn(site)
    return ins is not None and ins.mnemonic == mnemonic and ins.op_str == op_str


def vtable_slot(vtable_va, slot):
    return int.from_bytes(bytes(mem[vtable_va - base + 4 * slot:vtable_va - base + 4 * slot + 4]), 'little') - base


ITEM_AREA = [
    # item: the check list, the branch, requestItemFiles 0x7F6FA0
    ('item: the check list iterates ItemMstList 0x5E4F90 (call at 0x8E0CC7)', calls(0x8E0CC7, 0x5E4F90)),
    ('item: ... the array 0x5E5170 builds from its map (0x8E0CF3)', calls(0x8E0CF3, 0x5E5170)),
    ('item: key and value are the row id 0xE9960 (0x8E0D3B, 0x8E0D9F)', calls(0x8E0D3B, 0xE9960) and calls(0x8E0D9F, 0xE9960)),
    ('item: updateEvent finds "item" (0xF1140 at 0x8E377C) and calls requestItemFiles 0x7F6FA0 (0x8E3790)',
     calls(0x8E377C, 0xF1140) and calls(0x8E3790, 0x7F6FA0)),
    ('item: requestItemFiles looks the id up in ItemMstList\'s map (0x5E4F90 at 0x7F6FF6, find 0xE03F0 at 0x7F7010, '
     'row = node +0x28 at 0x7F7020)', calls(0x7F6FF6, 0x5E4F90) and calls(0x7F7010, 0xE03F0)
     and op_is(0x7F7020, 'mov', 'edi, dword ptr [eax + 0x28]')),
    ('item: the thumbnail getter 0xF1B80 (0x7F707F) and the request 0x63E120 (0x7F708F)',
     calls(0x7F707F, 0xF1B80) and calls(0x7F708F, 0x63E120)),
    ('item: the frame name 0x5E4D20 (0x7F70CF) asked for under /common/ when not empty (0x7F70DB, 0x63E120 at 0x7F70FC)',
     calls(0x7F70CF, 0x5E4D20) and op_is(0x7F70DB, 'cmp', 'dword ptr [ebp - 0x1c], 0') and calls(0x7F70FC, 0x63E120)),
    ('item: 0x5E4D20 forwards to 0x5E4D40 (0x5E4D2F)', calls(0x5E4D2F, 0x5E4D40)),
    # the frame name, branch by branch (item_frame_name)
    ('frame: rNmJkH6K +0x180 == 1 (0x5E4DA1) and the type +0x5C (0x5E4DA8) == 1 (0x5E4DB4) -> "item_frame_raid" (0x5E4DBB)',
     op_is(0x5E4DA1, 'cmp', 'dword ptr [edi + 0x180], 1') and op_is(0x5E4DA8, 'mov', 'eax, dword ptr [edi + 0x5c]')
     and op_is(0x5E4DB4, 'cmp', 'eax, 1') and literal_at(0x5E4DBB) == 'item_frame_raid'),
    ('frame: ZKQ1X69a +0x174 == 1 (0x5E4DCC), the type not 3 (0x5E4DD5) or 5 (0x5E4DDA): type 2 (0x5E4DE1) '
     '"item_frame_5" (0x5E4DE6), else "item_frame_raid" (0x5E4DF5)',
     op_is(0x5E4DCC, 'cmp', 'dword ptr [edi + 0x174], 1') and op_is(0x5E4DD5, 'cmp', 'eax, 3')
     and op_is(0x5E4DDA, 'cmp', 'eax, 5') and op_is(0x5E4DE1, 'cmp', 'eax, 2') and literal_at(0x5E4DE6) == 'item_frame_5'
     and literal_at(0x5E4DF5) == 'item_frame_raid'),
    ('frame: types 888002 / 888001 (0x5E4E04, 0x5E4E0F) -> "item_frame_" + 4 (0x5E4EB6, 0x5E4EC4)',
     op_is(0x5E4E04, 'cmp', 'eax, 0xd8cc2') and op_is(0x5E4E0F, 'cmp', 'eax, 0xd8cc1')
     and op_is(0x5E4EB6, 'mov', 'edx, 4') and literal_at(0x5E4EC4) == 'item_frame_'),
    ('frame: below 5 (0x5E4E1A) "item_frame_" + the type (to_string 0x65DEE0 at 0x5E4E24, 0x5E4E2A)',
     op_is(0x5E4E1A, 'cmp', 'eax, 5') and calls(0x5E4E24, 0x65DEE0) and literal_at(0x5E4E2A) == 'item_frame_'),
    ('frame: type 5 -> "item_frame_" + 6 (0x5E4E56, 0x5E4E64); type 6 (0x5E4E9C) -> "item_frame_7" (0x5E4EA5)',
     op_is(0x5E4E56, 'mov', 'edx, 6') and literal_at(0x5E4E64) == 'item_frame_' and op_is(0x5E4E9C, 'cmp', 'eax, 6')
     and literal_at(0x5E4EA5) == 'item_frame_7'),
    ('frame: every branch appends ".png" (0x5E4F41, append 0xEDD60 at 0x5E4F48)',
     literal_at(0x5E4F41) == '.png' and calls(0x5E4F48, 0xEDD60)),
    ('item: the parser 0x4EFCF0 reads the type h0K7wjeH with stream >> int (0x65E860 at 0x4EFF4B) into +0x5C (0x4EFF53)',
     calls(0x4EFF4B, 0x65E860) and op_is(0x4EFF53, 'mov', 'dword ptr [ecx + 0x5c], eax')),
    # area: the check list, the branch, requestAreaFiles 0x7F7140, the dungeon join
    ('area: the check list iterates AreaMstList 0x5C61E0 (0x8E0B4B, 0x8E0B64), key and value the row id (0x8E0B95, '
     '0x8E0BF9)', calls(0x8E0B4B, 0x5C61E0) and calls(0x8E0B64, 0x5C61E0) and calls(0x8E0B95, 0xE9960)
     and calls(0x8E0BF9, 0xE9960)),
    ('area: updateEvent finds "area" (0xF0FF0 at 0x8E3807) and calls requestAreaFiles 0x7F7140 (0x8E381F)',
     calls(0x8E3807, 0xF0FF0) and calls(0x8E381F, 0x7F7140)),
    ('area: requestAreaFiles: AreaMstList 0x5C61E0 (0x7F7196), getAreaMstWithAreaID 0x5C6360 (0x7F71A4)',
     calls(0x7F7196, 0x5C61E0) and calls(0x7F71A4, 0x5C6360)),
    ('area: img 0xF4FF0 (0x7F71B1, 0x7F7228) under "/area/" (0x7F71FC); plate 0xF1B80 (0x7F7278, 0x7F72E6, 0x7F7340)',
     calls(0x7F71B1, 0xF4FF0) and calls(0x7F7228, 0xF4FF0) and literal_at(0x7F71FC) == '/area/'
     and calls(0x7F7278, 0xF1B80) and calls(0x7F72E6, 0xF1B80) and calls(0x7F7340, 0xF1B80)),
    ('area: the plate under "/dungeon/" (0x7F7326) when the type +0x44 (0x7F72BF) is 1 (0x7F72C2) or 3 (0x7F72C7), '
     'else "/area/" (0x7F72CC)', op_is(0x7F72BF, 'mov', 'eax, dword ptr [edi + 0x44]') and op_is(0x7F72C2, 'cmp', 'eax, 1')
     and op_is(0x7F72C7, 'cmp', 'eax, 3') and literal_at(0x7F7326) == '/dungeon/' and literal_at(0x7F72CC) == '/area/'),
    ('area: the parser 0x4CBC80 reads 3v1qg7Uj with stream >> int (0x4CBE8D) into +0x44 (0x4CBE95)',
     calls(0x4CBE8D, 0x65E860) and op_is(0x4CBE95, 'mov', 'dword ptr [ecx + 0x44], eax')),
    ('area: then the area\'s dungeons: DungeonMstList 0x5DA030 (0x8E3842), getDungeonListWithAreaID 0x5DA320 (0x8E3851), '
     'each one\'s getImgIcon 0x399A90 (0x8E38BA) through 0x63E120 (0x8E38D2)',
     calls(0x8E3842, 0x5DA030) and calls(0x8E3851, 0x5DA320) and calls(0x8E38BA, 0x399A90) and calls(0x8E38D2, 0x63E120)),
    ('join: a dungeon belongs to the area when its +0x5C (getter 0xEE4C0 at 0x5DA3ED) equals the id',
     calls(0x5DA3ED, 0xEE4C0) and getter_offset(0xEE4C0) == 0x5C),
    ('join: ... AND the permit list accepts it -- singleton 0x1997730 (0x5DA4C7), 0x582D80 (0x5DA50C) on the dungeon id '
     '0xE9960 (0x5DA4BE): PermitPlaceInfoList::isPermitDungeon (a filter only; every dungeon is required)',
     op_is(0x5DA4C7, 'mov', 'eax, dword ptr [0x1997730]') and calls(0x5DA50C, 0x582D80) and calls(0x5DA4BE, 0xE9960)),
    ('join: the dungeon parser 0x4E3230 stores the area id VjCY7rX4 into +0x5C', store_holds(0x4E3230, 'VjCY7rX4', 0x5C, 0x5C)[0]),
    # who fills each list
    ('area: getResponseObject 0x493770 builds AreaMstResponse (vtable 0x18F2A60 at 0x495895) for 3SG2wX0R (0x49583C)',
     literal_at(0x49583C) == '3SG2wX0R' and op_is(0x495895, 'mov', 'dword ptr [esi], 0x18f2a60')),
    ('area: its readParam (vtable slot 5) is the parser 0x4CBC80', vtable_slot(0x18F2A60, 5) == 0x4CBC80),
    ('area: ... which REPLACES the list at row 0 / field 0: AreaMstList (0x4CBCDC), every row released (0x4CBCF4), '
     'end = begin (0x4CBD08)', calls(0x4CBCDC, 0x5C61E0) and op_is(0x4CBCF4, 'call', 'ebx')
     and op_is(0x4CBD08, 'mov', 'dword ptr [edi + 0x18], eax')),
    ('item: ItemMstResponse (vtable 0x18F39C0, readParam 0x4EFCF0) for 2C7LDzYk (0x4965D9, 0x496634)',
     literal_at(0x4965D9) == '2C7LDzYk' and op_is(0x496634, 'mov', 'dword ptr [esi], 0x18f39c0')
     and vtable_slot(0x18F39C0, 5) == 0x4EFCF0),
    ('dungeon: DungeonMstResponse (vtable 0x18F34E0, readParam 0x4E3230) for 01pzrTCm (0x4958A9, 0x495904)',
     literal_at(0x4958A9) == '01pzrTCm' and op_is(0x495904, 'mov', 'dword ptr [esi], 0x18f34e0')
     and vtable_slot(0x18F34E0, 5) == 0x4E3230),
    ('loaders: loadItemMst 0x54E310 / loadDungeonMst 0x544D40 / loadAreaMst 0x544C30 build those responses over the '
     'client\'s own F_ITEM_MST / F_DUNGEON_MST / F_AREA_MST (0x54E3F6, 0x544E26, 0x544D16) through 0x543030',
     op_is(0x54E365, 'mov', 'dword ptr [esi], 0x18f39c0') and literal_at(0x54E3F6) == 'F_ITEM_MST' and calls(0x54E408, 0x543030)
     and op_is(0x544D95, 'mov', 'dword ptr [esi], 0x18f34e0') and literal_at(0x544E26) == 'F_DUNGEON_MST'
     and calls(0x544E38, 0x543030) and op_is(0x544C85, 'mov', 'dword ptr [esi], 0x18f2a60')
     and literal_at(0x544D16) == 'F_AREA_MST' and calls(0x544D28, 0x543030)),
    ('loaders: called from the load-alls loadMst 0x53D340 (0x53DC3C, 0x53D428, 0x53D40D; LoginScene calls it at '
     '0x8E2B3D) and loadMsts 0x555A50 (0x5569B2, 0x5567F2, 0x556712)',
     calls(0x8E2B3D, 0x53D340) and calls(0x53DC3C, 0x54E310) and calls(0x53D428, 0x544D40) and calls(0x53D40D, 0x544C30)
     and calls(0x5569B2, 0x54E310) and calls(0x5567F2, 0x544D40) and calls(0x556712, 0x544C30)),
]
for label, ok in ITEM_AREA:
    check(label, bool(ok), 'see out/qa/current/session19/re')

# The weapon / summoner_img stand-in (still an unknown list) enumerates the folders its branches ask for:
# deriving item exactly took item/ out of the item stand-in, and the weapon branch asks for /item/.
weapon = next(c for c in sc.CATEGORIES if c[0] == 'weapon, summoner_img')
check('stand-in weapon, summoner_img: updateEvent\'s weapon branch (0x8E379E) calls 0x7F6CE0 (0x8E37BE), which '
      'asks for "/item/" (0x7F6D50); the summoner_img branch (0x8E37CC) calls 0x7F2690 (0x8E37EF) -> 0x7F27E0 '
      '(0x7F26FE), which asks for /unit/img/ (0x7F2817), /unit/cgg/ (0x7F29DD), /unit/cgs/ (0x7F2A41); the '
      'stand-in enumerates exactly those folders',
      literal_at(0x8E379E) == 'weapon' and calls(0x8E37BE, 0x7F6CE0) and literal_at(0x7F6D50) == '/item/'
      and literal_at(0x8E37CC) == 'summoner_img' and calls(0x8E37EF, 0x7F2690) and calls(0x7F26FE, 0x7F27E0)
      and literal_at(0x7F2817) == '/unit/img/' and literal_at(0x7F29DD) == '/unit/cgg/'
      and literal_at(0x7F2A41) == '/unit/cgs/' and set(weapon[3]) == {'item', 'unit/img', 'unit/cgg', 'unit/cgs'},
      weapon[3])

# The server side of "who fills each list" (not the binary): of the three keys only 3SG2wX0R is a field of
# any response the server writes (gimuserver/packets/all.hpp), so the item and dungeon lists stay the
# client's own tables.
packets = (HERE.parents[1] / 'gimuserver' / 'packets' / 'all.hpp').read_text(encoding='utf-8', errors='replace')
check('server: a response field carries 3SG2wX0R (UserInfo area_mst) and none carries 2C7LDzYk or 01pzrTCm',
      '"3SG2wX0R", &T::' in packets and '"2C7LDzYk", &T::' not in packets and '"01pzrTCm", &T::' not in packets,
      [k for k in ('3SG2wX0R', '2C7LDzYk', '01pzrTCm') if f'"{k}", &T::' in packets])

print(f'x86_startup_rules_check: {sum(results)} passed, {len(results) - sum(results)} failed')
raise SystemExit(0 if all(results) else 1)
