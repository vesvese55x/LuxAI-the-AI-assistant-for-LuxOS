#!/usr/bin/env python3
"""LuxAI Windows - i18n tablosu ureteci.

Kaynaklar:
  * src/luxai/i18n/{tr,ru,zh,ja,hi}_{a,b,c}.tsv      (repoda, LuxAI'ya ozgu metinler)
  * src/vendor/luxai-*.pkg.tar.zst  -> usr/share/luxai/i18n/*.tsv   (uygulama metinleri)
  * src/vendor/luxos-i18n-*.pkg.tar.zst -> usr/share/luxos/i18n/*.tsv (sistem metinleri)
  * windows/src/i18n_manual.py       (elle yazilan ceviriler, en yuksek oncelik)

Cikti: windows/src/i18n_data.c  (LUX_STR[S_COUNT][LUX_NLANG])
Kullanim: python3 windows/src/gen_i18n.py
"""
import os, re, sys, gzip, io, subprocess, tarfile, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
from i18n_manual import MANUAL
try:
    from i18n_extra import EXTRA
except Exception:
    EXTRA = {}
for _l, _d in EXTRA.items():
    MANUAL.setdefault(_l, {}).update(_d)

LANGS = ["tr", "en", "ru", "zh", "ja", "hi", "de", "fr", "es", "it",
         "nl", "pl", "pt", "sk", "sv", "cs", "hu", "uk"]
NATIVE = {"tr": "Türkçe", "en": "English", "ru": "Русский", "zh": "中文", "ja": "日本語",
          "hi": "हिन्दी", "de": "Deutsch", "fr": "Français", "es": "Español",
          "it": "Italiano", "nl": "Nederlands", "pl": "Polski", "pt": "Português",
          "sk": "Slovenčina", "sv": "Svenska", "cs": "Čeština", "hu": "Magyar",
          "uk": "Українська"}


def parse_ids():
    src = open(os.path.join(HERE, "i18n_ids.h"), encoding="utf-8").read()
    body = src[src.index("#define STRS(X)"):src.index("enum {")]
    out = []
    for m in re.finditer(r"X\(\s*(\w+),\s*((?:[^,()]|\"(?:[^\"\\]|\\.)*\")+?),\s*"
                         r"((?:[^,()]|\"(?:[^\"\\]|\\.)*\")+?)\)\s*\\", body, re.S):
        def join(g):
            return "".join(re.findall(r"\"((?:[^\"\\]|\\.)*)\"", g))
        out.append((m.group(1), join(m.group(2)), join(m.group(3))))
    return out


def vendor_tsv():
    """vendor paketlerinden tsv dosyalarini okur: lang -> {en: ceviri}"""
    res = {}
    vdir = os.path.join(REPO, "src", "vendor")
    for pat in ("luxai-2", "luxos-i18n"):
        hits = [f for f in os.listdir(vdir) if f.startswith(pat)]
        if not hits:
            continue
        path = os.path.join(vdir, hits[0])
        names = subprocess.run(["tar", "--zstd", "-tf", path],
                               capture_output=True, text=True, check=True).stdout.split()
        for name in names:
            if not name.endswith(".tsv"):
                continue
            lang = os.path.basename(name)[:-4]
            raw = subprocess.run(["tar", "--zstd", "-xOf", path, name],
                                 capture_output=True, check=True).stdout
            d = res.setdefault(lang, {})
            for line in raw.decode("utf-8", "replace").splitlines():
                if not line or line.startswith("#") or "\t" not in line:
                    continue
                k, v = line.split("\t", 1)
                if v.strip():
                    d.setdefault(k.strip(), v.strip())
    return res


def repo_tsv():
    """src/luxai/i18n/{lang}_{a,b,c}.tsv -> lang -> {en: xx}"""
    res = {}
    d = os.path.join(REPO, "src", "luxai", "i18n")
    if not os.path.isdir(d):
        return res
    for fn in sorted(os.listdir(d)):
        m = re.match(r"([a-z]{2})_[abc]\.tsv$", fn)
        if not m:
            continue
        lang = m.group(1)
        res.setdefault(lang, {})
        for line in open(os.path.join(d, fn), encoding="utf-8", errors="replace"):
            line = line.rstrip("\n")
            if not line or line.startswith("#") or "\t" not in line:
                continue
            k, v = line.split("\t", 1)
            if v.strip():
                res[lang].setdefault(k.strip(), v.strip())
    return res


def norm(s):
    return (s.replace("…", "...").replace("’", "'").replace("–", "-")
             .strip().rstrip(".!?:").lower())


def build_index(d):
    idx = {}
    for k, v in d.items():
        idx.setdefault(norm(k), v)
        idx.setdefault(norm(re.sub(r"\s*\(.*?\)\s*$", "", k)), v)
    return idx


def cstr(s):
    out = []
    for ch in s:
        if ch in '"\\':
            out.append("\\" + ch)
        elif ch == "\n":
            out.append("\\n")
        elif ord(ch) < 32:
            out.append(" ")
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def main():
    ids = parse_ids()
    tab = {}
    for src in (vendor_tsv(), repo_tsv()):
        for lang, d in src.items():
            tab.setdefault(lang, {}).update(d)
    idx = {l: build_index(tab.get(l, {})) for l in LANGS}
    man = {l: build_index({k: v for k, v in MANUAL.get(l, {}).items()}) for l in LANGS}

    lines = ['/* OTOMATIK URETILDI - windows/src/gen_i18n.py; elle duzenlemeyin. */',
             '#include "i18n_data.h"', ""]
    lines.append("const char *const LUX_LANG_CODE[LUX_NLANG] = {%s};"
                 % ", ".join(cstr(l) for l in LANGS))
    lines.append("const char *const LUX_LANG_NAME[LUX_NLANG] = {%s};"
                 % ", ".join(cstr(NATIVE[l]) for l in LANGS))
    lines.append("")
    lines.append("const char *const LUX_STR[S_COUNT][LUX_NLANG] = {")
    stats = {"tsv": 0, "manual": 0, "fallback": 0}
    fallback = []
    for eid, tr, en in ids:
        row = []
        for l in LANGS:
            if l == "tr":
                row.append(tr); continue
            if l == "en":
                row.append(en); continue
            v = (man[l].get(norm(en)) or man[l].get(norm(re.sub(r"\s*\(.*?\)\s*$", "", en)))
                 or idx[l].get(norm(en)) or idx[l].get(norm(re.sub(r"\s*\(.*?\)\s*$", "", en))))
            if v:
                stats["manual" if v in MANUAL.get(l, {}).values() else "tsv"] += 1
                row.append(v)
            else:
                stats["fallback"] += 1
                fallback.append((l, eid, en))
                row.append(en)
        lines.append("  /* %-16s */ {%s}," % (eid, ", ".join(cstr(x) for x in row)))
    lines.append("};")
    lines.append("")
    lines.append("const int LUX_STR_COUNT = S_COUNT;")
    out = os.path.join(HERE, "i18n_data.c")
    open(out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("yazildi:", out)
    print("toplam anahtar:", len(ids), "dil:", len(LANGS))
    if fallback:
        print("INGILIZCE'ye dusen (ceviri yok):", len(fallback))
        seen = {}
        for l, eid, en in fallback:
            seen.setdefault(eid, []).append(l)
        for eid, ls in list(seen.items())[:40]:
            print("   %-16s %d dil" % (eid, len(ls)))


if __name__ == "__main__":
    main()
