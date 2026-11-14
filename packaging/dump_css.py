#!/usr/bin/env python3
import os, sys
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".work")
REPO = os.path.normpath(os.path.join(WORK, ".."))
"""Targeted: decode every JSON-encoded CSS resource (starts with '"\\n' or '"@')."""
import os, struct, json

BIN = "" + WORK + "/upstream/usr/bin/luxai"
data = open(BIN, "rb").read()

starts = []
for pat in (b'"\x5cnwindow.luxai,', b'"\x5cn@define-color', b'"\x5cn.luxai'):
    i = 0
    while True:
        j = data.find(pat, i)
        if j < 0: break
        starts.append(j)
        i = j + 1
print("candidate CSS starts:", [hex(x) for x in starts])

for k, j in enumerate(starts):
    # header check: len u64 right before quote?
    ln = struct.unpack("<Q", data[j-8:j])[0]
    ok = (j - 8 - 16 + ln) >= 0
    end = j
    while True:
        end = data.find(b'"', end + 1)
        if end < 0: break
        nxt8 = data[end+1:end+9]
        if nxt8 == b"\x00" * 8 or nxt8[:1] == b"\x00":
            break
    d = data[j:end+1]
    try:
        css = json.loads(d.decode("utf-8"))
        fn = f"" + WORK + "/css_{k}.css"
        open(fn, "w").write(css)
        print(f"[{k}] data@{j:#x} declared_len_before={ln} decoded {len(css)} chars -> {fn}")
    except Exception as e:
        print(f"[{k}] FAIL {e}")
