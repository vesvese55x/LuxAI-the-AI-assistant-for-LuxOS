#!/usr/bin/env python3
import os, sys
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".work")
REPO = os.path.normpath(os.path.join(WORK, ".."))
"""Assemble the shared package tree (work/build) for all formats:
patched binary + vendored python + 16-language i18n + hicolor icons +
fully translated desktop entry + metainfo + polkit policy.
"""
import os, shutil, stat

W = "" + WORK + ""
UP = f"{W}/upstream/usr"
ACC = f"{W}/upstream/account/usr"
SFS = f"{W}/extracted/squashfs-root"
BLD = f"{W}/build"

def cp(src, dst, exe=False):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    if exe:
        st = os.stat(dst)
        os.chmod(dst, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

# 1. binary + root helpers
cp(f"{W}/stage/usr/bin/luxai", f"{BLD}/usr/bin/luxai", exe=True)
for f in ("luxai-root", "luxai-root-auto", "luxai-root-setup"):
    cp(f"{UP}/lib/luxai/{f}", f"{BLD}/usr/lib/luxai/{f}", exe=True)

# 2. data dir (icons used by the app at runtime)
if os.path.isdir(f"{UP}/share/luxai"):
    shutil.copytree(f"{UP}/share/luxai", f"{BLD}/usr/share/luxai", dirs_exist_ok=True)

# 3. vendored python (account CLI + modules)
cp(f"{ACC}/bin/luxos-account", f"{BLD}/usr/bin/luxos-account", exe=True)
for f in ("luxai_account.py", "luxai_account_gtk.py"):
    cp(f"{ACC}/lib/luxos/python/{f}", f"{BLD}/usr/lib/luxos/python/{f}")
cp(f"{W}/upstream/i18npkg/usr/lib/luxos/python/luxos_i18n.py",
   f"{BLD}/usr/lib/luxos/python/luxos_i18n.py")

# 4. hicolor icons - THE menu icon fix (all sizes)
for size in ("16x16", "24x24", "32x32", "48x48", "64x64", "128x128", "256x256", "512x512"):
    src = f"{UP}/share/icons/hicolor/{size}/apps/luxai.png"
    cp(src, f"{BLD}/usr/share/icons/hicolor/{size}/apps/luxai.png")

# 5. docs, license, metainfo, polkit
if os.path.isdir(f"{UP}/share/doc/luxai"):
    shutil.copytree(f"{UP}/share/doc/luxai", f"{BLD}/usr/share/doc/luxai", dirs_exist_ok=True)
cp(f"{UP}/share/licenses/luxai/LSL-LICENSE.txt", f"{BLD}/usr/share/licenses/luxai/LSL-LICENSE.txt")
cp(f"{UP}/share/metainfo/luxai.metainfo.xml", f"{BLD}/usr/share/metainfo/luxai.metainfo.xml")
cp(f"{UP}/share/polkit-1/actions/org.luxai.root.policy",
   f"{BLD}/usr/share/polkit-1/actions/org.luxai.root.policy")

# 6. translated desktop entry
GN = {
    "en": "AI Assistant", "cs": "AI asistent", "de": "KI-Assistent", "es": "Asistente de IA",
    "fr": "Assistant IA", "hu": "MI-asszisztens", "it": "Assistente IA", "ja": "AIアシスタント",
    "nl": "AI-assistent", "pl": "Asystent AI", "pt": "Assistente de IA", "ru": "ИИ-ассистент",
    "sk": "AI asistent", "sv": "AI-assistent", "tr": "Yapay Zeka Asistanı", "uk": "ШІ-асистент",
    "zh": "AI 助手",
}
CM = {
    "en": "AI assistant that can work with your computer",
    "cs": "AI asistent, který umí pracovat s vaším počítačem",
    "de": "KI-Assistent, der mit Ihrem Computer arbeiten kann",
    "es": "Asistente de IA que puede trabajar con su ordenador",
    "fr": "Assistant IA capable de travailler avec votre ordinateur",
    "hu": "Olyan MI-asszisztens, amely a számítógépével is dolgozhat",
    "it": "Assistente IA che può lavorare con il tuo computer",
    "ja": "コンピューターと一緒に作業できる AI アシスタント",
    "nl": "AI-assistent die met uw computer kan werken",
    "pl": "Asystent AI, który może pracować z twoim komputerem",
    "pt": "Assistente de IA que pode trabalhar com o seu computador",
    "ru": "ИИ-ассистент, который может работать с вашим компьютером",
    "sk": "AI asistent, ktorý vie pracovať s vaším počítačom",
    "sv": "AI-assistent som kan arbeta med din dator",
    "tr": "Bilgisayarınızla birlikte çalışabilen yapay zeka asistanı",
    "uk": "ШІ-асистент, який може працювати з вашим комп'ютером",
    "zh": "可以与您的电脑协同工作的 AI 助手",
}
KW = {
    "en": "AI;assistant;chat;agent;LuxAI;",
    "cs": "AI;asistent;chat;agent;LuxAI;umělá inteligence;",
    "de": "KI;Assistent;Chat;Agent;LuxAI;künstliche Intelligenz;",
    "es": "IA;asistente;chat;agente;LuxAI;inteligencia artificial;",
    "fr": "IA;assistant;chat;agent;LuxAI;intelligence artificielle;",
    "hu": "MI;asszisztens;csevegés;ügynök;LuxAI;mesterséges intelligencia;",
    "it": "IA;assistente;chat;agente;LuxAI;intelligenza artificiale;",
    "ja": "AI;アシスタント;チャット;エージェント;LuxAI;人工知能;",
    "nl": "AI;assistent;chat;agent;LuxAI;kunstmatige intelligentie;",
    "pl": "AI;asystent;czat;agent;LuxAI;sztuczna inteligencja;",
    "pt": "IA;assistente;conversa;agente;LuxAI;inteligência artificial;",
    "ru": "ИИ;ассистент;чат;агент;LuxAI;искусственный интеллект;",
    "sk": "AI;asistent;chat;agent;LuxAI;umelá inteligencia;",
    "sv": "AI;assistent;chatt;agent;LuxAI;artificiell intelligens;",
    "tr": "Yapay Zeka;Asistan;Sohbet;Ajan;LuxAI;",
    "uk": "ШІ;асистент;чат;агент;LuxAI;штучний інтелект;",
    "zh": "AI;人工智能;助手;聊天;智能体;",
}
order = ["cs", "de", "es", "fr", "hu", "it", "ja", "nl", "pl", "pt", "ru", "sk", "sv", "tr", "uk", "zh"]
lines = [
    "[Desktop Entry]",
    "Type=Application",
    "Name=LuxAI",
]
for l in order:
    lines.append(f"GenericName[{l}]={GN[l]}")
lines.append("GenericName=AI Assistant")
for l in order:
    lines.append(f"Comment[{l}]={CM[l]}")
lines.append(f"Comment={CM['en']}")
lines.append("Exec=luxai")
lines.append("Icon=luxai")
lines.append("Terminal=false")
lines.append("Categories=Utility;")
for l in order:
    lines.append(f"Keywords[{l}]={KW[l]}")
lines.append(f"Keywords={KW['en']}")
lines.append("StartupNotify=true")
lines.append("StartupWMClass=luxai")
desktop = "\n".join(lines) + "\n"
for dst in (f"{BLD}/usr/share/applications/luxai.desktop",
            f"{BLD}/usr/share/luxai/data/luxai.desktop"):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, "w", encoding="utf-8").write(desktop)

print("assembled build tree")
os.system(f"find {BLD} -type f | sort | head -60")
os.system(f"du -sh {BLD}")
