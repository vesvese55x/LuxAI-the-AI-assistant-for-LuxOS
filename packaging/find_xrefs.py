#!/usr/bin/env python3
import os, sys
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".work")
REPO = os.path.normpath(os.path.join(WORK, ".."))
"""Find code references (rip-relative LEA/MOV) to the language JSON strings,
locate exact schema JSON start, and inspect LANG/auto detection strings."""
import os, struct, re, sys

BIN = "" + WORK + "/upstream/usr/bin/luxai"
data = open(BIN, "rb").read()

TEXT_START, TEXT_END = 0x1000, 0x1000 + 0x1723f0

# .data file->vaddr shift: vaddr = file + 0x1000 for file >= 0x177a80
def f2v(off):
    return off + 0x1000 if off >= 0x177a80 else off

targets = {
    "dropdown": 0x18d3e0,
    "langmap": 0x181f48,
}

# exact schema start: the {"model":["flash",...  JSON
i = data.find(b'{"model":["flash","lite"],"mode"')
print("schema JSON start file off:", hex(i), "vaddr:", hex(f2v(i)))
targets["schema"] = i

# find end of schema
end = data.find(b'\x00', i)
print("schema len:", end - i)
print(repr(data[i:end].decode()))

# scan .text for rip-relative lea: REX.W 8d /r with mod=00 rm=101
found = []
for pos in range(TEXT_START, TEXT_END - 7):
    b = data[pos]
    if b == 0x48 and data[pos+1] == 0x8d:
        modrm = data[pos+2]
        if (modrm & 0xC7) == 0x05:  # mod=00, rm=101 -> rip-relative
            disp = struct.unpack("<i", data[pos+3:pos+7])[0]
            target_v = pos + 7 + disp  # vaddr of next instr + disp (vaddr==file for .text)
            found.append((pos, target_v))
    elif b in (0x4c,) and data[pos+1] == 0x8d:
        modrm = data[pos+2]
        if (modrm & 0xC7) == 0x05:
            disp = struct.unpack("<i", data[pos+3:pos+7])[0]
            target_v = pos + 7 + disp
            found.append((pos, target_v))

tvals = {name: f2v(off) for name, off in targets.items()}
print("\n target vaddrs:", {k: hex(v) for k, v in tvals.items()})
for pos, tgt in found:
    for name, v in tvals.items():
        if tgt == v:
            print(f"  XREF {name}: lea at file {pos:#x} (vaddr {pos:#x}) -> {tgt:#x}")
            ctx = data[max(0,pos-16):pos+16]
            print("   context:", ctx.hex())

# LANG env detection strings
for pat in [b"LANG", b"LC_ALL", b"LC_MESSAGES", b"LANGUAGE"]:
    locs = []
    start = 0
    while True:
        j = data.find(pat, start)
        if j < 0: break
        # ensure null-terminated standalone string
        if (j == 0 or data[j-1] == 0) and data[j+len(pat)] == 0:
            locs.append(j)
        start = j + 1
    print(f"\nstandalone {pat!r}: {[hex(x) for x in locs]}")

# .tsv context
j = data.find(b".tsv")
print("\n.tsv at", hex(j), "context:", repr(data[j-64:j+32]))

# getenv import?
if b"getenv" in data:
    print("getenv string present at", hex(data.find(b"getenv")))
