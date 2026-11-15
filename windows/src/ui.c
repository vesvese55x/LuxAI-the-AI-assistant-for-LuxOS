/* LuxAI native Windows — arayuz.
 *
 * Gorunum, orijinal Linux (GTK3) uygulamasinin CSS'i ile birebir ayni olacak
 * sekilde tamamen kendi cizimimizle (GDI+) olusturulur:
 *   - CSD basliksiz pencere + GTK headerbar (kendi pencere dugmeleri)
 *   - 260 px kenar cubugu (accent "Yeni sohbet", arama, sohbet listesi, hesap)
 *   - mesaj balonlari (gradyan kullanici balonu, kart asistan balonu, arac kartlari)
 *   - yuvarlak besteci + gonder/durdur
 *   - giris karti ve ayarlar sayfasi
 * Renkler/olculer luxai ikilisinin icindeki @define-color blogundan alindi.
 */
#include "luxai.h"
#include "skin.h"
#include "i18n_data.h"
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <windowsx.h>
#include <wctype.h>

static void layout_soft(void);
static int  resp_sink_ui(const char *d, size_t n, void *ud);
/* cizilmis modallar (izin/onay/hesap) - tanimlar dosya sonunda */
static void draw_modal(double w, double h);
static int  modal_msg(HWND h, UINT m, WPARAM w, LPARAM l);
static int  modal_active(void);
static int  confirm_ui(const wchar_t *title, const wchar_t *body, int danger);
static void account_open(void);

/* ---------------- genel ---------------- */
HWND g_hMain;
MsgList g_conv;

#define HS        50.0    /* headerbar yuksekligi */
#define SW        260.0   /* kenar cubugu genisligi */
#define PAD       16.0
#define CX_PX     15      /* govde yazi boyutu */
#define SM_PX     12
#define TI_PX     11

static HINSTANCE g_inst;
static DWORD g_ui_thread;
static int   g_dark, g_page, g_login_open, g_sidebar_open = 1;
static int   g_busy_ui;
static wchar_t g_conv_id[64];
static char   g_status[512];

/* akan metin (asistan yaniti gelirken) */
static wchar_t *g_stream;
static size_t   g_stream_n, g_stream_cap;

/* kaydirma */
static double g_scroll;
static int    g_at_bottom = 1;
static double g_content_h;

/* fare durumu */
static int g_hover_kind, g_hover_idx, g_press_kind, g_press_idx, g_tracking;

/* olcum DC'si (yerlesim oncesi metin olcumu icin) */
static HDC     g_mdc;
static HBITMAP g_mbmp;
static HGDIOBJ g_mold;

/* metin kutulari */
static HWND g_edComposer, g_edSearch, g_edMail, g_edPass,
            g_edWork, g_edEnd, g_edKey, g_edTok;
static HFONT g_fEdit, g_fEditMono;
static HBRUSH g_brInput, g_brField, g_brCode;
static double g_wtxt;   /* balon metin genisligi (olcum = cizim) */

/* /api/me ile gelen hesap bilgileri (hesap penceresi) */
static wchar_t g_me_name[160], g_me_email[256], g_me_plan[80], g_me_user[96];
static int g_me_used = -1, g_me_limit = -1, g_me_busy;
#define WM_APP_ACCME (WM_APP + 40)

/* ---------------- kucuk yardimcilar ---------------- */
typedef struct { double x, y, w, h; } Rect;
static Rect R(double x, double y, double w, double h)
{
    Rect r; r.x = x; r.y = y; r.w = w; r.h = h; return r;
}
static int in_rect(Rect r, double x, double y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
static const wchar_t *T(int id, wchar_t *b, size_t cch)
{
    const wchar_t *s = tw(id);
    wcsncpy(b, s ? s : L"", cch - 1);
    b[cch - 1] = 0;
    return b;
}

/* tiklanabilir alan listesi */
enum {
    H_NONE = 0, H_MIN, H_MAX, H_CLOSE, H_SIDEBAR, H_NEWCHATBTN, H_SEG_CHAT, H_SEG_AGENT,
    H_COMBO, H_MENU, H_NEWCHAT, H_SEARCH, H_ROW, H_ROWX, H_ACCT, H_SEND, H_STOP,
    H_EMPTY_MAIN, H_EMPTY_LINK, H_LOGIN_GO, H_LOGIN_WEB, H_LOGIN_CANCEL, H_LOGIN_CLOSE,
    H_SET_BACK, H_SET_SAVE, H_SET_CLOSE, H_SET_ROW, H_SET_BROWSE
};
typedef struct { int kind, idx; Rect r; } Hit;
static Hit g_hits[160];
static int g_nhits;

static void hit_add(int kind, int idx, Rect r)
{
    if (g_nhits >= 160) return;
    g_hits[g_nhits].kind = kind;
    g_hits[g_nhits].idx = idx;
    g_hits[g_nhits].r = r;
    g_nhits++;
}
static Hit hit_at(double x, double y)
{
    Hit none; none.kind = H_NONE; none.idx = 0; none.r = R(0, 0, 0, 0);
    for (int i = g_nhits - 1; i >= 0; i--)
        if (in_rect(g_hits[i].r, x, y)) return g_hits[i];
    return none;
}

/* ---------------- tema ---------------- */
static int system_is_dark(void)
{
    HKEY k;
    DWORD v = 1, sz = sizeof v;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueExW(k, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&v, &sz);
        RegCloseKey(k);
    }
    return v == 0;
}

static void theme_apply(void)
{
    g_dark = (g_cfg.theme == 2) || (g_cfg.theme == 0 && system_is_dark());
    sk_theme(g_dark);
    if (g_brInput) DeleteObject(g_brInput);
    if (g_brField) DeleteObject(g_brField);
    if (g_brCode) DeleteObject(g_brCode);
    g_brInput = CreateSolidBrush(sk_is_dark() ? RGB(0x26, 0x28, 0x38) : RGB(0xFF, 0xFF, 0xFF));
    g_brField = CreateSolidBrush(sk_is_dark() ? RGB(0x2C, 0x2E, 0x3D) : RGB(0xE9, 0xEA, 0xEC));
    g_brCode  = CreateSolidBrush(sk_is_dark() ? RGB(0x15, 0x16, 0x1F) : RGB(0xF3, 0xF4, 0xF8));
    if (g_edComposer) {
        InvalidateRect(g_edComposer, NULL, TRUE);
        InvalidateRect(g_edSearch, NULL, TRUE);
        InvalidateRect(g_edMail, NULL, TRUE);
        InvalidateRect(g_edPass, NULL, TRUE);
    }
    if (g_hMain) InvalidateRect(g_hMain, NULL, FALSE);
}

/* ---------------- sohbet dosyalari ---------------- */
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
static void conv_title(wchar_t *out, size_t cch)
{
    out[0] = 0;
    for (int i = 0; i < g_conv.n; i++) {
        if (!strcmp(g_conv.v[i].role, "user") && g_conv.v[i].content && *g_conv.v[i].content) {
            wchar_t *w = u16(g_conv.v[i].content);
            if (w) {
                size_t j = 0;
                for (; w[j] && j < cch - 1 && j < 48; j++)
                    out[j] = (w[j] == L'\n' || w[j] == L'\r') ? L' ' : w[j];
                out[j] = 0;
                free(w);
            }
            return;
        }
    }
    T(S_NEW_CONV, out, cch);
}

void ui_conversation_clear(void)
{
    msglist_clear(&g_conv);
    new_conv_id();
    g_scroll = 0;
    g_at_bottom = 1;
    ui_render_all();
}

void ui_conversation_save(void)
{
    if (!g_conv.n) return;
    wchar_t dir[MAX_PATH], path[MAX_PATH * 2];
    conv_dir(dir, MAX_PATH);
    _snwprintf(path, MAX_PATH * 2, L"%s\\%s.json", dir, g_conv_id);
    Buf b;
    buf_init(&b);
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
    g_conv_id[63] = 0;
    JVal *msgs = json_get(v, "messages");
    for (int i = 0; msgs && i < msgs->n; i++) {
        JVal *m = json_at(msgs, i);
        if (!m) continue;
        msglist_add(&g_conv, json_str(m, "role"), json_str(m, "content"),
                    json_str(m, "tool_calls"), json_str(m, "tool_call_id"));
    }
    json_free(v);
    g_scroll = 0;
    g_at_bottom = 1;
    ui_render_all();
}

/* kenar cubugu listesi */
typedef struct { wchar_t id[64], title[128]; FILETIME ft; } SideEntry;
static SideEntry g_side[128];
static int g_side_n, g_side_scroll;

static int side_cmp(const void *a, const void *b)
{
    const SideEntry *x = (const SideEntry *)a, *y = (const SideEntry *)b;
    return CompareFileTime(&y->ft, &x->ft);
}

void ui_sidebar_reload(void)
{
    wchar_t dir[MAX_PATH], pat[MAX_PATH * 2];
    conv_dir(dir, MAX_PATH);
    _snwprintf(pat, MAX_PATH * 2, L"%s\\*.json", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    g_side_n = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (g_side_n >= 128) break;
            wcsncpy(g_side[g_side_n].id, fd.cFileName, 63);
            g_side[g_side_n].id[63] = 0;
            wchar_t *dot = wcsrchr(g_side[g_side_n].id, L'.');
            if (dot) *dot = 0;
            g_side[g_side_n].ft = fd.ftLastWriteTime;
            g_side[g_side_n].title[0] = 0;
            wchar_t p[MAX_PATH * 2];
            _snwprintf(p, MAX_PATH * 2, L"%s\\%s", dir, fd.cFileName);
            char *d = file_read_utf8(p);
            if (d) {
                JVal *v = json_parse(d);
                const char *tt = json_str(v, "title");
                if (tt) {
                    wchar_t *w = u16(tt);
                    if (w) { wcsncpy(g_side[g_side_n].title, w, 127); free(w); }
                }
                json_free(v);
                free(d);
            }
            if (!g_side[g_side_n].title[0]) wcsncpy(g_side[g_side_n].title, L"\u2026", 8);
            g_side_n++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    qsort(g_side, (size_t)g_side_n, sizeof(SideEntry), side_cmp);
    if (g_hMain) InvalidateRect(g_hMain, NULL, FALSE);
}

/* ---------------- akan metin ---------------- */
static void stream_clear(void)
{
    free(g_stream);
    g_stream = NULL;
    g_stream_n = g_stream_cap = 0;
}
static void stream_add(const wchar_t *s)
{
    size_t n = wcslen(s);
    if (g_stream_n + n + 2 > g_stream_cap) {
        size_t nc = (g_stream_n + n + 2) * 2 + 256;
        wchar_t *p = (wchar_t *)realloc(g_stream, nc * sizeof(wchar_t));
        if (!p) return;
        g_stream = p;
        g_stream_cap = nc;
    }
    memcpy(g_stream + g_stream_n, s, n * sizeof(wchar_t));
    g_stream_n += n;
    g_stream[g_stream_n] = 0;
}

/* ================= markdown cizimi ================= */
typedef struct { wchar_t *t; unsigned char *sty; int n; } Rich;

static void rich_free(Rich *r)
{
    free(r->t); free(r->sty); r->t = NULL; r->sty = NULL; r->n = 0;
}

/* stil: 1 kalin, 2 egik, 4 mono (satir ici kod), 8 baglanti */
static void parse_inline(const wchar_t *s, int len, Rich *r)
{
    r->t = (wchar_t *)calloc((size_t)len + 2, sizeof(wchar_t));
    r->sty = (unsigned char *)calloc((size_t)len + 2, 1);
    if (!r->t || !r->sty) { rich_free(r); return; }
    int o = 0, bold = 0, ital = 0;
    for (int i = 0; i < len;) {
        if (i + 1 < len && s[i] == L'*' && s[i + 1] == L'*') { bold = !bold; i += 2; continue; }
        if (i + 1 < len && s[i] == L'_' && s[i + 1] == L'_') { bold = !bold; i += 2; continue; }
        if (s[i] == L'*' && !(i + 1 < len && s[i + 1] == L'*')) { ital = !ital; i++; continue; }
        if (s[i] == L'`') {
            int j = i + 1;
            while (j < len && s[j] != L'`') j++;
            if (j < len) {
                for (int k = i + 1; k < j; k++) {
                    r->t[o] = s[k];
                    r->sty[o++] = (unsigned char)(4 | (bold ? 1 : 0));
                }
                i = j + 1;
                continue;
            }
        }
        if (s[i] == L'[') {
            int j = i + 1;
            while (j < len && s[j] != L']') j++;
            if (j < len && j + 1 < len && s[j + 1] == L'(') {
                int k = j + 2;
                while (k < len && s[k] != L')') k++;
                if (k < len) {
                    for (int q = i + 1; q < j; q++) {
                        r->t[o] = s[q];
                        r->sty[o++] = (unsigned char)(8 | (bold ? 1 : 0));
                    }
                    i = k + 1;
                    continue;
                }
            }
        }
        r->t[o] = s[i];
        r->sty[o++] = (unsigned char)((bold ? 1 : 0) | (ital ? 2 : 0));
        i++;
    }
    r->t[o] = 0;
    r->n = o;
}

/* [a,b) araligini ciz/olc; genisligi dondur */
static double seg_run(const Rich *r, int a, int b, int px, DWORD col, int paint,
                      double x, double y, double lh)
{
    double adv = 0;
    int i = a;
    while (i < b) {
        unsigned char st = r->sty[i];
        int j = i;
        while (j < b && r->sty[j] == st) j++;
        int mono = (st & 4) ? 1 : 0;
        int weight = (st & 1) ? 700 : 400;
        DWORD c = (st & 8) ? SK.accent : col;
        if (mono) {
            double w = sk_seg(r->t + i, j - i, px - 1, weight, 1, 0, 0, 0, 0);
            if (paint) {
                sk_round(x + adv - 1, y + 1, w + 2, lh - 2, 4, SK.code);
                sk_seg(r->t + i, j - i, px - 1, weight, 1, c, 1, x + adv, y);
            }
            adv += w;
        } else {
            adv += sk_seg(r->t + i, j - i, px, weight, 0, c, paint, x + adv, y);
        }
        i = j;
    }
    return adv;
}

/* satir(lar)i sarar; yuksekligi dondurur, en genis satiri *outw'ye yazar */
static double rich_draw(const Rich *r, double x, double y, double maxw, int px,
                        DWORD col, int paint, double *outw)
{
    double lh = sk_th(px, 0);
    double cx = x, cy = y, linew = 0, widest = 0;
    int i = 0;
    while (i < r->n) {
        int j = i;
        while (j < r->n && r->t[j] != L' ') j++;
        int wend = j, snd = j;
        while (snd < r->n && r->t[snd] == L' ') snd++;
        double ww = (wend > i) ? seg_run(r, i, wend, px, col, 0, 0, 0, lh) : 0;
        if (linew > 0 && linew + ww > maxw) { cy += lh; cx = x; linew = 0; }
        if (wend > i) {
            if (paint) seg_run(r, i, wend, px, col, 1, cx, cy, lh);
            cx += ww;
            linew += ww;
            if (linew > widest) widest = linew;
        }
        if (snd > wend) {
            double sw = seg_run(r, wend, snd, px, col, 0, 0, 0, lh);
            if (linew + sw > maxw && linew > 0) { cy += lh; cx = x; linew = 0; }
            else { cx += sw; linew += sw; }
        }
        i = snd;
    }
    if (outw && widest > *outw) *outw = widest;
    return cy + lh - y;
}

/* tek renkli (kod/tablo) blok */
static double mono_block(const wchar_t *const *lines, int n, double x, double y,
                         double maxw, int px, int paint, double *outw)
{
    double lh = sk_th(px, 1);
    double maxlw = 0;
    for (int i = 0; i < n; i++) {
        double w = sk_seg(lines[i], (int)wcslen(lines[i]), px, 400, 1, 0, 0, 0, 0);
        if (w > maxlw) maxlw = w;
    }
    double bw = maxlw + 26;
    if (bw > maxw) bw = maxw;
    double bh = n * lh + 16;
    if (paint) {
        sk_round(x, y, bw, bh, 10, SK.code);
        sk_rline(x, y, bw, bh, 10, SK.border, 1);
        sk_clip(x, y, bw, bh);
        for (int i = 0; i < n; i++)
            sk_seg(lines[i], (int)wcslen(lines[i]), px, 400, 1, SK.fg, 1,
                   x + 13, y + 8 + i * lh);
        sk_unclip();
    }
    if (outw && bw > *outw) *outw = bw;
    return bh;
}

static int is_fence(const wchar_t *p, int len)
{
    return len >= 3 && p[0] == L'`' && p[1] == L'`' && p[2] == L'`';
}

/* markdown metnini ciz; yuksekligi dondurur */
static double md_draw(const wchar_t *txt, double x, double y, double maxw, int px,
                      DWORD col, int paint, double *outw)
{
    const wchar_t *p = txt;
    double cy = y;
    int blank = 0;
    while (*p) {
        const wchar_t *e = p;
        while (*e && *e != L'\n' && *e != L'\r') e++;
        int len = (int)(e - p);
        const wchar_t *next = e;
        while (*next == L'\r' || *next == L'\n') { next++; break; }

        /* ``` kod blogu */
        if (is_fence(p, len)) {
            wchar_t lang[24];
            int ln = 0;
            for (int i = 3; i < len && ln < 23; i++) lang[ln++] = p[i];
            lang[ln] = 0;
            const wchar_t *q = (*e) ? e + 1 : e;
            const wchar_t *lines[400];
            wchar_t *buf = NULL;
            size_t bn = 0, bc = 0;
            int nl = 0;
            while (*q) {
                const wchar_t *e2 = q;
                while (*e2 && *e2 != L'\n' && *e2 != L'\r') e2++;
                if (is_fence(q, (int)(e2 - q))) { q = *e2 ? e2 + 1 : e2; break; }
                size_t need = bn + (size_t)(e2 - q) + 1;
                if (need > bc) {
                    bc = need * 2 + 256;
                    wchar_t *nb = (wchar_t *)realloc(buf, bc * sizeof(wchar_t));
                    if (!nb) break;
                    buf = nb;
                }
                if (nl < 399) lines[nl++] = buf + bn;
                for (const wchar_t *t = q; t < e2; t++) buf[bn++] = *t;
                buf[bn++] = 0;
                q = *e2 ? e2 + 1 : e2;
            }
            if (!buf) { buf = (wchar_t *)calloc(1, sizeof(wchar_t)); bc = 1; }
            double bh = 0;
            if (nl > 0) {
                double off = lang[0] ? 18 : 0;
                if (paint && lang[0])
                    sk_seg(lang, (int)wcslen(lang), TI_PX, 400, 0, SK.dim, 1, x + 4, cy + 1);
                bh = mono_block(lines, nl, x, cy + off, maxw, SM_PX, paint, outw) + off;
            }
            free(buf);
            cy += bh + 8;
            blank = 0;
            p = q;
            continue;
        }

        /* baslik */
        if (len >= 2 && p[0] == L'#') {
            int lvl = 1;
            while (lvl < len && p[lvl] == L'#') lvl++;
            int st = lvl;
            while (st < len && p[st] == L' ') st++;
            Rich r;
            parse_inline(p + st, len - st, &r);
            int hp = lvl == 1 ? px + 5 : (lvl == 2 ? px + 3 : px + 1);
            double h = rich_draw(&r, x, cy, maxw, hp, col, paint, outw);
            rich_free(&r);
            cy += h + 4;
            blank = 0;
            p = next;
            continue;
        }

        /* madde imi */
        int bullet = 0, numw = 0;
        if (len >= 2 && (p[0] == L'-' || p[0] == L'*' || p[0] == 0x2022) && p[1] == L' ') {
            bullet = 1;
        } else if (len >= 3 && p[0] >= L'0' && p[0] <= L'9') {
            int k = 0;
            while (k < len && p[k] >= L'0' && p[k] <= L'9') k++;
            if (k + 1 <= len && p[k] == L'.' && k + 1 < len && p[k + 1] == L' ') numw = k;
        }
        if (bullet || numw) {
            int st = bullet ? 2 : numw + 2;
            Rich r;
            parse_inline(p + st, len - st, &r);
            double lh = sk_th(px, 0);
            if (paint) {
                if (bullet) sk_circle(x + 5, cy + lh * 0.52, 2.4, SK.dim);
                else {
                    wchar_t nb[16];
                    int k = 0;
                    for (int i = 0; i < numw && k < 15; i++) nb[k++] = p[i];
                    nb[k++] = L'.';
                    nb[k] = 0;
                    sk_text(x, cy, 24, nb, px, 400, 0, SK.dim, 2);
                }
            }
            double h = rich_draw(&r, x + 16, cy, maxw - 16, px, col, paint,
                                 NULL);
            rich_free(&r);
            cy += h + 3;
            blank = 0;
            p = next;
            continue;
        }

        /* alinti */
        if (len >= 1 && p[0] == L'>' && (len == 1 || p[1] == L' ')) {
            int st = (len >= 2) ? 2 : 1;
            Rich r;
            parse_inline(p + st, len - st, &r);
            double tw_ = maxw - 14;
            double h = rich_draw(&r, x + 14, cy, tw_, px, SK.dim, 0, NULL);
            if (paint) {
                sk_round(x, cy - 1, 3, h + 2, 1.5, (SK.accent & 0x00FFFFFF) | 0xB3000000);
                rich_draw(&r, x + 14, cy, tw_, px, SK.dim, 1, NULL);
            }
            rich_free(&r);
            cy += h + 3;
            blank = 0;
            p = next;
            continue;
        }

        /* tablo satiri -> mono */
        if (len >= 1 && p[0] == L'|') {
            const wchar_t *lines[64];
            wchar_t *buf = NULL;
            size_t bn = 0, bc = 0;
            int nl = 0;
            const wchar_t *q = p;
            while (*q) {
                const wchar_t *e2 = q;
                while (*e2 && *e2 != L'\n' && *e2 != L'\r') e2++;
                if (e2 == q || *q != L'|') break;
                size_t need = bn + (size_t)(e2 - q) + 1;
                if (need > bc) {
                    bc = need * 2 + 256;
                    wchar_t *nb = (wchar_t *)realloc(buf, bc * sizeof(wchar_t));
                    if (!nb) break;
                    buf = nb;
                }
                if (nl < 63) lines[nl++] = buf + bn;
                for (const wchar_t *t2 = q; t2 < e2; t2++) buf[bn++] = *t2;
                buf[bn++] = 0;
                q = *e2 ? e2 + 1 : e2;
            }
            if (!buf) { buf = (wchar_t *)calloc(1, sizeof(wchar_t)); bc = 1; }
            double bh = nl ? mono_block(lines, nl, x, cy, maxw, SM_PX, paint, NULL) : 0;
            free(buf);
            cy += bh + 6;
            blank = 0;
            p = q;
            continue;
        }

        /* bos satir */
        if (len == 0) {
            if (!blank) { cy += 6; blank = 1; }
            p = next;
            if (next == e) break;
            continue;
        }

        /* paragraf */
        Rich r;
        parse_inline(p, len, &r);
        double h = rich_draw(&r, x, cy, maxw, px, col, paint, outw);
        rich_free(&r);
        cy += h + 3;
        blank = 0;
        p = next;
        if (next == e) break;
    }
    return cy - y;
}

/* ================= mesajlar ================= */
static const double PADX_U = 14, PADY_U = 9, PADX_A = 15, PADY_A = 11;

static int is_tool_role(const char *role) { return role && !strcmp(role, "tool"); }

/* tek mesaj balonu; yuksekligi dondurur */
static double md_measure(const wchar_t *text, double w, int px, DWORD col, int tool,
                         double *nat)
{
    if (tool) {
        Rich r;
        parse_inline(text, (int)wcslen(text), &r);
        double h = rich_draw(&r, 0, 0, w, px, col, 0, nat);
        rich_free(&r);
        return h;
    }
    return md_draw(text, 0, 0, w, px, col, 0, nat);
}

static double bubble(const char *role, const wchar_t *text, const char *tool_calls,
                     double x, double y, double area, int paint)
{
    if (!text) text = L"";
    int user = role && !strcmp(role, "user");
    int arole = role && !strcmp(role, "assistant");
    int tool = is_tool_role(role);
    wchar_t toolbuf[420];
    if (tool) {
        int k = 0;
        for (int i = 0; text[i] && k < 400; i++) {
            wchar_t c = text[i];
            if (c == L'\r' || c == L'\n' || c == L'\t') {
                if (k && toolbuf[k - 1] != L' ') toolbuf[k++] = L' ';
            } else {
                toolbuf[k++] = c;
            }
        }
        while (k > 0 && toolbuf[k - 1] == L' ') k--;
        if (text[k] && k >= 400) { toolbuf[k++] = 0x2026; }
        toolbuf[k] = 0;
        text = toolbuf;
    }
    double padx = tool ? 12 : (user ? PADX_U : PADX_A);
    double pady = tool ? 8 : (user ? PADY_U : PADY_A);
    int px = tool ? SM_PX : CX_PX;
    DWORD col = user ? SK.on_accent : (tool ? SK.dim : SK.fg);

    double limit = user ? area * 0.64 : area * 0.80;
    if (limit > 780) limit = 780;
    if (limit < 180) limit = area * 0.5;
    if (limit < 90) limit = 90;

    int has_text = text && *text;
    double y0 = y;
    int label_h = (arole && has_text) ? 17 : 0;
    double bw = 0, bh = 0, inner = 0;

    if (has_text) {
        double nat = 0;
        md_measure(text, limit - padx * 2, px, col, tool, &nat);
        bw = nat + padx * 2 + 4;
        if (bw > limit) bw = limit;
        { double minw = user ? 90 : 200;
          if (bw < minw) bw = user ? (nat + padx * 2 + 4) : minw;
          if (bw < 60) bw = 60; }
        inner = bw - padx * 2;
        if (inner < 40) inner = 40;
        g_wtxt = tool ? inner - 7 : inner;
        bh = md_measure(text, g_wtxt, px, col, tool, NULL) + pady * 2 + 2;
    }
    double bx = user ? (x + area - bw) : x;

    if (label_h) {
        if (paint) sk_text(bx + 6, y, 240, L"LuxAI", TI_PX, 700, 0, SK.dim, 0);
        y += label_h;
    }

    if (has_text) {
        if (paint) {
            if (user) {
                sk_round4(bx, y, bw, bh, 18, 18, 4, 18, 0xFF3D7BFF);
                sk_clip(bx, y, bw, bh);
                sk_grad(bx + 1, y + 1, bw - 2, bh - 2, 17, 0xFF3D7BFF, 0xFF6A5CFF, 1);
                sk_unclip();
            } else if (tool) {
                sk_round(bx, y, bw, bh, 12, SK.code);
                sk_rline(bx, y, bw, bh, 12, SK.border, 1);
            } else {
                sk_round(bx, y, bw, bh, 18, SK.card);
                sk_rline(bx, y, bw, bh, 18, SK.border, 1);
            }
            sk_clip(bx, y, bw, bh);
            if (tool) {
                sk_circle(bx + padx - 2, y + pady + 6, 3, SK.ok);
                Rich r;
                parse_inline(text, (int)wcslen(text), &r);
                rich_draw(&r, bx + padx + 7, y + pady, g_wtxt, px, col, 1, NULL);
                rich_free(&r);
            } else {
                md_draw(text, bx + padx, y + pady, g_wtxt, px, col, 1, NULL);
            }
            sk_unclip();
        }
        y += bh;
    }

    /* asistan arac cagrilari -> kucuk cipler */
    if (arole && tool_calls && *tool_calls) {
        JVal *arr = json_parse(tool_calls);
        double cx = bx + 2;
        double hh = (arr && arr->n) ? 30 : 0;
        for (int k = 0; arr && k < arr->n && k < 6; k++) {
            JVal *fn = json_get(json_at(arr, k), "function");
            const char *nm = fn ? json_str(fn, "name") : NULL;
            if (!nm) continue;
            wchar_t *wn = u16(nm);
            if (!wn) continue;
            double w = sk_tw_s(wn, TI_PX, 400, 0) + 30;
            if (paint) {
                sk_round(cx, y + 6, w, 22, 8, SK.code);
                sk_rline(cx, y + 6, w, 22, 8, SK.border, 1);
                sk_circle(cx + 11, y + 17, 3.2, SK.ok);
                sk_seg(wn, (int)wcslen(wn), TI_PX, 400, 0, SK.dim,1, cx + 20, y + 10);
            }
            cx += w + 6;
            free(wn);
        }
        if (hh) y += hh;
        json_free(arr);
    }
    y += 12;
    return y - y0;
}

/* ================= yerlesim ================= */
typedef struct {
    Rect header, sidebar, main, msgs, composer, send, stop, status, newchat, search,
         list, acct, seg0, seg1, sbbtn, ncbtn, combo, hmenu, bmin, bmax, bclose,
         logo, ebtn, elink, lcard, lgo, lweb, lcancel, lclose, back, setbtn, closebtn;
} Lay;
static Lay L;

enum {
    SR_MODEL, SR_MODE, SR_LANG, SR_THEME, SR_SENDKEY, SR_AUTO, SR_WORK, SR_OPENAI,
    SR_END, SR_KEY, SR_TOK, SR_COUNT
};
static Rect g_srow[SR_COUNT], g_sval[SR_COUNT];
static double g_set_bottom;

static int g_composer_on = 1;
static int g_side_row0;      /* ilk gorunen sohbet satiri */
static int g_side_rows;      /* ekrana sigan satir sayisi */

/* yazilari ortalayarak sarar */
static double wrap_center(const wchar_t *s, double cx, double y, double maxw, int px,
                          int weight, DWORD col, int paint)
{
    int lh = sk_th(px, 0);
    int n = (int)wcslen(s);
    int i = 0, guard = 0;
    while (i < n && guard++ < 64) {
        int w0 = n;
        while (w0 > i && sk_tw(s + i, w0 - i, px, weight, 0) > maxw) {
            w0--;
            while (w0 > i && s[w0] != L' ') w0--;
            if (w0 == i) { w0 = i + 1; while (w0 < n && s[w0] != L' ') w0++; break; }
        }
        if (paint) {
            wchar_t buf[512];
            int cch = w0 - i;
            if (cch > 511) cch = 511;
            memcpy(buf, s + i, (size_t)cch * sizeof(wchar_t));
            buf[cch] = 0;
            double w = sk_tw_s(buf, px, weight, 0);
            sk_seg(buf, cch, px, weight, 0, col, 1, cx - w / 2.0, y);
        }
        y += lh;
        i = w0;
        while (i < n && s[i] == L' ') i++;
    }
    return (y) - (y - (double)(guard * lh));
}

static void compute_layout(double w, double h, int painting)
{
    if (!painting) sk_begin(g_mdc);
    g_nhits = 0;

    double sw = g_sidebar_open ? SW : 0;
    L.header = R(0, 0, w, HS);
    L.sbbtn  = R(12, 8, 44, 34);
    L.ncbtn  = R(62, 8, 44, 34);
    L.bclose = R(w - 44, 10, 30, 30);
    L.bmax   = R(w - 76, 10, 30, 30);
    L.bmin   = R(w - 108, 10, 30, 30);
    L.hmenu  = R(w - 108 - 6 - 40, 8, 40, 34);
    L.combo  = R(L.hmenu.x - 8 - 220, 8, 220, 34);
    L.seg0   = R((w - 180) / 2, 8, 90, 34);
    L.seg1   = R(L.seg0.x + 90, 8, 90, 34);
    hit_add(H_MIN, 0, L.bmin);
    hit_add(H_MAX, 0, L.bmax);
    hit_add(H_CLOSE, 0, L.bclose);
    hit_add(H_MENU, 0, L.hmenu);
    hit_add(H_COMBO, 0, L.combo);
    hit_add(H_SIDEBAR, 0, L.sbbtn);
    hit_add(H_NEWCHATBTN, 0, L.ncbtn);
    hit_add(H_SEG_CHAT, 0, L.seg0);
    hit_add(H_SEG_AGENT, 0, L.seg1);

    /* ---- kenar cubugu ---- */
    L.sidebar = R(0, HS, sw, h - HS);
    L.newchat = L.search = L.list = L.acct = R(0, 0, 0, 0);
    if (g_sidebar_open) {
        L.newchat = R(10, HS + 10, SW - 20, 34);
        L.search  = R(10, L.newchat.y + L.newchat.h + 8, SW - 20, 32);
        double lab = L.search.y + L.search.h + 24;
        L.acct = R(0, h - 64, SW, 64);
        L.list = R(0, lab, SW, L.acct.y - lab - 2);
        g_side_rows = (int)(L.list.h / 36.0);
        if (g_side_rows < 1) g_side_rows = 1;
        if (g_side_row0 > g_side_n - g_side_rows) g_side_row0 = g_side_n - g_side_rows;
        if (g_side_row0 < 0) g_side_row0 = 0;
        for (int i = 0; i < g_side_rows && g_side_row0 + i < g_side_n; i++) {
            Rect r = R(8, L.list.y + i * 36 + 2, SW - 16, 32);
            hit_add(H_ROW, g_side_row0 + i, r);
            hit_add(H_ROWX, g_side_row0 + i, R(r.x + r.w - 30, r.y + 4, 26, 24));
        }
        hit_add(H_NEWCHAT, 0, L.newchat);
        hit_add(H_SEARCH, 0, L.search);
        hit_add(H_ACCT, 0, L.acct);
    }

    /* ---- ana alan ---- */
    L.main = R(sw, HS, w - sw, h - HS);
    g_composer_on = (g_token[0] || g_cfg.use_openai || g_conv.n > 0 || g_stream != NULL);
    if (g_composer_on) {
        L.status = R(sw + PAD + 2, h - 24, w - sw - PAD * 2, 18);
        L.composer = R(sw + PAD, h - 24 - 12 - 56, w - sw - PAD * 2, 56);
        L.send = R(L.composer.x + L.composer.w - 46, L.composer.y + 11, 34, 34);
        {
            wchar_t sb[64];
            T(S_STOP, sb, 64);
            double sw = sk_tw_s(sb, 13, 700, 0) + 46;
            if (sw < 84) sw = 84;
            L.stop = R(L.send.x - sw - 8, L.composer.y + 13, sw, 30);
        }
        L.msgs = R(sw, HS, w - sw, L.composer.y - HS - 8);
    } else {
        L.status = L.composer = L.send = L.stop = R(0, 0, 0, 0);
        L.msgs = R(sw, HS, w - sw, h - HS);
    }

    /* ---- bos durum ---- */
    L.logo = L.ebtn = L.elink = R(0, 0, 0, 0);
    if (!g_conv.n && !g_stream && g_page == 0 && !g_login_open) {
        double cx = L.main.x + L.main.w / 2;
        double avail = L.msgs.h;
        double subh = 48, total = 64 + 20 + 34 + 12 + subh + 24 + 42 + 18 + 22;
        double y0 = HS + (avail - total) / 2;
        if (y0 < HS + 20) y0 = HS + 20;
        L.logo = R(cx - 32, y0, 64, 64);
        double bw = 0;
        wchar_t b[128];
        if (!g_token[0]) {
            T(S_SIGNIN_PILL, b, 128);
            bw = sk_tw_s(b, CX_PX, 700, 0) + 44;
            L.ebtn = R(cx - bw / 2, y0 + 64 + 20 + 34 + 12 + subh + 24, bw, 42);
            hit_add(H_EMPTY_MAIN, 0, L.ebtn);
            L.elink = R(cx - 110, L.ebtn.y + 42 + 16, 220, 24);
            hit_add(H_EMPTY_LINK, 0, L.elink);
        }
    }

    /* ---- giris karti ---- */
    L.lcard = L.lgo = L.lweb = L.lcancel = L.lclose = R(0, 0, 0, 0);
    if (g_login_open) {
        double cw = 424, ch = 348;
        double cx = (w - cw) / 2, cy = (h - ch) / 2;
        if (cx < 10) cx = 10;
        if (cy < HS + 10) cy = HS + 10;
        L.lcard = R(cx, cy, cw, ch);
        L.lclose = R(cx + cw - 38, cy + 10, 28, 28);
        L.lgo     = R(cx + 22, cy + ch - 62, 108, 36);
        L.lweb    = R(cx + 140, cy + ch - 62, 168, 36);
        L.lcancel = R(cx + cw - 100, cy + ch - 62, 78, 36);
        hit_add(H_LOGIN_CLOSE, 0, L.lclose);
        hit_add(H_LOGIN_GO, 0, L.lgo);
        hit_add(H_LOGIN_WEB, 0, L.lweb);
        hit_add(H_LOGIN_CANCEL, 0, L.lcancel);
    }

    /* ---- ayarlar ---- */
    L.back = L.setbtn = L.closebtn = R(0, 0, 0, 0);
    for (int i = 0; i < SR_COUNT; i++) g_srow[i] = g_sval[i] = R(0, 0, 0, 0);
    if (g_page == 1) {
        double x = L.main.x + PAD + 4, y = HS + 12;
        L.back = R(x, y + 3, 30, 30);
        hit_add(H_SET_BACK, 0, L.back);
        y += 44;
        double gw = L.main.w - PAD * 2 - 40;
        if (gw > 640) gw = 640;
        double rh = 42;
        int idx = 0;
        /* Genel */
        y += 26;
        for (int k = 0; k < 5; k++) {
            g_srow[idx] = R(x, y, gw, rh);
            g_sval[idx] = R(x + gw - 190, y + 6, 180, rh - 12);
            hit_add(H_SET_ROW, idx, g_srow[idx]);
            y += rh;
            idx++;
        }
        y += 30;
        /* Ajan */
        for (int k = 0; k < 2; k++) {
            g_srow[idx] = R(x, y, gw, rh);
            g_sval[idx] = R(x + gw - 190, y + 6, 180, rh - 12);
            hit_add(H_SET_ROW, idx, g_srow[idx]);
            y += rh;
            idx++;
        }
        g_sval[SR_WORK] = R(x + 210, y - rh + 6, gw - 300, rh - 12);
        y += 30;
        /* Baglanti */
        for (int k = 0; k < 4; k++) {
            g_srow[idx] = R(x, y, gw, rh);
            g_sval[idx] = R(x + 210, y + 6, gw - 226, rh - 12);
            hit_add(H_SET_ROW, idx, g_srow[idx]);
            y += rh;
            idx++;
        }
        y += 16;
        L.setbtn = R(x, y, 130, 40);
        L.closebtn = R(x + 142, y, 110, 40);
        hit_add(H_SET_SAVE, 0, L.setbtn);
        hit_add(H_SET_CLOSE, 0, L.closebtn);
        g_set_bottom = y + 40;
    }
    if (!painting) sk_end();
}

/* ---- yerlesim sonrasi metin kutularini yerlestir ---- */
static void place_edits(void)
{
    double s = sk_scale();
#define MV(hw, r, vis)                                                    \
    do {                                                                  \
        if (hw) {                                                         \
            MoveWindow((hw), (int)((r).x * s), (int)((r).y * s),           \
                       (int)((r).w * s), (int)((r).h * s), TRUE);          \
            ShowWindow((hw), (vis) ? SW_SHOWNA : SW_HIDE);                 \
        }                                                                 \
    } while (0)
    int chat = (g_page == 0 && !g_login_open && !modal_active());
    MV(g_edComposer, R(L.composer.x + 15, L.composer.y + 14,
                       L.composer.w - 15 - 60, L.composer.h - 28), chat && g_composer_on);
    MV(g_edSearch, R(L.search.x + 34, L.search.y + 6, L.search.w - 44,
                     L.search.h - 12),
       g_sidebar_open && !g_login_open && g_page == 0 && !modal_active());
    int lg = g_login_open && !modal_active();
    MV(g_edMail, R(L.lcard.x + 34, L.lcard.y + 113, L.lcard.w - 68, 24), lg);
    MV(g_edPass, R(L.lcard.x + 34, L.lcard.y + 179, L.lcard.w - 68, 24), lg);
    int st = (g_page == 1 && !g_login_open && !modal_active());
    MV(g_edWork, R(g_sval[SR_WORK].x, g_sval[SR_WORK].y, g_sval[SR_WORK].w - 84,
                   g_sval[SR_WORK].h), st);
    MV(g_edEnd, g_sval[SR_END], st);
    MV(g_edKey, g_sval[SR_KEY], st);
    MV(g_edTok, g_sval[SR_TOK], st);
#undef MV
}

/* ================= cizim: parcalar ================= */
static void draw_ibar(Rect r, int icon, int kind, DWORD col)
{
    int hov = (g_hover_kind == kind);
    sk_round(r.x, r.y, r.w, r.h, 8, hov ? SK.hbtn_hi : SK.hbtn);
    sk_icon(icon, r.x + r.w / 2, r.y + r.h / 2, 17, col, 1.7);
}

static void draw_header(double w, double h)
{
    wchar_t b[128];
    sk_fill(0, 0, w, HS, SK.header);
    sk_line(0, HS - 0.5, w, HS - 0.5, SK.border, 1);

    draw_ibar(L.sbbtn, SKI_SIDEBAR, H_SIDEBAR, SK.fg);
    draw_ibar(L.ncbtn, SKI_PENCIL, H_NEWCHATBTN, SK.fg);

    /* segment */
    sk_round(L.seg0.x, L.seg0.y, L.seg0.w + L.seg1.w, 34, 8, SK.hbtn);
    int chat = g_cfg.mode == 0;
    Rect sel = chat ? L.seg0 : L.seg1;
    sk_round4(sel.x, sel.y, sel.w, sel.h, chat ? 8 : 0, chat ? 0 : 8,
              chat ? 0 : 8, chat ? 8 : 0, SK.accent);
    sk_text_box(L.seg0.x, L.seg0.y, L.seg0.w, L.seg0.h, T(S_CHAT, b, 128), 14, 700, 0,
                chat ? 0xFFFFFFFF : SK.fg, 1);
    sk_text_box(L.seg1.x, L.seg1.y, L.seg1.w, L.seg1.h, T(S_AGENT, b, 128), 14, 700, 0,
                chat ? SK.fg : 0xFFFFFFFF, 1);

    /* model secici */
    sk_round(L.combo.x, L.combo.y, L.combo.w, L.combo.h, 8,
             g_hover_kind == H_COMBO ? SK.hbtn_hi : SK.hbtn);
    T(g_cfg.model ? S_MODEL_LITE : S_MODEL_FLASH, b, 128);
    sk_text_box(L.combo.x + 14, L.combo.y, L.combo.w - 46, L.combo.h, b, 14, 400, 0,
                SK.fg, 0);
    sk_icon(SKI_CHEV_D, L.combo.x + L.combo.w - 18, L.combo.y + L.combo.h / 2, 14,
            SK.fg, 1.6);

    draw_ibar(L.hmenu, SKI_MENU, H_MENU, SK.fg);

    /* pencere dugmeleri */
    Rect wr[3] = { L.bmin, L.bmax, L.bclose };
    int wi[3] = { SKI_MIN, IsZoomed(g_hMain) ? SKI_RESTORE : SKI_MAX, SKI_CLOSE };
    int wk[3] = { H_MIN, H_MAX, H_CLOSE };
    for (int i = 0; i < 3; i++) {
        int hov = g_hover_kind == wk[i];
        if (hov)
            sk_round(wr[i].x, wr[i].y, wr[i].w, wr[i].h, 15,
                     wk[i] == H_CLOSE ? SK.danger : (SK.fg & 0x00FFFFFF) | 0x1F000000);
        sk_icon(wi[i], wr[i].x + wr[i].w / 2, wr[i].y + wr[i].h / 2, 13,
                (hov && wk[i] == H_CLOSE) ? 0xFFFFFFFF : SK.fg, 1.5);
    }
}

static void draw_sidebar(double w, double h)
{
    wchar_t b[128], b2[128];
    (void)w; (void)h;
    if (!g_sidebar_open) return;
    sk_fill(0, HS, SW, h - HS, SK.side);
    sk_line(SW - 0.5, HS, SW - 0.5, h, SK.border, 1);

    /* yeni sohbet */
    int hov = g_hover_kind == H_NEWCHAT;
    if (hov) sk_round(L.newchat.x, L.newchat.y, L.newchat.w, L.newchat.h, 10, 0xFF5B8CFF);
    else sk_round(L.newchat.x, L.newchat.y, L.newchat.w, L.newchat.h, 10, SK.accent);
    sk_icon(SKI_PLUS, L.newchat.x + 18, L.newchat.y + 17, 14, 0xFFFFFFFF, 1.8);
    T(S_NEW, b, 128);
    sk_text(L.newchat.x + 32, L.newchat.y + 9, L.newchat.w - 90, b, 14, 700, 0,
            0xFFFFFFFF, 0);
    sk_text(L.newchat.x + L.newchat.w - 12, L.newchat.y + 11, 0, L"Ctrl+N", TI_PX, 600,
            0, 0xB8FFFFFF, 2);

    /* arama */
    sk_round(L.search.x, L.search.y, L.search.w, L.search.h, 9,
             sk_is_dark() ? 0xFF2C2E3D : 0xFFE9EAEC);
    sk_rline(L.search.x, L.search.y, L.search.w, L.search.h, 9, SK.border, 1);
    sk_icon(SKI_SEARCH, L.search.x + 16, L.search.y + L.search.h / 2, 13, SK.dim, 1.5);

    /* sohbetler basligi */
    T(S_CONVERSATIONS, b, 128);
    for (int i = 0; b[i]; i++) b[i] = (wchar_t)towupper(b[i]);
    sk_text(18, L.search.y + L.search.h + 6, 200, b, TI_PX, 700, 0, SK.dim, 0);

    /* satirlar */
    if (!g_side_n)
        sk_text(18, L.list.y + 14, SW - 36, T(S_NO_CONVS, b2, 128), 13, 400, 0, SK.dim, 0);
    for (int i = 0; i < g_side_rows && g_side_row0 + i < g_side_n; i++) {
        int si = g_side_row0 + i;
        Rect r = R(8, L.list.y + i * 36 + 2, SW - 16, 32);
        int sel = !wcscmp(g_side[si].id, g_conv_id);
        int ho = (g_hover_kind == H_ROW && g_hover_idx == si);
        if (sel) sk_round(r.x, r.y, r.w, r.h, 9, SK.accent);
        else if (ho) sk_round(r.x, r.y, r.w, r.h, 9, (SK.fg & 0x00FFFFFF) | 0x12000000);
        sk_text_box(r.x + 10, r.y, r.w - (ho ? 44 : 24), r.h, g_side[si].title, 14,
                    sel ? 600 : 400, 0, sel ? 0xFFFFFFFF : SK.fg, 0);
        if (ho) {
            int hx = (g_hover_kind == H_ROWX && g_hover_idx == si);
            if (hx) sk_round(r.x + r.w - 30, r.y + 4, 26, 24, 7, (SK.fg & 0x00FFFFFF) | 0x1A000000);
            sk_icon(SKI_TRASH, r.x + r.w - 17, r.y + 16, 13,
                    sel ? 0xDDFFFFFF : SK.dim, 1.4);
        }
    }

    /* hesap karti */
    sk_line(0, L.acct.y, SW, L.acct.y, SK.border, 1);
    double ax = 14, ay = L.acct.y + 16;
    sk_circle(ax + 15, ay + 15, 15, SK.accent);
    sk_grad(ax, ay, 30, 30, 15, 0xFF3D6BFF, 0xFF8A4DFF, 1);
    if (g_token[0] && g_username[0]) {
        wchar_t in[2] = { g_username[0], 0 };
        for (int i = 0; in[i]; i++) in[i] = (wchar_t)towupper(in[i]);
        sk_text_box(ax, ay, 30, 30, in, 14, 700, 0, 0xFFFFFFFF, 1);
        sk_text_box(ax + 40, ay + 2, SW - ax - 60, 26,
                    g_username[0] ? g_username : g_email, 14, 700, 0, SK.fg, 0);
        sk_icon(SKI_CHEV_U, SW - 22, ay + 13, 12, SK.dim, 1.5);
    } else {
        sk_text_box(ax, ay, 30, 30, L"?", 15, 700, 0, 0xFFFFFFFF, 1);
        sk_text_box(ax + 40, ay + 2, SW - ax - 60, 26, T(S_SIGNED_OUT, b, 128), 14, 700,
                    0, SK.fg, 0);
        sk_icon(SKI_CHEV_U, SW - 22, ay + 13, 12, SK.dim, 1.5);
    }
    /* seviye cubugu */
    sk_round(18, L.acct.y + 46, SW - 36, 5, 3, (SK.fg & 0x00FFFFFF) | 0x1A000000);
    sk_round(18, L.acct.y + 46, (SW - 36) * 0.12, 5, 3, SK.accent);
}

static void draw_empty(void)
{
    wchar_t b[256];
    double cx = L.main.x + L.main.w / 2;
    if (L.logo.w <= 0) return;
    /* yumusak golge */
    sk_round(L.logo.x + 2, L.logo.y + 4, 64, 64, 17, 0x14000000);
    sk_logo(L.logo.x, L.logo.y, 64, 17);
    T(S_WELCOME, b, 256);
    sk_text(cx - L.main.w / 2, L.logo.y + 84, L.main.w, b, 26, 700, 0, SK.fg, 1);
    T(S_EMPTY_TEXT, b, 256);
    wrap_center(b, cx, L.logo.y + 84 + 34 + 12, 470, 14, 400, SK.dim, 1);
    if (L.ebtn.w > 0) {
        sk_round(L.ebtn.x, L.ebtn.y, L.ebtn.w, L.ebtn.h, 21,
                 g_hover_kind == H_EMPTY_MAIN ? 0xFF5B8CFF : SK.accent);
        T(S_SIGNIN_PILL, b, 256);
        sk_text_box(L.ebtn.x, L.ebtn.y, L.ebtn.w, L.ebtn.h, b, 15, 700, 0, 0xFFFFFFFF, 1);
        T(S_SIGNUP_LINK, b, 256);
        double lw = sk_tw_s(b, 14, 400, 0);
        sk_text_box(L.elink.x, L.elink.y, L.elink.w, L.elink.h, b, 14, 400, 0, SK.accent, 1);
        sk_line(cx - lw / 2, L.elink.y + 21, cx + lw / 2, L.elink.y + 21, SK.accent, 1);
    }
}

static void draw_messages(void)
{
    double x = L.msgs.x + 26;
    double area = L.msgs.w - 56;
    double top = L.msgs.y + 10;
    double viewh = L.msgs.h - 14;
    double y = top - g_scroll;

    /* toplam yukseklik */
    double tot = 0;
    for (int i = 0; i < g_conv.n; i++) {
        wchar_t *wt = u16(g_conv.v[i].content);
        tot += bubble(g_conv.v[i].role, wt ? wt : L"", g_conv.v[i].tool_calls, 0, 0,
                      area, 0);
        free(wt);
    }
    if (g_stream)
        tot += 17 + sk_th(CX_PX, 0) + 22 + 14;
    tot += 14;

    double maxs = tot - viewh;
    if (maxs < 0) maxs = 0;
    if (g_at_bottom) g_scroll = maxs;
    if (g_scroll > maxs) g_scroll = maxs;
    if (g_scroll < 0) g_scroll = 0;
    y = top - g_scroll;
    g_content_h = tot;

    sk_clip(L.msgs.x, L.msgs.y, L.msgs.w, L.msgs.h);
    for (int i = 0; i < g_conv.n; i++) {
        wchar_t *wt = u16(g_conv.v[i].content);
        double bh = bubble(g_conv.v[i].role, wt ? wt : L"", g_conv.v[i].tool_calls,
                           x, y, area, 1);
        free(wt);
        y += bh;
    }
    if (g_stream) {
        y += bubble("assistant", g_stream, NULL, x, y, area, 1);
    }
    sk_unclip();

    /* ince kaydirma cubugu */
    if (maxs > 0.5) {
        double th = viewh * (viewh / tot);
        if (th < 30) th = 30;
        double ty = L.msgs.y + (viewh - th) * (g_scroll / maxs);
        sk_round(L.msgs.x + L.msgs.w - 9, ty, 6, th, 3, (SK.fg & 0x00FFFFFF) | 0x33000000);
    }
}

static void draw_composer(void)
{
    wchar_t b[128];
    sk_round(L.composer.x, L.composer.y, L.composer.w, L.composer.h, 16, SK.input);
    sk_rline(L.composer.x, L.composer.y, L.composer.w, L.composer.h, 16, SK.border, 1);
    if (g_busy_ui) {
        sk_round(L.stop.x, L.stop.y, L.stop.w, L.stop.h, 15, SK.danger);
        sk_icon(SKI_STOP, L.stop.x + 15, L.stop.y + 15, 11, 0xFFFFFFFF, 1.5);
        sk_text_box(L.stop.x + 26, L.stop.y, L.stop.w - 36, L.stop.h, T(S_STOP, b, 128),
                    13, 700, 0, 0xFFFFFFFF, 0);
        sk_circle(L.send.x + 17, L.send.y + 17, 17, (SK.fg & 0x00FFFFFF) | 0x22000000);
        sk_icon(SKI_UP, L.send.x + 17, L.send.y + 17, 16, 0x66FFFFFF, 1.8);
    } else {
        int hov = g_hover_kind == H_SEND;
        sk_circle(L.send.x + 17, L.send.y + 17, 17, hov ? 0xFF5B8CFF : SK.accent);
        sk_icon(SKI_UP, L.send.x + 17, L.send.y + 17, 16, 0xFFFFFFFF, 1.9);
    }
    {
        const char *src = g_status[0] ? g_status : t(S_READY);
        wchar_t *ws = u16(src);
        if (ws) {
            sk_text_box(L.status.x, L.status.y, L.status.w, L.status.h, ws, 12, 400, 0,
                        SK.dim, 0);
            free(ws);
        }
    }
    (void)b;
}

static void draw_login(double w, double h)
{
    wchar_t b[256], b2[256];
    sk_fill(0, 0, w, h, 0x66000000);
    Rect c = L.lcard;
    sk_round(c.x + 2, c.y + 6, c.w, c.h, 16, 0x33000000);
    sk_round(c.x, c.y, c.w, c.h, 14, SK.card);
    sk_rline(c.x, c.y, c.w, c.h, 14, SK.border, 1);
    sk_text(c.x + 22, c.y + 18, c.w - 60, T(S_LOGIN_TITLE, b, 256), 17, 700, 0, SK.fg, 0);
    sk_icon(SKI_CLOSE, L.lclose.x + 14, L.lclose.y + 14, 13, SK.dim, 1.5);
    T(S_LOGIN_HINT, b2, 256);
    wrap_center(b2, c.x + c.w / 2, c.y + 46, c.w - 44, 12, 400, SK.dim, 1);
    sk_text(c.x + 22, c.y + 82, 200, T(S_EMAIL, b, 256), 12, 600, 0, SK.dim, 0);
    sk_text(c.x + 22, c.y + 148, 200, T(S_PASSWORD, b, 256), 12, 600, 0, SK.dim, 0);
    sk_round(c.x + 21, c.y + 107, c.w - 42, 36, 9, SK.card);
    sk_rline(c.x + 21, c.y + 107, c.w - 42, 36, 9, SK.border, 1);
    sk_round(c.x + 21, c.y + 173, c.w - 42, 36, 9, SK.card);
    sk_rline(c.x + 21, c.y + 173, c.w - 42, 36, 9, SK.border, 1);
    /* dugmeler */
    sk_round(L.lgo.x, L.lgo.y, L.lgo.w, L.lgo.h, 18, SK.accent);
    sk_text_box(L.lgo.x, L.lgo.y, L.lgo.w, L.lgo.h, T(S_LOGIN, b, 256), 14, 700, 0,
                0xFFFFFFFF, 1);
    sk_round(L.lweb.x, L.lweb.y, L.lweb.w, L.lweb.h, 18,
             (SK.fg & 0x00FFFFFF) | 0x0F000000);
    sk_text_box(L.lweb.x, L.lweb.y, L.lweb.w, L.lweb.h, T(S_REGISTER, b, 256), 13, 600,
                0, SK.fg, 1);
    sk_round(L.lcancel.x, L.lcancel.y, L.lcancel.w, L.lcancel.h, 18,
             (SK.fg & 0x00FFFFFF) | 0x0F000000);
    sk_text_box(L.lcancel.x, L.lcancel.y, L.lcancel.w, L.lcancel.h, T(S_CANCEL, b, 256),
                13, 600, 0, SK.fg, 1);
    if (g_status[0] && g_login_open) {
        wchar_t *ws = u16(g_status);
        if (ws) {
            sk_text_box(c.x + 22, c.y + c.h - 96, c.w - 44, 22, ws, 12, 400, 0,
                        SK.danger, 0);
            free(ws);
        }
    }
}

/* ---- ayarlar sayfasi ---- */
static const wchar_t *sval_text(int idx, wchar_t *b, size_t cch)
{
    b[0] = 0;
    switch (idx) {
    case SR_MODEL: T(g_cfg.model ? S_MODEL_LITE : S_MODEL_FLASH, b, cch); break;
    case SR_MODE: T(g_cfg.mode ? S_AGENT : S_CHAT, b, cch); break;
    case SR_LANG: {
        const wchar_t *ln = u16(lang_name(g_cfg.lang));
        wcsncpy(b, ln ? ln : L"English", cch - 1);
        b[cch - 1] = 0;
        free((void *)ln);
        break;
    }
    case SR_THEME:
        T(g_cfg.theme == 0 ? S_THEME_SYS : (g_cfg.theme == 1 ? S_THEME_LIGHT : S_THEME_DARK),
          b, cch);
        break;
    case SR_SENDKEY:
        T(g_cfg.send_ctrl_enter ? S_CTRLENTER : S_ENTER, b, cch);
        break;
    default: break;
    }
    return b;
}

static void draw_toggle(Rect r, int on)
{
    double tw_ = 38, th_ = 21;
    double x = r.x, y = r.y + (r.h - th_) / 2;
    sk_round(x, y, tw_, th_, th_ / 2, on ? SK.accent : ((SK.fg & 0x00FFFFFF) | 0x2A000000));
    sk_circle(on ? x + tw_ - th_ / 2 : x + th_ / 2, y + th_ / 2, th_ / 2 - 2.5, 0xFFFFFFFF);
}

static void draw_settings(void)
{
    wchar_t b[256];
    double x = L.main.x + PAD + 4, y = HS + 12;
    sk_icon(SKI_BACK, L.back.x + 15, L.back.y + 14, 16, SK.fg, 1.8);
    sk_text(x + 42, y + 2, 300, T(S_SETTINGS, b, 256), 20, 700, 0, SK.fg, 0);

    const wchar_t *g1 = L"Genel";
    T(S_SETTINGS, b, 256);
    double gw = 0, gx = x, gy = 0;
    /* grup basliklari + satirlar */
    gw = g_srow[SR_MODEL].w;
    gx = g_srow[SR_MODEL].x;
    gy = g_srow[SR_MODEL].y;
    sk_text(gx + 4, gy - 24, 300, g1, 13, 700, 0, SK.fg, 0);
    sk_round(gx, gy, gw, 5 * 42, 12, SK.card);
    sk_rline(gx, gy, gw, 5 * 42, 12, SK.border, 1);
    for (int i = 0; i < 5; i++) {
        if (i) sk_line(g_srow[i].x + 14, g_srow[i].y, g_srow[i].x + gw - 14,
                       g_srow[i].y, SK.border, 1);
        sk_text_box(g_srow[i].x + 16, g_srow[i].y, 260, 42,
                    T(i == 0 ? S_MODEL : i == 1 ? S_MODE : i == 2 ? S_LANGUAGE :
                      i == 3 ? S_THEME : S_SENDKEY, b, 256), 14, 400, 0, SK.fg, 0);
        sk_text_box(g_sval[i].x, g_sval[i].y, g_sval[i].w - 26, g_sval[i].h,
                    sval_text(i, b, 256), 14, 400, 0, SK.dim, 2);
        sk_icon(SKI_CHEV_D, g_sval[i].x + g_sval[i].w - 12, g_sval[i].y + g_sval[i].h / 2,
                13, SK.dim, 1.5);
    }
    double y2 = g_srow[SR_AUTO].y;
    sk_text(gx + 4, y2 - 24, 300, T(S_AGENT, b, 256), 13, 700, 0, SK.fg, 0);
    sk_round(gx, y2, gw, 2 * 42, 12, SK.card);
    sk_rline(gx, y2, gw, 2 * 42, 12, SK.border, 1);
    sk_line(gx + 14, g_srow[SR_WORK].y, gx + gw - 14, g_srow[SR_WORK].y, SK.border, 1);
    sk_text_box(gx + 16, y2, 320, 42, T(S_AUTOAPPROVE, b, 256), 14, 400, 0, SK.fg, 0);
    draw_toggle(R(gx + gw - 62, y2, 46, 42), g_cfg.auto_approve);
    sk_text_box(gx + 16, g_srow[SR_WORK].y, 260, 42, T(S_WORKDIR, b, 256), 14, 400, 0,
                SK.fg, 0);
    sk_round(g_sval[SR_WORK].x - 4, g_sval[SR_WORK].y, g_sval[SR_WORK].w + 8,
             g_sval[SR_WORK].h, 8, SK.code);
    sk_text_box(g_sval[SR_WORK].x + g_sval[SR_WORK].w - 52, g_sval[SR_WORK].y, 48,
                g_sval[SR_WORK].h, T(S_BROWSE, b, 256), 12, 600, 0, SK.accent, 1);

    double y3 = g_srow[SR_OPENAI].y;
    sk_text(gx + 4, y3 - 24, 300, T(S_CONNECTION, b, 256), 13, 700, 0, SK.fg, 0);
    sk_round(gx, y3, gw, 4 * 42, 12, SK.card);
    sk_rline(gx, y3, gw, 4 * 42, 12, SK.border, 1);
    sk_text_box(gx + 16, y3, 320, 42, T(S_OPENAI, b, 256), 14, 400, 0, SK.fg, 0);
    draw_toggle(R(gx + gw - 62, y3, 46, 42), g_cfg.use_openai);
    for (int k = 1; k < 4; k++) {
        int i = SR_OPENAI + k;
        sk_line(gx + 14, g_srow[i].y, gx + gw - 14, g_srow[i].y, SK.border, 1);
        sk_text_box(gx + 16, g_srow[i].y, 200, 42,
                    T(i == SR_END ? S_ENDPOINT : i == SR_KEY ? S_APIKEY : S_MAXTOK, b, 256),
                    14, 400, 0, SK.fg, 0);
        sk_round(g_sval[i].x - 4, g_sval[i].y, g_sval[i].w + 8, g_sval[i].h, 8, SK.code);
    }
    sk_round(L.setbtn.x, L.setbtn.y, L.setbtn.w, L.setbtn.h, 20, SK.accent);
    sk_text_box(L.setbtn.x, L.setbtn.y, L.setbtn.w, L.setbtn.h, T(S_SAVE, b, 256), 14,
                700, 0, 0xFFFFFFFF, 1);
    sk_round(L.closebtn.x, L.closebtn.y, L.closebtn.w, L.closebtn.h, 20,
             (SK.fg & 0x00FFFFFF) | 0x14000000);
    sk_text_box(L.closebtn.x, L.closebtn.y, L.closebtn.w, L.closebtn.h,
                T(S_CANCEL, b, 256), 14, 600, 0, SK.fg, 1);
}

/* ================= ana cizim ================= */
static void draw_all(double w, double h)
{
    compute_layout(w, h, 1);
    sk_fill(0, 0, w, h, SK.bg);
    if (g_page == 1)
        sk_fill(L.main.x, HS, L.main.w, h - HS, SK.bg);
    draw_header(w, h);
    draw_sidebar(w, h);
    if (g_page == 1) {
        draw_settings();
    } else if (!g_conv.n && !g_stream) {
        draw_empty();
    } else {
        draw_messages();
    }
    if (g_page == 0 && g_composer_on) draw_composer();
    if (g_login_open) draw_login(w, h);
    place_edits();
}

/* ================= acilir menu ================= */
#define WM_APP_POPUP (WM_APP + 20)
enum { P_MODEL = 1, P_MODE, P_LANG, P_THEME, P_SENDKEY, P_MENU };

typedef struct {
    int id, n, cur, hov;
    const wchar_t **items;
    HWND h;
    int dpi;
} PopD;
static PopD *g_pop;

static LRESULT CALLBACK popup_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE: {
        HDC dc = GetDC(h);
        sk_begin(dc);
        sk_end();
        ReleaseDC(h, dc);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        sk_begin(dc);
        double wl2 = rc.right / sk_scale(), hl2 = rc.bottom / sk_scale();
        sk_round(0, 0, wl2, hl2, 10, SK.card);
        sk_rline(0.5, 0.5, wl2 - 1, hl2 - 1, 10, SK.border, 1);
        for (int i = 0; g_pop && i < g_pop->n; i++) {
            double y = 5 + i * 34;
            if (g_pop->hov == i)
                sk_round(4, y, wl2 - 8, 32, 7, (SK.accent & 0x00FFFFFF) | 0xD9000000);
            sk_text_box(14, y, wl2 - 40, 32, g_pop->items[i], 14, 400, 0,
                        g_pop->hov == i ? 0xFFFFFFFF : SK.fg, 0);
            if (g_pop->cur == i)
                sk_icon(SKI_SPARKLE, wl2 - 18, y + 16, 12,
                        g_pop->hov == i ? 0xFFFFFFFF : SK.accent, 1.5);
        }
        sk_end();
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!g_pop) return 0;
        int y = GET_Y_LPARAM(l);
        int i = (int)((y - 5) / 34);
        if (i < 0 || i >= g_pop->n) i = -1;
        if (i != g_pop->hov) { g_pop->hov = i; InvalidateRect(h, NULL, FALSE); }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!g_pop) return 0;
        int y = GET_Y_LPARAM(l);
        int i = (int)((y - 5) / 34);
        if (i >= 0 && i < g_pop->n) {
            PostMessageW(g_hMain, WM_APP_POPUP, (WPARAM)g_pop->id, (LPARAM)i);
            DestroyWindow(h);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) { DestroyWindow(h); return 0; }
        if (g_pop) {
            if (w == VK_DOWN) { g_pop->hov++; if (g_pop->hov >= g_pop->n) g_pop->hov = 0; InvalidateRect(h, NULL, FALSE); }
            if (w == VK_UP) { g_pop->hov--; if (g_pop->hov < 0) g_pop->hov = g_pop->n - 1; InvalidateRect(h, NULL, FALSE); }
            if (w == VK_RETURN && g_pop->hov >= 0) {
                PostMessageW(g_hMain, WM_APP_POPUP, (WPARAM)g_pop->id, (LPARAM)g_pop->hov);
                DestroyWindow(h);
            }
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE) DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        free(g_pop);
        g_pop = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void popup_open(int id, Rect anchor, const wchar_t **items, int n, int cur)
{
    if (g_pop) { HWND old = g_pop->h; g_pop = NULL; DestroyWindow(old); }
    static int reg;
    if (!reg) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = popup_proc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = L"LuxPopup";
        RegisterClassExW(&wc);
        reg = 1;
    }
    g_pop = (PopD *)calloc(1, sizeof(PopD));
    if (!g_pop) return;
    g_pop->id = id;
    g_pop->n = n;
    g_pop->cur = cur;
    g_pop->hov = cur;
    g_pop->items = items;
    double s = sk_scale();
    int wpx = (int)((236) * s), hpx = (int)((n * 34 + 10) * s);
    RECT wr;
    GetWindowRect(g_hMain, &wr);
    int x = wr.left + (int)(anchor.x * s);
    int y = wr.top + (int)((anchor.y + anchor.h + 4) * s);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    if (x + wpx > sw) x = sw - wpx - 4;
    if (y + hpx > sh) y = wr.top + (int)((anchor.y - n * 34 - 8) * s);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    g_pop->dpi = (int)s;
    HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"LuxPopup", L"",
                             WS_POPUP, x, y, wpx, hpx, g_hMain, NULL, g_inst, NULL);
    if (!h) { free(g_pop); g_pop = NULL; return; }
    g_pop->h = h;
    ShowWindow(h, SW_SHOWNA);
    SetFocus(h);
    InvalidateRect(h, NULL, FALSE);
}

/* ================= eylemler ================= */
static void do_login_flow(const wchar_t *email, const wchar_t *pass);
static void do_send(void);
static void settings_open(void);
static void settings_save(void);

static void new_chat(void)
{
    ui_conversation_save();
    ui_conversation_clear();
    ui_sidebar_reload();
    g_page = 0;
    layout_soft();
    InvalidateRect(g_hMain, NULL, FALSE);
}

static void pick_conversation(int i)
{
    if (i < 0 || i >= g_side_n) return;
    if (wcscmp(g_side[i].id, g_conv_id)) {
        ui_conversation_save();
        conv_load(g_side[i].id);
        ui_sidebar_reload();
    }
}

static void delete_conversation(int i)
{
    if (i < 0 || i >= g_side_n) return;
    wchar_t b[256], t[256];
    T(S_DELETE, b, 256);
    T(S_DELETEBTN, t, 256);
    if (!confirm_ui(t, b, 1)) return;
    wchar_t dir[MAX_PATH], path[MAX_PATH * 2];
    conv_dir(dir, MAX_PATH);
    _snwprintf(path, MAX_PATH * 2, L"%s\\%s.json", dir, g_side[i].id);
    DeleteFileW(path);
    if (!wcscmp(g_side[i].id, g_conv_id)) {
        msglist_clear(&g_conv);
        new_conv_id();
    }
    ui_sidebar_reload();
    InvalidateRect(g_hMain, NULL, FALSE);
}

static void do_logout(void)
{
    wchar_t t1[192], t2[600];
    T(S_LOGOUT_Q, t1, 192);
    T(S_LOGOUT_BODY, t2, 600);
    if (!confirm_ui(t1, t2, 1)) return;
    account_clear();
    g_me_name[0] = g_me_email[0] = g_me_plan[0] = g_me_user[0] = 0;
    g_me_used = g_me_limit = -1;
    ui_set_status(t(S_NOT_SIGNED));
    layout_soft();
    InvalidateRect(g_hMain, NULL, FALSE);
}

static void on_hit(Hit hit)
{
    wchar_t b[128];
    switch (hit.kind) {
    case H_MIN: ShowWindow(g_hMain, SW_MINIMIZE); break;
    case H_MAX: ShowWindow(g_hMain, IsZoomed(g_hMain) ? SW_RESTORE : SW_MAXIMIZE); break;
    case H_CLOSE: PostMessageW(g_hMain, WM_CLOSE, 0, 0); break;
    case H_SIDEBAR: g_sidebar_open = !g_sidebar_open; layout_soft(); InvalidateRect(g_hMain, NULL, FALSE); break;
    case H_NEWCHATBTN: case H_NEWCHAT: new_chat(); break;
    case H_SEG_CHAT: g_cfg.mode = 0; cfg_save(); InvalidateRect(g_hMain, NULL, FALSE); break;
    case H_SEG_AGENT: g_cfg.mode = 1; cfg_save(); InvalidateRect(g_hMain, NULL, FALSE); break;
    case H_COMBO: {
        static const wchar_t *it[2];
        wchar_t a[128], c[128];
        it[0] = _wcsdup(T(S_MODEL_FLASH, a, 128));
        it[1] = _wcsdup(T(S_MODEL_LITE, c, 128));
        popup_open(P_MODEL, L.combo, it, 2, g_cfg.model);
        break;
    }
    case H_MENU: {
        static const wchar_t *it[5];
        wchar_t a[128];
        it[0] = _wcsdup(T(S_NEW, a, 128));
        it[1] = _wcsdup(T(S_SETTINGS, a, 128));
        it[2] = _wcsdup(g_token[0] ? T(S_ACCT_TITLE, a, 128) : T(S_LOGIN, a, 128));
        it[3] = _wcsdup(g_token[0] ? T(S_LOGOUT, a, 128) : T(S_ACCT_TITLE, a, 128));
        it[4] = _wcsdup(L"GitHub");
        popup_open(P_MENU, L.hmenu, it, 5, -1);
        break;
    }
    case H_SEARCH: SetFocus(g_edSearch); break;
    case H_ROW: pick_conversation(hit.idx); break;
    case H_ROWX: delete_conversation(hit.idx); break;
    case H_ACCT:
        if (g_token[0]) account_open();
        else { g_login_open = 1; g_status[0] = 0; layout_soft(); InvalidateRect(g_hMain, NULL, FALSE); SetFocus(g_edMail); }
        break;
    case H_SEND: do_send(); break;
    case H_STOP: chat_cancel(); break;
    case H_EMPTY_MAIN:
        if (!g_cfg.use_openai && !g_token[0]) {
            g_login_open = 1;
            g_status[0] = 0;
            layout_soft();
            InvalidateRect(g_hMain, NULL, FALSE);
            SetFocus(g_edMail);
        } else {
            SetFocus(g_edComposer);
        }
        break;
    case H_EMPTY_LINK:
        ShellExecuteW(NULL, L"open", L"https://lux-ai.cz/register", NULL, NULL, SW_SHOWNORMAL);
        break;
    case H_LOGIN_CLOSE:
    case H_LOGIN_CANCEL:
        g_login_open = 0;
        g_status[0] = 0;
        layout_soft();
        InvalidateRect(g_hMain, NULL, FALSE);
        break;
    case H_LOGIN_WEB:
        ShellExecuteW(NULL, L"open", L"https://lux-ai.cz/register", NULL, NULL, SW_SHOWNORMAL);
        break;
    case H_LOGIN_GO: {
        wchar_t *em = NULL, *pw = NULL;
        int n1 = GetWindowTextLengthW(g_edMail), n2 = GetWindowTextLengthW(g_edPass);
        em = (wchar_t *)calloc((size_t)n1 + 2, sizeof(wchar_t));
        pw = (wchar_t *)calloc((size_t)n2 + 2, sizeof(wchar_t));
        if (em && pw) {
            GetWindowTextW(g_edMail, em, n1 + 1);
            GetWindowTextW(g_edPass, pw, n2 + 1);
            do_login_flow(em, pw);
        }
        free(em);
        free(pw);
        break;
    }
    case H_SET_BACK: case H_SET_CLOSE:
        g_page = 0;
        layout_soft();
        InvalidateRect(g_hMain, NULL, FALSE);
        break;
    case H_SET_SAVE: settings_save(); g_page = 0; layout_soft(); InvalidateRect(g_hMain, NULL, FALSE); break;
    case H_SET_ROW: {
        int i = hit.idx;
        static const wchar_t *it[4];
        wchar_t a[128];
        if (i == SR_AUTO) { g_cfg.auto_approve = !g_cfg.auto_approve; cfg_save(); InvalidateRect(g_hMain, NULL, FALSE); break; }
        if (i == SR_OPENAI) { g_cfg.use_openai = !g_cfg.use_openai; cfg_save(); InvalidateRect(g_hMain, NULL, FALSE); break; }
        if (i >= SR_WORK) { SetFocus(i == SR_WORK ? g_edWork : i == SR_END ? g_edEnd : i == SR_KEY ? g_edKey : g_edTok); break; }
        if (i == SR_MODEL) {
            it[0] = _wcsdup(T(S_MODEL_FLASH, a, 128));
            it[1] = _wcsdup(T(S_MODEL_LITE, a, 128));
            popup_open(P_MODEL, g_sval[i], it, 2, g_cfg.model);
        } else if (i == SR_MODE) {
            it[0] = _wcsdup(T(S_CHAT, a, 128));
            it[1] = _wcsdup(T(S_AGENT, a, 128));
            popup_open(P_MODE, g_sval[i], it, 2, g_cfg.mode);
        } else if (i == SR_LANG) {
            static const wchar_t *lang_it[LUX_NLANG];
            for (int k = 0; k < LUX_NLANG; k++) {
                wchar_t *wn = u16(lang_name(k));
                lang_it[k] = wn ? wn : L"English";
            }
            popup_open(P_LANG, g_sval[i], (const wchar_t **)lang_it, LUX_NLANG, g_cfg.lang);
        } else if (i == SR_THEME) {
            it[0] = _wcsdup(T(S_THEME_SYS, a, 128));
            it[1] = _wcsdup(T(S_THEME_LIGHT, a, 128));
            it[2] = _wcsdup(T(S_THEME_DARK, a, 128));
            popup_open(P_THEME, g_sval[i], it, 3, g_cfg.theme);
        } else if (i == SR_SENDKEY) {
            it[0] = _wcsdup(L"Enter");
            it[1] = _wcsdup(L"Ctrl+Enter");
            popup_open(P_SENDKEY, g_sval[i], it, 2, g_cfg.send_ctrl_enter ? 1 : 0);
        }
        break;
    }
    default: break;
    }
}

/* ================= giris ================= */
static int resp_sink_ui(const char *d, size_t n, void *ud)
{
    Buf *b = (Buf *)ud;
    buf_add(b, d, n);
    return 0;
}

static int do_login(const wchar_t *email, const wchar_t *pass, wchar_t *err, size_t errcch)
{
    Buf body;
    buf_init(&body);
    char *e8 = u8(email), *p8 = u8(pass);
    buf_adds(&body, "{\"email\":");
    json_write_str(&body, e8 ? e8 : "");
    buf_adds(&body, ",\"password\":");
    json_write_str(&body, p8 ? p8 : "");
    buf_addc(&body, '}');
    free(e8);
    free(p8);

    Buf resp, errb;
    buf_init(&resp);
    buf_init(&errb);
    int status = 0;
    volatile LONG cancel = 0;
    http_request(L"https://lux-ai.cz/api/auth/password/login",
                 L"Content-Type: application/json; charset=utf-8\r\nX-Lux-Client: luxai\r\n",
                 body.p, body.n, resp_sink_ui, &resp, &status, &errb, &cancel);
    buf_free(&body);
    int ok = 0;
    if (status == 200 && resp.p) {
        JVal *v = json_parse(resp.p);
        const char *tok = json_str(v, "sessionToken");
        if (tok) {
            wchar_t *wt = u16(tok);
            if (wt) { wcsncpy(g_token, wt, 1023); g_token[1023] = 0; free(wt); }
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
        err[errcch - 1] = 0;
    } else if (status == 0) {
        wcsncpy(err, tw(S_NET_ERR), errcch - 1);
        err[errcch - 1] = 0;
    } else {
        _snwprintf(err, errcch, L"HTTP %d", status);
        err[errcch - 1] = 0;
    }
    buf_free(&resp);
    buf_free(&errb);
    return ok;
}

static void do_login_flow(const wchar_t *email, const wchar_t *pass)
{
    wchar_t err[300];
    err[0] = 0;
    SetCursor(LoadCursorW(NULL, IDC_WAIT));
    int ok = do_login(email, pass, err, 300);
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
    if (ok) {
        g_login_open = 0;
        SetWindowTextW(g_edPass, L"");
        ui_set_status(t(S_READY));
        layout_soft();
    } else {
        char *e = u8(err[0] ? err : tw(S_LOGIN_FAIL));
        ui_set_status(e ? e : "login failed");
        free(e);
    }
    InvalidateRect(g_hMain, NULL, FALSE);
}

/* ================= gonderim ================= */
static wchar_t *get_text(HWND h)
{
    int n = GetWindowTextLengthW(h);
    wchar_t *w = (wchar_t *)calloc((size_t)n + 2, sizeof(wchar_t));
    if (!w) return NULL;
    GetWindowTextW(h, w, n + 1);
    return w;
}

static void do_send(void)
{
    if (chat_busy() || g_busy_ui) return;
    wchar_t *w = get_text(g_edComposer);
    if (!w) return;
    wchar_t *p = w;
    while (*p == L' ' || *p == L'\r' || *p == L'\n' || *p == L'\t') p++;
    if (!*p) { free(w); return; }
    if (p != w) memmove(w, p, (wcslen(p) + 1) * sizeof(wchar_t));
    if (!g_cfg.use_openai && !g_token[0]) {
        free(w);
        g_login_open = 1;
        g_status[0] = 0;
        layout_soft();
        InvalidateRect(g_hMain, NULL, FALSE);
        SetFocus(g_edMail);
        return;
    }
    char *u8txt = u8(w);
    free(w);
    SetWindowTextW(g_edComposer, L"");
    msglist_add(&g_conv, "user", u8txt ? u8txt : "", NULL, NULL);
    stream_clear();
    g_busy_ui = 1;
    g_at_bottom = 1;
    ui_set_status(t(S_THINKING));
    layout_soft();
    InvalidateRect(g_hMain, NULL, FALSE);
    chat_send(u8txt ? u8txt : "");
    free(u8txt);
}

/* ================= ayarlar ================= */
static void settings_open(void)
{
    g_page = 1;
    g_login_open = 0;
    wchar_t b[512];
    SetWindowTextW(g_edWork, g_cfg.workdir);
    {
        wchar_t *we = u16(g_cfg.endpoint);
        SetWindowTextW(g_edEnd, we ? we : L"");
        free(we);
        wchar_t *wk = u16(g_cfg.api_key);
        SetWindowTextW(g_edKey, wk ? wk : L"");
        free(wk);
    }
    _snwprintf(b, 512, L"%d", g_cfg.max_tokens);
    SetWindowTextW(g_edTok, b);
    layout_soft();
    InvalidateRect(g_hMain, NULL, FALSE);
}

static void settings_save(void)
{
    wchar_t *w = get_text(g_edWork);
    if (w && *w) { wcsncpy(g_cfg.workdir, w, MAX_PATH - 1); g_cfg.workdir[MAX_PATH - 1] = 0; }
    free(w);
    w = get_text(g_edEnd);
    if (w) {
        char *x = u8(w);
        if (x) { strncpy(g_cfg.endpoint, x, sizeof g_cfg.endpoint - 1); g_cfg.endpoint[sizeof g_cfg.endpoint - 1] = 0; free(x); }
        free(w);
    }
    w = get_text(g_edKey);
    if (w) {
        char *x = u8(w);
        if (x) { strncpy(g_cfg.api_key, x, sizeof g_cfg.api_key - 1); g_cfg.api_key[sizeof g_cfg.api_key - 1] = 0; free(x); }
        free(w);
    }
    w = get_text(g_edTok);
    if (w) {
        int v = _wtoi(w);
        if (v >= 256) g_cfg.max_tokens = v;
        free(w);
    }
    cfg_save();
    theme_apply();
}

/* ================= metin kutusu alt sinifi ================= */
static const wchar_t *edit_hint(HWND h, wchar_t *b, size_t cch)
{
    b[0] = 0;
    if (h == g_edComposer) T(S_MSG_HINT, b, cch);
    else if (h == g_edSearch) T(S_SEARCH_PH, b, cch);
    else if (h == g_edMail) wcsncpy(b, L"ornek@lux-ai.cz", cch - 1);
    else if (h == g_edPass) T(S_PASSWORD, b, cch);
    b[cch - 1] = 0;
    return b;
}

static LRESULT CALLBACK edit_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id,
                                  DWORD_PTR ref)
{
    (void)id; (void)ref;
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        if (h == g_edComposer) {
            BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int sendnow = g_cfg.send_ctrl_enter ? (ctrl && !shift) : (!ctrl && !shift);
            if (sendnow) { do_send(); return 0; }
        } else if (h == g_edMail) {
            SetFocus(g_edPass);
            return 0;
        } else if (h == g_edPass) {
            on_hit((Hit){ H_LOGIN_GO, 0, R(0, 0, 0, 0) });
            return 0;
        }
    }
    if (m == WM_CHAR && w == VK_RETURN && h == g_edComposer) {
        BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        int sendnow = g_cfg.send_ctrl_enter ? (ctrl && !shift) : (!ctrl && !shift);
        if (sendnow) return 0;
    }
    if (m == WM_PAINT) {
        LRESULT r = DefSubclassProc(h, m, w, l);
        if (GetWindowTextLengthW(h) == 0) {
            wchar_t hint[128];
            edit_hint(h, hint, 128);
            if (hint[0]) {
                HDC dc = GetDC(h);
                sk_begin(dc);
                int px = (h == g_edComposer) ? CX_PX : 14;
                sk_text(2, 3, 400, hint, px, 400, 0, SK.dim, 0);
                sk_end();
                ReleaseDC(h, dc);
            }
        }
        return r;
    }
    if (m == WM_SETFOCUS || m == WM_KILLFOCUS) {
        InvalidateRect(g_hMain, NULL, FALSE);
    }
    return DefSubclassProc(h, m, w, l);
}

static HFONT mk_edit_font(int px, int mono)
{
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return CreateFontW(-MulDiv(px, dpi, 96), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH, sk_font_name(mono, 400));
}

static HWND mk_edit(HWND parent, int id, int multiline)
{
    DWORD st = WS_CHILD | ES_LEFT | ES_AUTOHSCROLL;
    if (multiline) st = WS_CHILD | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
    HWND h = CreateWindowExW(0, L"EDIT", L"", st, 0, 0, 10, 10, parent,
                             (HMENU)(INT_PTR)id, g_inst, NULL);
    if (h) {
        SendMessageW(h, WM_SETFONT, (WPARAM)g_fEdit, TRUE);
        SetWindowSubclass(h, edit_proc, 1, 0);
    }
    return h;
}

/* ================= pencere ================= */
static void layout_soft(void)
{
    if (!g_hMain) return;
    RECT rc;
    GetClientRect(g_hMain, &rc);
    double w = rc.right / sk_scale(), h = rc.bottom / sk_scale();
    compute_layout(w, h, 0);
    place_edits();
}

static void on_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int bw = rc.right > 0 ? rc.right : 1, bh = rc.bottom > 0 ? rc.bottom : 1;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, bw, bh);
    HGDIOBJ ob = SelectObject(mem, bmp);
    sk_begin(mem);
    draw_all(bw / sk_scale(), bh / sk_scale());
    if (modal_active()) draw_modal(bw / sk_scale(), bh / sk_scale());
    sk_end();
    BitBlt(dc, 0, 0, bw, bh, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
    place_edits();
}

static int apply_popup(int id, int idx)
{
    switch (id) {
    case P_MODEL: g_cfg.model = idx ? 1 : 0; cfg_save(); break;
    case P_MODE: g_cfg.mode = idx ? 1 : 0; cfg_save(); break;
    case P_LANG: if (idx >= 0 && idx < LUX_NLANG) g_cfg.lang = idx; cfg_save(); break;
    case P_THEME: g_cfg.theme = idx; cfg_save(); theme_apply(); break;
    case P_SENDKEY: g_cfg.send_ctrl_enter = idx ? 1 : 0; cfg_save(); break;
    case P_MENU:
        if (idx == 0) new_chat();
        else if (idx == 1) settings_open();
        else if (idx == 2 && g_token[0]) account_open();
        else if (idx == 2) { g_login_open = 1; layout_soft(); SetFocus(g_edMail); }
        else if (idx == 3 && g_token[0]) do_logout();
        else if (idx == 3) account_open();
        else if (idx == 4) ShellExecuteW(NULL, L"open", L"https://github.com/vesvese55x/LuxAI-the-AI-assistant-for-LuxOS", NULL, NULL, SW_SHOWNORMAL);
        break;
    default: break;
    }
    InvalidateRect(g_hMain, NULL, FALSE);
    return 0;
}

static LRESULT CALLBACK main_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (modal_active()) {
        switch (m) {
        case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_KEYDOWN: case WM_MOUSEWHEEL: case WM_LBUTTONDBLCLK:
            modal_msg(h, m, w, l);
            return 0;
        }
    }
    switch (m) {
    case WM_CREATE:
        g_hMain = h;
        theme_apply();
        ui_sidebar_reload();
        ui_set_status(g_token[0] ? t(S_READY) : t(S_NOT_SIGNED));
        SetTimer(h, 1, 200, NULL);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        on_paint(h);
        return 0;

    case WM_SIZE:
        if (w != SIZE_MINIMIZED) { layout_soft(); InvalidateRect(h, NULL, FALSE); }
        return 0;

    case WM_NCCALCSIZE: {
        int fx = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        int fy = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        if (w == TRUE) {
            NCCALCSIZE_PARAMS *cp = (NCCALCSIZE_PARAMS *)l;
            cp->rgrc[0] = cp->rgrc[1];          /* cerceve yok: istemci = pencere */
            if (IsZoomed(h)) {                   /* buyutulmus: calisma alanina otur */
                cp->rgrc[0].left += fx;
                cp->rgrc[0].right -= fx;
                cp->rgrc[0].top += fy;
                cp->rgrc[0].bottom -= fy;
            }
            return 0;
        }
        RECT *rc = (RECT *)l;
        GetWindowRect(h, rc);
        if (IsZoomed(h)) {
            rc->left += fx;
            rc->right -= fx;
            rc->top += fy;
            rc->bottom -= fy;
        }
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt;
        pt.x = GET_X_LPARAM(l);
        pt.y = GET_Y_LPARAM(l);
        ScreenToClient(h, &pt);
        double s = sk_scale();
        double cx = pt.x / s, cy = pt.y / s;
        RECT rc;
        GetClientRect(h, &rc);
        double wl = rc.right / s, hl = rc.bottom / s;
        if (!IsZoomed(h)) {
            int b = 6;
            int left = pt.x < b, right = pt.x > rc.right - b;
            int top = pt.y < b, bottom = pt.y > rc.bottom - b;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
        }
        if (cy < HS) {
            /* baslikta bir denetim varsa istemci, yoksa tasima */
            const Rect *blocks[8] = { &L.bmin, &L.bmax, &L.bclose, &L.hmenu, &L.combo,
                                      &L.seg0, &L.sbbtn, &L.ncbtn };
            for (int i = 0; i < 8; i++)
                if (in_rect(*blocks[i], cx, cy)) return HTCLIENT;
            if (in_rect(R(L.seg0.x, L.seg0.y, L.seg0.w + L.seg1.w, L.seg0.h), cx, cy)) return HTCLIENT;
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_MOUSEMOVE: {
        if (!g_tracking) {
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof tme;
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = h;
            TrackMouseEvent(&tme);
            g_tracking = 1;
        }
        double s = sk_scale();
        Hit hit = hit_at(GET_X_LPARAM(l) / s, GET_Y_LPARAM(l) / s);
        if (hit.kind != g_hover_kind || hit.idx != g_hover_idx) {
            g_hover_kind = hit.kind;
            g_hover_idx = hit.idx;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_tracking = 0;
        if (g_hover_kind) { g_hover_kind = 0; g_hover_idx = 0; InvalidateRect(h, NULL, FALSE); }
        return 0;

    case WM_LBUTTONUP: {
        double s = sk_scale();
        Hit hit = hit_at(GET_X_LPARAM(l) / s, GET_Y_LPARAM(l) / s);
        if (hit.kind) { on_hit(hit); InvalidateRect(h, NULL, FALSE); }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        double s = sk_scale();
        POINT pt;
        pt.x = GET_X_LPARAM(l);
        pt.y = GET_Y_LPARAM(l);
        ScreenToClient(h, &pt);
        if (g_conv.n || g_stream) {
            int d = GET_WHEEL_DELTA_WPARAM(w);
            g_scroll -= (d / 120.0) * 54.0;
            if (g_scroll < 0) g_scroll = 0;
            g_at_bottom = 0;
            InvalidateRect(h, NULL, FALSE);
        }
        (void)s;
        return 0;
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
        if (s) {
            stream_add(s);
            free(s);
            g_at_bottom = 1;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    }
    case WM_APP_DONE:
        g_busy_ui = 0;
        stream_clear();
        ui_render_all();
        ui_set_status(t(S_READY));
        ui_conversation_save();
        ui_sidebar_reload();
        layout_soft();
        InvalidateRect(h, NULL, FALSE);
        return 0;

    case WM_APP_CONFIRM:
        return confirm_ui((const wchar_t *)w, (const wchar_t *)l, 0);

    case WM_APP_ACCME:
        InvalidateRect(h, NULL, FALSE);
        return 0;

    case WM_APP_POPUP: {
        /* secenek listelerini serbest birak */
        return apply_popup((int)w, (int)l);
    }

    case WM_TIMER:
        if (w == 1) {
            KillTimer(h, 1);
            if (g_startup_account && g_token[0]) account_open();
            else if (g_startup_ask[0]) {
                SetWindowTextW(g_edComposer, g_startup_ask);
                do_send();
            }
        }
        return 0;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        HWND who = (HWND)l;
        SetTextColor(dc, sk_is_dark() ? RGB(0xEC, 0xEE, 0xF7) : RGB(0x1D, 0x1D, 0x2B));
        HBRUSH br = g_brInput;
        COLORREF bg = sk_is_dark() ? RGB(0x26, 0x28, 0x38) : RGB(0xFF, 0xFF, 0xFF);
        if (who == g_edSearch) {
            br = g_brField;
            bg = sk_is_dark() ? RGB(0x2C, 0x2E, 0x3D) : RGB(0xE9, 0xEA, 0xEC);
        } else if (who == g_edWork || who == g_edEnd || who == g_edKey || who == g_edTok) {
            br = g_brCode;
            bg = sk_is_dark() ? RGB(0x15, 0x16, 0x1F) : RGB(0xF3, 0xF4, 0xF8);
        }
        SetBkColor(dc, bg);
        return (LRESULT)br;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l;
        mm->ptMinTrackSize.x = (LONG)(820 * sk_scale());
        mm->ptMinTrackSize.y = (LONG)(560 * sk_scale());
        return 0;
    }

    case WM_DESTROY:
        ui_conversation_save();
        sk_shutdown();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ================= disa acilan API ================= */
void ui_render_all(void)
{
    layout_soft();
    if (g_hMain) InvalidateRect(g_hMain, NULL, FALSE);
}

void ui_append_stream(const wchar_t *text)
{
    if (!text) return;
    stream_add(text);
    g_at_bottom = 1;
    if (g_hMain) InvalidateRect(g_hMain, NULL, FALSE);
}

void ui_render_label(const wchar_t *role_utf8) { (void)role_utf8; }

void ui_set_status(const char *utf8)
{
    strncpy(g_status, utf8 ? utf8 : "", sizeof g_status - 1);
    g_status[sizeof g_status - 1] = 0;
    if (g_hMain) InvalidateRect(g_hMain, NULL, FALSE);
}

void ui_post_report(const char *utf8) { ui_set_status(utf8); }

/* ================= cizilmis modallar (GTK gorunumlu) =================
 * Linux uygulamasindaki onay/izin/hesap pencereleriyle ayni gorunum:
 * karartilmis arka plan, yuvarlatilmis kart, accent birincil dugme.
 * MessageBox (klasik Win32) kullanilmaz.
 */
enum { DM_NONE = 0, DM_CONFIRM, DM_ALERT, DM_ACCOUNT };

static void draw_modal(double w, double h);
static int  modal_msg(HWND h, UINT m, WPARAM w, LPARAM l);
static int  confirm_ui(const wchar_t *title, const wchar_t *body, int danger);
static void alert_ui(const wchar_t *title, const wchar_t *body);

typedef struct {
    int mode, running, result, hov, nbtn, danger;
    wchar_t title[192], body[1024];
    wchar_t btn[3][80];
    int  primary[3];
    Rect card, rb[3], rclose, rrow[8];
    int  nrow;
} Modal;
static Modal D;

static int modal_active(void) { return D.mode != DM_NONE; }

/* soldan hizali satir kaydirma (olcum = cizim) */
static double wrap_text(const wchar_t *s, double x, double y, double maxw, int px,
                        int weight, DWORD col, int paint)
{
    if (!s || !*s) return 0;
    double lh = sk_th(px, 0);
    wchar_t line[512];
    size_t n = 0;
    int lines = 0;
    double cy = y;
    const wchar_t *p = s;
    while (*p) {
        const wchar_t *end = wcschr(p, L'\n');
        size_t seglen = end ? (size_t)(end - p) : wcslen(p);
        wchar_t seg[512];
        if (seglen > 511) seglen = 511;
        wcsncpy(seg, p, seglen);
        seg[seglen] = 0;
        n = 0;
        line[0] = 0;
        wchar_t word[256];
        const wchar_t *q = seg;
        while (*q) {
            size_t wl = 0;
            while (*q == L' ') q++;
            while (q[wl] && q[wl] != L' ' && wl < 255) wl++;
            wcsncpy(word, q, wl);
            word[wl] = 0;
            q += wl;
            wchar_t test[512];
            if (n) { wcscpy(test, line); wcscat(test, L" "); wcscat(test, word); }
            else wcscpy(test, word);
            if (n && sk_tw_s(test, px, weight, 0) > maxw) {
                if (paint) sk_text(x, cy, maxw, line, px, weight, 0, col, 0);
                cy += lh; lines++;
                wcscpy(line, word); n = wcslen(word);
            } else {
                wcscpy(line, test); n = wcslen(line);
            }
        }
        if (paint && line[0]) sk_text(x, cy, maxw, line, px, weight, 0, col, 0);
        cy += lh; lines++;
        line[0] = 0; n = 0;
        if (!end) break;
        p = end + 1;
    }
    return lines * lh;
}

static void modal_buttons(const wchar_t *b1, const wchar_t *b2, const wchar_t *b3)
{
    D.nbtn = 0;
    const wchar_t *src[3] = { b1, b2, b3 };
    int prim[3] = { 1, 0, 0 };
    for (int i = 0; i < 3; i++) {
        if (!src[i] || !src[i][0]) continue;
        wcsncpy(D.btn[D.nbtn], src[i], 79);
        D.btn[D.nbtn][79] = 0;
        D.primary[D.nbtn] = prim[i];
        D.nbtn++;
    }
    D.hov = -1;
}

/* kart olcusu: icerigi olcup kart yuksekligini belirler */
static void modal_layout(double w, double h)
{
    double cw = (D.mode == DM_ACCOUNT) ? 560 : 470;
    if (cw > w - 60) cw = w - 60;
    double cx = (w - cw) / 2, cy;
    double y = 0;
    if (D.mode == DM_ACCOUNT) {
        y = 178 + D.nrow * 46 + 10;
    } else {
        y = 54;                                   /* ikon + baslik */
        y += wrap_text(D.body, 0, 0, cw - 48, 13, 400, 0, 0);
        y += 22;
        y += 36;                                  /* dugmeler */
        y += 18;
    }
    double ch = y + 20;
    if (ch > h - 40) ch = h - 40;
    cy = (h - ch) / 2;
    D.card = R(cx, cy, cw, ch);
    D.rclose = R(cx + cw - 40, cy + 12, 28, 26);
    /* dugmeler sagdan sola */
    double bx = cx + cw - 18, by = cy + ch - 54;
    for (int i = D.nbtn - 1; i >= 0; i--) {
        double bw = sk_tw_s(D.btn[i], 13, 600, 0) + 40;
        if (bw < 72) bw = 72;
        D.rb[i] = R(bx - bw, by, bw, 36);
        bx -= bw + 8;
    }
    /* hesap satirlari */
    double ry = cy + 178;
    for (int i = 0; i < D.nrow; i++) {
        D.rrow[i] = R(cx + 14, ry, cw - 28, 44);
        ry += 46;
    }
}

static void draw_account_body(void)
{
    wchar_t b[256], b2[256];
    Rect c = D.card;
    sk_text(c.x + 22, c.y + 14, c.w - 70, T(S_ACCT_TITLE, b, 256), 17, 700, 0, SK.fg, 0);
    /* hero: gradyan kart + avatar */
    double hx = c.x + 18, hy = c.y + 48, hw = c.w - 36, hh = 64;
    sk_grad(hx, hy, hw, hh, 14, 0xFF3D6BFF, 0xFF8A4DFF, 1);
    sk_circle(hx + 38, hy + hh / 2, 21, 0x40FFFFFF);
    const wchar_t *nm = g_me_name[0] ? g_me_name : (g_username[0] ? g_username : g_email);
    wchar_t initial[4] = L"?";
    if (nm[0]) { initial[0] = towupper(nm[0]); initial[1] = 0; }
    sk_text_box(hx + 17, hy + hh / 2 - 14, 42, 28, initial, 18, 800, 0, 0xFFFFFFFF, 1);
    sk_text(hx + 70, hy + 12, hw - 90, nm, 15, 700, 0, 0xFFFFFFFF, 0);
    const wchar_t *em = g_me_email[0] ? g_me_email : g_email;
    sk_text(hx + 70, hy + 34, hw - 90, em, 12, 400, 0, 0xCCFFFFFF, 0);
    if (g_me_plan[0]) {
        double pw = sk_tw_s(g_me_plan, 11, 700, 0) + 18;
        sk_round(hx + hw - pw - 14, hy + 12, pw, 20, 10, 0x33FFFFFF);
        sk_text_box(hx + hw - pw - 14, hy + 12, pw, 20, g_me_plan, 11, 700, 0, 0xFFFFFFFF, 1);
    }
    /* kullanim */
    double uy = c.y + 126;
    sk_text(c.x + 22, uy, c.w - 44, T(S_USAGE, b, 256), 12, 700, 0, SK.dim, 0);
    uy += 20;
    wchar_t val[128];
    if (g_me_limit > 0) {
        double frac = (double)g_me_used / (double)g_me_limit;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        sk_round(c.x + 22, uy, c.w - 44, 6, 3, (SK.fg & 0x00FFFFFF) | 0x1A000000);
        sk_round(c.x + 22, uy, (c.w - 44) * frac, 6, 3, SK.accent);
        _snwprintf(val, 128, L"%d / %d", g_me_used, g_me_limit);
        sk_text(c.x + 22, uy + 12, c.w - 44, val, 11, 400, 0, SK.dim, 0);
    } else {
        const char *src = g_me_busy ? t(S_THINKING) : t(S_OFFLINE_INFO);
        wchar_t *ws = u16(src);
        if (ws) {
            wrap_text(ws, c.x + 22, uy, c.w - 44, 12, 400, SK.dim, 1);
            free(ws);
        }
    }
    /* satirlar */
    static const int row_ids[5] = { S_MANAGE, S_EXPORT, S_TERMS, S_DELREQ, S_LOGOUT };
    static const int row_icons[5] = { SKI_PENCIL, SKI_UP, SKI_SPARKLE, SKI_TRASH, SKI_CLOSE };
    for (int i = 0; i < D.nrow && i < 5; i++) {
        Rect r = D.rrow[i];
        int hov = (D.hov == 100 + i);
        if (hov) sk_round(r.x, r.y, r.w, r.h, 10, (SK.fg & 0x00FFFFFF) | 0x0F000000);
        int danger = (row_ids[i] == S_LOGOUT);
        int icon = danger ? SKI_CLOSE : row_icons[i];
        DWORD col = danger ? SK.danger : SK.fg;
        sk_icon(icon, r.x + 22, r.y + r.h / 2, 14, col, 1.6);
        T(row_ids[i], b2, 256);
        sk_text_box(r.x + 44, r.y, r.w - 70, r.h, b2, 14, 500, 0, col, 0);
        sk_icon(SKI_CHEV_D, r.x + r.w - 20, r.y + r.h / 2 + 2, 12, SK.dim, 1.5);
    }
}

static void draw_modal(double w, double h)
{
    if (!D.mode) return;
    modal_layout(w, h);
    sk_fill(0, 0, w, h, 0x77000000);
    Rect c = D.card;
    sk_round(c.x + 2, c.y + 8, c.w, c.h, 18, 0x40000000);
    sk_round(c.x, c.y, c.w, c.h, 16, SK.card);
    sk_rline(c.x, c.y, c.w, c.h, 16, SK.border, 1);
    if (D.mode == DM_ACCOUNT) {
        draw_account_body();
    } else {
        wchar_t b[256];
        /* uyari ikonu + baslik */
        sk_circle(c.x + 34, c.y + 32, 13, D.danger ? 0x22FF453A : 0x223D7BFF);
        sk_icon(D.danger ? SKI_CLOSE : SKI_SPARKLE, c.x + 34, c.y + 32, 14,
                D.danger ? SK.danger : SK.accent, 1.8);
        sk_text(c.x + 56, c.y + 22, c.w - 100, D.title, 15, 700, 0, SK.fg, 0);
        wrap_text(D.body, c.x + 24, c.y + 54, c.w - 48, 13, 400, SK.dim, 1);
        for (int i = 0; i < D.nbtn; i++) {
            Rect r = D.rb[i];
            int hov = (D.hov == i);
            DWORD bg = D.primary[i] ? (hov ? 0xFF5B8CFF : SK.accent)
                                    : ((SK.fg & 0x00FFFFFF) | (hov ? 0x1F000000 : 0x0F000000));
            sk_round(r.x, r.y, r.w, r.h, 18, bg);
            sk_text_box(r.x, r.y, r.w, r.h, D.btn[i], 13, 700, 0,
                        D.primary[i] ? 0xFFFFFFFF : SK.fg, 1);
        }
    }
    /* kapat (X) */
    if (D.mode == DM_ACCOUNT) {
        Rect r = D.rclose;
        if (D.hov == 99) sk_round(r.x, r.y, r.w, r.h, 8, (SK.fg & 0x00FFFFFF) | 0x12000000);
        sk_icon(SKI_CLOSE, r.x + r.w / 2, r.y + r.h / 2, 13, SK.dim, 1.6);
    }
}

/* hesap penceresi eylemleri */
static void account_export(void);

static void modal_click(double x, double y)
{
    if (D.mode == DM_ACCOUNT) {
        if (in_rect(D.rclose, x, y)) { D.result = 0; D.running = 0; return; }
        for (int i = 0; i < D.nrow && i < 5; i++) {
            if (!in_rect(D.rrow[i], x, y)) continue;
            switch (i) {
            case 0:
                ShellExecuteW(NULL, L"open", L"https://lux-ai.cz/settings", NULL, NULL, SW_SHOWNORMAL);
                break;
            case 1:
                account_export();
                break;
            case 2:
                ShellExecuteW(NULL, L"open", L"https://lux-ai.cz/terms", NULL, NULL, SW_SHOWNORMAL);
                break;
            case 3:
                ShellExecuteW(NULL, L"open",
                              L"mailto:info@lux-ai.cz?subject=LuxAI%20account%20deletion%20request",
                              NULL, NULL, SW_SHOWNORMAL);
                break;
            case 4: {
                wchar_t t1[192], t2[600];
                T(S_LOGOUT_Q, t1, 192);
                T(S_LOGOUT_BODY, t2, 600);
                D.mode = DM_NONE;
                if (confirm_ui(t1, t2, 1)) {
                    account_clear();
                    ui_set_status(t(S_NOT_SIGNED));
                    g_me_name[0] = g_me_email[0] = g_me_plan[0] = g_me_user[0] = 0;
                    g_me_used = g_me_limit = -1;
                }
                D.mode = DM_ACCOUNT;
                D.hov = -1;
                InvalidateRect(g_hMain, NULL, FALSE);
                break;
            }
            default: break;
            }
            return;
        }
        return;
    }
    for (int i = 0; i < D.nbtn; i++) {
        if (in_rect(D.rb[i], x, y)) { D.result = D.primary[i] ? 1 : 0; D.running = 0; return; }
    }
}

static void modal_key(int vk)
{
    if (vk == VK_ESCAPE) { D.result = 0; D.running = 0; return; }
    if (vk == VK_RETURN) {
        if (D.mode == DM_ACCOUNT) { D.result = 0; D.running = 0; }
        else { D.result = (D.nbtn > 0 && D.primary[0]) ? 1 : 0; D.running = 0; }
        return;
    }
    if (vk == VK_TAB || vk == VK_LEFT || vk == VK_RIGHT) {
        if (D.mode == DM_ACCOUNT) return;
        D.hov = (D.hov + 1) % (D.nbtn > 0 ? D.nbtn : 1);
    }
}

static int modal_msg(HWND h, UINT m, WPARAM w, LPARAM l)
{
    double s = sk_scale();
    double x = GET_X_LPARAM(l) / s, y = GET_Y_LPARAM(l) / s;
    switch (m) {
    case WM_MOUSEMOVE: {
        int hov = -1;
        if (D.mode == DM_ACCOUNT) {
            if (in_rect(D.rclose, x, y)) hov = 99;
            for (int i = 0; i < D.nrow && i < 5; i++)
                if (in_rect(D.rrow[i], x, y)) hov = 100 + i;
        } else {
            for (int i = 0; i < D.nbtn; i++)
                if (in_rect(D.rb[i], x, y)) hov = i;
        }
        if (hov != D.hov) { D.hov = hov; InvalidateRect(h, NULL, FALSE); }
        return 1;
    }
    case WM_LBUTTONUP:
        modal_click(x, y);
        InvalidateRect(h, NULL, FALSE);
        return 1;
    case WM_LBUTTONDOWN:
        return 1;
    case WM_KEYDOWN:
        modal_key((int)w);
        InvalidateRect(h, NULL, FALSE);
        return 1;
    case WM_MOUSEWHEEL:
        return 1;
    default:
        return 0;
    }
}

/* modal dongu: MessageBox gibi bloklar, ama kendi cizimimizle */
static int modal_pump(void)
{
    D.running = 1;
    D.result = 0;
    InvalidateRect(g_hMain, NULL, TRUE);
    UpdateWindow(g_hMain);
    MSG msg;
    while (D.running) {
        BOOL got = GetMessageW(&msg, NULL, 0, 0);
        if (got <= 0) { D.running = 0; break; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    D.mode = DM_NONE;
    InvalidateRect(g_hMain, NULL, TRUE);
    return D.result;
}

/* ---------- genel onay/uyari ---------- */
static int confirm_ui(const wchar_t *title, const wchar_t *body, int danger)
{
    if (!g_hMain || !title) return 0;
    /* tw() statik tampon kullaniyor: once kopyala, sonra dugme metinleri */
    wcsncpy(D.title, title, 191); D.title[191] = 0;
    wcsncpy(D.body, body ? body : L"", 1023); D.body[1023] = 0;
    wchar_t b1[80], b2[80];
    T(S_ALLOWBTN, b1, 80);
    T(S_NOT_NOW, b2, 80);
    if (danger) { T(S_DELETEBTN, b1, 80); T(S_CANCEL, b2, 80); }
    D.mode = DM_CONFIRM;
    D.danger = danger;
    modal_buttons(b1, b2, NULL);
    return modal_pump();
}

static void alert_ui(const wchar_t *title, const wchar_t *body)
{
    if (!g_hMain) return;
    wcsncpy(D.title, title ? title : L"", 191); D.title[191] = 0;
    wcsncpy(D.body, body ? body : L"", 1023); D.body[1023] = 0;
    wchar_t b1[80];
    T(S_CLOSEBTN, b1, 80);
    D.mode = DM_ALERT;
    D.danger = 0;
    modal_buttons(b1, NULL, NULL);
    modal_pump();
}

/* ---------- hesap penceresi ---------- */
static int accme_sink(const char *d, size_t n, void *ud)
{
    buf_add((Buf *)ud, d, n);
    return 0;
}

static DWORD WINAPI accme_thread(LPVOID p)
{
    (void)p;
    wchar_t hdr[1400];
    _snwprintf(hdr, 1400, L"Cookie: lux_account_session=%s\r\nAccept: application/json", g_token);
    Buf b;
    buf_init(&b);
    int st = 0;
    Buf err;
    buf_init(&err);
    http_request(L"https://lux-ai.cz/api/me", hdr, NULL, 0, accme_sink, &b, &st, &err, NULL);
    if (st == 200 && b.p) {
        JVal *root = json_parse(b.p);
        if (root) {
            JVal *user = json_get(root, "user");
            if (!user) user = root;
            const char *s;
            if ((s = json_str(user, "displayName")) || (s = json_str(user, "name"))) {
                wchar_t *w = u16(s);
                if (w) { wcsncpy(g_me_name, w, 159); g_me_name[159] = 0; free(w); }
            }
            if ((s = json_str(user, "username"))) {
                wchar_t *w = u16(s);
                if (w) { wcsncpy(g_me_user, w, 95); g_me_user[95] = 0; free(w); }
            }
            if ((s = json_str(user, "email"))) {
                wchar_t *w = u16(s);
                if (w) { wcsncpy(g_me_email, w, 255); g_me_email[255] = 0; free(w); }
            }
            if ((s = json_str(user, "plan")) || (s = json_str(user, "tier"))) {
                wchar_t *w = u16(s);
                if (w) { wcsncpy(g_me_plan, w, 79); g_me_plan[79] = 0; free(w); }
            }
            JVal *us = json_get(user, "usage");
            if (us) {
                JVal *rl = json_get(us, "limit");
                JVal *ru = json_get(us, "used");
                if (rl == NULL) { rl = json_get(us, "requestsLimit"); ru = json_get(us, "requestsUsed"); }
                if (rl && ru) { g_me_limit = (int)rl->num; g_me_used = (int)ru->num; }
            }
            json_free(root);
        }
    }
    buf_free(&b);
    buf_free(&err);
    g_me_busy = 0;
    if (g_hMain) PostMessageW(g_hMain, WM_APP_ACCME, 0, 0);
    return 0;
}

static void account_export(void)
{
    wchar_t dir[MAX_PATH] = L"", path[MAX_PATH * 2];
    if (!SHGetSpecialFolderPathW(NULL, dir, CSIDL_PERSONAL, FALSE))
        SHGetSpecialFolderPathW(NULL, dir, CSIDL_PROFILE, FALSE);
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf(path, MAX_PATH * 2, L"%s\\luxai-account-%04d-%02d-%02d.json", dir,
               st.wYear, st.wMonth, st.wDay);
    FILE *f = _wfopen(path, L"wb");
    if (!f) return;
    fputs("{\"version\":1,\"exportedAt\":", f);
    char tmp[64];
    _snprintf(tmp, sizeof tmp, "\"%04d-%02d-%02dT%02d:%02d:%02d\"", st.wYear, st.wMonth,
              st.wDay, st.wHour, st.wMinute, st.wSecond);
    fputs(tmp, f);
    char email[512] = "", name[512] = "";
    WideCharToMultiByte(CP_UTF8, 0, g_email, -1, email, sizeof email, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, g_username, -1, name, sizeof name, NULL, NULL);
    Buf b;
    buf_init(&b);
    buf_adds(&b, ",\"user\":{");
    buf_adds(&b, "\"name\":");
    json_write_str(&b, name);
    buf_adds(&b, ",\"email\":");
    json_write_str(&b, email);
    buf_adds(&b, "},\"conversations\":[");
    wchar_t cdir[MAX_PATH], pat[MAX_PATH * 2];
    conv_dir(cdir, MAX_PATH);
    _snwprintf(pat, MAX_PATH * 2, L"%s\\*.json", cdir);
    WIN32_FIND_DATAW fd;
    HANDLE fh = FindFirstFileW(pat, &fd);
    int first = 1;
    if (fh != INVALID_HANDLE_VALUE) {
        do {
            wchar_t fp[MAX_PATH * 2];
            _snwprintf(fp, MAX_PATH * 2, L"%s\\%s", cdir, fd.cFileName);
            FILE *cf = _wfopen(fp, L"rb");
            if (!cf) continue;
            char *buf = (char *)malloc((size_t)fd.nFileSizeLow + 1);
            if (!buf) { fclose(cf); continue; }
            size_t rd = fread(buf, 1, (size_t)fd.nFileSizeLow, cf);
            buf[rd] = 0;
            fclose(cf);
            if (!first) buf_adds(&b, ",");
            buf_add(&b, buf, rd);
            first = 0;
            free(buf);
        } while (FindNextFileW(fh, &fd));
        FindClose(fh);
    }
    buf_adds(&b, "]}");
    if (b.p) fwrite(b.p, 1, b.n, f);
    buf_free(&b);
    fclose(f);
    wchar_t msg[600], fmt[200];
    T(S_EXPORTED_TO, fmt, 200);
    _snwprintf(msg, 600, fmt, path);
    wchar_t ttl[128];
    T(S_EXPORT, ttl, 128);
    alert_ui(ttl, msg);
}

static void account_open(void)
{
    if (!g_token[0]) return;
    D.title[0] = 0;
    D.nrow = 5;
    D.mode = DM_ACCOUNT;
    D.hov = -1;
    if (!g_me_name[0] && !g_me_busy) {
        g_me_busy = 1;
        HANDLE th = CreateThread(NULL, 0, accme_thread, NULL, 0, NULL);
        if (th) CloseHandle(th);
    }
    modal_pump();
}

int ui_confirm(const wchar_t *title, const wchar_t *detail, int twice)
{
    if (g_cfg.auto_approve && !twice) return 1;
    if (GetCurrentThreadId() != g_ui_thread) {
        if (!g_hMain) return 0;
        return (int)SendMessageW(g_hMain, WM_APP_CONFIRM, (WPARAM)title, (LPARAM)detail) == 1;
    }
    int r = confirm_ui(title, detail, 0);
    if (r && twice) r = confirm_ui(title, detail, 1);
    return r;
}

void ui_run(void)
{
    g_inst = GetModuleHandleW(NULL);
    g_ui_thread = GetCurrentThreadId();

    SetProcessDPIAware();
    sk_init();
    {
        HDC dc = GetDC(NULL);
        sk_set_dpi((UINT)GetDeviceCaps(dc, LOGPIXELSX));
        ReleaseDC(NULL, dc);
    }

    /* olcum yuzeyi */
    g_mdc = CreateCompatibleDC(NULL);
    HDC sdc = GetDC(NULL);
    g_mbmp = CreateCompatibleBitmap(sdc, 8, 8);
    ReleaseDC(NULL, sdc);
    g_mold = SelectObject(g_mdc, g_mbmp);

    theme_apply();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = main_proc;
    wc.hInstance = g_inst;
    wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"LuxAIWindow";
    RegisterClassExW(&wc);

    int wpx = 1100, hpx = 760;
    double s = sk_scale();
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    if ((int)(wpx * s) > sw - 16) wpx = (int)((sw - 16) / s);
    if ((int)(hpx * s) > sh - 48) hpx = (int)((sh - 48) / s);
    if (wpx < 820) wpx = 820;
    if (hpx < 560) hpx = 560;
    int x = (int)((sw - wpx * s) / 2), y = (int)((sh - hpx * s) / 2);
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    g_hMain = CreateWindowExW(0, L"LuxAIWindow", L"LuxAI",
                              WS_OVERLAPPEDWINDOW, x, y, (int)(wpx * s), (int)(hpx * s),
                              NULL, NULL, g_inst, NULL);
    if (!g_hMain) return;

    g_fEdit = mk_edit_font(CX_PX, 0);
    g_fEditMono = mk_edit_font(13, 1);
    g_edComposer = mk_edit(g_hMain, 100, 1);
    g_edSearch   = mk_edit(g_hMain, 101, 0);
    g_edMail     = mk_edit(g_hMain, 102, 0);
    g_edPass     = mk_edit(g_hMain, 103, 0);
    g_edWork     = mk_edit(g_hMain, 104, 0);
    g_edEnd      = mk_edit(g_hMain, 105, 0);
    g_edKey      = mk_edit(g_hMain, 106, 0);
    g_edTok      = mk_edit(g_hMain, 107, 0);
    if (g_edPass) SendMessageW(g_edPass, EM_SETPASSWORDCHAR, (WPARAM)L'\u25CF', 0);
    {
        HDC dc = GetDC(g_hMain);
        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(g_hMain, dc);
        if (g_edComposer) {
            HFONT f = CreateFontW(-MulDiv(CX_PX, dpi, 96), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                  DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, sk_font_name(0, 400));
            SendMessageW(g_edComposer, WM_SETFONT, (WPARAM)f, TRUE);
        }
    }

    ShowWindow(g_hMain, SW_SHOW);
    UpdateWindow(g_hMain);
    layout_soft();

    if (!g_startup_ask[0] && !g_cfg.use_openai && !g_token[0]) {
        g_login_open = 1;
        layout_soft();
        InvalidateRect(g_hMain, NULL, FALSE);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_mdc) {
        SelectObject(g_mdc, g_mold);
        DeleteObject(g_mbmp);
        DeleteDC(g_mdc);
        g_mdc = NULL;
    }
}

