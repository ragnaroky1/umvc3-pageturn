"""Find RIP-relative code references and absolute data pointers to given VAs in umvc3.exe.
Usage: python scripts/xref_scan.py 0x140b3f0c0 [more VAs...]
"""
import sys, struct
EXE = r"C:\Program Files (x86)\Steam\steamapps\common\ULTIMATE MARVEL VS. CAPCOM 3\umvc3.exe"
BASE = 0x140000000
SECS = [(".text", 0x1000, 0x400, 0xa59200), (".rdata", 0xa5b000, 0xa59600, 0x1f5e00), (".data", 0xc51000, 0xc4f400, 0xf3000)]
d = open(EXE, 'rb').read()
targets = [int(a, 16) for a in sys.argv[1:]]
tset = set(targets)
tv, tr, ts = SECS[0][1], SECS[0][2], SECS[0][3]
code = d[tr:tr+ts]
hits = {t: [] for t in targets}
OPS = {0x8D, 0x8B, 0x89, 0x3B, 0x39, 0x83, 0x81, 0xC7, 0x8A, 0x88, 0x0F, 0xFF, 0x63, 0x48, 0x4C}
for i in range(len(code) - 8):
    op = code[i]
    if op not in OPS: continue
    for mo in (1, 2, 3):
        if i + mo + 5 > len(code): break
        modrm = code[i + mo]
        if (modrm & 0xC7) == 0x05:
            disp = struct.unpack_from('<i', code, i + mo + 1)[0]
            for extra in (0, 1, 4):
                end = i + mo + 1 + 4 + extra
                tgt = BASE + tv + end + disp
                if tgt in tset:
                    hits[tgt].append(("code", BASE + tv + i, code[i:i+mo+5].hex()))
                    break
for n, v, r, s in SECS[1:]:
    blob = d[r:r+s]
    for i in range(0, len(blob) - 8, 8):
        q = struct.unpack_from('<Q', blob, i)[0]
        if q in tset:
            hits[q].append(("ptr", BASE + v + i, n))
for t in targets:
    print(hex(t))
    seen=set()
    for h in hits[t]:
        if h[1] in seen: continue
        seen.add(h[1]); print("   ", h[0], hex(h[1]), h[2])
