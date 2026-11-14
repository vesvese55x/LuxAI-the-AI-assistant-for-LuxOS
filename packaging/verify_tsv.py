#!/usr/bin/env python3
import os, sys
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".work")
REPO = os.path.normpath(os.path.join(WORK, ".."))
"""Verify a generated luxai .tsv against the upstream key set:
- every expected English key present exactly once
- no unexpected keys
- placeholders ({0},{1},{2},%d,%s,{1:.0f} etc.) preserved
Prints a per-file summary and lists problems."""
import re, sys, glob

REF = "" + WORK + "/upstream/usr/share/luxai/i18n/sk.tsv"

def load(path):
    tr = {}
    order = []
    for i, line in enumerate(open(path, encoding="utf-8"), 1):
        line = line.rstrip("\n")
        if not line or line.startswith("#") or "\t" not in line:
            continue
        k, v = line.split("\t", 1)
        if k in tr:
            print(f"{path}:{i}: DUPLICATE key {k!r}")
        tr[k] = v
        order.append(k)
    return tr, order

ref, ref_order = load(REF)
print(f"reference keys: {len(ref)}")

ph = re.compile(r"\{[^}]*\}|%[dsc]")
ok = True
for path in sys.argv[1:]:
    tr, order = load(path)
    missing = [k for k in ref_order if k not in tr]
    extra = [k for k in order if k not in ref]
    nph = []
    for k, v in tr.items():
        if k in ref and sorted(ph.findall(k)) != sorted(ph.findall(v)):
            nph.append(k)
    print(f"\n== {path}: {len(tr)} keys; missing={len(missing)} extra={len(extra)} placeholder-mismatch={len(nph)}")
    for k in missing[:6]: print("  MISSING:", repr(k))
    for k in extra[:6]: print("  EXTRA:", repr(k))
    for k in nph[:6]: print("  PH:", repr(k), "->", repr(tr[k]))
    if missing or extra or nph: ok = False
print("\nALL OK" if ok else "\nPROBLEMS FOUND")
