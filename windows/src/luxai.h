/* LuxAI Native for Windows — ortak baslik (common header)
 * Windows 7 SP1+ hedefli, GTK/WSL gerektirmeyen native Win32 istemci.
 * API sozlesmesi LuxAI 2.0.2 ikili dosyasindan ve luxai_account.py'den cikarildi:
 *   POST https://lux-ai.cz/api/ide/chat/completions   (SSE, OpenAI uyumlu)
 *   Cookie: lux_account_session=<token>   X-Lux-Client: luxai
 *   POST https://lux-ai.cz/api/auth/password/login    {"email","password"} -> {sessionToken,user}
 */
#ifndef LUXAI_H
#define LUXAI_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  /* Windows 7 */
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0800
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define APP_NAME      L"LuxAI"
#define APP_VERSION   L"2.0.2-win.1"

/* ---------- util ---------- */
typedef struct { char *p; size_t n, cap; } Buf;
void  buf_init(Buf *b);
void  buf_free(Buf *b);
void  buf_reserve(Buf *b, size_t extra);
void  buf_add(Buf *b, const void *d, size_t n);
void  buf_adds(Buf *b, const char *s);
void  buf_addc(Buf *b, char c);
char *buf_detach(Buf *b);
void  buf_clear(Buf *b);

wchar_t *u16(const char *utf8);          /* caller frees */
char    *u8(const wchar_t *w);           /* caller frees */
void    path_appdata(wchar_t *out, size_t cch, const wchar_t *leaf);
void    path_ensure(wchar_t *path);      /* creates all folders of a dir path */
char   *file_read_utf8(const wchar_t *path);
int     file_write_utf8(const wchar_t *path, const char *data, size_t len);
int     mkdir_p(const wchar_t *dir);

/* ---------- json (sadece okuma) ---------- */
typedef enum { JNULL, JBOOL, JNUM, JSTR, JARR, JOBJ } JType;
typedef struct JVal {
    JType t;
    double num; int b;
    char *s;
    struct JVal **items;
    char **keys;
    int n;
} JVal;
JVal       *json_parse(const char *text);
void        json_free(JVal *v);
JVal       *json_get(const JVal *v, const char *key);
JVal       *json_at(const JVal *v, int i);
const char *json_str(const JVal *v, const char *key);
const char *json_strv(const JVal *v);
void        json_write_str(Buf *b, const char *s);   /* "..." kacisli */

/* ---------- http ---------- */
typedef int (*HttpSink)(const char *data, size_t len, void *ud);
int  http_request(const wchar_t *url, const wchar_t *extra_headers,
                  const char *body, size_t bodylen,
                  HttpSink sink, void *ud, int *status, Buf *err,
                  volatile LONG *cancel);

/* ---------- ayarlar / hesap ---------- */
typedef struct {
    int  model;              /* 0 flash, 1 lite */
    int  mode;               /* 0 chat, 1 agent */
    int  lang;               /* 0 tr, 1 en */
    int  theme;              /* 0 sistem, 1 acik, 2 koyu */
    int  send_ctrl_enter;
    int  auto_approve;
    int  max_tokens;
    int  stream;
    int  use_openai;         /* 1 = OpenAI uyumlu uç nokta + api key */
    char endpoint[512];      /* tam URL */
    char api_key[256];
    char model_name[128];    /* ozel saglayicida kullanilacak model adi */
    wchar_t workdir[MAX_PATH];
} Config;

/* ---- OpenAI uyumlu ozel saglayicilar (birden fazla) ---- */
#define MAX_PROV 8
typedef struct {
    char name[64];        /* kullanicinin verdigi ad */
    char endpoint[512];   /* tam URL: .../v1/chat/completions */
    char key[256];
    char model[128];      /* bos ise sunucu varsayilani */
    int  agent;           /* 1 = ajan (arac cagrilari) de kullanilabilir */
} Provider;

extern Provider g_prov[MAX_PROV];
extern int g_nprov;      /* kayitli saglayici sayisi */
extern int g_prov_sel;   /* -1 = LuxAI native, aksi halde 0..g_nprov-1 */

void prov_load(void);
void prov_save(void);
void prov_apply(int idx);      /* idx < 0 -> LuxAI native */
const wchar_t *prov_label(int idx, wchar_t *b, size_t cch);

extern Config g_cfg;
extern wchar_t g_token[1024];
extern int g_startup_account;
extern int g_startup_tab;
extern wchar_t g_email[256];
extern wchar_t g_username[128];

void cfg_defaults(void);
void cfg_load(void);
void cfg_save(void);
void account_load(void);
void account_save(void);
void account_clear(void);

/* ---------- i18n ---------- */
#include "i18n_ids.h"
const char *t(int id);
const wchar_t *tw(int id);   /* statik donusum tamponu */
int  lang_from_windows(void);
const char *lang_name(int i);

/* ---------- mesaj listesi ---------- */
typedef struct {
    char *role;        /* user | assistant | tool | system */
    char *content;
    char *tool_calls;  /* assistant icin ham JSON dizisi, yoksa NULL */
    char *tool_call_id;
} Msg;

typedef struct { Msg *v; int n, cap; } MsgList;
void msglist_add(MsgList *l, const char *role, const char *content,
                 const char *tool_calls, const char *tool_call_id);
void msglist_clear(MsgList *l);

/* ---------- agent araclari ---------- */
const char *tools_json(void);            /* LuxAI'ye gonderilen arac semasi */
char *tool_execute(const char *name, const char *args_json);
int   tool_is_write(const char *name);
int   tool_blocked(const char *name, const char *args_json); /* tehlikeli -> 1 */

/* ---------- ui ---------- */
extern HWND g_hMain;
extern wchar_t g_startup_ask[2048];   /* --ask "..." : acilista otomatik gonderim (CI testi) */
extern MsgList g_conv;
void ui_run(void);
void ui_render_all(void);
void ui_append_stream(const wchar_t *text);
void ui_render_label(const wchar_t *role_utf8);
void ui_set_status(const char *utf8);
void ui_conversation_clear(void);
void ui_conversation_save(void);
void ui_sidebar_reload(void);
int  ui_confirm(const wchar_t *title, const wchar_t *detail, int twice);
void ui_post_report(const char *utf8);   /* thread -> ui durum satiri */

#define WM_APP_STATUS   (WM_APP + 1)
#define WM_APP_REPORT   (WM_APP + 2)
#define WM_APP_DELTA    (WM_APP + 3)
#define WM_APP_DONE     (WM_APP + 4)
#define WM_APP_CONFIRM  (WM_APP + 5)

/* ---------- sohbet motoru (chat.c) ---------- */
char *prompt_for(int mode, const wchar_t *workdir);
void  chat_send(const char *user_utf8);
void  chat_cancel(void);
int   chat_busy(void);

#endif /* LUXAI_H */
