#!/usr/bin/env python3
import os, sys
WORK = os.environ.get("WORK") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".work")
REPO = os.path.normpath(os.path.join(WORK, ".."))
"""Assemble the shared package tree (work/build) for all formats:
patched binary + vendored python + 17-language i18n + hicolor icons +
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

# 1b. KENAR CUBUGU "OTURUMU KAPAT" DUZELTMESI
#     Upstream ikilide onay penceresinin govdesi bos (hicbir syscall yok).
#     Gercek ikili /usr/lib/luxai/luxai-bin'e tasinir; /usr/bin/luxai ince bir
#     baslatici olur ve luxfix.so'yu LD_PRELOAD ile yukler. luxfix.so onay
#     penceresini yakalar -> `luxos-account logout` + uygulamayi yeniden baslatir.
os.makedirs(f"{BLD}/usr/lib/luxai", exist_ok=True)
os.replace(f"{BLD}/usr/bin/luxai", f"{BLD}/usr/lib/luxai/luxai-bin")
cp(f"{REPO}/packaging/luxfix/luxai-wrapper", f"{BLD}/usr/bin/luxai", exe=True)
import subprocess, shutil as _sh
_gcc = _sh.which("gcc") or _sh.which("cc")
_luxfix_src = f"{REPO}/packaging/luxfix/luxfix.c"
if _gcc:
    _cmd = [_gcc, "-shared", "-fPIC", "-O2", "-o", f"{BLD}/usr/lib/luxai/luxfix.so", _luxfix_src,
            "-lgtk-3", "-lgdk-3", "-lglib-2.0", "-lgobject-2.0", "-ldl"]
    _r = subprocess.run(_cmd, capture_output=True, text=True)
    if _r.returncode != 0:   # pkg-config varsa onun bayraklariyla tekrar dene
        _pc = subprocess.run(["pkg-config", "--cflags", "--libs", "gtk+-3.0"],
                             capture_output=True, text=True)
        if _pc.returncode == 0:
            _r = subprocess.run([_gcc, "-shared", "-fPIC", "-O2", "-o",
                                 f"{BLD}/usr/lib/luxai/luxfix.so", _luxfix_src,
                                 "-ldl"] + _pc.stdout.split(), capture_output=True, text=True)
    print("luxfix.so:", "OK" if os.path.isfile(f"{BLD}/usr/lib/luxai/luxfix.so") else "DERLENEMEDI",
          _r.stderr[:300])
    if not os.path.isfile(f"{BLD}/usr/lib/luxai/luxfix.so"):
        print("  ! uyari: luxfix.so yok - kenar cubugu duzeltmesi devre disi")
for f in ("luxai-root", "luxai-root-auto", "luxai-root-setup"):
    cp(f"{UP}/lib/luxai/{f}", f"{BLD}/usr/lib/luxai/{f}", exe=True)

# 2. data dir (icons used by the app at runtime)
if os.path.isdir(f"{UP}/share/luxai"):
    shutil.copytree(f"{UP}/share/luxai", f"{BLD}/usr/share/luxai", dirs_exist_ok=True)

# 2b. HOS GELDIN EKRANINA ATIF NOTU (18 dil, idempotent)
#     "Bilgisayarinizla birlikte calisabilen yapay zeka asistani" alt satiri
#     LuxAI'nin bos sohbet ekraninda basligin hemen altinda gorunur.
import glob, json as _json
NOTE_FILE = os.path.join(REPO, "packaging", "attrib.json")
NOTE_KEY = "AI assistant that can work with your computer"
_i18n_dir = f"{BLD}/usr/share/luxai/i18n"
if os.path.isfile(NOTE_FILE) and os.path.isdir(_i18n_dir):
    notes = _json.load(open(NOTE_FILE, encoding="utf-8"))
    _done = 0
    for path in sorted(glob.glob(os.path.join(_i18n_dir, "*.tsv"))):
        lang = os.path.basename(path)[:-4]
        note = notes.get(lang)
        if not note:
            print(f"  ! atif notu yok: {lang}")
            continue
        rows = open(path, encoding="utf-8").read().splitlines()
        hit = False
        for i, line in enumerate(rows):
            if line.startswith(NOTE_KEY + "\t"):
                val = line.split("\t", 1)[1]
                base = val.split(" · BELGESELSEMO")[0].split(" · Z původní")[0]
                # onceki imzalari temizle (idempotent)
                for sep in (" · ",):
                    if "belgeselsemo.com.tr" in base:
                        base = base.split(" · ")[0]
                rows[i] = NOTE_KEY + "\t" + base.rstrip(" .") + " · " + note
                hit = True
                break
        if hit:
            open(path, "w", encoding="utf-8").write("\n".join(rows) + "\n")
            _done += 1
        else:
            print(f"  ! anahtar bulunamadi: {lang}")
    # Ingilizce kaynak metin disinda oldugu icin en.tsv yok: atif notu icin olustur
    en_path = os.path.join(_i18n_dir, "en.tsv")
    if not os.path.isfile(en_path):
        with open(en_path, "w", encoding="utf-8") as fh:
            fh.write("# LuxAI English strings (BELGESELSEMO note only)\n")
            fh.write("%s\t%s \u00b7 %s\n" % (NOTE_KEY,
                     "AI assistant that can work with your computer", notes.get("en", "")))
    print(f"atif notu eklenen dil dosyasi: {_done}/18")

# 3. vendored python (account CLI + modules)
cp(f"{ACC}/bin/luxos-account", f"{BLD}/usr/bin/luxos-account", exe=True)
for f in ("luxai_account.py", "luxai_account_gtk.py"):
    cp(f"{ACC}/lib/luxos/python/{f}", f"{BLD}/usr/lib/luxos/python/{f}")
cp(f"{W}/upstream/i18npkg/usr/lib/luxos/python/luxos_i18n.py",
   f"{BLD}/usr/lib/luxos/python/luxos_i18n.py")

# 3b. "PROJELERIMIZ" SEKMESI (Python/GTK hesap penceresi)
#     Linux ana penceresi upstream ikili oldugu icin menu ogeleri degistirilemez;
#     hesap penceresi (kenar cubugundaki kullanici kartindan acilir) tamamen
#     bizim Python kodumuz. Oraya 4. sekme olarak proje katalogu ekleniyor.
_pj = f"{BLD}/usr/lib/luxos/python/luxai_projects_gtk.py"
if os.path.isfile(os.path.join(REPO, "packaging", "projects_gtk.py")):
    cp(os.path.join(REPO, "packaging", "projects_gtk.py"), _pj)
    _acc = f"{BLD}/usr/lib/luxos/python/luxai_account_gtk.py"
    src = open(_acc, encoding="utf-8").read()
    if "luxai_projects_gtk" not in src:
        src = src.replace("from gi.repository import Gdk, GLib, Gtk",
                          "from gi.repository import Gdk, GLib, Gtk\nimport luxai_projects_gtk as _projects_mod", 1)
        src = src.replace('("register", _("New Account"))):',
                          '("register", _("New Account")),\n                            ("projects", _("Our projects"))):', 1)
        src = src.replace('self.stack.add_named(self.code_reg, "register-verify")',
                          'self.stack.add_named(self.code_reg, "register-verify")\n'
                          '        self.stack.add_named(_projects_mod.projects_box(self.open_url), "projects")', 1)
        src = src.replace('"register": self.reg_email}[self.mode] \\',
                          '"register": self.reg_email}.get(self.mode, self.pw_email) \\', 1)
        src = src.replace('"register": "register-" + self.stage["register"]}[self.mode]',
                          '"register": "register-" + self.stage["register"],\n'
                          '                "projects": "projects"}[self.mode]', 1)
        open(_acc, "w", encoding="utf-8").write(src)
        print("  projeler sekmasi:", "luxai_projects_gtk" in src)

# 3c. hesap penceresi icin 18 dilli /usr/share/luxos/i18n dosyalari
#     (upstream yalnizca 5 dil getiriyor; kalan diller ana arayuz TSV'sinden turetilir)
import glob as _glob
_osdir = f"{BLD}/usr/share/luxos/i18n"
_appdir = f"{BLD}/usr/share/luxai/i18n"
os.makedirs(_osdir, exist_ok=True)
_shim = {
    "Our projects": {
        "tr": "Projelerimiz", "en": "Our projects", "ru": "Наши проекты", "zh": "我们的项目",
        "ja": "私たちのプロジェクト", "hi": "हमारे प्रोजेक्ट", "de": "Unsere Projekte",
        "fr": "Nos projets", "es": "Nuestros proyectos", "it": "I nostri progetti",
        "nl": "Onze projecten", "pl": "Nasze projekty", "pt": "Os nossos projetos",
        "sk": "Naše projekty", "sv": "Våra projekt", "cs": "Naše projekty",
        "hu": "Projektjeink", "uk": "Наші проєкти",
    },
    "Open": {
        "tr": "Aç", "en": "Open", "ru": "Открыть", "zh": "打开", "ja": "開く", "hi": "खोलें",
        "de": "Öffnen", "fr": "Ouvrir", "es": "Abrir", "it": "Apri", "nl": "Openen",
        "pl": "Otwórz", "pt": "Abrir", "sk": "Otvoriť", "sv": "Öppna", "cs": "Otevřít",
        "hu": "Megnyitás", "uk": "Відкрити",
    },
    "Web portals, Android apps, desktop tools and a YouTube channel": {
        "tr": "Web portalları, Android uygulamaları, masaüstü araçları ve YouTube kanalı",
        "en": "Web portals, Android apps, desktop tools and a YouTube channel",
        "ru": "Веб-порталы, приложения Android, настольные инструменты и канал YouTube",
        "zh": "网页门户、安卓应用、桌面工具和 YouTube 频道",
        "ja": "Web ポータル、Android アプリ、デスクトップツール、YouTube チャンネル",
        "hi": "वेब पोर्टल, Android ऐप्स, डेस्कटॉप टूल्स और YouTube चैनल",
        "de": "Webportale, Android-Apps, Desktop-Werkzeuge und ein YouTube-Kanal",
        "fr": "Portails web, applications Android, outils de bureau et une chaîne YouTube",
        "es": "Portales web, aplicaciones Android, herramientas de escritorio y un canal de YouTube",
        "it": "Portali web, app Android, strumenti desktop e un canale YouTube",
        "nl": "Webportalen, Android-apps, desktoptools en een YouTube-kanaal",
        "pl": "Portale internetowe, aplikacje Android, narzędzia desktopowe i kanał YouTube",
        "pt": "Portais web, aplicações Android, ferramentas de ambiente de trabalho e um canal do YouTube",
        "sk": "Webové portály, aplikácie pre Android, nástroje pre počítač a kanál YouTube",
        "sv": "Webbportaler, Android-appar, skrivbordsverktyg och en YouTube-kanal",
        "cs": "Webové portály, aplikace pro Android, nástroje pro počítač a kanál YouTube",
        "hu": "Webportálok, Android-alkalmazások, asztali eszközök és egy YouTube-csatorna",
        "uk": "Вебпортали, застосунки Android, настільні інструменти та канал YouTube",
    },
}
_os_made = 0
for path in sorted(_glob.glob(os.path.join(_appdir, "*.tsv"))):
    lang = os.path.basename(path)[:-4]
    dst = os.path.join(_osdir, f"{lang}.tsv")
    rows = {}
    if os.path.isfile(dst):
        for line in open(dst, encoding="utf-8"):
            if "\t" in line and not line.startswith("#"):
                k, v = line.rstrip("\n").split("\t", 1)
                rows[k] = v
    for line in open(path, encoding="utf-8"):
        if "\t" in line and not line.startswith("#"):
            k, v = line.rstrip("\n").split("\t", 1)
            rows.setdefault(k, v)
    for k in _shim:
        if lang in _shim[k]:
            rows[k] = _shim[k][lang]
    with open(dst, "w", encoding="utf-8") as fh:
        fh.write("# LuxAI hesap penceresi cevirileri (%s) - BELGESELSEMO\n" % lang)
        for k, v in rows.items():
            fh.write(f"{k}\t{v}\n")
    _os_made += 1
print(f"luxos/i18n dosyasi: {_os_made} dil")

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
    "hi": "AI असिस्टेंट",
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
    "hi": "आपके कंप्यूटर के साथ काम कर सकने वाला AI असिस्टेंट",
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
    "hi": "AI;असिस्टेंट;चैट;एजेंट;LuxAI;कृत्रिम बुद्धिमत्ता;",
}
order = ["cs", "de", "es", "fr", "hi", "hu", "it", "ja", "nl", "pl", "pt", "ru", "sk", "sv", "tr", "uk", "zh"]
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
