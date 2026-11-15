/* LuxAI native — cizim katmani (GDI+ tabanli, GTK CSS ile birebir)
 * Bu baslik hem C hem C++ derlenebilir; uygulama kodu C, cizim kodu C++.
 */
#ifndef LUXAI_SKIN_H
#define LUXAI_SKIN_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- palet (degerler orijinal uygulamanin GTK CSS'inden alindi) ---- */
typedef struct {
    DWORD bg, side, fg, dim, border, card, code, input;
    DWORD accent, danger, warn, ok;
    DWORD header, hbtn, hbtn_hi;     /* headerbar + buton yuzeyleri */
    DWORD on_accent;                 /* vurgu uzerindeki yazi rengi */
} SkPal;

extern SkPal SK;

void  sk_init(void);
void  sk_shutdown(void);
void  sk_theme(int dark);
int   sk_is_dark(void);
void  sk_set_dpi(UINT dpi);
double sk_scale(void);

/* cerceve: dc'ye baglan, mantiksal (96 dpi) koordinatlarla ciz */
void  sk_begin(HDC dc);
void  sk_end(void);

/* ---- sekiller ---- */
void  sk_fill(double x, double y, double w, double h, DWORD argb);
void  sk_round(double x, double y, double w, double h, double r, DWORD argb);
/* koseleri ayri yuvarlatilmis dolgu (balonlar icin) */
void  sk_round4(double x, double y, double w, double h, double rtl, double rtr,
                double rbr, double rbl, DWORD argb);
void  sk_rline(double x, double y, double w, double h, double r, DWORD argb, double th);
void  sk_grad(double x, double y, double w, double h, double r, DWORD c1, DWORD c2, int diagonal);
void  sk_line(double x1, double y1, double x2, double y2, DWORD argb, double th);
void  sk_circle(double cx, double cy, double r, DWORD argb);

/* ---- yazi ---- */
int    sk_th(int px, int mono);                    /* satir yuksekligi */
double sk_tw(const wchar_t *s, int n, int px, int weight, int mono);
/* seg'i (x,y) noktasina ciz (draw=1) ve genisligini dondur */
double sk_seg(const wchar_t *s, int n, int px, int weight, int mono, DWORD argb,
              int draw, double x, double y);
/* tek satir hizalama: align 0 sol, 1 orta, 2 sag */
void   sk_text(double x, double y, double w, const wchar_t *s, int px, int weight,
               int mono, DWORD argb, int align);
/* kutu icine hizali, dikey ortali, gerekirse "..." ile kisaltilmis tek satir */
void   sk_text_box(double x, double y, double w, double h, const wchar_t *s, int px,
                   int weight, int mono, DWORD argb, int align);
double sk_text_w(const wchar_t *s, int px, int weight, int mono);
double sk_tw_s(const wchar_t *s, int px, int weight, int mono);
double sk_text_h(int px, int mono);

/* ---- ikonlar ---- */
enum {
    SKI_SIDEBAR = 1, SKI_PENCIL, SKI_CHEV_D, SKI_CHEV_U, SKI_MENU, SKI_MIN, SKI_MAX,
    SKI_CLOSE, SKI_PLUS, SKI_SEARCH, SKI_UP, SKI_STOP, SKI_CLIP, SKI_TRASH, SKI_BACK,
    SKI_SPARKLE, SKI_RESTORE
};
void sk_icon(int kind, double cx, double cy, double size, DWORD argb, double th);
/* uygulama logosu: yuvarlatilmis kare + parildama */
void sk_logo(double x, double y, double size, double radius);

/* arayuz yazi tipi (metin kutulari icin) */
const wchar_t *sk_font_name(int mono, int weight);

/* ---- kirpma ---- */
void sk_clip(double x, double y, double w, double h);
void sk_unclip(void);

#ifdef __cplusplus
}
#endif
#endif
