# LuxAI

Nativní [přepis do LSL](docs/LSL-MIGRATION.md) je připravený jako kandidát k ověření.
Dokument obsahuje rozsah kontrol, nalezené chyby LSL a postup sestavení bez instalace.

AI asistent pro LuxOS jako desktopová aplikace (LSL + GTK 3). Stejný asistent jako
[Lux Code](../Lux-Code), ale s grafickým rozhraním a dvěma režimy:

- **Chat** – obyčejná konverzace se streamovanými odpověďmi, Markdown (nadpisy, seznamy,
  tabulky, bloky kódu s tlačítkem Kopírovat, odkazy), historie konverzací v postranním panelu.
- **Agent** – LuxAI pracuje s počítačem pomocí nástrojů: shell, čtení/zápis/úprava/mazání
  souborů, výpis a hledání souborů, otevírání aplikací a URL (`gtk-launch`, `xdg-open`),
  snímek obrazovky, myš a klávesnice (`xdotool`), seznam/aktivace/zavření oken (`wmctrl`),
  schránka, oznámení. Každé volání nástroje je karta s výsledkem a podrobnostmi.
  Nástroj `send_feedback` pošle hlášení chyby / nápad autorovi LuxOS (viz níže).

V obou režimech má model offline příručku LuxOS a LSL z balíku `luxos-docs`
(`/usr/share/luxos/docs`, pro testy `LUXAI_DOCS_DIR`): `docs_search(query, collection?)` vrátí
nejlepší sekce (soubor, nadpis, úryvek; hledání bez ohledu na velikost písmen a diakritiku, BM25)
a `docs_read(path, heading?)` přečte soubor nebo jednu sekci. Nástroje jen čtou soubory uvnitř
složky příručky (cesty s `..`, absolutní a symbolické odkazy ven odmítnou) a nevyžadují schválení;
v režimu Chat jsou kromě nich jen webové nástroje (viz níže). Bez nainstalované příručky vrátí srozumitelnou chybu.

Ke zprávě lze přiložit obrázky a textové soubory: tlačítko **+** vedle pole pro zprávu (výběr souborů),
**Ctrl+V** obrázku nebo souborů zkopírovaných ve správci souborů. Přílohy se ukážou jako náhledy
s křížkem pro odebrání. Obrázky se zmenší a převedou na JPEG (do ~220 kB), textové soubory se vloží jako
text (nejvýše 60 000 znaků), jiné soubory aplikace odmítne se srozumitelnou hláškou. Nejvýše 4 obrázky
a 8 příloh v jedné zprávě.

## Web (web_search, web_fetch)

V obou režimech (Chat i Agent) může model hledat na webu a číst stránky (`lsl/web.lsl`):

- `web_search(query, max_results?)` – nejvýše 8 výsledků (název, URL, úryvek). Nejdřív přes účet LuxAI
  (`POST /api/ide/search` s cookie účtu, stejný endpoint jako Lux Code; vyhledávač a jeho klíč jsou na
  serveru). Když server klienta `luxai` nepřijme (401/403/404 …), LuxAI to v relaci už nezkouší a hledá
  přímo přes DuckDuckGo HTML (`https://html.duckduckgo.com/html/`, bez klíče).
- `web_fetch(url)` – jen `http(s)`, bez cookies a přihlašovacích údajů (URL s `user:heslo@` odmítne),
  nejvýše 5 přesměrování, stáhne nejvýše 2 MB, z HTML vytáhne čitelný text (bez skriptů, stylů, navigace,
  patičky; přednost má `<main>`/`<article>`) a zkrátí ho na 20 000 znaků. Jen textové typy (HTML, text,
  JSON, XML …). User-Agent `LuxAI/<verze> (LuxOS)`, časový limit 15 s na spojení a 40 s celkem.
- **SSRF:** název serveru se přeloží a každá adresa musí být veřejná (žádný loopback, LAN, link-local,
  CGNAT, multicast, IPv4 v IPv6 …); spojení jde přímo na ověřenou adresu (bez druhého DNS dotazu), TLS
  ověřuje certifikát pro původní název. Totéž při každém přesměrování.
- **Nedůvěryhodná data:** výsledek nese `"untrusted": true`, poznámku pro model a text ohraničený značkami
  `<<<UNTRUSTED WEB CONTENT …>>>` / `<<<END OF UNTRUSTED WEB CONTENT>>>` (značky uvnitř obsahu se
  znehodnotí). Systémová zpráva modelu říká, že pokyny z webu nesmí plnit.
- **Ochrana před prompt injection:** jakmile je v konverzaci výsledek `web_search`/`web_fetch`
  (`safety.web_tainted`, platí i po znovuotevření uložené konverzace), každý `run_command` (i jako root
  v režimu správce bez potvrzení) a `type_text` se potvrzuje jednotlivě – i při *Automaticky schvalovat*
  a „Vždy v této relaci“. Nebezpečné akce dál vyžadují dvě potvrzení, tvrdé bloky zůstávají.
- **Ochrana před vynesením dat v URL** (`web.fetch_trusted`): bez dotazu otevře `web_fetch` jen URL, kterou
  doslova napsal uživatel nebo vrátilo `web_search` v této konverzaci (stejná URL, `#fragment` se ignoruje),
  a odkazy na stejný server ze stránky, kterou už `web_fetch` přečetl – ty jen bez query stringu (`?…`).
  Odkazy stránky se ukládají u zprávy (`_links`, modelu se neposílají). Jakoukoli jinou URL (sestavenou
  modelem, s jiným query stringem) musí uživatel jednorázově schválit – i v Chatu a i při automatickém
  schvalování; karta ukáže celou URL.
- Oprávnění: v Chatu se nástroje spouští bez dotazu (Chat nemá přístup k počítači). V Agentovi je
  `web_search` bez dotazu a `web_fetch` je síťová akce podle nastavení *Přístup k síti* (výchozí „ptát se“).
  „Nikdy“ oba nástroje modelu vůbec nenabídne.

## Mapy a jízdní řády

V Chatu i Agentovi má model stejné mapové nástroje jako LuxAI v telefonu (`lsl/maps.lsl`,
reference `luxai/maps.py`): `map_geocode`, `map_reverse`, `map_nearby`, `map_route`,
`map_transit_route`, `transit_timetable` a `current_location` (stejné názvy, popisy a parametry);
v Agentovi navíc `map_open` (ukáže místo nebo trasu v okně Map, kategorie „Otevírání aplikací“).

- LuxAI nikdy nevolá mapové služby přímo: vše jde přes `luxos-maps --json …` (aplikace Mapy), která
  má cache, limity (1 dotaz za sekundu pro celý počítač), pravidla OpenStreetMap/Transitous/Overpass
  a souhlas s polohou. Bez balíčku `luxos-maps` nástroje jasně ohlásí, že Mapy nejsou nainstalované.
- Nástroje jen čtou data, proto se nepotvrzují (stejně jako `web_search`); síť „nikdy“ je skryje a
  zablokuje. `current_location` funguje jen po povolení „Moje poloha“ v Mapách, jinak karta i model
  dostanou návod, jak ji povolit; „busy“ znamená nezkoušet znovu ve smyčce.
- Výsledek pro model je JSON z Map bez `preview` (souřadnice trasy). Trasa a spojení MHD se ukážou jako
  karta: obrázek mapy z `luxos-maps --render-preview` (480×270, v `~/.cache/luxai/maps`), barvy linek,
  „Mapová data © přispěvatelé OpenStreetMap“ a tlačítko **Otevřít v Mapách**. Data karty se ukládají u
  zprávy (`_map`), po znovuotevření konverzace se karta obnoví. Spojení a odjezdy jsou vypsané jako v
  telefonu (čas, linka, směr, nástupiště, zpoždění, zrušení).

## Kalendář LuxOS (jen nativní verze)

LuxAI pracuje s vlastním úložištěm Kalendáře LuxOS: `$XDG_DATA_HOME/luxos/calendar/events.ics`
(výchozí `~/.local/share/luxos/calendar/events.ics`); ostatní `*.ics` v té složce jsou kalendáře jen
ke čtení, stejně jako v aplikaci Kalendář. Implementace `lsl/calendar*.lsl`; Python reference
(`luxai/`) kalendář nemá.

- `calendar_list_events(from, to, query?)` – události v rozsahu (nejvýš 400 dní, 100 výsledků),
  rozbalená opakování (RRULE DAILY/WEEKLY/MONTHLY/YEARLY, EXDATE, RECURRENCE-ID jako v Kalendáři);
  vrací `uid`, název, začátek/konec v místním pásmu (ISO 8601 s posunem; celodenní jako data
  s posledním dnem včetně), `all_day`, místo, popis (zkrácený), kalendář, `editable`, `recurring`,
  připomenutí. Jen čtení: bez potvrzení, v Chatu i v Agentovi.
- `calendar_create_event`, `calendar_update_event(uid, …)`, `calendar_delete_event(uid)` – jen
  v Agentovi a vždy s potvrzovací kartou (úroveň „ask“: „Povolit jednou“, „Vždy v této relaci“,
  automatické schvalování jako u ostatních „ask“ akcí; nezávisí na nastavení souborů). Karta ukáže
  událost (název, den v týdnu, datum a čas v místním pásmu se zkratkou CET/CEST, místo,
  připomenutí); úprava ukáže stav před a po, mazání mazanou událost; u opakované události
  upozornění, že změna platí pro celou řadu. Události kalendářů jen ke čtení se měnit nedají.
- Čas: bez posunu je to místní čas; ISO 8601 s posunem (`+01:00`, `Z`) se převede. Neexistující
  místní čas (jarní posun hodin) se odmítne, dvojznačný (podzimní) je první výskyt; `duration_min`
  počítá skutečné minuty i přes změnu času. Celodenní události jako `VALUE=DATE` s výlučným `DTEND`.
  Zápis `DTSTART;TZID=<místní pásmo>` jako aplikace Kalendář.
- Zápis stejným protokolem jako most LuxOS Companion (`calendar_note.py`): zámek
  `.events.ics.lock` (flock), znovu načíst soubor pod zámkem, změnit jen text dotčené VEVENT (vše
  ostatní – cizí události, VTODO, VTIMEZONE, neznámé vlastnosti – zůstane bajt po bajtu, včetně
  konců řádků a práv souboru), dočasný soubor + fsync, kontrola, že soubor mezitím nikdo nezměnil
  (aplikace Kalendář ukládá bez zámku; jinak znovu načíst a změnu zopakovat), přejmenování, fsync
  složky. Text se escapuje podle RFC 5545 (`\,` `\;` `\\` `\n`), řádky se zalamují po 75 bajtech.
  Poškozený soubor (bez `END:VCALENDAR`) nebo neplatné UTF-8 LuxAI nemění.
- Systémová výzva dostane krátký návod a dnešní den v týdnu, datum a pásmo, aby model správně
  přepočítal „zítra“ nebo „příští pondělí“.
- Vzdálená relace z telefonu smí kalendář číst; změny se odmítnou („vyžaduje potvrzení na
  počítači“) – telefon má vlastní kalendářovou poznámku přes most.

## Matematika, kód a kopírování

- Matematika v odpovědích (`lsl/mathtex.lsl`, bez závislostí): `$…$`, `\(…\)`, `$$…$$` a `\[…\]`
  (samostatný řádek = vystředěná rovnice) i „holý“ LaTeX v textu (`\sum_{k=0}^{\infty}`, `x^2`) se převede na
  Unicode a Pango markup: řecká písmena, operátory a relace, `\sum \prod \int \lim` s mezemi, `\frac`,
  `\sqrt`, horní/dolní indexy (Unicode, jinak `<sup>/<sub>`), `\left \right`, `\text`, `\mathbb`, akcenty,
  `cases`/`pmatrix`. Neznámý příkaz se vypíše bez zpětného lomítka. Bloky a řádkový kód zůstávají beze změny,
  `$5 a $10` není matematika.
- Bloky kódu mají zvýraznění syntaxe (`lsl/highlight.lsl`, vlastní lehký tokenizér: Python, Bash, LSL,
  JavaScript/TypeScript, C/C++, JSON, HTML/XML, CSS, SQL, YAML/TOML/INI, diff, Java, Go, Rust, C#, PHP,
  Kotlin, jinak obecné řetězce/čísla/komentáře), barvy pro světlý i tmavý motiv (kontrast ≥ 4,5 : 1)
  a skutečný název jazyka v hlavičce.
- Kopírování: text zpráv lze vybrat myší a zkopírovat **Ctrl+C** (fokus zůstává v poli pro zprávu),
  pod každou odpovědí je **Kopírovat** (celá odpověď v Markdownu) a u bloku kódu **Kopírovat** (jen kód).
  Schránka se předá správci schránky (`store()`) a na X11 ji drží i `xclip`, takže vydrží i po zavření LuxAI.

## Názvy konverzací

Po první věcné zprávě a první odpovědi LuxAI na pozadí požádá nejlevnější model **lite**
(stejně jako web lux-ai.cz) o krátký název v jazyce konverzace; UI se nikdy neblokuje. Pozdrav
(„ahoj“, „hi“) název neurčuje – čeká se na skutečné téma. Při chybě zůstane záložní název
z prvních slov zprávy. Konverzaci lze kdykoli přejmenovat (pravé tlačítko / dvojklik v seznamu);
ručně zadaný název se už nepřepisuje. Pokud na účtu zrovna běží jiný požadavek (`chat_busy`),
název i hlavní dotaz to po 2 s zkusí znovu. Vypnout: Nastavení › Chování AI.

## Nastavení

Tlačítko profilu dole v postranním panelu otevře nabídku (jméno, e-mail, spotřeba, **Nastavení…**,
**Spravovat účet** = `luxos-account`, **Odhlásit se…** = `luxos-account logout`, platí pro celý
LuxOS). Nastavení (také Ctrl+, nebo nabídka ⋮):

- **Obecné** – motiv (systém/světlý/tmavý), odesílání Enter / Ctrl+Enter, oznámení, režim nových konverzací.
- **Chování AI** – výchozí model, obrázky pro Flash/Lite, styl odpovědí (vyvážený, stručný, podrobný,
  technický, přátelský), délka odpovědi (`max_tokens` 1500/4096/8192, agent vždy aspoň 4096), jazyk,
  vlastní teplota (posílá se jen když je zapnutá), vlastní pokyny (do systémové zprávy), automatické názvy.
- **Oprávnění agenta** – viz níže; pracovní složka, povolené a zakázané složky, časový limit příkazů.
- **Soukromí a historie** – ukládání historie, smazání celé historie, smazání snímků obrazovky.
- **Klávesové zkratky** – přehled (Enter, Esc, Ctrl+N, Ctrl+F, Ctrl+, , F9, Ctrl+Q).

## Účet

LuxAI používá jen sdílený účet LuxOS (`docs/luxai-account.md` v LuxOS-Arch): token čte
z `~/.config/luxai/account.json` (soubor nikdy nezapisuje), volá
`POST https://lux-ai.cz/api/ide/chat/completions` (SSE) a `GET /api/me` s cookie
`lux_account_session` a hlavičkou `X-Lux-Client: luxai`. Modely `flash` a `lite`, měsíční
spotřeba je vidět dole v postranním panelu. Bez účtu (nebo při 401/403) aplikace nabídne
**Přihlásit se účtem LuxAI** / **Přihlásit se znovu**, což spustí `luxos-account login`
a po jeho skončení účet znovu načte. Token se nikde nevypisuje ani neloguje.

## Bezpečnost agenta

Model potvrzování vychází z Lux Code (`permissions.lsl`, `safety.lsl`):

| Úroveň | Příklady | Chování |
|---|---|---|
| čtení | `read_file`, `list_directory`, `find_files`, `search_text`, `list_windows` | bez dotazu |
| akce | shell, zápis/mazání, myš/klávesnice, zavření okna, otevření aplikace/URL, snímek obrazovky, schránka | karta **Povolit jednou / Vždy v této relaci / Zamítnout**; přepínač *Automaticky schvalovat* dotaz přeskočí |
| citlivé | čtení `~/.ssh`, `.env`, klíčů; odeslání hlášení autorovi LuxOS (`send_feedback`) | vždy jednorázové potvrzení |
| nebezpečné | vypnutí PC, `sudo` uvnitř příkazu, `curl … \| sh`, zápis mimo domovskou složku, příkaz jako root (podle režimu správce)… | **dvě potvrzení**, nelze povolit natrvalo ani automaticky |
| blokované | token účtu LuxAI (`account.json`), tvrdé bloky `rootguard.hard_block` (`rm -rf /`, `~` nebo systémové složky, `mkfs`, `dd`/zápis na disk, fork bomba, `chmod -R` systému, zavaděč/EFI/jádra, `SigLevel = Never`, účty, sudoers/polkit/PAM, režim správce sám), předání hesla do sudo/su (`echo … \| sudo -S`, `sudo -S … <<<`, `SUDO_ASKPASS=`, `… \| su`, `sshpass`) | vždy odmítnuto, pro každý model a při jakémkoli nastavení |

Nastavení › Oprávnění agenta upravuje úroveň „akce“ po kategoriích (`safety.TOOL_CATEGORY`):
spouštět příkazy (vždy se ptát / ptát se jen u rizikových / nikdy), číst soubory (povolit / ptát se / nikdy),
zapisovat a mazat, přístup k síti, otevírat aplikace a odkazy, ovládat myš a klávesnici, snímky obrazovky
a schránka (ptát se / povolit bez dotazu / nikdy). „Nikdy“ nástroj modelu vůbec nenabídne a volání odmítne,
„jen u rizikových“ spustí bez dotazu jen zjevně čtecí příkazy (`ls`, `df`, `git status` … bez přesměrování
do souboru a bez `$(…)`). Síť se rozpoznává z příkazu (curl, wget, git clone, pip/npm/pacman -S, URL)
a z `open_url` http(s) – je to heuristika, ne síťová izolace. Povolené složky omezí souborové nástroje jen
na ně (symlinky se rozbalují), zakázané složky jsou vždy odmítnuté a `find_files`/`search_text` do nich
nevstupují; u shellu se kontroluje pracovní složka a cesty v příkazu. Nebezpečné a citlivé akce nastavení
nezmírní; soubory `~/.config/luxai/` (účet i nastavení) agent nezmění. Model hesla nikdy nezadává, nehádá ani
je nepředává rourou (`sudo -S` apod. je blokované).

### Režim správce (příkazy jako root)

Agent spouští příkazy jako root přes `run_command` s `root=true` (příkaz začínající `sudo`/`pkexec` se na
něj převede, `lsl/rootmode.lsl`). Běží přes `pkexec /usr/lib/luxai/luxai-root[-auto]`: pomocník příkaz
znovu zkontroluje `rootguard.hard_block` (stejná pravidla jako aplikace), spustí ho s čistým prostředím,
hlídá limit a skončí, když aplikace zavře stdin (Zastavit). Kódy: 125 odmítnuto, 124 limit, 130 zastaveno.
Nastavení › Oprávnění agenta › **Příkazy správce** (`policy["root_mode"]`):

| Režim | Chování |
|---|---|
| Pokaždé chtít heslo (výchozí) | dvě potvrzení karty + heslo správce v dialogu polkit (`org.luxai.root`) |
| Heslo jednou na 5 minut, každý příkaz schválit | jedno potvrzení karty; heslo správce jednou, pak ~5 minut bez hesla (`org.luxai.root-auto`) – jen model Flash |
| Heslo jednou na 5 minut, bez ptaní | bez karty; heslo správce jednou, pak ~5 minut bez hesla – jen model Flash |

Model Lite má vždy „heslo“. **Režim správce nikdy nedává root bez hesla jiným programům** (od 2.0.2):

- `org.luxai.root-auto` má v politice `auth_admin_keep`. Polkit si zadané heslo pamatuje ~5 minut (pevně
  v polkitu) jen pro **subjekt = proces, který zavolal pkexec** – tedy běžící LuxAI (pkexec je její přímý
  potomek). Jiný program stejného uživatele ve stejné relaci (i jiná LuxAI) se musí prokázat heslem sám;
  ověřeno ve VM (polkit 128): druhé volání z téhož procesu bez hesla, sourozenecký proces se stejným
  rodičem, terminálem i cgroup i proces po `setsid` dostal dialog s heslem.
- Polkit 128 považuje procesy se stejným rodičem, cgroup a řídicím terminálem za jeden subjekt (jako
  sudo pro terminál). LuxAI se proto při startu vzdá řídicího terminálu (`TIOCNOTTY`, `lsl/main.lsl`;
  ne jako vedoucí relace).
- Pomocník `luxai-root-auto` navíc pustí jen přímého rodiče `/usr/bin/luxai` (`/proc/PPID/exe`, i
  „(deleted)“ po aktualizaci – soubor v `/usr/bin` smí zapsat jen root), který není laděný
  (`TracerPid` 0) a nemá `LD_PRELOAD`/`LD_AUDIT`/`LD_LIBRARY_PATH`; jinak kód 125. Je to druhá pojistka,
  hlavní hranice je heslo.
- Dotaz, který LuxAI poslal jiný program (`luxai "text"` se předá běžící LuxAI přes D-Bus), nese
  `run["external"]`: příkaz správce z něj se vždy potvrdí kartou, i v režimu „bez ptaní“
  (`safety.external_root`, důvod „Dotaz poslal jiný program, ne okno LuxAI“).
- Žádné pravidlo polkit se nezapisuje. LuxAI do 2.0.1 při zapnutí zapsala
  `/etc/polkit-1/rules.d/49-luxai-root-<uživatel>.rules` s `polkit.Result.YES` – bez hesla tak root
  dostal **kterýkoli** program uživatele (`pkexec /usr/lib/luxai/luxai-root-auto -- sh -c …`; seznam
  zakázaných příkazů v pomocníkovi se dá obejít). `luxai.install` (LuxOS) tato pravidla při aktualizaci
  smaže, `luxai-root-setup enable|disable` je už jen maže (i když ho zavolá starší běžící LuxAI),
  `status` = „on“, dokud staré pravidlo existuje. Volba v nastavení zůstává a znamená nový režim.

Zbývající riziko: program uživatele může během zapamatovaných ~5 minut ovládat okno LuxAI (na X11 umí
kterýkoli program uživatele posílat klávesy a kliknutí do jiných oken, např. `xdotool`) – stejně jako
může psát do terminálu s platným `sudo`. Proti tomu chrání jen krátká doba, karta u každého příkazu
(režim „každý příkaz schválit“) a heslo. Tvrdé bloky platí v každém režimu; jiné nebezpečné důvody
(čtení hesel, vypnutí PC, `sudo -u`) dál chtějí dvě potvrzení. Každý příkaz jako root se zapíše do
`~/.local/state/luxai/root-commands.log` a do žurnálu (`journalctl -t luxai-root`). Tvrdé bloky chrání
před chybou modelu, ne před záměrným útokem: kdo smí spouštět příkazy jako root, může je obejít
(např. vlastním programem), proto je režim správce volitelný, jen pro Flash a vždy za heslo.

Příkazy běží neinteraktivně (stdin `/dev/null`), s časovým limitem (výchozí 120 s, nastavitelný)
ve vlastní skupině procesů; **Zastavit** (nebo Esc) přeruší odpověď modelu i běžící příkaz.
Výstupy nástrojů se pro model zkracují (max. 30 000 znaků). Automatické schvalování se po
restartu vždy vypne. Karta nástroje ukazuje stav: *Čeká na schválení* → po schválení hned *Probíhá*
(s ukazatelem a uplynulým časem) → *Hotovo* / *Selhalo* / *Zastaveno*.

Obrázky (přílohy i snímky obrazovky agenta) zůstávají v historii konverzace vždy, i když aktuální model
obrázky nevidí. Teprve při sestavení dotazu se pro model bez obrázků (Lite, nebo vypnuté *Model vidí
obrázky*) nahradí krátkou poznámkou. Modelu s obrázky se posílají jen **nejnovější 4 obrázky** z historie
o celkové velikosti nejvýše 360 000 znaků data URL (`agent.MAX_HISTORY_IMAGES`,
`MAX_HISTORY_IMAGE_CHARS`; šetří tokeny a vejde se do limitu požadavku serveru 768 KiB), starší se
nahradí poznámkou. Po přepnutí z Lite na Flash tak Flash uvidí poslední obrázky znovu.

Před každým dotazem se posloupnost volání nástrojů zkontroluje a opraví podle pravidel serveru
(`agent.repair_tool_sequence`): za voláním musí hned následovat právě jedna odpověď na každé volání;
chybějící odpověď (přerušená akce, uložení při přepnutí konverzace) se doplní náhradní, osiřelé
a zdvojené odpovědi se vynechají, volání nástroje, který model v dotazu nemá (zakázaný v nastavení,
neexistující), se převede na poznámku v textu. Rozbitá historie tak chat nikdy nezablokuje.

## Hlášení autorovi LuxOS

Když uživatel chce nahlásit chybu nebo nápad k LuxOS (nebo souhlasí, když to agent navrhne), agent zavolá
`send_feedback` (druh `bug`/`idea`/`other`, název, podrobný popis s kroky, očekávaným a skutečným chováním,
volitelně snímek obrazovky a anonymizované systémové údaje, volitelně e-mail pro odpověď). Systémová zpráva
modelu zakazuje odeslání bez výslovného souhlasu uživatele a vyžaduje text v jazyce uživatele.

Formát je stejný jako aplikace Zpětná vazba (`luxos-feedback`): `POST https://lux-ai.cz/api/feedback`,
hlavička `X-Lux-Client: luxos`, cookie účtu LuxAI (bez účtu anonymně), `app: "luxai"`. Sběr systémových
údajů a pravidla anonymizace (domovská složka, jméno uživatele/počítače, e-maily, IP/MAC, tokeny a dlouhé
klíče) jsou převzaté z `luxos-feedback` (`lsl/feedback.lsl`; test je s ním porovnává); stejně se
anonymizuje i text hlášení. Hlášení se sestaví **před** potvrzením a karta ukáže celý text, druh, účet/anonymně,
rozbalovací systémové údaje a snímek; po schválení se odešle přesně to. Nástroj se potvrzuje vždy
(neplatí „Vždy v této relaci“, automatické schvalování ani „povolit“ u sítě), přístup k síti = „nikdy“
ho zakáže, snímky obrazovky = „nikdy“ odešle hlášení bez snímku. Po odeslání karta ukáže ID hlášení;
limit serveru je 5 hlášení za 10 minut. Bez internetu (nebo při chybě serveru) karta nabídne
**Uložit a odeslat později** – hlášení se uloží do `~/.local/share/luxos/feedback-outbox` a odešle ho
aplikace Zpětná vazba při dalším spuštění.

## Spuštění ze zdrojů

```sh
cd ~/Projects/LuxAI
tools/build-lsl.sh app
tools/build-lsl.sh helpers
dbus-run-session -- ./build/luxai
```

Sestavení používá LSL 0.10.6 a vytvoří samostatné ELF soubory v `build/`; nic neinstaluje.
`dbus-run-session` zabrání předání dotazu dosavadní nainstalované instanci. Aplikace používá běžný
účet a historii. Běhové závislosti zahrnují GTK 3, GLib, libcurl, nástroje pro desktop a účet LuxOS.
Python a PyGObject slouží pouze vývojovým srovnávacím testům a exportu konstant. Původní
`luxai/` je ponecháno jako referenční implementace; nativní aplikace jej nespouští.

## Vzdálená relace (LuxOS Companion)

`luxai --remote-session` je bezhlavý režim pro Bluetooth most LuxOS Companion: JSON řádky na
stdin/stdout (`ready`, `request`, `cancel`, `delta`, `done`, `error`), stejný účet, nastavení a
bezpečnostní pravidla jako aplikace, jeden dotaz najednou, historie jen v paměti procesu.
Akce, které by v aplikaci vyžadovaly potvrzení (a příkazy jako správce i ovládání plochy), se
vzdáleně odmítnou s hláškou „Tato akce vyžaduje potvrzení na počítači“. Protokol a rozhodnutí
jsou v [docs/REMOTE-SESSION.md](docs/REMOTE-SESSION.md). Jen nativní verze.

## Data

- konverzace: `~/.local/share/luxai/conversations/*.json` (0600),
- obrázky z konverzací: `~/.local/share/luxai/images/<id konverzace>/` (0600; mažou se se smazáním
  konverzace nebo celé historie),
- nastavení: `~/.config/luxai/settings.json` (0600; starší `luxai-app.json` se při prvním spuštění převezme),
- snímky obrazovky: `~/.cache/luxai/screenshots/`.

## Překlady

Texty UI jsou anglicky a překládají se přes `_()`: nejdřív systémový
`/usr/share/luxos/i18n/<jazyk>.tsv` (balíček luxos-i18n), pak vlastní `i18n/<jazyk>.tsv`.

## Testy

```sh
tools/test-lsl.sh                         # nativní srovnávací a integrační kontroly
tools/build-lsl.sh candidate-check        # ověří také otisk právě testovaných zdrojů
```

Testy používají dočasné profily, syntetické účty a loopback API. GTK běží v odděleném Broadway displeji,
D-Bus test ve vlastní relaci; nepoužívají skutečnou obrazovku, účet ani nainstalovanou instanci.
Úspěšný běh zapíše `docs/lsl-verification.json` a `docs/lsl-test-report.txt`.
Původní Python testy v `tests/` slouží také jako referenční korpus.
`LUXAI_ORIGIN` přijímá jen `https://…` nebo `http://` na loopback.

## Balíček

```sh
tools/make-lsl-source.sh                  # zdrojový kandidát a PKGBUILD do build/candidate/
cd build/candidate
makepkg --nodeps --nocheck --nosign        # po úspěšných kontrolách výše; nic neinstaluje
```

Kandidát instaluje nativní `/usr/bin/luxai`, tři ELF pomocníky do `/usr/lib/luxai` a data do
`/usr/share/luxai`. Python moduly ani původní Python spouštěče nejsou součástí běhového balíčku.
Soubory v `packaging/` připravují změnu pro LuxOS-Arch; oficiální repozitář se tím nemění.
Podpis, instalace a publikování následují až po uživatelově ověření. `release-check` zůstává oddělený
od kontroly kandidáta a vyžaduje označení dokončeného vydávacího ověření v manifestu.
