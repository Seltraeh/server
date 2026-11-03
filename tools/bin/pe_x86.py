"""Read the Windows x86 client like so_disasm.py reads libgame.so: functions, imports, callers, strings.

    python tools/bin/pe_x86.py fn 0x62B270 [--max 400]      disassemble one function (to its last ret)
    python tools/bin/pe_x86.py callers 0x62B270              direct E8 call sites, with their functions
    python tools/bin/pe_x86.py import "CCParticleSystemQuad" every call through an IAT slot whose name matches
    python tools/bin/pe_x86.py strref "is missing."          code that pushes/moves a string literal's address
    python tools/bin/pe_x86.py imm 0x2E53414D               code containing a 32-bit immediate

Targets: BraveFrontier.Windows.exe (default) or --cocos for libcocos2d_v2.2.5_Windows_8.1.dll.
Calls are annotated with import names (the cocos DLL exports C++ names, so an EXE call into
cocos reads as its mangled name) and with the cocos export a direct call lands on.  Addresses
are RVAs.  Function starts are found by walking back to int3/nop padding: good enough for MSVC
code, wrong inside the rare function without padding before it (check the prologue).

The client is the supported x86 build, BraveFrontier.Windows.exe SHA-256 fb4f3c25f02944ca... (build
6253F827-01756000-00ED273D), never distributed with this repository.  BF_CLIENT_EXE names it when it is not
at the default path below; the cocos DLL is read from the same folder.
"""
from __future__ import annotations

import os
import re
import struct
import sys
from functools import lru_cache

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM

EXE = os.environ.get('BF_CLIENT_EXE') or r'C:\Users\Evan\BF\BraveFrontierAppxClient\BraveFrontier.Windows.exe'
COCOS = os.path.join(os.path.dirname(EXE), 'libcocos2d_v2.2.5_Windows_8.1.dll')


class Image:
    def __init__(self, path: str):
        if not os.path.isfile(path):
            raise SystemExit(f'{path}: not found. These checks read the supported client binary '
                             '(BraveFrontier.Windows.exe fb4f3c25...); set BF_CLIENT_EXE to its path.')
        self.pe = pefile.PE(path)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = self.pe.get_memory_mapped_image()
        self.text = [(s.VirtualAddress, s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData))
                     for s in self.pe.sections if s.Characteristics & 0x20000000]
        self.imports = {}      # IAT slot RVA -> "dll!name"
        for d in getattr(self.pe, 'DIRECTORY_ENTRY_IMPORT', []):
            dll = d.dll.decode(errors='replace')
            for imp in d.imports:
                name = imp.name.decode(errors='replace') if imp.name else f'#{imp.ordinal}'
                self.imports[imp.address - self.base] = f'{dll}!{name}'
        self.exports = {}      # RVA -> name
        if hasattr(self.pe, 'DIRECTORY_ENTRY_EXPORT'):
            for e in self.pe.DIRECTORY_ENTRY_EXPORT.symbols:
                if e.name:
                    self.exports.setdefault(e.address, e.name.decode(errors='replace'))
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = True

    def in_text(self, rva: int) -> bool:
        return any(a <= rva < b for a, b in self.text)

    def cstring(self, rva: int, limit: int = 160) -> str | None:
        if rva <= 0 or rva >= len(self.mem) or self.in_text(rva):
            return None
        raw = self.mem[rva:rva + limit]
        end = raw.find(b'\0')
        if end < 2:
            return None
        s = raw[:end]
        if all(32 <= c < 127 or c in (9, 10, 13) for c in s):
            return s.decode()
        return None

    def func_start(self, rva: int) -> int:
        """Walks back to the byte after int3/nop padding (MSVC aligns functions)."""
        p = rva
        while p > 0:
            if self.mem[p - 1] in (0xCC,) and self.mem[p - 2] in (0xCC, 0xC3, 0xC2, 0x90):
                return p
            p -= 1
            if rva - p > 0x20000:
                break
        return rva

    def insns(self, start: int, maxn: int = 4000):
        """Linear sweep from start to the end of the function (a ret followed by padding or a new start)."""
        out = []
        pos = start
        while len(out) < maxn:
            chunk = self.mem[pos:pos + 64]
            got = False
            for ins in self.md.disasm(chunk, pos):
                out.append(ins)
                pos = ins.address + ins.size
                got = True
                if ins.mnemonic in ('ret', 'retf') and (self.mem[pos] in (0xCC,) or pos in self.exports):
                    return out
                if ins.mnemonic == 'int3':
                    return out
                break
            if not got:
                break
        return out

    def describe_target(self, ins) -> str:
        notes = []
        for op in ins.operands:
            if op.type == X86_OP_IMM:
                t = op.imm - (self.base if op.imm >= self.base else 0)
                if ins.mnemonic.startswith('call') or ins.mnemonic.startswith('j'):
                    t = op.imm
                    if t in self.exports:
                        notes.append(self.exports[t])
                else:
                    s = self.cstring(op.imm - self.base) if op.imm >= self.base else None
                    if s is not None:
                        notes.append(repr(s))
            elif op.type == X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                slot = op.mem.disp - self.base
                if slot in self.imports:
                    notes.append(self.imports[slot])
                else:
                    s = self.cstring(slot)
                    if s is not None:
                        notes.append(repr(s))
        return '; ' + ' | '.join(notes) if notes else ''


@lru_cache(maxsize=2)
def image(which: str) -> Image:
    return Image(COCOS if which == 'cocos' else EXE)


def cmd_fn(img: Image, rva: int, maxn: int):
    start = img.func_start(rva) if rva not in img.exports else rva
    if start != rva:
        print(f'(function containing 0x{rva:X} starts at 0x{start:X})')
    insns = img.insns(start, maxn)
    print(f'fn 0x{start:X} {img.exports.get(start, "")}: {len(insns)} instructions')
    for ins in insns:
        # capstone gives absolute addresses for an image based at 0; we map at RVA directly.
        print(f'  0x{ins.address:06X}  {ins.mnemonic:8s} {ins.op_str}  {img.describe_target(ins)}')


def e8_callers(img: Image, target: int):
    out = []
    for a, b in img.text:
        seg = img.mem[a:b]
        i = seg.find(b'\xe8')
        while i >= 0:
            if i + 5 <= len(seg):
                rel = struct.unpack_from('<i', seg, i + 1)[0]
                if a + i + 5 + rel == target:
                    out.append(a + i)
            i = seg.find(b'\xe8', i + 1)
    return out


def cmd_callers(img: Image, target: int):
    sites = e8_callers(img, target)
    print(f'{len(sites)} direct call site(s) to 0x{target:X} {img.exports.get(target, "")}')
    for s in sites:
        f = img.func_start(s)
        print(f'  call at 0x{s:06X} in fn 0x{f:06X} {img.exports.get(f, "")}')


def mem_refs(img: Image, value: int):
    needle = struct.pack('<I', value)
    out = []
    for a, b in img.text:
        seg = img.mem[a:b]
        i = seg.find(needle)
        while i >= 0:
            out.append(a + i)
            i = seg.find(needle, i + 1)
    return out


def cmd_import(img: Image, pattern: str):
    rx = re.compile(pattern)
    for slot, name in sorted(img.imports.items()):
        if not rx.search(name):
            continue
        refs = mem_refs(img, img.base + slot)
        print(f'IAT 0x{slot:X} {name}: {len(refs)} reference(s)')
        for r in refs:
            f = img.func_start(r)
            print(f'    at ~0x{r:06X} in fn 0x{f:06X}')


def cmd_strref(img: Image, text: str):
    raw = text.encode()
    i = img.mem.find(raw)
    found = 0
    while i >= 0:
        if (i == 0 or img.mem[i - 1] == 0) and not img.in_text(i):
            refs = mem_refs(img, img.base + i)
            s = img.cstring(i, 200)
            print(f'string 0x{i:X} {s!r}: {len(refs)} code reference(s)')
            for r in refs:
                print(f'    at ~0x{r:06X} in fn 0x{img.func_start(r):06X}')
            found += 1
        i = img.mem.find(raw, i + 1)
    if not found:
        print('no NUL-preceded occurrence of', repr(text))


def cmd_imm(img: Image, value: int):
    refs = mem_refs(img, value)
    print(f'{len(refs)} occurrence(s) of 0x{value:X} in code')
    for r in refs:
        print(f'    at 0x{r:06X} in fn 0x{img.func_start(r):06X}')


def main(argv: list[str]) -> int:
    which = 'cocos' if '--cocos' in argv else 'exe'
    argv = [a for a in argv if a != '--cocos']
    maxn = 4000
    if '--max' in argv:
        i = argv.index('--max')
        maxn = int(argv[i + 1], 0)
        del argv[i:i + 2]
    if len(argv) < 2:
        print(__doc__)
        return 1
    img = image(which)
    cmd, arg = argv[0], argv[1]
    if cmd == 'fn':
        cmd_fn(img, int(arg, 0), maxn)
    elif cmd == 'callers':
        cmd_callers(img, int(arg, 0))
    elif cmd == 'import':
        cmd_import(img, arg)
    elif cmd == 'strref':
        cmd_strref(img, arg)
    elif cmd == 'imm':
        cmd_imm(img, int(arg, 0))
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
