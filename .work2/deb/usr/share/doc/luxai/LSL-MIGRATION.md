# Nativní LuxAI v LSL — kandidát k ověření

Pracovní adresář: `~/Projects/LuxAI`. Překladač: `/usr/bin/lsl` **0.10.6**,
cíl `linux-x86_64-gnu`, GTK 3 / GLib / libcurl. Místní instalace LuxAI, LSL ani
polkit pravidla se při přepisu neměnily. Oficiální repozitář LuxOS nebyl změněn.

Implementace celé aplikace je v `lsl/`. Nativní vstupní bod je `lsl/main.lsl`;
nepoužívá původní Python aplikaci ani Python root helpery. Původní `luxai/`,
`tests/` a exportéry v `tools/` zůstávají vývojovou referencí pro porovnání.

## Rozsah přepisu

| Oblast | Nativní moduly | Ověření |
|---|---|---|
| Hlavní okno | `app_window`, `app_model`, `gtk_ext`, `ui_dispatch`, `version`, `main` | oddělený GTK displej, skutečný agent a loopback API, druhá instance přes D-Bus, historie, stop, zavření a úlohy na pozadí |
| Zprávy a karty | `ui_widgets`, `ui_cards`, `ui_format`, `ui_data` | Markdown, matematika, kód, tabulky, přílohy, výběr textu, časovače, dvě potvrzení nebezpečné akce, zákaz trvalého schválení hlášení |
| Nastavení | `settings_window`, `settings` | všech pět sekcí, změny ovládacích prvků, okamžité použití, synchronizace, debouncing, zápis při zavření, seznamy složek |
| Účet a API | `api`, `transport` | účet, HTTP, SSE po bajtech, Unicode, reasoning, tool cally, tokeny, chyby a přerušení z jiného vlákna |
| Agent | `agent_core`, `prompts`, `protocol`, `tool_format` | skutečná oprávnění, validní historie, schválení/odmítnutí, oprava vypsaného JSONu místo tool callu bez jeho spuštění |
| Historie | `store`, `json_store` | Python ↔ LSL formát, privátní soubory, obrázky mimo JSON, oprava sekvence nástrojů, soubor přes 4 MiB, znovunačtení v GTK |
| Bezpečnost | `rootguard`, `guard_data`, `shellwords`, `safety`, `safety_data` | původní korpus příkazů a skriptů, náhodné shellové tokeny, proměnné, substituce, symlinky, chráněné soubory, oprávnění a nedůvěryhodný web |
| Procesy a správce | `child_process`, `rootmode`, `root_helper`, dva helper vstupní body | výstup/UTF-8, stdin, timeout, rušení skupiny procesů, zachování stop signálu, odmítnutí nebezpečných příkazů a testovací polkit soubor bez práv správce |
| Nástroje | `tools`, `gui_bridge` | soubory, hledání, skutečný proces v dočasné složce, přesné desktopové argv bez ovládání skutečné plochy, propojení s kartami a schvalováním |
| Web | `web`, `urls`, `net_address`, `html_content`, `web_data` | veřejné IP, DNS pinning, přesměrování, zákaz privátních adres, HTML/entity, 2MiB stránka, kódování, DuckDuckGo, zdroje důvěryhodných URL a odkazy bez query |
| Obsah odpovědí | `markdown`, `mathtex`, `highlight` | porovnání s původními testy, neúplné streamované bloky, matematické vstupy a všechny jazyky zvýrazňování |
| Přílohy | `attachments`, `images` | limity, text, obrázky, JPEG shodný s referencí, průhlednost, náhledy, vložení souboru do skutečného okna |
| Názvy | `titles`, `app_model` | pozdravy, témata z příloh, generování, fallback/retry, úlohy na pozadí, zrušení a přednost ručního názvu |
| Dokumentace a překlady | `docs`, `i18n`, `catalog` | BM25, české tvary, sekce, cesty/symlinky, prostředí a všechny dodané TSV |
| Zpětná vazba | `feedback`, `tools`, `ui_cards` | anonymizace, systémové údaje, přesný schvalovaný payload, obrázek, lokální HTTP, zrušení a kompatibilní soukromý outbox |
| Mapy a jízdní řády | `maps`, `maps_coord`, `tools`, `ui_cards` | argumenty a souhrny proti Pythonu, falešný `luxos-maps` (souhlas, busy, chybějící Mapy, neplatný JSON), PNG náhled v cache, karta trasy na Broadway, běh agenta v okně a obnova karty z historie |
| Kalendář LuxOS (jen nativní) | `calendar`, `calendar_ics`, `calendar_clock`, `calendar_time`, `calendar_data`, `tools`, `safety`, `ui_cards` | dočasný `events.ics`: výpis/vytvoření/úprava/smazání, cizí komponenty bajt po bajtu, souběžný zápis mostu (flock) i Kalendáře (bez zámku) během změny, letní čas, české escapování, karta s potvrzením a obnova z historie na Broadway, odmítnutí ve vzdálené relaci, čtení v Chatu |
| Vzdálená relace (jen nativní) | `remote_session`, `main`, `agent_core` | `luxai --remote-session` pro most LuxOS Companion ([REMOTE-SESSION.md](REMOTE-SESSION.md)): skutečný program proti loopback API, streamování bez reasoningu, busy, cancel, EOF, nepřihlášený účet, odmítnutí akcí vyžadujících potvrzení, root i při `noask` |

Testovací ovladače `bridge`, `testing`, `gui_test_main`, `model_test_main` a
`window_test_main` nejsou desktopovým vstupním bodem ani permissivní náhradou
oprávnění. Produkční agent před každým nástrojem požaduje připojený posuzovač;
bez něj volání odmítne. Vypsaný JSON se nikdy nepředává do shellu.

Kalendářové nástroje (`calendar_list_events`, `calendar_create_event`, `calendar_update_event`,
`calendar_delete_event`, viz README) nemají protějšek v Python referenci; jsou v
[lsl-parity.json](lsl-parity.json) jako `calendar_2026_10_27`. Úložiště a protokol zápisu jsou stejné
jako u Kalendáře LuxOS (`luxos_ics.lsl`, `luxos_calendar_ics.py`) a mostu LuxOS Companion
(`calendar_note.py`); test čte zapsaný soubor i modulem `luxos_calendar_ics`, pokud je nainstalovaný.

Vzdálená relace `luxai --remote-session` nemá protějšek v Python referenci (ta je čistě GTK
aplikace); je zapsaná v [lsl-parity.json](lsl-parity.json) jako `remote_session_2026_10_27`.
Přepínač se zpracuje před vytvořením `GtkApplication`, takže se nikdy nepředá běžící instanci.

## Audit parity 2026-10-26

Každý modul Pythonu byl znovu porovnán funkci po funkci s nativním protějškem a
vygenerovaná data (`catalog`, `ui_data`, `guard_data`, `safety_data`, `web_data`)
byla znovu exportována a porovnána. Žádná funkce nechyběla, ale našly se rozdíly
v chování a chování Python testů bez nativního testu. Seznam je v
[lsl-parity.json](lsl-parity.json) (`audit_2026_10_26`); vše z něj je opravené a
pokryté srovnávacími testy nebo kontrolou GTK na Broadway:

- chyba modelu uprostřed SSE je chyba modelu (ne „neplatná odpověď“), SSE bez limitu 4 MiB,
  rychlý parser (5MiB odpověď v testu),
- potvrzovací karty: pořadí nadpisu, celé URL a důvodů, ⚠ a červený styl varování,
- postranní panel (zvýraznění, dvojklik = přejmenovat), odhlášení, dialogy se sekundárním textem
  a destruktivními tlačítky, přeložená tlačítka výběru souborů, tooltipy a ikony, avatar v profilu,
  ikony stránek nastavení, `GTK_THEME`, nahrazování oznámení,
- agent: historie se přepisuje jen při skutečné opravě, vypnutý nástroj odmítne posuzovač,
  JSON v historii jako `json.dumps`,
- přílohy, dokumentace, i18n, hlášení, názvy a historie: chybové texty, dekódování, konce řádků,
  pořadí procházení, mazání,
- `open()`/`poll()` se znaménkovým rozšířením a lineární `strip` (viz [LSL-ISSUES.md](LSL-ISSUES.md)).

Známé zbývající drobné rozdíly (bez vlivu na bezpečnost): menu v popoverech jsou běžná tlačítka
místo `Gtk.ModelButton`, pracovní složka se vybírá tlačítkem s dialogem místo `FileChooserButton`,
dialogy napodobují `Gtk.MessageDialog`, zaokrouhlení skóre dokumentace a velikosti paměti je
„half-up“ místo bankovního (jen přesné remízy), poškozený index dokumentace hlásí vždy
`JSONDecodeError`, libcurl dekomprimuje odpověď sám, `pkcheck`/`pkexec` při nastavení režimu
správce běží přes `timeout`, přípony obrázků jsou zmrazené při exportu z `mimetypes`.

## Izolace testů

`tools/test-lsl.sh` spouští všechny testy v dočasném HOME a XDG adresářích, s privátním
`XDG_RUNTIME_DIR` (0700), ve vlastní `dbus-run-session`, bez `DISPLAY`/`WAYLAND_DISPLAY`,
bez portálů, keyringu, ssh-agenta a at-spi skutečné relace a s `nice -n 10`. GTK testy
používají vlastní Broadway displej, servery jsou jen na loopbacku. Nativní GUI testy se
odmítnou spustit mimo toto prostředí (`tests_lsl/isolation.py`). Dříve test příkazové řádky
procházel jen proto, že `luxai --help` doručil skutečné běžící LuxAI přes relační D-Bus
uživatele; nyní jde jen testovací instanci.

## Kontroly a artefakty

```sh
tools/test-lsl.sh
tools/build-lsl.sh candidate-check
tools/build-lsl.sh app
tools/build-lsl.sh helpers
tools/make-lsl-source.sh
```

Úspěšný test zapíše počet kontrol, verzi překladače a otisk zdrojů do
[lsl-verification.json](lsl-verification.json), celý výstup do
[lsl-test-report.txt](lsl-test-report.txt). Kontrola kandidáta odmítne změněné
zdroje bez nového úspěšného testu. Úplný inventář původních symbolů a testů je v
[lsl-parity.json](lsl-parity.json). Nalezené problémy LSL včetně samostatných
reproduktorů jsou v [LSL-ISSUES.md](LSL-ISSUES.md).

Sestavení vytvoří `build/luxai`, `build/luxai-root`, `build/luxai-root-auto` a
`build/luxai-root-setup`. Zdrojový kandidát a PKGBUILD vzniknou v
`build/candidate/`. Nic z toho se samo neinstaluje, nepodepisuje ani nepublikuje.
Nativní běhový balíček obsahuje ELF programy a data; Python je jen vývojová a
testovací závislost tohoto projektu. Externí nástroje LuxOS zůstávají vlastními
aplikacemi se svými závislostmi.

Pro ruční spuštění nativního programu vedle dosavadní aplikace:

```sh
dbus-run-session -- ./build/luxai
```

Tato nová D-Bus relace oddělí doručování dotazů mezi instancemi. Ručně spuštěná
aplikace nadále používá běžný účet, nastavení a historii. Automatické testy
oproti tomu používají pouze dočasné profily, syntetický účet, oddělený Broadway
backend a loopback servery.

## Náhrada Python aplikace

`build/candidate/PKGBUILD` (z `packaging/PKGBUILD` přes `tools/make-lsl-source.sh`) má stejné
`pkgname=luxai` a vyšší `pkgrel` než balíček v repozitáři LuxOS, takže aktualizace nahradí
Python aplikaci: `/usr/bin/luxai` je nativní ELF, pomocníci správce jsou nativní ELF na stejných
cestách (`/usr/lib/luxai/luxai-root[-auto|-setup]`, polkit politika beze změny), data jsou v
`/usr/share/luxai`. Python, PyGObject a dbus jsou jen `makedepends`/`checkdepends` (srovnávací
testy a export dat); běhový balíček Python nepotřebuje. Nic se neinstaluje, nepodepisuje ani
nepublikuje.

## Před podpisem a nasazením

Zbývá uživatelova vizuální kontrola v běžném desktopu a kontrola skutečného
polkit dialogu/pravidel po instalaci kandidáta do ověřovacího prostředí.
Automatické testy neaktivují systémová pravidla správce ani neovládají skutečnou
obrazovku a schránku. Vývojový program záměrně nepoužije dosavadní Python
root helper místo nativního; kandidátní balíček dodá všechny nativní pomocníky.

Manifest označuje připravenost kandidáta zvlášť od `release_ready`.
`tools/build-lsl.sh release-check` proto před dokončením vydávacího ověření
nadále odmítá vydání. Podpis uživatelovým klíčem a nasazení do oficiálního
repozitáře jsou další samostatné kroky.
