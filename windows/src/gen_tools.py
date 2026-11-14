#!/usr/bin/env python3
"""tools.json -> build/tools_data.c (C dizesi). Harici bagimlilik yok."""
import json, sys

src, out = sys.argv[1], sys.argv[2]
data = json.load(open(src, encoding="utf-8"))
# kompakt gosterim (her istekte gonderilir, boyut onemli)
text = json.dumps(data, separators=(",", ":"), ensure_ascii=False)
esc = text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")

step = 110
chunks = []
i = 0
while i < len(esc):
    j = min(i + step, len(esc))
    # kacis dizisi ortadan bolunmesin: parca tek sayida ters bolu ile bitmesin
    k = j
    while k > i + 1 and (esc[k - 1] == "\\" and (k - 1 - i) - esc[i:k - 1].count("\\") * 2 >= 0):
        # basit ve guvenli: sonunda tek ters bolu varsa bir karakter geri
        backslashes = 0
        m = k - 1
        while m >= i and esc[m] == "\\":
            backslashes += 1
            m -= 1
        if backslashes % 2 == 0:
            break
        k -= 1
    chunks.append(esc[i:k])
    i = k
with open(out, "w", encoding="utf-8") as f:
    f.write("/* otomatik uretildi: src/tools.json -> tools_data.c (gen_tools.py) */\n")
    f.write("const char *TOOLS_JSON =\n")
    f.write("\n".join('    "%s"' % c for c in chunks))
    f.write(";\n")
print("tools_data.c: %d bayt JSON, %d parca" % (len(text), len(chunks)))
