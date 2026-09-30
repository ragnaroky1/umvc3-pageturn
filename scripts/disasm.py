"""Disassemble a function from the unpacked umvc3.exe using .pdata function bounds.
Usage: python scripts/disasm.py <VA inside function> [maxbytes]
Annotates rip-relative operands that point at strings in .rdata.
"""
import os, sys, struct, re
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
EXE = os.environ.get("UMVC3_EXE", "C:/Tools/ghidra_projects/umvc3.exe.unpacked.exe")
BASE = 0x140000000
d = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]; nsec = struct.unpack_from('<H', d, pe+6)[0]; optsz = struct.unpack_from('<H', d, pe+20)[0]
so = pe + 24 + optsz
SECS = []
for i in range(nsec):
    n = d[so+i*40:so+i*40+8].rstrip(b'\0').decode(); vs, va, rs, ro = struct.unpack_from('<IIII', d, so+i*40+8)
    SECS.append((n, va, ro, rs, vs))
def rva2raw(rva):
    for n, va, ro, rs, vs in SECS:
        if va <= rva < va + max(rs, vs): return ro + (rva - va)
    return None
def read(va, n): return d[rva2raw(va-BASE):rva2raw(va-BASE)+n]
pd = next(s for s in SECS if s[0] == '.pdata')
funcs = []
for i in range(0, pd[3], 12):
    s, e, u = struct.unpack_from('<III', d, pd[2]+i)
    if s == 0: break
    funcs.append((s, e))
funcs.sort()
def func_bounds(va):
    rva = va - BASE
    best = None
    for s, e in funcs:
        if s <= rva < e: best = (s, e)
    return best
def annot(va):
    raw = rva2raw(va-BASE)
    if raw is None: return ""
    b = d[raw:raw+48]
    m = re.match(rb'[\x20-\x7e]{3,}', b)
    if m: return '"' + m.group().decode() + '"'
    q = struct.unpack_from('<Q', d, raw)[0] if raw+8 <= len(d) else 0
    if BASE <= q < BASE+0x1000000: return f"ptr->{q:#x}"
    return ""
if __name__ == "__main__":
    va = int(sys.argv[1], 16)
    fb = func_bounds(va)
    if fb and len(sys.argv) < 3:
        start, end = BASE+fb[0], BASE+fb[1]
    else:
        start = va; end = va + int(sys.argv[2],0) if len(sys.argv) > 2 else va + 0x200
    print(f"; function {start:#x}-{end:#x} ({end-start} bytes)")
    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
    code = read(start, end-start)
    for ins in md.disasm(code, start):
        note = ""
        m = re.search(r'rip \+ (0x[0-9a-f]+)\]', ins.op_str) or re.search(r'rip - (0x[0-9a-f]+)\]', ins.op_str)
        if m:
            off = int(m.group(1), 16); tgt = ins.address + ins.size + (off if '+' in m.group(0) else -off)
            note = f"; {tgt:#x} {annot(tgt)}"
        elif ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
            note = f"; {annot(int(ins.op_str,16))}"
        print(f"{ins.address:#x}  {ins.mnemonic:8} {ins.op_str:40} {note}")
