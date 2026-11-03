"""The client's own list of resourceMap names it never downloads (x86 BraveFrontier.Windows.exe).

DLCManagerComplementor::loadBundleResourceMap (0x16F7F0) splits each resourceMap line and keeps a
token only if it is not "\\r" (0x168F604) and equals none of the std::string objects of the global
table at VA 0x184DF00 (MSVC std::string, stride 0x18: the loop walks their size fields from
0x184DF10, bound 0x1308 = 203 entries; 0x16FA90-0x16FB1C).  Static initializers starting at
0x006B10 fill the table: for each slot `mov ecx, <slot>`, `push <literal>` (and its length), then
std::string::assign(const char*, size_t) 0xDDDC0 or, from slot 34 on, the const char* constructor
0xDDEC0 -- in either order, and split over several functions.  This sweeps that code linearly and
pairs each assign call with the slot and literal set before it.

    python tools/bin/x86_bundle_exclusions.py [--json OUT] [--check]

--check compares the table with scripts/startup_coverage.py's BUNDLE_EXCLUSIONS (slot by slot)
and exits 1 on any difference.  Built for client build 6253F827-01756000-00ED273D (fb4f3c25...).
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import pe_x86  # noqa: E402

TABLE = 0x184DF00
STRIDE = 0x18
COUNT = 0x1308 // STRIDE
ASSIGN = (0xDDDC0, 0xDDEC0)   # assign(const char*, size_t); the const char* constructor
SWEEP = (0x006B10, 0x00A000)


def sweep(img, lo, hi):
    """Linear disassembly that resynchronises one byte at a time over anything it cannot decode."""
    pos = lo
    while pos < hi:
        got = False
        for ins in img.md.disasm(bytes(img.mem[pos:hi]), pos):
            got = True
            yield ins
            pos = ins.address + ins.size
        if not got or pos < hi:
            pos += 1


def read_table(img) -> tuple[list, dict]:
    """(names in slot order -- None for a slot no initializer names --, slot -> assign call RVA)."""
    base = img.base
    calls = {hex(a) for a in ASSIGN}
    slots = {}
    slot = literal = None
    for ins in sweep(img, *SWEEP):
        if ins.mnemonic == 'mov' and ins.op_str.startswith('ecx, 0x'):
            v = int(ins.op_str.split(',')[1], 16)
            slot = v if TABLE <= v < TABLE + STRIDE * COUNT and (v - TABLE) % STRIDE == 0 else None
        elif ins.mnemonic == 'push' and ins.op_str.startswith('0x'):
            v = int(ins.op_str, 16)
            s = img.cstring(v - base) if v > base else None
            if s is not None:
                literal = s
        elif ins.mnemonic == 'call':
            if ins.op_str in calls and slot is not None and literal is not None:
                slots.setdefault((slot - TABLE) // STRIDE, (literal, ins.address))
            slot = literal = None
    names = [slots.get(i, (None, None))[0] for i in range(COUNT)]
    return names, {i: a for i, (_s, a) in slots.items()}


def main() -> int:
    img = pe_x86.image('exe')
    names, sites = read_table(img)
    missing = [i for i, n in enumerate(names) if n is None]
    print(f'table {TABLE:#x}: {COUNT} slots, {COUNT - len(missing)} named by their initializer, {len(missing)} not found'
          + (f' (slots {missing[:20]})' if missing else ''))
    if '--json' in sys.argv:
        out = Path(sys.argv[sys.argv.index('--json') + 1])
        out.write_text(json.dumps({'table_va': hex(TABLE), 'count': COUNT, 'loader': '0x16F7F0 (0x16FA90-0x16FB1C)',
                                   'assign_sites': {str(i): hex(a) for i, a in sorted(sites.items())},
                                   'names': names}, indent=1), encoding='utf-8')
    if '--check' in sys.argv:
        sys.path.insert(0, str(HERE.parents[1] / 'scripts'))
        import startup_coverage as sc
        embedded = list(sc.BUNDLE_EXCLUSIONS)
        diff = [i for i in range(max(len(names), len(embedded)))
                if (names[i] if i < len(names) else None) != (embedded[i] if i < len(embedded) else None)]
        print(f'scripts/startup_coverage.py BUNDLE_EXCLUSIONS: {len(embedded)} slots; '
              + ('identical to the binary' if not diff and not missing else f'{len(diff)} slot(s) differ, e.g. {diff[:8]}'))
        return 1 if diff or missing else 0
    for i, n in enumerate(names):
        print(f'  {i:3d} {n!r}')
    return 1 if missing else 0


if __name__ == '__main__':
    raise SystemExit(main())
