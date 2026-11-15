#!/usr/bin/env python3
import os, sys
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".work")
REPO = os.path.normpath(os.path.join(WORK, ".."))
"""Patch LuxAI binary (luxai-2.0.2-3):
1. Minify embedded main CSS (frees space inside its length-prefixed resource).
2. Write 3 NEW resources into the freed space: language dropdown list (with
   tr/ru/zh/ja added), settings schema (with new codes), English language map.
3. Redirect the 3 init-function LEAs to the new resource headers.
Result: dropdown + settings validation + auto-detect know 13 languages;
CSS is extended with modern titlebar + popup fixes.
"""
import struct, json, re, sys

SRC = "" + WORK + "/upstream/usr/bin/luxai"
DST = "" + WORK + "/stage/usr/bin/luxai"

data = bytearray(open(SRC, "rb").read())

# ---- resource locations (file offsets; .data: vaddr = file+0x1000) ----
CSS0_HDR   = 0x18D498          # [type=1][len=8736][json-encoded css]
CSS0_DATA  = 0x18D4A8
DROP_HDR   = 0x18D3D0          # dropdown list resource
SCHEMA_HDR = 0x178490          # settings schema resource
LANGMAP_HDR= 0x181F38          # English language-name map
LEA_DROP   = 0x14FA3F
LEA_SCHEMA = 0x14F429
LEA_LANGMAP= 0x14F604

def get_res(hdr):
    t, ln = struct.unpack("<QQ", bytes(data[hdr:hdr+16]))
    return t, ln, bytes(data[hdr+16:hdr+16+ln])

# ---- sanity: original resources ----
t, ln, css_enc = get_res(CSS0_HDR)
assert t == 1 and ln == 8736, (t, ln)
css = json.loads(css_enc.decode("utf-8"))
print(f"original CSS: {len(css)} decoded chars")

t, ln, drop_json = get_res(DROP_HDR); assert ln == 176
t, ln, schema_json = get_res(SCHEMA_HDR); assert ln == 290
t, ln, langmap_json = get_res(LANGMAP_HDR); assert ln == 132

# ---- 1. new language JSONs ----
langs = json.loads(drop_json.decode("utf-8"))
have = {c for c, _ in langs}
for code, name in (("tr","Türkçe"), ("ru","Русский"), ("zh","中文"), ("ja","日本語"), ("hi","हिन्दी"), ("az","Azərbaycan"), ("id","Bahasa Indonesia"), ("vi","Tiếng Việt")):
    assert code not in have
    langs.append([code, name])
new_drop = json.dumps(langs, ensure_ascii=False, separators=(",", ",")) \
    .replace('", "', '","').replace('", ', '","').replace('[["', '[["')
# canonical compact form
new_drop = json.dumps(langs, ensure_ascii=False).replace(", ", ",")
new_drop = re.sub(r'\], \[', '],[', new_drop)
print("new dropdown:", new_drop, len(new_drop.encode()), "bytes")

schema = json.loads(schema_json.decode("utf-8"))
order = ["auto","cs","sk","en","de","pl","fr","es","it","uk","tr","ru","zh","ja","hi","az","id","vi"]
schema["language"] = order
new_schema = json.dumps(schema, ensure_ascii=False, separators=(",", ":"))
print("new schema len:", len(new_schema.encode()))

lmap = json.loads(langmap_json.decode("utf-8"))
lmap.update({"tr":"Turkish","ru":"Russian","zh":"Chinese","ja":"Japanese","hi":"Hindi","az":"Azerbaijani","id":"Indonesian","vi":"Vietnamese"})
new_langmap = json.dumps(lmap, ensure_ascii=False, separators=(",", ":"))
print("new langmap len:", len(new_langmap.encode()))

# ---- 2. new CSS (minify + additions) ----
new_css = open(os.path.join(REPO, "src", "luxai", "patches", "css_new.css"), encoding="utf-8").read()

def minify(css):
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
    css = re.sub(r"\s*\n\s*", "", css)
    css = re.sub(r"\s*([{};:,])\s*", r"\1", css)
    css = css.replace(";}", "}")
    # safe extra shrink (GTK CSS): #aabbcc -> #abc, 0.5 -> .5, 0px -> 0,
    # rgba(255,255,255,a) -> alpha(#fff,a) (alpha(color,a) is valid GTK CSS)
    css = re.sub(r"#([0-9a-fA-F])\1([0-9a-fA-F])\2([0-9a-fA-F])\3\b", r"#\1\2\3", css)
    css = re.sub(r"([: ,\(])0\.([0-9])", r"\1.\2", css)
    css = re.sub(r"([: ,\(])0(px|em|rem|%)", r"\g<1>0", css)
    css = css.replace("rgba(255,255,255,", "alpha(#fff,")
    # ek kucultme (GTK CSS'de esdeger): background-color -> background, white -> #fff
    css = css.replace("background-color:", "background:")
    css = css.replace(":white", ":#fff").replace(": white", ":#fff")
    css = re.sub(r",\s*0\.", ",.", css)
    return css

min_css = minify(new_css)
print(f"minified CSS: {len(min_css)} chars")
enc = json.dumps(min_css, ensure_ascii=False).encode("utf-8")
print(f"encoded CSS: {len(enc)} bytes")

# ---- 3. layout inside freed region ----
def pack1(payload: bytes) -> bytes:
    return struct.pack("<QQ", 1, len(payload)) + payload

res_bytes = pack1(new_drop.encode()) + pack1(new_schema.encode()) + pack1(new_langmap.encode())
free_start = CSS0_DATA
free_total = 8736
need = len(enc) + len(res_bytes) + 16  # 16 = worst-case alignment padding
print(f"need {need} of {free_total}")
if need > free_total:
    sys.exit(f"CSS too big: need {need} > {free_total}")

# write shortened CSS resource (header len updated, data = enc)
data[CSS0_HDR+8:CSS0_HDR+16] = struct.pack("<Q", len(enc))
data[CSS0_DATA:CSS0_DATA+len(enc)] = enc
tail = CSS0_DATA + len(enc)

# place new resources 8-aligned in the tail
pos = (tail + 7) & ~7
hdrs = {}
for name, payload in (("dropdown", new_drop.encode()),
                      ("schema", new_schema.encode()),
                      ("langmap", new_langmap.encode())):
    pos = (pos + 7) & ~7
    hdrs[name] = pos
    blob = pack1(payload)
    data[pos:pos+len(blob)] = blob
    pos += len(blob)
end_used = pos
print(f"new resources at {[hex(h) for h in hdrs.values()]}, used until {end_used:#x} (limit {CSS0_DATA+8736:#x})")
assert end_used <= CSS0_DATA + 8736
# zero the remaining tail
for i in range(end_used, CSS0_DATA + 8736):
    data[i] = 0

# ---- 4. patch the three LEAs ----
def vaddr(foff):
    return foff + 0x1000

def patch_lea(code_off, target_file):
    orig = bytes(data[code_off:code_off+7])
    assert orig[:3] == b"\x48\x8d\x3d", orig.hex()
    new_disp = vaddr(target_file) - (code_off + 7)
    data[code_off+3:code_off+7] = struct.pack("<i", new_disp)
    print(f"lea {code_off:#x}: {orig.hex()} -> {bytes(data[code_off:code_off+7]).hex()} (target {vaddr(target_file):#x})")

patch_lea(LEA_DROP, hdrs["dropdown"])
patch_lea(LEA_SCHEMA, hdrs["schema"])
patch_lea(LEA_LANGMAP, hdrs["langmap"])

import os
os.makedirs(os.path.dirname(DST), exist_ok=True)
open(DST, "wb").write(bytes(data))
print("written:", DST, len(data), "bytes")

# ---- 5. verify ----
d2 = bytes(data)
t2, ln2, css2 = get_res(CSS0_HDR)
assert t2 == 1 and ln2 == len(enc) and css2 == enc
for name, hdr in hdrs.items():
    t2, ln2, payload = get_res(hdr)
    assert t2 == 1
    json.loads(payload.decode("utf-8"))   # must be valid JSON
    print(f"verify {name}: len={ln2} OK")
dd = json.loads(get_res(hdrs["dropdown"])[2])
assert [c for c, _ in dd][-8:] == ["tr", "ru", "zh", "ja", "hi", "az", "id", "vi"]
print("dropdown codes:", [c for c, _ in dd])
# css brace balance
assert min_css.count("{") == min_css.count("}"), "brace mismatch"
print("CSS braces balanced:", min_css.count("{"))
