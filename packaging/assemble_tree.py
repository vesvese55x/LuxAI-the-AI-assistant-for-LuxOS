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

# 3b2. "SAGLAYICILAR" SEKMESI (coklu OpenAI uyumlu saglayici, Linux tarafi)
#      Windows'taki model secici + "Saglayici ekle/duzenle" ile birebir ayni
#      islevi hesap penceresine 5. sekme olarak ekler; ayarlar Windows ile ayni
#      bicimde (~/.config/luxai/providers.ini <-> %APPDATA%\providers.ini).
#      Kopru (providers_bridge) sohbet isteklerini secili saglayiciya yonlendirir.
_pcmod = os.path.join(REPO, "packaging", "providers_common.py")
_pgmod = os.path.join(REPO, "packaging", "providers_gtk.py")
if os.path.isfile(_pcmod) and os.path.isfile(_pgmod):
    cp(_pcmod, f"{BLD}/usr/lib/luxos/python/providers_common.py")
    cp(_pgmod, f"{BLD}/usr/lib/luxos/python/luxai_providers_gtk.py")
    cp(_pcmod, f"{BLD}/usr/lib/luxai/providers_bridge.py")
    _acc = f"{BLD}/usr/lib/luxos/python/luxai_account_gtk.py"
    src = open(_acc, encoding="utf-8").read()
    if "luxai_providers_gtk" not in src:
        src = src.replace("import luxai_projects_gtk as _projects_mod",
                          "import luxai_projects_gtk as _projects_mod\n"
                          "import luxai_providers_gtk as _providers_mod", 1)
        src = src.replace('("projects", _("Our projects"))):',
                          '("projects", _("Our projects")),\n'
                          '                            ("providers", _("Providers"))):', 1)
        src = src.replace('self.stack.add_named(_projects_mod.projects_box(self.open_url), "projects")',
                          'self.stack.add_named(_projects_mod.projects_box(self.open_url), "projects")\n'
                          '        self.stack.add_named(_providers_mod.providers_box(self.open_url), "providers")\n'
                          '        self.stack.show_all()   # GtkStack gorunur sayfayi ancak cocuklar gorunurken degistirir', 1)
        # ekran goruntusu/otomasyon icin baslangic sekmesi: LUXAI_ACCOUNT_TAB
        src = src.replace("        self.mode = mode\n",
                          "        mode = (__import__(\"os\").environ.get(\"LUXAI_ACCOUNT_TAB\") or mode)\n"
                          "        self.mode = mode\n", 1)
        src = src.replace('"projects": "projects"}[self.mode]',
                          '"projects": "projects",\n'
                          '                "providers": "providers"}[self.mode]', 1)
        open(_acc, "w", encoding="utf-8").write(src)
        print("  saglayici sekmasi:", "luxai_providers_gtk" in src)
    else:
        print("  saglayici sekmasi: zaten var")

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
    "Providers": {
        "tr": "Sağlayıcılar", "en": "Providers", "ru": "Провайдеры", "zh": "服务提供商",
        "ja": "プロバイダー", "hi": "प्रदाता", "de": "Anbieter", "fr": "Fournisseurs",
        "es": "Proveedores", "it": "Fornitori", "nl": "Providers", "pl": "Dostawcy",
        "pt": "Fornecedores", "sk": "Poskytovatelia", "sv": "Leverantörer",
        "cs": "Poskytovatelé", "hu": "Szolgáltatók", "uk": "Постачальники",
    },
    "built-in": {
        "tr": "yerleşik", "en": "built-in", "ru": "встроенный", "zh": "内置",
        "ja": "組み込み", "hi": "अंतर्निहित", "de": "integriert", "fr": "intégré",
        "es": "integrado", "it": "integrato", "nl": "ingebouwd", "pl": "wbudowany",
        "pt": "integrado", "sk": "vstavaný", "sv": "inbyggd", "cs": "vestavěný",
        "hu": "beépített", "uk": "вбудований",
    },
    "Active provider": {
        "tr": "Etkin sağlayıcı", "en": "Active provider", "ru": "Активный провайдер",
        "zh": "当前服务提供商", "ja": "使用中のプロバイダー", "hi": "सक्रिय प्रदाता",
        "de": "Aktiver Anbieter", "fr": "Fournisseur actif", "es": "Proveedor activo",
        "it": "Fornitore attivo", "nl": "Actieve provider", "pl": "Aktywny dostawca",
        "pt": "Fornecedor ativo", "sk": "Aktívny poskytovateľ", "sv": "Aktiv leverantör",
        "cs": "Aktivní poskytovatel", "hu": "Aktív szolgáltató", "uk": "Активний постачальник",
    },
    "Add provider": {
        "tr": "Sağlayıcı ekle", "en": "Add provider", "ru": "Добавить провайдера",
        "zh": "添加服务提供商", "ja": "プロバイダーを追加", "hi": "प्रदाता जोड़ें",
        "de": "Anbieter hinzufügen", "fr": "Ajouter un fournisseur",
        "es": "Añadir proveedor", "it": "Aggiungi fornitore", "nl": "Provider toevoegen",
        "pl": "Dodaj dostawcę", "pt": "Adicionar fornecedor",
        "sk": "Pridať poskytovateľa", "sv": "Lägg till leverantör",
        "cs": "Přidat poskytovatele", "hu": "Szolgáltató hozzáadása",
        "uk": "Додати постачальника",
    },
    "Edit provider": {
        "tr": "Sağlayıcıyı düzenle", "en": "Edit provider", "ru": "Изменить провайдера",
        "zh": "编辑服务提供商", "ja": "プロバイダーを編集", "hi": "प्रदाता संपादित करें",
        "de": "Anbieter bearbeiten", "fr": "Modifier le fournisseur",
        "es": "Editar proveedor", "it": "Modifica fornitore", "nl": "Provider bewerken",
        "pl": "Edytuj dostawcę", "pt": "Editar fornecedor", "sk": "Upraviť poskytovateľa",
        "sv": "Redigera leverantör", "cs": "Upravit poskytovatele",
        "hu": "Szolgáltató szerkesztése", "uk": "Змінити постачальника",
    },
    "Endpoint (base URL)": {
        "tr": "Uç nokta (temel adres)", "en": "Endpoint (base URL)", "ru": "Конечная точка (base URL)",
        "zh": "接口地址（base URL）", "ja": "エンドポイント（base URL）",
        "hi": "एंडपॉइंट (बेस URL)", "de": "Endpunkt (Basis-URL)",
        "fr": "Point de terminaison (URL de base)", "es": "Punto de conexión (URL base)",
        "it": "Endpoint (URL di base)", "nl": "Eindpunt (basis-URL)",
        "pl": "Punkt końcowy (base URL)", "pt": "Ponto final (URL base)",
        "sk": "Koncový bod (base URL)", "sv": "Slutpunkt (bas-URL)",
        "cs": "Koncový bod (base URL)", "hu": "Végpont (base URL)",
        "uk": "Кінцева точка (base URL)",
    },
    "Everything before /chat/completions": {
        "tr": "/chat/completions öncesi kısım", "en": "Everything before /chat/completions",
        "ru": "Всё до /chat/completions", "zh": "/chat/completions 之前的部分",
        "ja": "/chat/completions より前の部分", "hi": "/chat/completions से पहले का भाग",
        "de": "Alles vor /chat/completions", "fr": "Tout ce qui précède /chat/completions",
        "es": "Todo antes de /chat/completions", "it": "Tutto prima di /chat/completions",
        "nl": "Alles vóór /chat/completions", "pl": "Wszystko przed /chat/completions",
        "pt": "Tudo antes de /chat/completions", "sk": "Všetko pred /chat/completions",
        "sv": "Allt före /chat/completions", "cs": "Vše před /chat/completions",
        "hu": "Minden a /chat/completions előtt", "uk": "Усе до /chat/completions",
    },
    "Model": {
        "tr": "Model", "en": "Model", "ru": "Модель", "zh": "模型", "ja": "モデル",
        "hi": "मॉडल", "de": "Modell", "fr": "Modèle", "es": "Modelo", "it": "Modello",
        "nl": "Model", "pl": "Model", "pt": "Modelo", "sk": "Model", "sv": "Modell",
        "cs": "Model", "hu": "Modell", "uk": "Модель",
    },
    "Name": {
        "tr": "Ad", "en": "Name", "ru": "Название", "zh": "名称", "ja": "名前", "hi": "नाम",
        "de": "Name", "fr": "Nom", "es": "Nombre", "it": "Nome", "nl": "Naam",
        "pl": "Nazwa", "pt": "Nome", "sk": "Názov", "sv": "Namn", "cs": "Název",
        "hu": "Név", "uk": "Назва",
    },
    "Save": {
        "tr": "Kaydet", "en": "Save", "ru": "Сохранить", "zh": "保存", "ja": "保存",
        "hi": "सहेजें", "de": "Speichern", "fr": "Enregistrer", "es": "Guardar",
        "it": "Salva", "nl": "Opslaan", "pl": "Zapisz", "pt": "Guardar", "sk": "Uložiť",
        "sv": "Spara", "cs": "Uložit", "hu": "Mentés", "uk": "Зберегти",
    },
    "Cancel": {
        "tr": "Vazgeç", "en": "Cancel", "ru": "Отмена", "zh": "取消", "ja": "キャンセル",
        "hi": "रद्द करें", "de": "Abbrechen", "fr": "Annuler", "es": "Cancelar",
        "it": "Annulla", "nl": "Annuleren", "pl": "Anuluj", "pt": "Cancelar",
        "sk": "Zrušiť", "sv": "Avbryt", "cs": "Zrušit", "hu": "Mégse", "uk": "Скасувати",
    },
    "Delete": {
        "tr": "Sil", "en": "Delete", "ru": "Удалить", "zh": "删除", "ja": "削除",
        "hi": "हटाएँ", "de": "Löschen", "fr": "Supprimer", "es": "Eliminar",
        "it": "Elimina", "nl": "Verwijderen", "pl": "Usuń", "pt": "Eliminar",
        "sk": "Odstrániť", "sv": "Ta bort", "cs": "Smazat", "hu": "Törlés", "uk": "Видалити",
    },
    "Edit": {
        "tr": "Düzenle", "en": "Edit", "ru": "Изменить", "zh": "编辑", "ja": "編集",
        "hi": "संपादित करें", "de": "Bearbeiten", "fr": "Modifier", "es": "Editar",
        "it": "Modifica", "nl": "Bewerken", "pl": "Edytuj", "pt": "Editar",
        "sk": "Upraviť", "sv": "Redigera", "cs": "Upravit", "hu": "Szerkesztés",
        "uk": "Змінити",
    },
    "API key": {
        "tr": "API anahtarı", "en": "API key", "ru": "API-ключ", "zh": "API 密钥",
        "ja": "API キー", "hi": "API कुंजी", "de": "API-Schlüssel", "fr": "Clé API",
        "es": "Clave API", "it": "Chiave API", "nl": "API-sleutel", "pl": "Klucz API",
        "pt": "Chave API", "sk": "API kľúč", "sv": "API-nyckel", "cs": "API klíč",
        "hu": "API-kulcs", "uk": "API-ключ",
    },
    "Show API key": {
        "tr": "API anahtarını göster", "en": "Show API key", "ru": "Показать API-ключ",
        "zh": "显示 API 密钥", "ja": "API キーを表示", "hi": "API कुंजी दिखाएँ",
        "de": "API-Schlüssel anzeigen", "fr": "Afficher la clé API",
        "es": "Mostrar clave API", "it": "Mostra chiave API", "nl": "API-sleutel tonen",
        "pl": "Pokaż klucz API", "pt": "Mostrar chave API", "sk": "Zobraziť API kľúč",
        "sv": "Visa API-nyckel", "cs": "Zobrazit API klíč", "hu": "API-kulcs megjelenítése",
        "uk": "Показати API-ключ",
    },
    "A label you choose": {
        "tr": "Kendi seçtiğiniz bir ad", "en": "A label you choose", "ru": "Любое имя на ваш выбор",
        "zh": "自定义名称", "ja": "任意の名前", "hi": "अपनी पसंद का नाम", "de": "Ein Name Ihrer Wahl",
        "fr": "Un nom de votre choix", "es": "Un nombre a su elección", "it": "Un nome a piacere",
        "nl": "Een naam naar keuze", "pl": "Dowolna nazwa", "pt": "Um nome à sua escolha",
        "sk": "Ľubovoľný názov", "sv": "Ett namn du väljer", "cs": "Libovolný název",
        "hu": "Ön által választott név", "uk": "Будь-яка назва на ваш вибір",
    },
    "Model name sent to the provider": {
        "tr": "Sağlayıcıya gönderilen model adı", "en": "Model name sent to the provider",
        "ru": "Имя модели, отправляемое провайдеру", "zh": "发送给服务提供商的模型名称",
        "ja": "プロバイダーに送信するモデル名", "hi": "प्रदाता को भेजा जाने वाला मॉडल नाम",
        "de": "Modellname, der an den Anbieter geht", "fr": "Nom du modèle envoyé au fournisseur",
        "es": "Nombre del modelo enviado al proveedor", "it": "Nome del modello inviato al fornitore",
        "nl": "Modelnaam die naar de provider gaat", "pl": "Nazwa modelu wysyłana do dostawcy",
        "pt": "Nome do modelo enviado ao fornecedor", "sk": "Názov modelu odosielaný poskytovateľovi",
        "sv": "Modellnamn som skickas till leverantören", "cs": "Název modelu odesílaný poskytovateli",
        "hu": "A szolgáltatónak küldött modellnév", "uk": "Назва моделі, що надсилається постачальнику",
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
