#!/usr/bin/env python3
import os, sys
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".work")
REPO = os.path.normpath(os.path.join(WORK, ".."))
"""Finalize the i18n file set for packaging:
1. Verify account tsvs (39 keys, %d/%s placeholders).
2. Append the BELGESELSEMO signature to the empty-state line of the 12
   upstream language files (localized per language).
3. Produce build/usr/share/luxai/i18n/*.tsv (13 languages + signature) and
   build/usr/share/luxos/i18n/{tr,ru,zh,jа}.tsv (account strings).
"""
import os, shutil, sys

UP = "" + WORK + "/upstream/usr/share/luxai/i18n"
BLD = "" + WORK + "/build/usr/share/luxai/i18n"
BLD_OS = "" + WORK + "/build/usr/share/luxos/i18n"
W = "" + WORK + ""
KEY = "AI assistant that can work with your computer"

SIG = {
    "cs": "Z původní distribuce LuxOS získáno a pomocí Z.AI zkompilováno společností BELGESELSEMO jako balíčky pro Linux/Windows · belgeselsemo.com.tr",
    "de": "Aus der originalen LuxOS-Distribution gewonnen und mit Hilfe von Z.AI von BELGESELSEMO als Linux-/Windows-Pakete kompiliert · belgeselsemo.com.tr",
    "es": "Obtenido de la distribución original de LuxOS y compilado por BELGESELSEMO, con ayuda de Z.AI, como paquetes para Linux/Windows · belgeselsemo.com.tr",
    "fr": "Obtenu à partir de la distribution LuxOS d'origine et compilé par BELGESELSEMO, grâce à Z.AI, en paquets Linux/Windows · belgeselsemo.com.tr",
    "hu": "Az eredeti LuxOS disztribúcióból készült, és a BELGESELSEMO Z.AI segítségével Linux/Windows csomagokként fordította · belgeselsemo.com.tr",
    "it": "Ottenuto dalla distribuzione LuxOS originale e compilato da BELGESELSEMO, con l'aiuto di Z.AI, come pacchetti Linux/Windows · belgeselsemo.com.tr",
    "nl": "Verkregen uit de originele LuxOS-distributie en door BELGESELSEMO, met hulp van Z.AI, gecompileerd als Linux/Windows-pakketten · belgeselsemo.com.tr",
    "pl": "Pozyskano z oryginalnej dystrybucji LuxOS i skompilowano przez BELGESELSEMO, przy pomocy Z.AI, jako pakiety Linux/Windows · belgeselsemo.com.tr",
    "pt": "Obtido da distribuição LuxOS original e compilado pela BELGESELSEMO, com ajuda da Z.AI, como pacotes Linux/Windows · belgeselsemo.com.tr",
    "sk": "Získané z pôvodnej distribúcie LuxOS a zostavené spoločnosťou BELGESELSEMO pomocou Z.AI ako balíky pre Linux/Windows · belgeselsemo.com.tr",
    "sv": "Hämtad från den ursprungliga LuxOS-distributionen och kompilerad av BELGESELSEMO, med hjälp av Z.AI, som Linux/Windows-paket · belgeselsemo.com.tr",
    "uk": "Отримано з оригінальної дистрибуції LuxOS і скомпільовано BELGESELSEMO за допомогою Z.AI як пакети Linux/Windows · belgeselsemo.com.tr",
}

os.makedirs(BLD, exist_ok=True)
os.makedirs(BLD_OS, exist_ok=True)

# 1. new full translations
for lang in ("tr", "ru", "zh", "ja"):
    shutil.copy(f"{W}/luxai_{lang}.tsv", f"{BLD}/{lang}.tsv")
    shutil.copy(os.path.join(REPO, "src", "luxai", "i18n", f"account_{lang}.tsv"), f"{BLD_OS}/{lang}.tsv")

# 2. upstream languages with signature appended
problems = 0
for lang in ("cs", "de", "es", "fr", "hu", "it", "nl", "pl", "pt", "sk", "sv", "uk"):
    src = f"{UP}/{lang}.tsv"
    lines = open(src, encoding="utf-8").read().splitlines()
    hit = False
    out = []
    for line in lines:
        if line.startswith(KEY + "\t"):
            val = line.split("\t", 1)[1]
            if "belgeselsemo" not in val:  # idempotent
                line = KEY + "\t" + val + " · " + SIG[lang]
            hit = True
        out.append(line)
    if not hit:
        print(f"WARNING: key not found in {lang}.tsv")
        problems += 1
    open(f"{BLD}/{lang}.tsv", "w", encoding="utf-8").write("\n".join(out) + "\n")

# 3. verify account tsvs
import re
for lang in ("tr", "ru", "zh", "ja"):
    p = f"{BLD_OS}/{lang}.tsv"
    n = 0
    bad = 0
    for line in open(p, encoding="utf-8"):
        if line.startswith("#") or "\t" not in line:
            continue
        k, v = line.rstrip("\n").split("\t", 1)
        n += 1
        if sorted(re.findall(r"%[ds]", k)) != sorted(re.findall(r"%[ds]", v)):
            bad += 1
            print(f"placeholder mismatch in {p}: {k!r}")
    print(f"account_{lang}: {n} keys, {bad} placeholder mismatches")

# 4. summary of app i18n dir
files = sorted(os.listdir(BLD))
print("\nbuild i18n files:", files)
for f in files:
    n = sum(1 for line in open(f"{BLD}/{f}", encoding="utf-8")
            if line.strip() and not line.startswith("#") and "\t" in line)
    sig = any("belgeselsemo.com.tr" in line for line in open(f"{BLD}/{f}", encoding="utf-8"))
    print(f"  {f}: {n} keys, signature={'yes' if sig else 'NO'}")
sys.exit(1 if problems else 0)
