# LuxAI — `vesvese55x/LuxAI-the-AI-assistant-for-LuxOS` derin inceleme + native Windows portu

> ⚠️ **Önce en kritik bulgu:** Bu depoda **LuxAI'nin kaynak kodu yok.** `src/` altında yalnızca
> LuxOS 1.0.1 imajından alınmış **derlenmiş, strip edilmiş ELF ikili dosyaları** ve topluluk
> yamaları (çeviri `.tsv`'leri, CSS, Python hesap arayüzü) var. Yani "kaynaktan native Windows
> derlemesi" **teknik olarak mümkün değil** — ortada derlenecek kaynak yok. Bu yüzden aşağıdaki
> analizden sonra **protokolü birebir uygulayan, sıfırdan yazılmış native Win32 istemcisini**
> (`LuxAI.exe`, WSL'siz, Windows 7 SP1+) derledim. Ayrıntı: [bölüm 11–13](#11-neden-kaynaktan-native-windows-derlemesi-mümkün-değil).

---

## 1. Depo envanteri (didik didik)

```
luxai/
├── README.md                    29 KB, 7 dilde tanıtım + kurulum tabloları
├── LICENSE                      MIT (Copyright 2026 BELGESELSEMO / vesvese55x)
├── belgeselsemo-luxai-installer tek satır kurulum (curl … | sh) kabuğu
├── .github/workflows/build.yml  CI: linux-packages · windows-native · screenshots · release
├── docs/
│   ├── BUILDING.md              derleme hattı + "binary yaması nasıl çalışır"
│   └── INSTALL.md               paket/kurulum/kaldırma + yapılandırma yolları
├── packaging/                   üretim hattı (Python + bash)
│   ├── prepare.sh               vendor .zst paketlerini açar
│   ├── verify_tsv.py            457 anahtar + {0}/{1}/{2} yer tutucu doğrulaması
│   ├── patch_binary.py          ⚠️ binary yamalama (aşağıda)
│   ├── finalize_i18n.py         16–17 dil dosyası + imza + hesap tsv'leri
│   ├── assemble_tree.py         hicolor ikonlar, .desktop, python vendor, polkit
│   ├── build_packages.py        deb / rpm / Arch zst / AppImage + SHA256SUMS
│   ├── build-exe.sh             mingw-w64 ile Windows kurucu
│   (Eski WSL tabanlı setup kaldırıldı — artık yalnız native exe var)
│   ├── capture_gui.py           Xvfb + python-xlib ile gerçek ekran görüntüsü
│   ├── dump_css.py, find_xrefs.py  ikili analiz yardımcıları
│   └── RELEASE_NOTES.md         sürüm notları
├── src/
│   ├── vendor/                  🔒 DEĞİŞTİRİLMEMİŞ LuxOS paketleri
│   │   ├── luxai-2.0.2-3-x86_64.pkg.tar.zst          (uygulamanın kendisi)
│   │   ├── luxos-account-1.0.0-8-any.pkg.tar.zst     (hesap penceresi, Python+GTK)
│   │   ├── luxos-i18n-1.0.0-64-any.pkg.tar.zst       (16 dil, 358–491 KB tsv)
│   │   └── luxos-docs-1.0.0-43-any.pkg.tar.zst       (çevrimdışı LSL/LuxOS kütüphanesi)
│   └── luxai/
│       ├── i18n/{tr,ru,zh,ja,hi}_{a,b,c}.tsv         457 anahtar × 3 parça
│       ├── i18n/account_*.tsv                        hesap penceresi çevirileri
│       └── patches/css_new.css                       modernize edilmiş GTK CSS
└── promo/                       tanıtım HTML + gerçek runner ekran görüntüleri
```

Dikkat: `src/luxai/` içinde **.c/.lsl/.py kaynak yok** — yalnız çeviri verisi ve CSS var.

---

## 2. LuxAI aslında ne? (ikili dosya adli incelemesi)

`src/vendor/luxai-2.0.2-3-x86_64.pkg.tar.zst` açıldığında:

| Dosya | Boyut | Tür |
|---|---|---|
| `usr/bin/luxai` | 1 949 880 B | ELF 64-bit PIE, x86-64, **stripped** |
| `usr/lib/luxai/luxai-root` | 361 768 B | ayrıcalıklı yardımcı (root) |
| `usr/lib/luxai/luxai-root-auto` | 361 768 B | aynı yardımcının "otomatik" modu |
| `usr/lib/luxai/luxai-root-setup` | 187 152 B | polkit kurulum yardımcısı |
| `usr/share/applications/luxai.desktop` | 420 B | masaüstü girdisi |
| `usr/share/doc/luxai/{README,REMOTE-SESSION,LSL-MIGRATION,LSL-ISSUES}.md` | ~63 KB | uygulama dokümanları |

`ldd` çıktısı bağımlılıkları netleştiriyor: **libgtk-3, libgdk-3, libgdk_pixbuf, libgio,
libglib, libgobject, libcurl**, ayrıca libpng/jpeg/zstd/brotli (libcurl'ün TLS yığını).
`.PKGINFO`:

```
pkgver   = 2.0.2-3
depends  = gtk3 glib2 gdk-pixbuf2 curl bash coreutils util-linux ca-certificates
           xdotool wmctrl polkit luxos-account luxos-i18n luxos-docs
makedepend = lsl>=0.10.6          ← kritik satır
```

**`makedepend = lsl`** + ikilinin içindeki hata mesajları uygulamanın neyle yazıldığını
ele veriyor:

```
IndexError: index out of range at lsl/app_model.lsl:111
NameError: global variable 'TOOLS' read before assignment at lsl/tools.lsl:1140
NameError: global variable 'ALWAYS_CONFIRM_TOOLS' read before assignment at lsl/safety.lsl:349
```

Yani LuxAI, LuxOS'un kendi dili olan **LSL (LuxOS Script Language)** ile yazılmış ve
`lsl` derleyicisiyle **makine koduna** çevrilmiş bir uygulama. Kaynak dosya adları
(izlerinden okunabiliyor): `app_model.lsl`, `tools.lsl`, `safety.lsl`, `agent_core.lsl`,
`rootguard.lsl`, `remote_session.lsl`, `ui_cards.lsl`, `tool_format.lsl`, `main` …

Sonuç: kaynak kod **kapalı** (LSL derleyicisi de depoda yok), elimizde yalnız makine kodu var.

---

## 3. Binary'ye gömülü kaynaklar ve `patch_binary.py` hilesi

`docs/BUILDING.md`'nin anlattığı mekanizma şu: uygulama, CSS'i, dil listesini, ayar şemasını
ve dil adı haritasını **ikilinin içine** şu formatta gömüyor:

```
[u64 tip][u64 uzunluk][JSON veri]
```

ve başlatma fonksiyonu her kaynağı doğrudan `lea` (yükle-adres) talimatıyla okuyor.
`packaging/patch_binary.py` bu gerçeği kullanıyor:

| Adım | Ne yapıyor |
|---|---|
| 1 | Gömülü ana CSS'i küçültüyor (yorum/satır sonu temizliği) → **8 736 baytlık bütçenin altına** iniyor |
| 2 | Boşalan alana **3 yeni kaynak** yazıyor: 14 dilli dropdown listesi, genişletilmiş ayar şeması, İngilizce dil adı haritası |
| 3 | Init fonksiyonundaki **3 `lea`** talimatının `disp32` alanını yeni kaynak başlıklarına çeviriyor |

Sabit adresler script içinde gömülü: `CSS0_HDR=0x18D498`, `DROP_HDR=0x18D3D0`,
`SCHEMA_HDR=0x178490`, `LANGMAP_HDR=0x181F38`, `LEA_DROP=0x14FA3F`, `LEA_SCHEMA=0x14F429`,
`LEA_LANGMAP=0x14F604`; uzunluk doğrulamaları `assert ln == 8736 / 176 / 290 / 132`.
Hiçbir bayt kaydırılmıyor — bu yüzden yama "güvenli" ve sürüm sabit kalıyor
(yeni LuxAI sürümü çıkarsa bu adresler kayar ve yama kırılır).

---

## 4. Üretim hattı (CI)

`.github/workflows/build.yml` dört iş çalıştırıyor:

1. **linux-packages** — `packaging/build-all.sh`: prepare → tsv birleştir/doğrula → binary yamala →
   i18n finalize → paket ağacı → `.deb/.rpm/.pkg.tar.zst/.AppImage` + `SHA256SUMS`
2. **windows-native** — `windows/build-win.sh` + `windows/build-installer.sh` (native Win32, WSL'siz)
3. **screenshots** — derlenen `.deb`'i runner'a kurar, **Xvfb altında gerçekten GUI'yi açar**,
   `capture_gui.py` (python-xlib) ile tr/en/hi ekran görüntüsü alır ve repoya commit eder
4. **release** — `v*` etiketinde veya `workflow_dispatch`'ta 5 paketi + `SHA256SUMS`'ı release'e yazar

---

## 5. i18n mimarisi

* Dil listesi ikiliden **dıştan** yüklenen `i18n/*.tsv` dosyalarıyla çalışıyor; binary'deki
  sabit boyutlu liste yamayla **14 dile** çıkarılmış (tr, ru, zh, ja, hi eklendi).
* Toplam **457 arayüz anahtarı**, her dilde 3 parça dosya (`_a`, `_b`, `_c`).
* `verify_tsv.py`: eksik/fazla anahtar + `{0}/{1}/{2}` yer tutucu uyuşmazlığı denetimi.
* Sistem dili algılama sırası: `LC_ALL → LC_MESSAGES → LANG`; desteklenmeyen dilde İngilizce.
* Ayrıca hesap penceresi (Python/GTK) için `account_*.tsv` dosyaları.

---

## 6. Release'deki `LuxAI-2.0.2-Setup.exe` gerçekte ne yapıyor?

~~Eski WSL tabanlı kurucu~~ **kaldırıldı.** Yerine native `luxai-kur.bat` ve `Setup-native.exe` var. Akış:

```
1. MessageBox: "LuxAI (Windows) kurulacak … (Windows 11 / WSLg gerekir)"
2. wsl.exe --status                      → yoksa: wsl.exe --install -d Debian --no-launch
3. gömülü luxai_2.0.2_amd64.deb'i %LOCALAPPDATA%\LuxAI\luxai.deb'e çıkarır
4. wsl.exe -d Debian -- bash -lc "sudo apt-get update && sudo apt-get install -y /mnt/c/…/luxai.deb"
5. Başlat menüsüne kısayol: C:\Windows\System32\wsl.exe  -d Debian -- luxai
6. wsl.exe -d Debian -- luxai           (WSLg penceresi açılır)
```

Yani **exe bir Windows programı değil, bir WSL kurulum aracı**. İçinde LuxAI yok; Debian'ın
içine `.deb` kurup `wsl.exe` üzerinden çalıştırıyor. Sonuçları:

* Windows 11 (WSLg) şart — WSLg Windows 10'da yok, Windows 7'de WSL hiç yok.
* Her açılışta WSL VM'i + Debian + X sunucusu ayağa kalkıyor (yavaş, RAM/disk maliyeti yüksek).
* Ajan araçları Linux tarafında çalışıyor: Windows dosyalarına `\\wsl$` üzerinden gidiyor,
  `cmd.exe`/PowerShell yok, Windows uygulamalarını açmak için `explorer.exe` hilesi gerekiyor.

**Senin istediğin şey (WSL'siz, doğrudan native exe) bu mimaride mümkün değil.**

---

## 7. API sözleşmesi (ikiliden + Python kaynağından çıkarıldı)

| Öğe | Değer |
|---|---|
| Sohbet | `POST https://lux-ai.cz/api/ide/chat/completions` — **SSE** (`text/event-stream`), OpenAI uyumlu `choices[].delta` akışı |
| Kimlik | `Cookie: lux_account_session=<token>` + `X-Lux-Client: luxai` |
| Giriş | `POST https://lux-ai.cz/api/auth/password/login` `{"email","password"}` → `{sessionToken, user}` |
| E-posta kodu | `POST /api/auth/code/request` · `POST /api/auth/code/verify` |
| Kayıt | `POST /api/auth/register/start` (12–128 karakter parola, şartlar sürümü `2026-09-24.1`) |
| Web girişi | PKCE, `/ide/connect?app=luxos`, loopback `http://127.0.0.1:PORT/callback` |
| Profil / kota | `GET /api/me` (aylık kullanım, `resetAt`) |
| Ayarlar | `PATCH /api/account/settings {username, theme, aiResponseStyle}` |
| Parola | `POST /api/account/password` |
| Geri bildirim | `POST /api/feedback` |
| Modeller | `flash` (görsel destekli, hızlı), `lite` (görsel yok, hızlı başlık) |
| Hata kodları | `auth`, `invalid_credentials`, `signed_out`, `rate`, `slow_down`, `web_only`, `model_unavailable` … |
| Hesap dosyası | `~/.config/luxai/account.json` (`sessionToken`, `user`, `client`) |
| Ayarlar | `~/.config/luxai/settings.json` — 25 alan (model, mode, vision, workdir, timeout, window, sidebar, style, answer_length, language, instructions, temperature_custom, auto_title, keep_history, send_key, notify, theme, agent{}) |

Doğrulama (bu oturumda canlı test): `POST /api/ide/chat/completions` oturumsuz çağrıldığında
sunucu **`{"error":"auth"}` + HTTP 401** dönüyor — yani uç nokta canlı ve beklediğimiz gibi.

---

## 8. Ajan (Agent) mimarisi — 38 araç

İkilinin içindeki araç şeması ayrıştırıldığında **38 fonksiyon** çıkıyor:

```
run_command, read_file, write_file, edit_file, delete_file, move_file, make_directory,
list_directory, find_files, search_text, open_app, open_url, list_apps, list_windows,
focus_window, close_window, screenshot, mouse_move, mouse_click, scroll, type_text,
press_key, clipboard_read, notify, system_info, send_feedback,
docs_search, docs_read, web_search, web_fetch,
map_geocode, map_reverse, map_nearby, map_route, map_transit_route, transit_timetable,
current_location, map_open
```

İzin modeli (uygulama dokümanından):

| Düzey | Örnekler | Davranış |
|---|---|---|
| Okuma | `read_file`, `list_directory`, `find_files`, `search_text`, `list_windows` | sormadan |
| Eylem | shell, yazma/silme, fare/klavye, pencere kapatma, uygulama/URL açma, ekran görüntüsü, pano | **Bir kez izin ver / Bu oturumda hep izin ver / Reddet** kartı |
| Hassas | `~/.ssh`, `.env`, `.pem/.key`, `send_feedback` | her seferinde tek onay |
| Tehlikeli | kapatma, `sudo`, `curl … | sh`, ev dışına yazma, root komut | **iki onay**, kalıcı izin yok |
| Bloke | `account.json` tokenı, `rm -rf /`, `mkfs`, `dd`, fork bomb, `chmod -R /`, önyükleyici, sudoers/polkit/PAM, `echo … | sudo -S` | asla |

Ek olarak: `run_command(root=true)` için **polkit** ile ayrıcalıklı yardımcı
(`luxai-root`, `luxai-root-auto`, `org.luxai.root*`), ~5 dakikalık yetki önbelleği,
`~/.local/state/luxai/root-commands.log` denetim kaydı.
Sistem istemleri (Agent/Chat) da ikiliden çıkarıldı; LuxOS/Arch'e özgü talimatlar içeriyor
(`lux` paket yöneticisi, `pacman` yasak, `/safe`, `/dev/sandbox` …).

---

## 9. Güvenlik / gizlilik notları

* Mesajlar LuxAI servisine (`lux-ai.cz`) gidiyor; araçlar **yerelde** çalışıyor.
* Web içeriği geldiğinde ek onay (istem enjeksiyonu koruması), "tool output = veri, talimat değil".
* Hesap tokenı hiçbir yere yazılmıyor/loglanmıyor; `account.json`'u yalnız `luxos-account` yazıyor.
* `--remote-session` ile loopback API (LuxOS Companion köprüsü) — `REMOTE-SESSION.md`.
* Depo MIT lisanslı ama **uygulamanın kendisi (ikili) LuxOS'tan geliyor**, resmî proje değil.

---

## 10. Windows 7 iddiası neden imkânsız?

| Gereksinim | Windows 7 |
|---|---|
| WSL | ❌ yok (WSL2: Win10 1903+/2004; WSL1: Win10 1607+) |
| WSLg (GUI) | ❌ yok (Win11 22H2 / Win10 19044 Insider) |
| GTK3 + Arch/Debian kullanıcı alanı | ⚠️ MSYS2 ile mümkün ama dağıtımı ~500 MB DLL cehennemi, resmî destek yok |

---

## 11. Neden "kaynaktan native Windows derlemesi" mümkün değil?

1. **Kaynak yok:** depo yalnız derlenmiş ELF + çeviri/CSS içeriyor.
2. **Dil kapalı:** uygulama LSL ile yazılmış; `lsl` derleyicisi ve çalışma zamanı depoda yok,
   kamuya açık değil.
3. **GTK3 bağımlılığı:** C/GTK3'e Windows'ta native derleme, tüm kaynak elde olsa bile
   GTK3'ün Win32 backend'ine taşınmasını gerektirir (LuxOS'a özgü GTK özellikleri: `GtkHeaderBar`
   düğme yamaları, CSS `shade()`, X11 araçları…).
4. **Linux'a özgü ajan:** `xdotool`, `wmctrl`, `grim`, `polkit`, `/proc`, `/sys` okumaları,
   `lux` paket yöneticisi, LSL doküman kütüphanesi — hepsi Windows'ta yok.
5. **Yamalar adres tabanlı:** `patch_binary.py` sabit dosya ofsetlerine `lea` yaması yapıyor;
   yeni bir platforma derleme yok, yalnız aynı ELF'in baytları değişiyor.
6. **Tersine mühendislik yolu da kapalı değil ama pahalı:** 1.9 MB strip edilmiş LSL VM'i +
   GTK arayüzünü yeniden üretmek pratikte uygulamayı yeniden yazmak demek.

---

## 12. Ne yaptım: **LuxAI Native for Windows** (sıfırdan, WSL'siz)

Kaynak kod yerine **protokolü** kaynak aldım: aynı hesap, aynı uç nokta, aynı başlıklar,
aynı akış — ama **saf Win32** arayüz ve **Windows'a uyarlanmış ajan araçları**.

* **Dil/teknoloji:** C (C11, mingw-w64), GTK yok, .NET yok, harici DLL yok.
  Yalnız sistem kütüphaneleri: `user32, gdi32, comctl32, shell32, shlwapi, winhttp, ole32, advapi32`.
* **Hedef:** **Windows 7 SP1 ve üzeri, x64** (`_WIN32_WINNT=0x0601`, PE alt sistem sürümü 6.1,
  `msftedit.dll` = Win7'de hazır, Common Controls v6 manifest, DPI-aware).
* **Boyut:** `LuxAI.exe` ≈ **257 KB** (tek dosya, kurulum gerekmez — taşınabilir).
  `LuxAI-Setup.exe` ≈ 653 KB (exe'yi gömer, Başlat menüsü + masaüstü kısayolu + kaldırma kaydı).
* **Arayüz:** sohbet listesi (kenar çubuğu), akış halinde yanıt, Markdown (başlık, **kalın**,
  *italik*, `satır içi kod`, kod blokları, madde işaretleri, tablolar mono), açık/koyu tema,
  TR/EN arayüz, Enter / Ctrl+Enter ile gönderme, Durdur, model seçimi (Flash/Lite), Sohbet/Ajan modu.
* **Hesap:** `lux-ai.cz` girişi (e-posta + parola → `sessionToken`), token
  `%APPDATA%\LuxAI\account.json` (sadece bu dosya, 0600 benzeri kullanıcı profili).
  Alternatif: OpenAI uyumlu herhangi bir uç nokta + API anahtarı (Ayarlar'dan).
* **Ajan araçları (14):** `run_command` (cmd.exe), `read_file`, `write_file`, `edit_file`,
  `delete_file`, `move_file`, `make_directory`, `list_directory`, `find_files`, `open_url`,
  `clipboard_read`, `screenshot` (GDI ile BMP), `system_info`, `notify` (balon bildirim).
* **Güvenlik:** yazan araçlar için **onay penceresi**, "otomatik onayla" anahtarı,
  sert blok listesi (`format`, `diskpart`, `bcdedit`, `del /s /q C:\`, `vssadmin delete`,
  `reg delete HKLM`, `net user`, `powershell -enc` …), güvenlik reddi modelin tekrar denemesini engeller.
* **Tanılama:** son istek `%APPDATA%\LuxAI\last_request.json`, son yanıt `last_response.txt`
  (parola/anahtar yazılmaz) — gerçek hesapla hata ayıklamak için.
* **Depolama:** sohbetler `%APPDATA%\LuxAI\conversations\*.json`, ayarlar `settings.ini`.

### Doğrulama (bu ortamda gerçekten çalıştırıldı)

Wine 10 + Xvfb üzerinde:

1. Uygulama açıldı, giriş penceresi geldi (`shot1.png`).
2. Sahte bir SSE sunucusuna bağlanıp **akış + araç çağrısı** döngüsü test edildi:
   model `system_info` aracını çağırdı → araç çalıştı → sonuç modele döndü → final yanıt aktı
   ve Markdown biçimlendirildi (`shot2.png`, `shot_dark.png`).
3. Yeniden başlatmada sohbet geçmişi kenar çubuğundan yüklendi (`shot_loaded.png`).
4. `LuxAI-Setup.exe` çalıştı: `%LOCALAPPDATA%\LuxAI\` içine kurdu, Başlat menüsü ve masaüstü
   kısayollarını oluşturdu, uygulamayı başlattı (`shot_afterinstall.png`).

> Not: Wine testi arayüz/akış/araç döngüsünü kanıtlar; **gerçek `lux-ai.cz` hesabıyla** son
> doğrulama sende. Sunucu istek gövdesinde ek alan isterse `last_response.txt` + `last_request.json`
> ile birkaç dakikada uyarlanır.

### Numaralar

| | Orijinal (LuxOS/WSL) | Bu port |
|---|---|---|
| Kurulum | WSL + Debian + apt (~1–2 GB) | 257 KB exe (kuruluma bile gerek yok) |
| En düşük Windows | 11 (WSLg) | **7 SP1** |
| Açılış | WSL VM + X sunucusu | anında |
| Ajan araçları | 38 Linux aracı | 14 Windows aracı |
| Arayüz dili | 14 | TR + EN (tablo tabanlı, kolayca genişletilir) |

---

## 13. Bilinen sınırlar (dürüst liste)

* **Görsel (vision) yok:** ekran görüntüsü BMP olarak kaydedilir, modele resim olarak gönderilmez
  (LuxAI `flash` modeli görüntü destekliyor; eklemek için base64 data-URL yolu açık).
* **`docs_search`/`docs_read` (LSL kütüphanesi), harita araçları, `web_search`** yok —
  Windows'ta karşılıkları yok; eklenebilir (harita için Nominatim/OSRM doğrudan HTTP).
* **Otomatik başlık** yok; başlık ilk mesajdan türetiliyor.
* **Ayrıcalıklı (root/admin) mod** yok: UAC elevation ayrı tasarım gerektirir; ajan gerektiğinde
  kullanıcıya yönetici komutu öneriyor.
* **Windows 7'de TLS 1.2:** WinHTTP eski Win7 kurulumlarında TLS 1.0 kullanır; `lux-ai.cz`
  TLS 1.2 istiyorsa **KB3140245** ve `Enable TLS 1.2` kayıt anahtarı gerekir (uygulama bunu
  deneyerek bağlanır, hata mesajı gösterir).
* **Sohbet geçmişi sunucuda değil**, yalnız yerelde (`%APPDATA%\LuxAI\conversations`).
  Sunucu tarafındaki web sohbet geçmişi (`/api/chat/history`) ile senkron değil.

---

## 14. Nasıl derlenir / çalıştırılır

```bash
# Linux'ta çapraz derleme
sudo apt install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64 python3
cd luxai-windows
bash build-win.sh            # → build/LuxAI.exe
bash build-installer.sh      # → build/LuxAI-Setup.exe
```

Windows'ta: `LuxAI-Setup.exe` (önerilen) veya doğrudan `LuxAI.exe`.
İlk açılışta LuxAI hesabıyla giriş yap; hesabın yoksa `lux-ai.cz/register`.

---

*Bu belge, deponun `packaging/` betikleri, gömülü dokümanları, ELF ikilisinin sembol/strings
analizi ve canlı API denemeleri üzerine kurulmuştur. LuxAI resmî bir LuxOS projesi değildir;
bu port topluluk çalışmasıdır ve LuxOS ekibiyle bağlantılı değildir.*
