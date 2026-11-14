#define _RICHEDIT_VER 0x0410
#include "luxai.h"
#include <commctrl.h>
#include <richedit.h>
#include <shlobj.h>
#include <shlwapi.h>

HWND g_hMain, g_hList, g_hRich, g_hInput, g_hSend, g_hStatus,
     g_hModel, g_hMode, g_hNew, g_hSettings, g_hAcct, g_hStop;
MsgList g_conv;

static HINSTANCE g_inst;
static HFONT g_font;
static HBRUSH g_bg;
static int  g_dark;
static DWORD g_ui_thread;

static wchar_t g_conv_id[64];
static int     g_busy_ui;

/* ---------- renkler ---------- */
typedef struct { COLORREF bg, fg, dim, accent, card, code, border; } Theme;
static Theme TH;

static void theme_apply(void)
{
    int dark = (g_cfg.theme == 2) || (g_cfg.theme == 0 && 0);
    g_dark = dark;
    if (dark) {
        TH.bg = RGB(0x16, 0x18, 0x1d); TH.fg = RGB(0xe8, 0xea, 0xf0);
        TH.dim = RGB(0x9a, 0xa1, 0xb2); TH.accent = RGB(0x6f, 0x9c, 0xff);
        TH.card = RGB(0x1e, 0x21, 0x28); TH.code = RGB(0x24, 0x28, 0x31);
        TH.border = RGB(0x2c, 0x31, 0x3c);
    } else {
        TH.bg = RGB(0xff, 0xff, 0xff); TH.fg = RGB(0x15, 0x18, 0x1e);
        TH.dim = RGB(0x6b, 0x72, 0x83); TH.accent = RGB(0x3d, 0x6b, 0xff);
        TH.card = RGB(0xf6, 0xf7, 0xfa); TH.code = RGB(0xf1, 0xf3, 0xf7);
        TH.border = RGB(0xdf, 0xe3, 0xea);
    }
    if (g_bg) DeleteObject(g_bg);
    g_bg = CreateSolidBrush(TH.bg);
}

/* ---------- yardimcilar ---------- */
static HWND mk(const wchar_t *cls, const wchar_t *txt, DWORD st, int x, int y,
               int w, int h, int id, HWND parent)
{
    HWND hw = CreateWindowExW(0, cls, txt, st, x, y, w, h, parent, (HMENU)(INT_PTR)id,
                              g_inst, NULL);
    if (hw && g_font) SendMessageW(hw, WM_SETFONT, (WPARAM)g_font, TRUE);
    return hw;
}

static wchar_t *get_text(HWND h)
{
    int n = GetWindowTextLengthW(h);
    wchar_t *w = (wchar_t *)calloc((size_t)n + 2, sizeof(wchar_t));
    if (!w) return NULL;
    GetWindowTextW(h, w, n + 1);
    return w;
}

/* ---------- rich edit ---------- */
static void cf_init(CHARFORMAT2W *cf, int bold, int italic, int mono, COLORREF col,
                    int pt, int back)
{
    ZeroMemory(cf, sizeof *cf);
    cf->cbSize = sizeof *cf;
    cf->dwMask = CFM_BOLD | CFM_ITALIC | CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf->dwEffects = (bold ? CFE_BOLD : 0) | (italic ? CFE_ITALIC : 0);
    cf->yHeight = (LONG)(pt * 20);
    wcscpy(cf->szFaceName, mono ? L"Consolas" : L"Segoe UI");
    cf->crTextColor = col;
    if (back) {
        cf->dwMask |= CFM_BACKCOLOR;
        cf->crBackColor = TH.code;
        cf->dwEffects &= ~CFE_AUTOBACKCOLOR;
    }
}

static void rich_ins(HWND h, const wchar_t *s, CHARFORMAT2W *cf)
{
    if (!s || !*s) return;
    SendMessageW(h, EM_SETREADONLY, FALSE, 0);
    int len = GetWindowTextLengthW(h);
    SendMessageW(h, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    if (cf) SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)cf);
    SendMessageW(h, EM_REPLACESEL, FALSE, (LPARAM)s);
    SendMessageW(h, EM_SETREADONLY, TRUE, 0);
}

static void rich_scroll_end(HWND h)
{
    int len = GetWindowTextLengthW(h);
    SendMessageW(h, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(h, EM_SCROLLCARET, 0, 0);
}

/* basit markdown: basliklar, kalin, italik, satir ici kod, kod bloklari, madde isaretleri */
static void render_inline(HWND h, const wchar_t *line, int pt)
{
    wchar_t run[2048];
    int rn = 0;
    int bold = 0, italic = 0, mono = 0;
    CHARFORMAT2W cf;
    for (int i = 0; line[i]; ) {
        if (line[i] == L'*' && line[i + 1] == L'*') {
            if (rn) { run[rn] = 0; cf_init(&cf, bold, italic, mono, TH.fg, pt, mono); rich_ins(h, run, &cf); rn = 0; }
            bold = !bold; i += 2; continue;
        }
        if (line[i] == L'`') {
            if (rn) { run[rn] = 0; cf_init(&cf, bold, italic, mono, TH.fg, pt, mono); rich_ins(h, run, &cf); rn = 0; }
            mono = !mono; i++; continue;
        }
        if (line[i] == L'_' && line[i + 1] == L'_') {
            if (rn) { run[rn] = 0; cf_init(&cf, bold, italic, mono, TH.fg, pt, mono); rich_ins(h, run, &cf); rn = 0; }
            italic = !italic; i += 2; continue;
        }
        if (line[i] == L'[') {                     /* [metin](adres) -> metin (adres) */
            const wchar_t *close = wcschr(line + i, L']');
            if (close && close[1] == L'(') {
                const wchar_t *end = wcschr(close + 2, L')');
                if (end) {
                    int n = (int)(close - (line + i + 1));
                    if (rn + n + 300 < 2040) {
                        for (int k = 0; k < n; k++) run[rn++] = line[i + 1 + k];
                        run[rn++] = L' ';
                        run[rn++] = L'(';
                        int m = (int)(end - (close + 2));
                        for (int k = 0; k < m && rn < 2000; k++) run[rn++] = close[2 + k];
                        run[rn++] = L')';
                    }
                    i = (int)(end - line) + 1;
                    continue;
                }
            }
        }
        if (rn < 2040) run[rn++] = line[i];
        i++;
    }
    if (rn) { run[rn] = 0; cf_init(&cf, bold, italic, mono, TH.fg, pt, mono); rich_ins(h, run, &cf); }
}

static void render_md(HWND h, const wchar_t *text)
{
    CHARFORMAT2W cf;
    const wchar_t *p = text;
    int in_code = 0;
    while (p && *p) {
        const wchar_t *nl = wcschr(p, L'\n');
        size_t len = nl ? (size_t)(nl - p) : wcslen(p);
        wchar_t line[4096];
        size_t n = len < 4095 ? len : 4095;
        memcpy(line, p, n * sizeof(wchar_t));
        line[n] = 0;
        if (line[0] && line[n - 1] == L'\r') line[n - 1] = 0;

        if (!wcsncmp(line, L"```", 3)) {
            in_code = !in_code;
            if (in_code && n > 3) {
                /* dil etiketi (```python) sadece kucuk bir ipucu olarak gosterilir */
                cf_init(&cf, 0, 1, 1, TH.dim, 8, 0);
                rich_ins(h, line + 3, &cf);
                rich_ins(h, L"\n", &cf);
            }
        } else if (in_code) {
            cf_init(&cf, 0, 0, 1, TH.fg, 9, 1);
            rich_ins(h, line, &cf);
            rich_ins(h, L"\n", &cf);
        } else if (line[0] == L'#') {
            int lvl = 0;
            while (line[lvl] == L'#') lvl++;
            const wchar_t *t = line + lvl;
            while (*t == L' ') t++;
            int pt = lvl == 1 ? 16 : lvl == 2 ? 14 : 12;
            cf_init(&cf, 1, 0, 0, TH.fg, pt, 0);
            rich_ins(h, t, &cf);
            cf_init(&cf, 0, 0, 0, TH.fg, 10, 0);
            rich_ins(h, L"\n", &cf);
        } else if ((line[0] == L'-' || line[0] == L'*') && line[1] == L' ') {
            cf_init(&cf, 0, 0, 0, TH.fg, 10, 0);
            rich_ins(h, L"\u2022  ", &cf);
            render_inline(h, line + 2, 10);
            rich_ins(h, L"\n", &cf);
        } else if (line[0] == L'>') {
            cf_init(&cf, 0, 1, 0, TH.dim, 10, 0);
            rich_ins(h, line + 1, &cf);
            rich_ins(h, L"\n", &cf);
        } else if (line[0] == L'|') {
            cf_init(&cf, 0, 0, 1, TH.fg, 9, 1);
            rich_ins(h, line, &cf);
            rich_ins(h, L"\n", &cf);
        } else {
            render_inline(h, line, 10);
            cf_init(&cf, 0, 0, 0, TH.fg, 10, 0);
            rich_ins(h, L"\n", &cf);
        }
        if (!nl) break;
        p = nl + 1;
    }
}

static void rich_clear(void)
{
    SendMessageW(g_hRich, EM_SETREADONLY, FALSE, 0);
    SetWindowTextW(g_hRich, L"");
    CHARFORMAT2W cf;
    cf_init(&cf, 0, 0, 0, TH.fg, 10, 0);
    SendMessageW(g_hRich, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

static void append_role_label(const wchar_t *label, COLORREF col)
{
    CHARFORMAT2W cf;
    cf_init(&cf, 1, 0, 0, col, 9, 0);
    rich_ins(g_hRich, label, &cf);
    cf_init(&cf, 0, 0, 0, TH.fg, 10, 0);
    rich_ins(g_hRich, L"\n", &cf);
}

/* ---------- conversation ---------- */
static void conv_dir(wchar_t *out, size_t cch)
{
    path_appdata(out, cch, L"conversations");
    mkdir_p(out);
}

static void new_conv_id(void)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf(g_conv_id, 64, L"%04d%02d%02d-%02d%02d%02d-%03d", st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond, (int)(GetTickCount() % 1000));
}

void ui_conversation_clear(void)
{
    msglist_clear(&g_conv);
    new_conv_id();
    if (g_hRich) { rich_clear(); ui_render_all(); }
}

static void conv_title(wchar_t *out, size_t cch)
{
    out[0] = 0;
    for (int i = 0; i < g_conv.n; i++) {
        if (!strcmp(g_conv.v[i].role, "user") && g_conv.v[i].content && *g_conv.v[i].content) {
            wchar_t *w = u16(g_conv.v[i].content);
            if (w) {
                size_t j = 0;
                for (; w[j] && j < cch - 1 && j < 48; j++) {
                    out[j] = (w[j] == L'\n' || w[j] == L'\r') ? L' ' : w[j];
                }
                out[j] = 0;
                free(w);
            }
            return;
        }
    }
    wcsncpy(out, tw(S_NEW_CONV), cch - 1);
}

void ui_conversation_save(void)
{
    if (!g_conv.n) return;
    wchar_t dir[MAX_PATH], path[MAX_PATH * 2];
    conv_dir(dir, MAX_PATH);
    _snwprintf(path, MAX_PATH * 2, L"%s\\%s.json", dir, g_conv_id);
    Buf b; buf_init(&b);
    buf_adds(&b, "{\"id\":");
    char *id8 = u8(g_conv_id);
    json_write_str(&b, id8);
    free(id8);
    wchar_t title[128];
    conv_title(title, 128);
    char *t8 = u8(title);
    buf_adds(&b, ",\"title\":");
    json_write_str(&b, t8);
    free(t8);
    buf_adds(&b, ",\"client\":\"luxai-win\",\"messages\":[");
    for (int i = 0; i < g_conv.n; i++) {
        if (i) buf_addc(&b, ',');
        buf_adds(&b, "{\"role\":");
        json_write_str(&b, g_conv.v[i].role);
        buf_adds(&b, ",\"content\":");
        json_write_str(&b, g_conv.v[i].content);
        if (g_conv.v[i].tool_calls) {
            buf_adds(&b, ",\"tool_calls\":");
            json_write_str(&b, g_conv.v[i].tool_calls);
        }
        if (g_conv.v[i].tool_call_id) {
            buf_adds(&b, ",\"tool_call_id\":");
            json_write_str(&b, g_conv.v[i].tool_call_id);
        }
        buf_addc(&b, '}');
    }
    buf_adds(&b, "]}");
    file_write_utf8(path, b.p, b.n);
    buf_free(&b);
}

static void conv_load(const wchar_t *id)
{
    wchar_t dir[MAX_PATH], path[MAX_PATH * 2];
    conv_dir(dir, MAX_PATH);
    _snwprintf(path, MAX_PATH * 2, L"%s\\%s.json", dir, id);
    char *d = file_read_utf8(path);
    if (!d) return;
    JVal *v = json_parse(d);
    free(d);
    if (!v) return;
    msglist_clear(&g_conv);
    wcsncpy(g_conv_id, id, 63);
    JVal *msgs = json_get(v, "messages");
    for (int i = 0; msgs && i < msgs->n; i++) {
        JVal *m = json_at(msgs, i);
        if (!m) continue;
        msglist_add(&g_conv, json_str(m, "role"), json_str(m, "content"),
                    json_str(m, "tool_calls"), json_str(m, "tool_call_id"));
    }
    json_free(v);
    ui_render_all();
}

/* ---------- sidebar ---------- */
typedef struct { wchar_t id[64], title[128]; FILETIME ft; } SideEntry;
static SideEntry side[256];
static int side_n;

static int side_cmp(const void *a, const void *b)
{
    const SideEntry *x = (const SideEntry *)a, *y = (const SideEntry *)b;
    return CompareFileTime(&y->ft, &x->ft);
}

void ui_sidebar_reload(void)
{
    if (!g_hList) return;
    wchar_t dir[MAX_PATH], pat[MAX_PATH * 2];
    conv_dir(dir, MAX_PATH);
    _snwprintf(pat, MAX_PATH * 2, L"%s\\*.json", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    side_n = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (side_n >= 256) break;
            wcsncpy(side[side_n].id, fd.cFileName, 63);
            side[side_n].id[63] = 0;
            wchar_t *dot = wcsrchr(side[side_n].id, L'.');
            if (dot) *dot = 0;
            side[side_n].ft = fd.ftLastWriteTime;
            side[side_n].title[0] = 0;
            /* basligi dosyadan oku */
            wchar_t p[MAX_PATH * 2];
            _snwprintf(p, MAX_PATH * 2, L"%s\\%s", dir, fd.cFileName);
            char *d = file_read_utf8(p);
            if (d) {
                JVal *v = json_parse(d);
                const char *tt = json_str(v, "title");
                if (tt) {
                    wchar_t *w = u16(tt);
                    if (w) { wcsncpy(side[side_n].title, w, 127); free(w); }
                }
                json_free(v);
                free(d);
            }
            if (!side[side_n].title[0]) wcsncpy(side[side_n].title, L"…", 8);
            side_n++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    qsort(side, (size_t)side_n, sizeof(SideEntry), side_cmp);

    SendMessageW(g_hList, LB_RESETCONTENT, 0, 0);
    int sel = -1;
    for (int i = 0; i < side_n; i++) {
        SendMessageW(g_hList, LB_ADDSTRING, 0, (LPARAM)side[i].title);
        if (!wcscmp(side[i].id, g_conv_id)) sel = i;
    }
    if (sel >= 0) SendMessageW(g_hList, LB_SETCURSEL, (WPARAM)sel, 0);
}

static void sidebar_select(int idx)
{
    if (idx < 0 || idx >= side_n) return;
    if (wcscmp(side[idx].id, g_conv_id)) {
        ui_conversation_save();
        conv_load(side[idx].id);
    }
}

/* ---------- render ---------- */
void ui_render_all(void)
{
    if (!g_hRich) return;
    rich_clear();
    if (!g_conv.n) {
        CHARFORMAT2W cf;
        cf_init(&cf, 1, 0, 0, TH.fg, 18, 0);
        rich_ins(g_hRich, tw(S_EMPTY_TITLE), &cf);
        rich_ins(g_hRich, L"\n\n", &cf);
        cf_init(&cf, 0, 0, 0, TH.dim, 10, 0);
        rich_ins(g_hRich, tw(S_EMPTY_TEXT), &cf);
        rich_ins(g_hRich, L"\n\n", &cf);
        cf_init(&cf, 0, 0, 0, TH.accent, 9, 0);
        rich_ins(g_hRich, tw(S_ABOUT), &cf);
        rich_scroll_end(g_hRich);
        return;
    }
    for (int i = 0; i < g_conv.n; i++) {
        Msg *m = &g_conv.v[i];
        if (!strcmp(m->role, "user")) {
            append_role_label(tw(S_YOU), TH.accent);
            CHARFORMAT2W cf;
            cf_init(&cf, 0, 0, 0, TH.fg, 10, 0);
            wchar_t *w = u16(m->content);
            rich_ins(g_hRich, w ? w : L"", &cf);
            free(w);
            rich_ins(g_hRich, L"\n\n", &cf);
        } else if (!strcmp(m->role, "assistant")) {
            append_role_label(L"LuxAI", TH.dim);
            if (m->content && *m->content) {
                wchar_t *w = u16(m->content);
                render_md(g_hRich, w ? w : L"");
                free(w);
            }
            if (m->tool_calls && *m->tool_calls) {
                CHARFORMAT2W cf;
                cf_init(&cf, 0, 1, 1, TH.dim, 8, 1);
                rich_ins(g_hRich, L"\u2699 ", &cf);
                {
                    /* sadece arac adlarini goster */
                    JVal *arr = json_parse(m->tool_calls);
                    for (int k = 0; arr && k < arr->n; k++) {
                        JVal *fn = json_get(json_at(arr, k), "function");
                        const char *nm = fn ? json_str(fn, "name") : NULL;
                        if (nm) {
                            wchar_t *wn = u16(nm);
                            rich_ins(g_hRich, wn ? wn : L"", &cf);
                            rich_ins(g_hRich, L"  ", &cf);
                            free(wn);
                        }
                    }
                    json_free(arr);
                }
                rich_ins(g_hRich, L"\n", &cf);
            }
            CHARFORMAT2W cf2;
            cf_init(&cf2, 0, 0, 0, TH.fg, 10, 0);
            rich_ins(g_hRich, L"\n", &cf2);
        } else if (!strcmp(m->role, "tool")) {
            CHARFORMAT2W cf;
            cf_init(&cf, 0, 0, 1, TH.dim, 8, 1);
            wchar_t *w = u16(m->content);
            wchar_t small[1200];
            wcsncpy(small, w ? w : L"", 1199);
            small[1199] = 0;
            if (wcslen(w ? w : L"") > 1199) wcscat(small, L"\n…");
            rich_ins(g_hRich, L"  \u21b3 ", &cf);
            rich_ins(g_hRich, small, &cf);
            rich_ins(g_hRich, L"\n\n", &cf);
            free(w);
        }
    }
    rich_scroll_end(g_hRich);
}

void ui_append_stream(const wchar_t *text)
{
    if (!g_hRich || !text) return;
    CHARFORMAT2W cf;
    cf_init(&cf, 0, 0, 0, TH.fg, 10, 0);
    rich_ins(g_hRich, text, &cf);
    rich_scroll_end(g_hRich);
}

void ui_render_label(const wchar_t *role_utf8)
{
    (void)role_utf8;
}

void ui_set_status(const char *utf8)
{
    if (!g_hStatus) return;
    wchar_t *w = u16(utf8);
    SetWindowTextW(g_hStatus, w ? w : L"");
    free(w);
}

void ui_post_report(const char *utf8) { ui_set_status(utf8); }

/* ---------- onay ---------- */
int ui_confirm(const wchar_t *title, const wchar_t *detail, int twice)
{
    if (g_cfg.auto_approve && !twice) return 1;
    if (GetCurrentThreadId() != g_ui_thread)
        return (int)SendMessageW(g_hMain, WM_APP_CONFIRM, (WPARAM)title, (LPARAM)detail) == 1;
    int r = MessageBoxW(g_hMain, detail, title, MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES;
    if (r && twice)
        r = MessageBoxW(g_hMain, detail, title, MB_YESNO | MB_ICONWARNING | MB_TOPMOST) == IDYES;
    return r;
}

/* ---------- giris penceresi ---------- */
typedef struct { HWND hEmail, hPass, hMsg; int done, ok; } LoginD;

typedef struct { Buf *b; } RespCtx;
static int resp_sink(const char *d, size_t n, void *ud)
{
    RespCtx *c = (RespCtx *)ud;
    buf_add(c->b, d, n);
    return 0;
}

static int do_login(const wchar_t *email, const wchar_t *pass, wchar_t *err, size_t errcch)
{
    Buf body; buf_init(&body);
    char *e8 = u8(email), *p8 = u8(pass);
    buf_adds(&body, "{\"email\":");
    json_write_str(&body, e8 ? e8 : "");
    buf_adds(&body, ",\"password\":");
    json_write_str(&body, p8 ? p8 : "");
    buf_addc(&body, '}');
    free(e8); free(p8);

    Buf resp; buf_init(&resp);
    Buf errb; buf_init(&errb);
    int status = 0;
    volatile LONG cancel = 0;
    RespCtx ctx = { &resp };
    http_request(L"https://lux-ai.cz/api/auth/password/login",
                 L"Content-Type: application/json; charset=utf-8\r\nX-Lux-Client: luxai\r\n",
                 body.p, body.n, resp_sink, &ctx, &status, &errb, &cancel);
    buf_free(&body);

    int ok = 0;
    if (status == 200 && resp.p) {
        JVal *v = json_parse(resp.p);
        const char *tok = json_str(v, "sessionToken");
        if (tok) {
            wchar_t *wt = u16(tok);
            if (wt) { wcsncpy(g_token, wt, 1023); free(wt); }
            JVal *u = json_get(v, "user");
            if (u) {
                const char *em = json_str(u, "email");
                const char *nm = json_str(u, "name");
                const char *un = json_str(u, "username");
                if (em) { wchar_t *w = u16(em); if (w) { wcsncpy(g_email, w, 255); free(w); } }
                if (un) { wchar_t *w = u16(un); if (w) { wcsncpy(g_username, w, 127); free(w); } }
                else if (nm) { wchar_t *w = u16(nm); if (w) { wcsncpy(g_username, w, 127); free(w); } }
            }
            account_save();
            ok = 1;
        } else {
            const char *code = json_str(v, "error");
            if (code) {
                wchar_t *w = u16(!strcmp(code, "invalid_credentials") ? t(S_BAD_CREDS) : code);
                if (w) { wcsncpy(err, w, errcch - 1); err[errcch - 1] = 0; free(w); }
            }
        }
        json_free(v);
    } else if (status == 401 || status == 403) {
        wcsncpy(err, tw(S_BAD_CREDS), errcch - 1);
    } else if (status == 0) {
        wcsncpy(err, tw(S_NET_ERR), errcch - 1);
    } else {
        _snwprintf(err, errcch, L"HTTP %d", status);
        err[errcch - 1] = 0;
    }
    buf_free(&resp);
    buf_free(&errb);
    return ok;
}

static LRESULT CALLBACK login_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    LoginD *d = (LoginD *)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == 1 && d) {
            wchar_t *em = get_text(d->hEmail), *pw = get_text(d->hPass);
            if (!em || !pw || !*em || !*pw) {
                SetWindowTextW(d->hMsg, tw(S_BAD_CREDS));
                free(em); free(pw);
                return 0;
            }
            SetWindowTextW(d->hMsg, tw(S_THINKING));
            SetCursor(LoadCursorW(NULL, IDC_WAIT));
            wchar_t err[300] = L"";
            int ok = do_login(em, pw, err, 300);
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
            free(em); free(pw);
            if (ok) { d->ok = 1; d->done = 1; DestroyWindow(h); }
            else SetWindowTextW(d->hMsg, err[0] ? err : tw(S_LOGIN_FAIL));
            return 0;
        }
        if (id == 2) {
            ShellExecuteW(NULL, L"open", L"https://lux-ai.cz/register", NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }
        if (id == 3 || id == IDCANCEL) { if (d) { d->done = 1; d->ok = 0; } DestroyWindow(h); return 0; }
        break;
    }
    case WM_CLOSE:
        if (d) { d->done = 1; d->ok = 0; }
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static int show_login(void)
{
    static int reg = 0;
    if (!reg) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = login_proc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"LuxLogin";
        RegisterClassExW(&wc);
        reg = 1;
    }
    LoginD d;
    ZeroMemory(&d, sizeof d);
    RECT mr; GetWindowRect(g_hMain, &mr);
    int W = 460, H = 290;
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"LuxLogin", tw(S_LOGIN_TITLE),
                             WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             mr.left + ((mr.right - mr.left) - W) / 2,
                             mr.top + ((mr.bottom - mr.top) - H) / 2,
                             W, H, g_hMain, NULL, g_inst, NULL);
    if (!h) return 0;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)&d);
    mk(L"STATIC", tw(S_LOGIN_HINT), WS_CHILD | WS_VISIBLE, 18, 12, W - 40, 34, 100, h);
    mk(L"STATIC", tw(S_EMAIL), WS_CHILD | WS_VISIBLE, 18, 56, 100, 20, 101, h);
    d.hEmail = mk(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  18, 78, W - 60, 26, 110, h);
    mk(L"STATIC", tw(S_PASSWORD), WS_CHILD | WS_VISIBLE, 18, 114, 100, 20, 102, h);
    d.hPass = mk(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
                 18, 136, W - 60, 26, 111, h);
    d.hMsg = mk(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 18, 172, W - 40, 34, 103, h);
    mk(L"BUTTON", tw(S_LOGIN), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 18, H - 78, 120, 30, 1, h);
    mk(L"BUTTON", tw(S_REGISTER), WS_CHILD | WS_VISIBLE, 146, H - 78, 170, 30, 2, h);
    mk(L"BUTTON", tw(S_CANCEL), WS_CHILD | WS_VISIBLE, W - 130, H - 78, 100, 30, 3, h);
    ShowWindow(h, SW_SHOW);
    SetFocus(d.hEmail);
    EnableWindow(g_hMain, FALSE);
    MSG msg;
    while (IsWindow(h) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(g_hMain, TRUE);
    SetForegroundWindow(g_hMain);
    return d.ok;
}

/* ---------- ayarlar penceresi ---------- */
typedef struct {
    HWND hModel, hMode, hLang, hTheme, hSendKey, hMaxTok, hWork, hAuto, hOpenAI, hEndpoint, hKey;
    int done;
} SetD;

static int CALLBACK browse_cb(HWND h, UINT m, LPARAM l, LPARAM d)
{
    (void)h; (void)m; (void)l; (void)d;
    return 0;
}

static LRESULT CALLBACK settings_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    SetD *d = (SetD *)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == 1 && d) {
            g_cfg.model = (int)SendMessageW(d->hModel, CB_GETCURSEL, 0, 0);
            g_cfg.mode = (int)SendMessageW(d->hMode, CB_GETCURSEL, 0, 0);
            g_cfg.lang = (int)SendMessageW(d->hLang, CB_GETCURSEL, 0, 0);
            g_cfg.theme = (int)SendMessageW(d->hTheme, CB_GETCURSEL, 0, 0);
            g_cfg.send_ctrl_enter = (int)SendMessageW(d->hSendKey, CB_GETCURSEL, 0, 0);
            g_cfg.auto_approve = (int)SendMessageW(d->hAuto, BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_cfg.use_openai = (int)SendMessageW(d->hOpenAI, BM_GETCHECK, 0, 0) == BST_CHECKED;
            wchar_t *mt = get_text(d->hMaxTok);
            if (mt) { int v = _wtoi(mt); if (v >= 256) g_cfg.max_tokens = v; free(mt); }
            wchar_t *wd = get_text(d->hWork);
            if (wd && *wd) { wcsncpy(g_cfg.workdir, wd, MAX_PATH - 1); }
            free(wd);
            wchar_t *ep = get_text(d->hEndpoint);
            if (ep) { char *e = u8(ep); if (e) { strncpy(g_cfg.endpoint, e, sizeof g_cfg.endpoint - 1); free(e); } free(ep); }
            wchar_t *ak = get_text(d->hKey);
            if (ak) { char *k = u8(ak); if (k) { strncpy(g_cfg.api_key, k, sizeof g_cfg.api_key - 1); free(k); } free(ak); }
            cfg_save();
            theme_apply();
            SendMessageW(g_hRich, EM_SETBKGNDCOLOR, 0, (LPARAM)TH.bg);
            InvalidateRect(g_hMain, NULL, TRUE);
            d->done = 1;
            DestroyWindow(h);
            return 0;
        }
        if (id == 2) { d->done = 1; DestroyWindow(h); return 0; }
        if (id == 3 && d) {
            BROWSEINFOW bi;
            ZeroMemory(&bi, sizeof bi);
            bi.hwndOwner = h;
            bi.pszDisplayName = g_cfg.workdir;
            bi.lpszTitle = tw(S_WORKDIR);
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            bi.lpfn = browse_cb;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                SHGetPathFromIDListW(pidl, g_cfg.workdir);
                SetWindowTextW(d->hWork, g_cfg.workdir);
                CoTaskMemFree(pidl);
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        if (d) d->done = 1;
        DestroyWindow(h);
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)w, GetSysColor(COLOR_BTNFACE));
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    return DefWindowProcW(h, m, w, l);
}

static void combo_add(HWND c, const wchar_t *s) { SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s); }

static void show_settings(void)
{
    static int reg = 0;
    if (!reg) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = settings_proc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"LuxSettings";
        RegisterClassExW(&wc);
        reg = 1;
    }
    SetD d;
    ZeroMemory(&d, sizeof d);
    int W = 560, H = 560;
    RECT mr; GetWindowRect(g_hMain, &mr);
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"LuxSettings", tw(S_SETTINGS),
                             WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             mr.left + ((mr.right - mr.left) - W) / 2,
                             mr.top + ((mr.bottom - mr.top) - H) / 2,
                             W, H, g_hMain, NULL, g_inst, NULL);
    if (!h) return;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)&d);

    int x1 = 20, x2 = 190, fw = W - x2 - 40, y = 18, rh = 32;
#define LBL(id_, txt_) mk(L"STATIC", txt_, WS_CHILD | WS_VISIBLE, x1, y + 6, 160, 20, id_, h)
#define NEXT() y += rh

    LBL(200, tw(S_MODEL));
    d.hModel = mk(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                  x2, y, 180, 200, 210, h);
    combo_add(d.hModel, L"Flash"); combo_add(d.hModel, L"Lite");
    SendMessageW(d.hModel, CB_SETCURSEL, g_cfg.model, 0);
    NEXT();

    LBL(201, tw(S_MODE));
    d.hMode = mk(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                 x2, y, 180, 200, 211, h);
    combo_add(d.hMode, tw(S_CHAT)); combo_add(d.hMode, tw(S_AGENT));
    SendMessageW(d.hMode, CB_SETCURSEL, g_cfg.mode, 0);
    NEXT();

    LBL(202, tw(S_LANGUAGE));
    d.hLang = mk(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                 x2, y, 180, 200, 212, h);
    combo_add(d.hLang, L"Türkçe"); combo_add(d.hLang, L"English");
    SendMessageW(d.hLang, CB_SETCURSEL, g_cfg.lang, 0);
    NEXT();

    LBL(203, tw(S_THEME));
    d.hTheme = mk(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                  x2, y, 180, 200, 213, h);
    combo_add(d.hTheme, tw(S_THEME_SYS)); combo_add(d.hTheme, tw(S_THEME_LIGHT)); combo_add(d.hTheme, tw(S_THEME_DARK));
    SendMessageW(d.hTheme, CB_SETCURSEL, g_cfg.theme, 0);
    NEXT();

    LBL(204, tw(S_SENDKEY));
    d.hSendKey = mk(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                    x2, y, 180, 200, 214, h);
    combo_add(d.hSendKey, L"Enter"); combo_add(d.hSendKey, L"Ctrl+Enter");
    SendMessageW(d.hSendKey, CB_SETCURSEL, g_cfg.send_ctrl_enter, 0);
    NEXT();

    LBL(205, tw(S_MAXTOK));
    d.hMaxTok = mk(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
                   x2, y, 110, 24, 215, h);
    { wchar_t b[16]; _snwprintf(b, 16, L"%d", g_cfg.max_tokens); SetWindowTextW(d.hMaxTok, b); }
    NEXT();

    LBL(206, tw(S_WORKDIR));
    d.hWork = mk(L"EDIT", g_cfg.workdir, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                 x2, y, fw - 90, 24, 216, h);
    mk(L"BUTTON", tw(S_BROWSE), WS_CHILD | WS_VISIBLE, x2 + fw - 84, y, 84, 24, 3, h);
    NEXT();

    d.hAuto = mk(L"BUTTON", tw(S_AUTOAPPROVE), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                 x1, y, W - 60, 24, 217, h);
    SendMessageW(d.hAuto, BM_SETCHECK, g_cfg.auto_approve ? BST_CHECKED : BST_UNCHECKED, 0);
    NEXT();

    d.hOpenAI = mk(L"BUTTON", tw(S_OPENAI), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                   x1, y, W - 60, 24, 218, h);
    SendMessageW(d.hOpenAI, BM_SETCHECK, g_cfg.use_openai ? BST_CHECKED : BST_UNCHECKED, 0);
    NEXT();

    LBL(207, tw(S_ENDPOINT));
    d.hEndpoint = mk(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                     x1, y, W - 60, 24, 219, h);
    { wchar_t *w = u16(g_cfg.endpoint); SetWindowTextW(d.hEndpoint, w ? w : L""); free(w); }
    NEXT();

    LBL(208, tw(S_APIKEY));
    d.hKey = mk(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
                x1, y, W - 60, 24, 220, h);
    { wchar_t *w = u16(g_cfg.api_key); SetWindowTextW(d.hKey, w ? w : L""); free(w); }
    NEXT();

    mk(L"STATIC", tw(S_TLS7), WS_CHILD | WS_VISIBLE, x1, y, W - 60, 20, 209, h);

    mk(L"BUTTON", tw(S_SAVE), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, W - 250, H - 60, 110, 30, 1, h);
    mk(L"BUTTON", tw(S_CANCEL), WS_CHILD | WS_VISIBLE, W - 130, H - 60, 100, 30, 2, h);

    ShowWindow(h, SW_SHOW);
    EnableWindow(g_hMain, FALSE);
    MSG msg;
    while (IsWindow(h) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(g_hMain, TRUE);
    SetForegroundWindow(g_hMain);
}

/* ---------- gonderim ---------- */
static void do_send(void)
{
    if (chat_busy()) return;
    wchar_t *w = get_text(g_hInput);
    if (!w) return;
    wchar_t *p = w;
    while (*p == L' ' || *p == L'\r' || *p == L'\n' || *p == L'\t') p++;
    if (!*p) { free(w); return; }
    if (p != w) memmove(w, p, (wcslen(p) + 1) * sizeof(wchar_t));
    if (!g_cfg.use_openai && !g_token[0]) {
        free(w);
        if (show_login()) { ui_set_status(t(S_READY)); }
        return;
    }
    char *u8txt = u8(w);
    free(w);
    SetWindowTextW(g_hInput, L"");
    msglist_add(&g_conv, "user", u8txt ? u8txt : "", NULL, NULL);
    ui_render_all();
    /* LuxAI etiketi + akis */
    append_role_label(L"LuxAI", TH.dim);
    g_busy_ui = 1;
    SetWindowTextW(g_hSend, tw(S_STOP));
    ui_set_status(t(S_THINKING));
    chat_send(u8txt ? u8txt : "");
    free(u8txt);
}

/* ---------- input subclass ---------- */
static LRESULT CALLBACK input_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    (void)ref;
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        int sendnow = g_cfg.send_ctrl_enter ? (ctrl && !shift) : (!ctrl && !shift);
        if (sendnow) { do_send(); return 0; }
    }
    if (m == WM_CHAR && w == VK_RETURN) {
        BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        int sendnow = g_cfg.send_ctrl_enter ? (ctrl && !shift) : (!ctrl && !shift);
        if (sendnow) return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

/* ---------- layout ---------- */
static void layout(HWND h)
{
    RECT r;
    GetClientRect(h, &r);
    int w = r.right, ht = r.bottom;
    int toolh = 46, sidew = 232;
    MoveWindow(g_hMode, 8, 8, 150, 30, TRUE);
    MoveWindow(g_hModel, 168, 8, 130, 200, TRUE);
    MoveWindow(g_hNew, 306, 8, 120, 30, TRUE);
    MoveWindow(g_hSettings, w - 100, 8, 92, 30, TRUE);
    MoveWindow(g_hAcct, w - 250, 8, 142, 30, TRUE);
    MoveWindow(g_hList, 8, toolh + 6, sidew - 16, ht - toolh - 46, TRUE);
    MoveWindow(g_hNew, 306, 8, 120, 30, TRUE);
    MoveWindow(g_hRich, sidew, toolh, w - sidew - 10, ht - toolh - 150, TRUE);
    MoveWindow(g_hInput, sidew, ht - 144, w - sidew - 110, 96, TRUE);
    MoveWindow(g_hSend, w - 100, ht - 144, 90, 44, TRUE);
    MoveWindow(g_hStop, w - 100, ht - 94, 90, 46, TRUE);
    MoveWindow(g_hStatus, sidew, ht - 40, w - sidew - 110, 22, TRUE);
}

/* ---------- main wndproc ---------- */
static LRESULT CALLBACK main_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE: {
        g_hMode = mk(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHLIKE | BS_AUTORADIOBUTTON | WS_GROUP,
                     8, 8, 150, 30, 30, h);
        SetWindowTextW(g_hMode, g_cfg.mode ? tw(S_AGENT) : tw(S_CHAT));
        g_hModel = mk(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                      168, 8, 130, 200, 31, h);
        SendMessageW(g_hModel, CB_ADDSTRING, 0, (LPARAM)L"Flash");
        SendMessageW(g_hModel, CB_ADDSTRING, 0, (LPARAM)L"Lite");
        SendMessageW(g_hModel, CB_SETCURSEL, g_cfg.model, 0);
        g_hNew = mk(L"BUTTON", tw(S_NEW), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 306, 8, 120, 30, 32, h);
        g_hAcct = mk(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10, 33, h);
        g_hSettings = mk(L"BUTTON", tw(S_SETTINGS), WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, 34, h);
        g_hList = mk(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | WS_BORDER,
                     8, 52, 200, 400, 35, h);
        g_hRich = CreateWindowExW(WS_EX_STATICEDGE, MSFTEDIT_CLASS, L"",
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                  0, 0, 10, 10, h, (HMENU)(INT_PTR)36, g_inst, NULL);
        g_hInput = mk(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE |
                      ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 10, 10, 37, h);
        g_hSend = mk(L"BUTTON", tw(S_SEND), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, 38, h);
        g_hStop = mk(L"BUTTON", tw(S_STOP), WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, 39, h);
        g_hStatus = mk(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, 40, h);
        if (g_hRich) {
            SendMessageW(g_hRich, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessageW(g_hRich, EM_SETBKGNDCOLOR, 0, (LPARAM)TH.bg);
            SendMessageW(g_hRich, EM_SETLIMITTEXT, (WPARAM)-1, 0);
        }
        SetWindowSubclass(g_hInput, input_proc, 1, 0);
        ui_sidebar_reload();
        ui_render_all();
        ui_set_status(g_token[0] ? t(S_READY) : t(S_NOT_SIGNED));
        {
            wchar_t b[256];
            _snwprintf(b, 256, tw(S_SIGNED_IN_AS), g_username[0] ? g_username : (g_email[0] ? g_email : L"—"));
            b[255] = 0;
            SetWindowTextW(g_hAcct, b);
        }
        layout(h);
        return 0;
    }
    case WM_SIZE:
        layout(h);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetBkColor(dc, TH.bg);
        if ((HWND)l == g_hStatus) SetTextColor(dc, TH.dim);
        else SetTextColor(dc, TH.fg);
        return (LRESULT)g_bg;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetBkColor((HDC)w, g_dark ? TH.card : GetSysColor(COLOR_WINDOW));
        SetTextColor((HDC)w, g_dark ? TH.fg : GetSysColor(COLOR_WINDOWTEXT));
        if (g_dark) return (LRESULT)g_bg;
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(h, &r);
        FillRect((HDC)w, &r, g_bg);
        return 1;
    }
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == 38) { do_send(); return 0; }                       /* Gönder */
        if (id == 39) { chat_cancel(); return 0; }                   /* Durdur */
        if (id == 32) { ui_conversation_save(); ui_conversation_clear(); ui_sidebar_reload(); return 0; }
        if (id == 34) { show_settings(); ui_set_status(t(S_READY)); return 0; }
        if (id == 33) {
            if (g_token[0]) {
                if (MessageBoxW(h, tw(S_LOGOUT), tw(S_ACCOUNT), MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    account_clear();
                    SetWindowTextW(g_hAcct, tw(S_NOT_SIGNED));
                    ui_set_status(t(S_NOT_SIGNED));
                }
            } else if (show_login()) {
                wchar_t b[256];
                _snwprintf(b, 256, tw(S_SIGNED_IN_AS), g_username[0] ? g_username : g_email);
                b[255] = 0;
                SetWindowTextW(g_hAcct, b);
                ui_set_status(t(S_READY));
            }
            return 0;
        }
        if (id == 31 && code == CBN_SELCHANGE) {
            g_cfg.model = (int)SendMessageW(g_hModel, CB_GETCURSEL, 0, 0);
            cfg_save();
            return 0;
        }
        if (id == 30 && code == BN_CLICKED) {
            g_cfg.mode = g_cfg.mode ? 0 : 1;
            SetWindowTextW(g_hMode, g_cfg.mode ? tw(S_AGENT) : tw(S_CHAT));
            cfg_save();
            return 0;
        }
        if (id == 35 && code == LBN_SELCHANGE) {
            int sel = (int)SendMessageW(g_hList, LB_GETCURSEL, 0, 0);
            sidebar_select(sel);
            return 0;
        }
        break;
    }
    case WM_APP_STATUS: {
        char *s = (char *)l;
        ui_set_status(s ? s : "");
        free(s);
        return 0;
    }
    case WM_APP_REPORT:
        return 0;
    case WM_APP_DELTA: {
        wchar_t *s = (wchar_t *)l;
        if (s) { ui_append_stream(s); free(s); }
        return 0;
    }
    case WM_APP_CONFIRM:
        return MessageBoxW(h, (const wchar_t *)l, (const wchar_t *)w,
                           MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES ? 1 : 0;
    case WM_APP_DONE: {
        /* akis bitince tum doku yeniden bicimlendir */
        g_busy_ui = 0;
        SetWindowTextW(g_hSend, tw(S_SEND));
        ui_render_all();
        ui_set_status(t(S_READY));
        ui_conversation_save();
        ui_sidebar_reload();
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l;
        mm->ptMinTrackSize.x = 820;
        mm->ptMinTrackSize.y = 560;
        return 0;
    }
    case WM_DESTROY:
        ui_conversation_save();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ui_run(void)
{
    g_inst = GetModuleHandleW(NULL);
    g_ui_thread = GetCurrentThreadId();
    theme_apply();

    NONCLIENTMETRICSW ncm;
    ZeroMemory(&ncm, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof icc;
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    LoadLibraryW(L"msftedit.dll");

    new_conv_id();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = main_proc;
    wc.hInstance = g_inst;
    wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"LuxAIWindow";
    RegisterClassExW(&wc);

    int w = 1100, hgt = 760;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_hMain = CreateWindowExW(0, L"LuxAIWindow", L"LuxAI 3.1 — Windows native",
                              WS_OVERLAPPEDWINDOW, (sw - w) / 2, (sh - hgt) / 2, w, hgt,
                              NULL, NULL, g_inst, NULL);
    if (!g_hMain) return;
    ShowWindow(g_hMain, SW_SHOW);
    UpdateWindow(g_hMain);

    if (!g_cfg.use_openai && !g_token[0]) {
        if (show_login()) ui_set_status(t(S_READY));
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
