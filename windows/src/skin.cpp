/* LuxAI native — GDI+ cizim motoru.
 * Renkler/olculer orijinal GTK arayuzunun CSS'inden (binary icindeki
 * gtk_css_provider_load_from_data blogu) ve gercek ekran goruntularinden alindi.
 */
#include "skin.h"
#include <gdiplus.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace Gdiplus;

SkPal SK;
static Graphics *g_gfx;
static ULONG_PTR g_tok;
static int   g_dark;
static double g_scale = 1.0;
static GraphicsState g_state[8];
static int g_sp = -1;

/* ---------------- yardimcilar ---------------- */
static Color C(DWORD a) { return Color((ARGB)a); }

static void add_round(GraphicsPath *p, REAL x, REAL y, REAL w, REAL h,
                      REAL rtl, REAL rtr, REAL rbr, REAL rbl)
{
    if (w <= 0 || h <= 0) return;
    REAL m = (w < h ? w : h) / 2.0f;
    if (rtl > m) rtl = m;
    if (rtr > m) rtr = m;
    if (rbr > m) rbr = m;
    if (rbl > m) rbl = m;
    if (rtl <= 0.4f && rtr <= 0.4f && rbr <= 0.4f && rbl <= 0.4f) {
        p->AddRectangle(RectF(x, y, w, h));
        return;
    }
    REAL right = x + w, bottom = y + h;
    p->StartFigure();
    p->AddLine(x + rtl, y, right - rtr, y);
    if (rtr > 0.4f)
        p->AddArc(right - rtr * 2, y, rtr * 2, rtr * 2, 270.0f, 90.0f);
    else
        p->AddLine(right, y, right, y);
    p->AddLine(right, y + rtr, right, bottom - rbr);
    if (rbr > 0.4f)
        p->AddArc(right - rbr * 2, bottom - rbr * 2, rbr * 2, rbr * 2, 0.0f, 90.0f);
    else
        p->AddLine(right, bottom, right, bottom);
    p->AddLine(right - rbr, bottom, x + rbl, bottom);
    if (rbl > 0.4f)
        p->AddArc(x, bottom - rbl * 2, rbl * 2, rbl * 2, 90.0f, 90.0f);
    else
        p->AddLine(x, bottom, x, bottom);
    p->AddLine(x, bottom - rbl, x, y + rtl);
    if (rtl > 0.4f)
        p->AddArc(x, y, rtl * 2, rtl * 2, 180.0f, 90.0f);
    else
        p->AddLine(x, y, x, y);
    p->CloseFigure();
}

/* ---------------- yasam dongusu ---------------- */
void sk_init(void)
{
    GdiplusStartupInput in;
    GdiplusStartup(&g_tok, &in, NULL);
    sk_theme(0);
}

void sk_shutdown(void)
{
    if (g_gfx) { delete g_gfx; g_gfx = NULL; }
    if (g_tok) { GdiplusShutdown(g_tok); g_tok = 0; }
}

void sk_set_dpi(UINT dpi) { g_scale = dpi ? (double)dpi / 96.0 : 1.0; }
double sk_scale(void) { return g_scale; }
int sk_is_dark(void) { return g_dark; }

void sk_theme(int dark)
{
    g_dark = dark;
    if (!dark) {
        /* acik: CSS @define-color blogu (luxai ikilisinden) */
        SK.bg      = 0xFFFBFBFD;
        SK.side    = 0xFFF1F2F7;
        SK.fg      = 0xFF1D1D2B;
        SK.dim     = 0x8F1D1D2B;      /* rgba(29,29,43,.56) */
        SK.border  = 0x17000000;      /* rgba(0,0,0,.09) */
        SK.card    = 0xFFFFFFFF;
        SK.code    = 0xFFF3F4F8;
        SK.input   = 0xFFFFFFFF;
        SK.accent  = 0xFF3D7BFF;
        SK.danger  = 0xFFE5484D;
        SK.warn    = 0xFFD48A00;
        SK.ok      = 0xFF1F9D55;
        SK.header  = 0xFFDEDBD7;      /* GTK Adwaita headerbar (olculdu) */
        SK.hbtn    = 0xFFF1EFED;
        SK.hbtn_hi = 0xFFE5E2DE;
        SK.on_accent = 0xFFFFFFFF;
    } else {
        SK.bg      = 0xFF1C1D29;
        SK.side    = 0xFF222330;
        SK.fg      = 0xFFECEEF7;
        SK.dim     = 0x94ECEEF7;      /* rgba(236,238,247,.58) */
        SK.border  = 0x17FFFFFF;      /* rgba(255,255,255,.09) */
        SK.card    = 0xFF262838;
        SK.code    = 0xFF15161F;
        SK.input   = 0xFF262838;
        SK.accent  = 0xFF4A7DFF;
        SK.danger  = 0xFFFF5F62;
        SK.warn    = 0xFFF2B13A;
        SK.ok      = 0xFF35C47A;
        SK.header  = 0xFF2A2B39;
        SK.hbtn    = 0xFF383A4C;
        SK.hbtn_hi = 0xFF454861;
        SK.on_accent = 0xFFFFFFFF;
    }
}

void sk_begin(HDC dc)
{
    if (g_gfx) { delete g_gfx; g_gfx = NULL; }
    g_gfx = new Graphics(dc);
    g_gfx->SetSmoothingMode(SmoothingModeAntiAlias);
    g_gfx->SetPixelOffsetMode(PixelOffsetModeHalf);
    g_gfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
    if (g_scale != 1.0) g_gfx->ScaleTransform((REAL)g_scale, (REAL)g_scale);
    g_sp = -1;
}

void sk_end(void)
{
    if (g_gfx) { delete g_gfx; g_gfx = NULL; }
}

/* ---------------- sekiller ---------------- */
void sk_fill(double x, double y, double w, double h, DWORD argb)
{
    if (!g_gfx || w <= 0 || h <= 0 || (argb >> 24) == 0) return;
    SolidBrush b(C(argb));
    g_gfx->FillRectangle(&b, RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h));
}

void sk_round4(double x, double y, double w, double h, double rtl, double rtr,
               double rbr, double rbl, DWORD argb)
{
    if (!g_gfx || w <= 0 || h <= 0 || (argb >> 24) == 0) return;
    GraphicsPath p;
    add_round(&p, (REAL)x, (REAL)y, (REAL)w, (REAL)h,
              (REAL)rtl, (REAL)rtr, (REAL)rbr, (REAL)rbl);
    SolidBrush b(C(argb));
    g_gfx->FillPath(&b, &p);
}

void sk_round(double x, double y, double w, double h, double r, DWORD argb)
{
    sk_round4(x, y, w, h, r, r, r, r, argb);
}

void sk_rline(double x, double y, double w, double h, double r, DWORD argb, double th)
{
    if (!g_gfx || w <= 0 || h <= 0) return;
    GraphicsPath p;
    REAL i = (REAL)(th / 2.0);
    add_round(&p, (REAL)x + i, (REAL)y + i, (REAL)w - i * 2, (REAL)h - i * 2,
              (REAL)r, (REAL)r, (REAL)r, (REAL)r);
    Pen pen(C(argb), (REAL)th);
    g_gfx->DrawPath(&pen, &p);
}

void sk_grad(double x, double y, double w, double h, double r, DWORD c1, DWORD c2,
             int diagonal)
{
    if (!g_gfx || w <= 0 || h <= 0) return;
    GraphicsPath p;
    add_round(&p, (REAL)x, (REAL)y, (REAL)w, (REAL)h, (REAL)r, (REAL)r, (REAL)r, (REAL)r);
    PointF a((REAL)x, (REAL)y), b = diagonal
        ? PointF((REAL)(x + w), (REAL)(y + h))
        : PointF((REAL)x, (REAL)(y + h));
    LinearGradientBrush br(a, b, C(c1), C(c2));
    g_gfx->FillPath(&br, &p);
}

void sk_line(double x1, double y1, double x2, double y2, DWORD argb, double th)
{
    if (!g_gfx) return;
    Pen pen(C(argb), (REAL)th);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g_gfx->DrawLine(&pen, (REAL)x1, (REAL)y1, (REAL)x2, (REAL)y2);
}

void sk_circle(double cx, double cy, double r, DWORD argb)
{
    if (!g_gfx || r <= 0) return;
    SolidBrush b(C(argb));
    g_gfx->FillEllipse(&b, (REAL)(cx - r), (REAL)(cy - r), (REAL)(r * 2), (REAL)(r * 2));
}

/* ---------------- yazi ---------------- */
struct FEnt { int px; int weight; int mono; int script; Gdiplus::Font *f; };
static FEnt g_fonts[96];
static int  g_nfont;

static int family_ok(const wchar_t *name)
{
    /* IsAvailable() bazi ortamlarda (Wine) yanlis 0 donuyor: aile adini
     * dogrudan okuyup kontrol ediyoruz. */
    FontFamily ff(name);
    if (ff.GetLastStatus() != Ok) return 0;
    WCHAR got[LF_FACESIZE];
    got[0] = 0;
    ff.GetFamilyName(got);
    return got[0] != 0;
}

static const wchar_t *pick_sans(void)
{
    static const wchar_t *c[] = { L"Segoe UI", L"Liberation Sans", L"DejaVu Sans",
                                  L"Arial", L"Tahoma", L"Microsoft Sans Serif" };
    for (int i = 0; i < 6; i++) if (family_ok(c[i])) return c[i];
    return L"Tahoma";
}
static const wchar_t *pick_mono(void)
{
    static const wchar_t *c[] = { L"Consolas", L"DejaVu Sans Mono", L"Liberation Mono",
                                  L"Courier New", L"Lucida Console" };
    for (int i = 0; i < 5; i++) if (family_ok(c[i])) return c[i];
    return L"Courier New";
}

/* ---- yazi tipi secimi (alfabe bazli) ----
 * Segoe UI Latin/Kiril/Yunan kapsar; CJK ve Devanagari icin Windows'un kendi
 * fontlari gerekir, yoksa kutular cizilir. */
static int script_of(const wchar_t *s, int n)
{
    for (int i = 0; i < n; i++) {
        unsigned c = (unsigned)s[i];
        if (c >= 0x0900 && c <= 0x097F) return 1;          /* Devanagari */
        if (c >= 0x0E00 && c <= 0x0E7F) return 1;          /* Thai (ayni yedek) */
        if (c >= 0x3040 && c <= 0x30FF) return 3;          /* Hiragana/Katakana -> JA */
        if (c >= 0x4E00 && c <= 0x9FFF) return 2;          /* CJK ideograflari */
        if (c >= 0xAC00 && c <= 0xD7AF) return 4;          /* Hangul */
        if (c >= 0x0590 && c <= 0x06FF) return 5;          /* Ibrani/Arapca */
    }
    return 0;
}
static const wchar_t *pick_script(int script)
{
    static const wchar_t *dev[] = { L"Nirmala UI", L"Mangal", L"Noto Sans Devanagari",
                                    L"Noto Sans Devanagari Condensed", L"Lohit Devanagari",
                                    L"Arial Unicode MS", L"Segoe UI", NULL };
    static const wchar_t *hans[] = { L"Microsoft YaHei UI", L"Microsoft YaHei", L"SimSun",
                                     L"Noto Sans CJK SC", L"Noto Sans SC",
                                     L"Noto Sans CJK TC", NULL };
    static const wchar_t *jpan[] = { L"Yu Gothic UI", L"Meiryo", L"MS Gothic",
                                     L"Noto Sans CJK JP", L"Noto Sans JP", NULL };
    static const wchar_t *hang[] = { L"Malgun Gothic", L"Gulim", L"Noto Sans CJK KR", NULL };
    static const wchar_t *arab[] = { L"Segoe UI", L"Tahoma", L"Noto Naskh Arabic", NULL };
    const wchar_t **list = script == 1 ? dev : script == 2 ? hans : script == 3 ? jpan
                        : script == 4 ? hang : script == 5 ? arab : NULL;
    if (!list) return NULL;
    for (int i = 0; list[i]; i++) if (family_ok(list[i])) return list[i];
    return NULL;
}

static Gdiplus::Font *get_font_s(int px, int weight, int mono, int script)
{
    if (px < 6) px = 6;
    for (int i = 0; i < g_nfont; i++)
        if (g_fonts[i].px == px && g_fonts[i].weight == weight &&
            g_fonts[i].mono == mono && g_fonts[i].script == script)
            return g_fonts[i].f;
    const wchar_t *fam;
    int style = FontStyleRegular;
    if (mono) {
        fam = pick_mono();
    } else if (script && pick_script(script)) {
        fam = pick_script(script);
    } else if (weight >= 600 && family_ok(L"Segoe UI Semibold")) {
        fam = L"Segoe UI Semibold";
    } else {
        fam = pick_sans();
        if (weight >= 600) style = FontStyleBold;
    }
    Gdiplus::Font *f = new Gdiplus::Font(fam, (REAL)px, style, UnitPixel);
    if (f->GetLastStatus() != Ok) {
        delete f;
        f = new Gdiplus::Font(pick_sans(), (REAL)px, style, UnitPixel);
    }
    if (g_nfont < 96) {
        g_fonts[g_nfont].px = px;
        g_fonts[g_nfont].weight = weight;
        g_fonts[g_nfont].mono = mono;
        g_fonts[g_nfont].script = script;
        g_fonts[g_nfont].f = f;
        g_nfont++;
    }
    return f;
}

static Gdiplus::Font *get_font(int px, int weight, int mono)
{
    return get_font_s(px, weight, mono, 0);
}

static const StringFormat *typo(void)
{
    static StringFormat sf(StringFormat::GenericTypographic());
    static int once;
    if (!once) {
        sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap |
                          StringFormatFlagsMeasureTrailingSpaces);
        once = 1;
    }
    return &sf;
}

double sk_seg(const wchar_t *s, int n, int px, int weight, int mono, DWORD argb,
              int draw, double x, double y)
{
    if (!g_gfx || !s || n <= 0) return 0;
    Gdiplus::Font *f = get_font_s(px, weight, mono, mono ? 0 : script_of(s, n));
    RectF box;
    g_gfx->MeasureString(s, n, f, PointF(0, 0), typo(), &box);
    double w = box.Width;
    if (draw && (argb >> 24)) {
        SolidBrush b(C(argb));
        g_gfx->DrawString(s, n, f, PointF((REAL)x, (REAL)y), typo(), &b);
    }
    return w;
}

double sk_tw(const wchar_t *s, int n, int px, int weight, int mono)
{
    return sk_seg(s, n, px, weight, mono, 0, 0, 0, 0);
}

double sk_tw_s(const wchar_t *s, int px, int weight, int mono)
{
    return sk_seg(s, (int)wcslen(s), px, weight, mono, 0, 0, 0, 0);
}

double sk_text_w(const wchar_t *s, int px, int weight, int mono)
{
    return sk_tw_s(s, px, weight, mono);
}

int sk_th(int px, int mono)
{
    return (int)(px * (mono ? 1.52 : 1.46) + 0.5);
}
double sk_text_h(int px, int mono) { return (double)sk_th(px, mono); }

void sk_text(double x, double y, double w, const wchar_t *s, int px, int weight,
             int mono, DWORD argb, int align)
{
    if (!g_gfx || !s || !*s) return;
    double tw = sk_tw_s(s, px, weight, mono);
    double ox = x;
    if (align == 1) ox = x + (w - tw) / 2.0;
    else if (align == 2) ox = x + w - tw;
    sk_seg(s, (int)wcslen(s), px, weight, mono, argb, 1, ox, y);
}

void sk_text_box(double x, double y, double w, double h, const wchar_t *s, int px,
                 int weight, int mono, DWORD argb, int align)
{
    if (!g_gfx || !s) return;
    int lh = sk_th(px, mono);
    double ty = y + (h - lh) / 2.0;
    /* sigmazsa kirp */
    int n = (int)wcslen(s);
    double tw = sk_tw(s, n, px, weight, mono);
    wchar_t buf[512];
    if (tw > w && n > 0) {
        int lo = 0, hi = n;
        while (lo < hi) {
            int mid = (lo + hi + 1) / 2;
            if (sk_tw(s, mid, px, weight, mono) <= w - 10) lo = mid;
            else hi = mid - 1;
        }
        if (lo > n) lo = n;
        int cch = lo < 510 ? lo : 510;
        memcpy(buf, s, (size_t)cch * sizeof(wchar_t));
        buf[cch] = 0;
        if (cch < n - 1) { buf[cch] = 0x2026; buf[cch + 1] = 0; }
        s = buf;
        tw = sk_tw_s(s, px, weight, mono);
    }
    double ox = x;
    if (align == 1) ox = x + (w - tw) / 2.0;
    else if (align == 2) ox = x + w - tw;
    sk_seg(s, (int)wcslen(s), px, weight, mono, argb, 1, ox, ty);
}

/* ---------------- ikonlar ---------------- */
void sk_icon(int kind, double cx, double cy, double sz, DWORD argb, double th)
{
    if (!g_gfx) return;
    Pen pen(C(argb), (REAL)th);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    pen.SetLineJoin(LineJoinRound);
    double h = sz / 2.0;
    switch (kind) {
    case SKI_SIDEBAR: {
        GraphicsPath p;
        add_round(&p, (REAL)(cx - h), (REAL)(cy - h * 0.78), (REAL)sz * 0.95,
                  (REAL)sz * 0.78, 3.0f, 3.0f, 3.0f, 3.0f);
        g_gfx->DrawPath(&pen, &p);
        sk_line(cx - h + 5.2, cy - h * 0.78 + 1, cx - h + 5.2, cy + h * 0.78 - 1, argb, th);
        break;
    }
    case SKI_PENCIL: {
        GraphicsPath p;
        add_round(&p, (REAL)(cx - h), (REAL)(cy - h * 0.8), (REAL)sz * 0.95,
                  (REAL)sz * 0.85, 3.0f, 3.0f, 3.0f, 3.0f);
        g_gfx->DrawPath(&pen, &p);
        sk_line(cx - 2.4, cy + 0.2, cx + 2.4, cy + 0.2, argb, th);
        sk_line(cx, cy - 2.2, cx, cy + 2.6, argb, th);
        break;
    }
    case SKI_CHEV_D:
        sk_line(cx - h * 0.62, cy - h * 0.22, cx, cy + h * 0.30, argb, th);
        sk_line(cx, cy + h * 0.30, cx + h * 0.62, cy - h * 0.22, argb, th);
        break;
    case SKI_CHEV_U:
        sk_line(cx - h * 0.62, cy + h * 0.22, cx, cy - h * 0.30, argb, th);
        sk_line(cx, cy - h * 0.30, cx + h * 0.62, cy + h * 0.22, argb, th);
        break;
    case SKI_BACK:
        sk_line(cx + h * 0.25, cy - h * 0.6, cx - h * 0.35, cy, argb, th);
        sk_line(cx - h * 0.35, cy, cx + h * 0.25, cy + h * 0.6, argb, th);
        break;
    case SKI_MENU:
        for (int i = -1; i <= 1; i++)
            sk_line(cx - h * 0.62, cy + i * h * 0.42, cx + h * 0.62, cy + i * h * 0.42,
                    argb, th);
        break;
    case SKI_MIN:
        sk_line(cx - h * 0.5, cy, cx + h * 0.5, cy, argb, th);
        break;
    case SKI_MAX:
        sk_line(cx - h * 0.45, cy - h * 0.45, cx + h * 0.45, cy - h * 0.45, argb, th);
        sk_line(cx + h * 0.45, cy - h * 0.45, cx + h * 0.45, cy + h * 0.45, argb, th);
        sk_line(cx + h * 0.45, cy + h * 0.45, cx - h * 0.45, cy + h * 0.45, argb, th);
        sk_line(cx - h * 0.45, cy + h * 0.45, cx - h * 0.45, cy - h * 0.45, argb, th);
        break;
    case SKI_RESTORE:
        sk_line(cx - h * 0.15, cy - h * 0.5, cx + h * 0.5, cy - h * 0.5, argb, th);
        sk_line(cx + h * 0.5, cy - h * 0.5, cx + h * 0.5, cy + h * 0.15, argb, th);
        sk_line(cx - h * 0.5, cy - h * 0.15, cx + h * 0.15, cy - h * 0.15, argb, th);
        sk_line(cx + h * 0.15, cy - h * 0.15, cx + h * 0.15, cy + h * 0.5, argb, th);
        sk_line(cx + h * 0.15, cy + h * 0.5, cx - h * 0.5, cy + h * 0.5, argb, th);
        sk_line(cx - h * 0.5, cy + h * 0.5, cx - h * 0.5, cy - h * 0.15, argb, th);
        break;
    case SKI_CLOSE:
        sk_line(cx - h * 0.5, cy - h * 0.5, cx + h * 0.5, cy + h * 0.5, argb, th);
        sk_line(cx + h * 0.5, cy - h * 0.5, cx - h * 0.5, cy + h * 0.5, argb, th);
        break;
    case SKI_PLUS:
        sk_line(cx - h * 0.55, cy, cx + h * 0.55, cy, argb, th);
        sk_line(cx, cy - h * 0.55, cx, cy + h * 0.55, argb, th);
        break;
    case SKI_SEARCH: {
        GraphicsPath p;
        p.AddEllipse((REAL)(cx - h * 0.75), (REAL)(cy - h * 0.75), (REAL)(sz * 0.62),
                     (REAL)(sz * 0.62));
        g_gfx->DrawPath(&pen, &p);
        sk_line(cx + h * 0.30, cy + h * 0.30, cx + h * 0.78, cy + h * 0.78, argb, th);
        break;
    }
    case SKI_UP:
        sk_line(cx, cy + h * 0.62, cx, cy - h * 0.55, argb, th);
        sk_line(cx - h * 0.5, cy - h * 0.10, cx, cy - h * 0.62, argb, th);
        sk_line(cx, cy - h * 0.62, cx + h * 0.5, cy - h * 0.10, argb, th);
        break;
    case SKI_STOP:
        sk_round(cx - sz * 0.26, cy - sz * 0.26, sz * 0.52, sz * 0.52, 2.0, argb);
        break;
    case SKI_CLIP: {
        GraphicsPath p;
        p.AddArc((REAL)(cx - h * 0.10), (REAL)(cy - h * 0.72), (REAL)(sz * 0.55),
                 (REAL)(sz * 0.55), 300, 300);
        g_gfx->DrawPath(&pen, &p);
        sk_line(cx - h * 0.42, cy - h * 0.30, cx - h * 0.42, cy + h * 0.45, argb, th);
        sk_line(cx + h * 0.30, cy - h * 0.05, cx + h * 0.30, cy + h * 0.62, argb, th);
        break;
    }
    case SKI_TRASH:
        sk_line(cx - h * 0.62, cy - h * 0.42, cx + h * 0.62, cy - h * 0.42, argb, th);
        sk_line(cx - h * 0.22, cy - h * 0.42, cx - h * 0.18, cy - h * 0.66, argb, th);
        sk_line(cx + h * 0.22, cy - h * 0.42, cx + h * 0.18, cy - h * 0.66, argb, th);
        sk_line(cx - h * 0.18, cy - h * 0.66, cx + h * 0.18, cy - h * 0.66, argb, th);
        sk_line(cx - h * 0.45, cy - h * 0.20, cx - h * 0.36, cy + h * 0.62, argb, th);
        sk_line(cx + h * 0.45, cy - h * 0.20, cx + h * 0.36, cy + h * 0.62, argb, th);
        sk_line(cx - h * 0.36, cy + h * 0.62, cx + h * 0.36, cy + h * 0.62, argb, th);
        break;
    case SKI_SPARKLE: {
        GraphicsPath p;
        REAL a = (REAL)(sz * 0.46), b = (REAL)(sz * 0.13);
        REAL X = (REAL)cx, Y = (REAL)cy, b3 = b * 0.35f;
        p.AddBezier(X, Y - a, X + b3, Y - b, X + b, Y - b3, X + a, Y);
        p.AddBezier(X + a, Y, X + b, Y + b3, X + b3, Y + b, X, Y + a);
        p.AddBezier(X, Y + a, X - b3, Y + b, X - b, Y + b3, X - a, Y);
        p.AddBezier(X - a, Y, X - b, Y - b3, X - b3, Y - b, X, Y - a);
        p.CloseFigure();
        SolidBrush br(C(argb));
        g_gfx->FillPath(&br, &p);
        break;
    }
    default:
        break;
    }
}

int sk_png_res(int id, double x, double y, double w, double h)
{
    if (!g_gfx) return 0;
    HMODULE hm = GetModuleHandleW(NULL);
    HRSRC rs = FindResourceW(hm, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!rs) return 0;
    DWORD sz = SizeofResource(hm, rs);
    HGLOBAL hg = LoadResource(hm, rs);
    if (!hg || !sz) return 0;
    void *data = LockResource(hg);
    if (!data) return 0;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sz);
    if (!mem) return 0;
    void *dst = GlobalLock(mem);
    memcpy(dst, data, sz);
    GlobalUnlock(mem);
    IStream *st = NULL;
    if (CreateStreamOnHGlobal(mem, TRUE, &st) != S_OK || !st) { GlobalFree(mem); return 0; }
    Image *img = Image::FromStream(st);
    int ok = 0;
    if (img && img->GetLastStatus() == Ok) {
        g_gfx->SetInterpolationMode(InterpolationModeHighQualityBicubic);
        ok = (g_gfx->DrawImage(img, RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h)) == Ok);
    }
    if (img) delete img;
    st->Release();
    return ok;
}

void sk_logo(double x, double y, double size, double radius)
{
    sk_grad(x, y, size, size, radius, 0xFF3D7BFF, 0xFF8A4DFF, 1);
    sk_icon(SKI_SPARKLE, x + size / 2.0, y + size / 2.0, size * 0.62, 0xFFFFFFFF, 2.0);
    /* kucuk ikinci parildama */
    sk_icon(SKI_SPARKLE, x + size * 0.72, y + size * 0.26, size * 0.20, 0xCCFFFFFF, 1.0);
}

const wchar_t *sk_font_name(int mono, int weight)
{
    if (mono) return pick_mono();
    if (weight >= 600 && family_ok(L"Segoe UI Semibold")) return L"Segoe UI Semibold";
    return pick_sans();
}

/* ---------------- kirpma ---------------- */
void sk_clip(double x, double y, double w, double h)
{
    if (!g_gfx) return;
    if (g_sp < 7) {
        g_state[++g_sp] = g_gfx->Save();
        g_gfx->SetClip(RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h), CombineModeIntersect);
    }
}

void sk_unclip(void)
{
    if (!g_gfx) return;
    if (g_sp >= 0) { g_gfx->Restore(g_state[g_sp--]); }
}
