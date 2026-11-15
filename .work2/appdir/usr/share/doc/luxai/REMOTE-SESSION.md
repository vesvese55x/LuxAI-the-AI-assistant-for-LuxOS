# `luxai --remote-session` – protokol pro most LuxOS Companion

Bezhlavý režim nativní LuxAI pro Bluetooth most LuxOS Companion (telefon posílá dotazy LuxAI
přes most na počítači). Implementace: `lsl/remote_session.lsl`, vstup v `lsl/main.lsl`.
Jen nativní LSL verze; Python reference (`luxai/`) tento režim nemá.

```sh
luxai --remote-session      # most spustí proces a mluví s ním přes stdin/stdout
```

- Nevytváří `GtkApplication` ani okno a nepotřebuje displej ani D-Bus. Dotaz se proto nikdy
  nepředá běžící desktopové instanci (a ta ho nemůže převzít).
- Používá stejný přihlášený účet (`~/.config/luxai/account.json`), nastavení
  (`settings.json`: model, styl, jazyk, délka odpovědi, pracovní složka, timeout, oprávnění
  agenta) a stejná bezpečnostní pravidla jako aplikace. Účet a nastavení se čtou znovu pro každý
  dotaz, takže přihlášení/odhlášení a změny v aplikaci platí hned. Žádná nová oprávnění; jediné
  rozšíření je jednorázový souhlas z telefonu tam, kde by aplikace ukázala potvrzovací kartu
  úrovně „ptát se“/„citlivé“ (viz níže).
- Běží pod uživatelem, který ho spustil; most ho musí spouštět pod přihlášeným uživatelem.

## Formát

Jeden JSON objekt na řádek, UTF-8, ukončený `\n`. stdin = most → LuxAI, stdout = LuxAI → most,
stderr jen pro logy. Řádek je nejvýše 256 KiB; neznámé klíče se ignorují.

### LuxAI → most

| Zpráva | Význam |
|---|---|
| `{"type":"ready","protocol":1,"capabilities":["models","tools","approvals"]}` | první řádek po spuštění; `capabilities` vyjmenuje jen implementovaná rozšíření |
| `{"type":"delta","reply_to":ID,"text":str}` | další kus textu odpovědi (jen text odpovědi, nikdy reasoning) |
| `{"type":"done","reply_to":ID}` | dotaz úspěšně dokončen |
| `{"type":"error","reply_to":ID\|null,"code":str,"message":str}` | dotaz skončil chybou (`reply_to` je `null`, pokud řádek nešel přiřadit) |
| `{"type":"models","reply_to":ID,"models":[{"id":str,"name":str}]}` | odpověď na `models` (schopnost `models`) |
| `{"type":"tool","reply_to":ID,"tool_id":str,"name":str,"state":"running"\|"completed"\|"failed"\|"waiting","detail":str}` | průběh skutečného volání nástroje (schopnost `tools`) |
| `{"type":"approval","reply_to":ID,"tool_id":str,"approval_id":UUID,"name":str,"detail":str,"timeout_ms":60000}` | nástroj čeká na jednorázový souhlas z telefonu (schopnost `approvals`) |

Každý přijatý `request` skončí právě jednou zprávou `done` nebo `error` se svým `reply_to`
(`tool` a `approval` jsou jen průběžné zprávy). `message` je text pro uživatele v jazyce prostředí
(`LANG`, překlady z `i18n/`). Pořadí klíčů v objektech není závazné.

### Most → LuxAI

| Zpráva | Význam |
|---|---|
| `{"type":"request","id":UUID,"text":str,"mode":"chat"\|"agent","model":str}` | nový dotaz; `id` je řetězec do 128 znaků, `text` 1–8000 znaků, `mode` výchozí `chat`; `model` je nepovinný (viz Modely) |
| `{"type":"models","id":UUID}` | seznam modelů dostupných přihlášenému účtu |
| `{"type":"approval","request_id":UUID,"approval_id":UUID,"approved":bool}` | odpověď na `approval` (viz Souhlas z telefonu) |
| `{"type":"cancel","request_id":UUID}` | co nejdřív zastavit běžící dotaz – stejná cesta jako tlačítko Stop v aplikaci (přeruší stream, ukončí skupinu procesů nástroje). Neznámé nebo už dokončené `request_id` se tiše ignoruje. |

Konec vstupu (EOF na stdin) zruší běžící dotaz (ten ještě dostane `error` `cancelled`, pokud most
stdout čte) a proces skončí s kódem 0.

### Chybové kódy

| `code` | Kdy |
|---|---|
| `busy` | už běží jiný dotaz (běží vždy nejvýše jeden) |
| `not_signed_in` | LuxAI není přihlášená nebo relace vypršela (HTTP 401/403) |
| `monthly_limit` | vyčerpaný měsíční limit (`flash_monthly_limit`, `monthly_limit`) |
| `rate_limited` | příliš mnoho dotazů nebo na účtu běží jiná konverzace (HTTP 429) |
| `network` | síťová chyba nebo timeout |
| `service_unavailable` | služba nedostupná, ochrana webu, HTML místo API, neplatná odpověď, 5xx |
| `too_large` | dotaz je pro server příliš velký |
| `model_error` | chyba modelu nebo odmítnutý chat |
| `cancelled` | dotaz zrušen (`cancel` nebo EOF) |
| `too_long` | `text` má víc než 8000 znaků |
| `model_unavailable` | `model` v dotazu není mezi modely ze zprávy `models` (dotaz se vůbec nepošle) |
| `invalid_request` | řádek není JSON objekt/UTF-8, chybí `id`/`text`, neplatný `mode`, `model` není řetězec, neplatná zpráva `approval` (chybí `request_id`/`approval_id` nebo `approved` není bool; `reply_to` je `null`), neznámý `type`, příliš dlouhý řádek |
| `internal` | jiná chyba |

## Modely

`{"type":"models","id":ID}` → `{"type":"models","reply_to":ID,"models":[{"id":"flash","name":"Flash – smarter"},{"id":"lite","name":"Lite – faster, larger limit"}]}`
– přesně stejná nabídka jako přepínač modelu v liště aplikace (názvy přeložené podle `LANG`,
česky „Flash – chytřejší“, „Lite – rychlejší, větší limit“), nejvýše 100 položek, žádné tajné
údaje. Bez přihlášení `error` `not_signed_in` s `reply_to` = ID. Odpovídá se hned i během běžícího
dotazu.

`request` s `"model":"lite"` použije pro tento dotaz přesně tento model (posílá se serveru jako
`model`); neznámé ID (rozlišují se velká/malá písmena, i prázdný řetězec) → `error`
`model_unavailable` a nic se neodešle, nikdy se tiše nepoužije jiný model. Bez `model` se použije
výchozí model z nastavení aplikace. Výběr z telefonu se do nastavení neukládá.

## Průběh nástrojů

Zprávy `tool` se posílají jen pro skutečná volání nástrojů (Chat i Agent), nikdy odhadem z textu
modelu a nikdy s reasoningem:

- `waiting` – nástroj čeká na souhlas z telefonu (hned po ní přijde `approval` se stejným
  `tool_id`, `name` a `detail`),
- `running` – nástroj se právě spouští (posuzovač ho povolil nebo ho uživatel schválil); `detail`
  je u `run_command` přesně spouštěný (schválený) příkaz, jinak název nástroje a předmět (cesta,
  URL, …) jako v kartě aplikace,
- `completed` – hotovo; `detail` u příkazů `Exit N`, jinak „Done.“ (přeloženo),
- `failed` – chyba nebo nespuštěno; `detail` je `Exit N`, první řádek chyby, nebo přeložený důvod:
  „Denied on the phone.“, „Not approved on the phone in time.“, „This action requires
  confirmation on the computer“ (nelze schválit z telefonu), „Blocked by LuxAI safety rules.“,
  „Stopped.“.

`tool_id` je ID volání (`call_…`, do 200 znaků), `name` interní název nástroje (`run_command`,
`write_file`, `calendar_create_event`, …, do 200 znaků), `detail` nejvýše 4000 znaků. Před odesláním
se z `detail` odstraní tajné údaje: přihlašovací token účtu, hlavičky `Authorization`, `Bearer …`,
hodnoty `password=`/`token=`/`secret=`/`api_key=`/`cookie=` apod., `--password …`, hesla v URL
(`https://user:heslo@`) a známé formáty klíčů (`sk-…`, `ghp_…`, `glpat-…`, `xox?-…`, `AKIA…`, JWT).
Nejvýše 100 různých `tool_id` na dotaz; další volání už zprávy `tool` nemají a nelze je schválit
z telefonu.

## Souhlas z telefonu

Když by aplikace ukázala potvrzovací kartu úrovně „ptát se“ (`ask`) nebo „citlivé“ (`confirm`),
běh se pozastaví a pošle se `tool` `waiting` a

```json
{"type":"approval","reply_to":"<request id>","tool_id":"call_1","approval_id":"<UUID v4>","name":"calendar_create_event","detail":"Add to calendar\ntitle: Schůzka\nstart: 2026-10-29T16:00","timeout_ms":60000}
```

`detail` je úplný popis akce: název nástroje a všechny argumenty (každý zkrácen na 1500 znaků,
celé na 4000, tajné údaje odstraněné). Most odpoví

```json
{"type":"approval","request_id":"<request id>","approval_id":"<UUID>","approved":true}
```

Pravidla:

- `approval_id` je náhodné UUID v4, svázané s běžícím dotazem (`request_id`), voláním nástroje
  (`tool_id`) a SHA-256 přesných argumentů nástroje. Těsně před spuštěním se otisk argumentů
  ověří znovu; kdyby se lišily, akce se nespustí.
- Přijme se právě jedna odpověď: jen pro čekající souhlas, se správným `request_id` i
  `approval_id`, před vypršením a dokud dotaz nebyl zrušen. Neznámé, cizí, pozdní, opakované nebo
  vypršelé odpovědi se **tiše ignorují** (jen řádek v stderr), stejně jako `cancel` s neznámým ID;
  na protokol se nic neposílá. Strukturálně neplatná zpráva → `error` `invalid_request` s
  `reply_to: null`.
- `approved:true` → nástroj se jednou spustí (`running`, pak `completed`/`failed`).
  `approved:false` → nespustí se, `failed` „Denied on the phone.“, model dostane výsledek
  `permission_denied` („The user denied this action in the LuxOS Companion phone app…“) a odpověď
  normálně doběhne (`done`).
- Bez odpovědi do `timeout_ms` (60 s) = zamítnuto: `failed` „Not approved on the phone in time.“,
  model dostane `permission_denied` s vysvětlením a běh pokračuje/končí normálně.
  (Testy mohou limit jen zkrátit proměnnou `LUXAI_REMOTE_APPROVAL_TIMEOUT_MS`, 200–60000 ms;
  prodloužit nad 60 s nejde.)
- `cancel` nebo EOF čekající souhlas zneplatní; dotaz skončí `error` `cancelled` a nic se nespustí.
- Každý souhlas platí jen pro jedno volání; „Vždy v této relaci“ se z telefonu nikdy neukládá.
- Během čekání na souhlas je proces obsazený (`busy` pro další `request`).

## Konverzace

Historie se drží jen v paměti po dobu běhu procesu (jedna konverzace, oba režimy se mohou střídat)
a **neukládá se** do historie desktopové aplikace: nevzniká soubor v
`~/.local/share/luxai/conversations`, nepřibude položka v postranním panelu a negeneruje se název.
Důvod: aplikace by o konverzaci za běhu nevěděla (nepřenačítá cizí zápisy) a dotazy z telefonu
zůstanou soukromé na telefonu. Nový proces = nová konverzace.

Model dostane navíc krátkou systémovou poznámku, že píše uživatel z telefonu přes LuxOS Companion,
vidí jen text odpovědi a stav nástrojů, akce vyžadující potvrzení schvaluje jednorázově na
telefonu a příkazy správce, ovládání plochy, zpětná vazba a nebezpečné akce se odmítnou. Text dotazu se vždy jen posílá
modelu jako zpráva uživatele; nikdy se sám nespouští jako příkaz.

## Agent a potvrzování

Nástroje posuzuje stejný posuzovač jako v aplikaci (`safety.judge`: kategorie a úrovně z
nastavení, blokované/povolené složky, tvrdé bloky, web-taint, nedůvěryhodný `web_fetch`).

- Nástroje, které aplikace spustí bez dotazu (`allow`, např. čtení souborů, výpis složky,
  `docs_*`, bezpečné příkazy v režimu „risky“), běží normálně (zprávy `tool`).
- Akce, u kterých by aplikace ukázala kartu úrovně `ask` nebo `confirm` (např. příkaz v režimu
  „ptát se“, zápis/úprava/smazání souboru v domovské složce, otevření aplikace/URL/Map, změny
  kalendáře, `web_fetch` adresy, kterou uživatel nezadal), se **jednorázově schvalují na telefonu**
  (viz Souhlas z telefonu). Mobilní souhlas neobchází posuzovač: co aplikace blokuje, zůstane
  blokované, a co je `danger`, schválit nejde.
- **Nikdy** se nenabídne ke schválení a odmítne se hned (model dostane `permission_denied` s textem
  „This action requires confirmation on the computer (Tato akce vyžaduje potvrzení na počítači)…“
  a pokynem nezkoušet to jinak, uživatel uvidí v odpovědi řádek
  `⚠ Tato akce vyžaduje potvrzení na počítači (nástroj: příkaz).` a `tool` `failed`):
  - příkaz jako správce: `root=true`, `sudo`/`pkexec`/`su`/`doas`/`run0` kdekoli v příkazu – ani
    v režimu správce „bez ptaní“ (`noask`),
  - ovládání plochy: myš, klávesnice, okna (`focus_window`, `close_window`), snímek obrazovky,
    schránka – ani při úrovni „povolit“ (na plochu z telefonu nikdo nevidí),
  - `send_feedback`,
  - vše, co posuzovač označí jako `danger` (např. mazání celé domovské složky, zápis mimo domov,
    vypnutí počítače).
- Kalendář LuxOS: `calendar_list_events` (jen čtení) běží z telefonu v Chatu i Agentovi.
  `calendar_create_event`, `calendar_update_event` a `calendar_delete_event` vyžadují vždy souhlas
  (i kdyby je posuzovač povolil); v Chatu se vůbec nenabízejí.
- „Vždy v této relaci“ se z telefonu nikdy nezapamatuje; auto-schvalování neexistuje.
- Bezpečnost procesu: protokol používá soukromé kopie stdin/stdout; fd 0 je přesměrovaný na
  `/dev/null` a fd 1 na stderr, takže žádný nástroj, knihovna ani spuštěná aplikace nemůže číst
  dotazy, posílat souhlasy ani psát do protokolu.

## Testy

`tests_lsl/test_remote_session.py` (součást `tools/test-lsl.sh`) spouští skutečný `luxai
--remote-session` proti loopback API v dočasném profilu: streamování a `done`, historie v paměti,
žádný reasoning, `busy`, `cancel` a pokračování, `not_signed_in` (bez účtu i s neplatnou relací),
neplatné řádky a `too_long`, odmítnutí příkazu vyžadujícího potvrzení (soubor nevznikne, česká
hláška), bezpečný nástroj běží, root/`sudo`/nebezpečný příkaz a vstup na plochu se odmítnou i při
`noask`/„povolit“, výpis kalendáře v Chatu i Agentovi funguje a jeho změny čekají na souhlas
(zamítnutí = soubor beze změny, schválení = událost přidána), EOF během dotazu i v klidu → kód 0.
Rozšíření: `ready` s `capabilities`, seznam modelů (i bez přihlášení), volba modelu
(`lite`/`flash`/výchozí z nastavení, `model_unavailable` bez odeslání dotazu), zprávy `tool`
(`running`/`completed`, `Exit 0`, odstranění tokenů/hesel/`Authorization`, limit 4000 znaků),
souhlas schválit/zamítnout, vypršení (zkrácený limit, pozdní odpověď ignorována, limit nejde
prodloužit nad 60 s), opakovaná/cizí/špatná `approval_id` a `request_id`, zrušení a EOF během
čekání, root/`sudo`/`pkexec`/`su`, vstup, okna, snímek, schránka a `send_feedback` se nikdy
nenabídnou ke schválení.
