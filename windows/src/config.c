#include "luxai.h"
#include <shlobj.h>
#include <shlwapi.h>

/* ---------------- i18n ---------------- */
#include "i18n_ids.h"

static const char *const TBL[2][S_COUNT] = {
    {
#define X(id, tr, en) tr,
        STRS(X)
#undef X
    },
    {
#define X(id, tr, en) en,
        STRS(X)
#undef X
    }
};

const char *t(int id)
{
    int l = (g_cfg.lang == 1) ? 1 : 0;
    if (id < 0 || id >= S_COUNT) return "";
    return TBL[l][id];
}

const wchar_t *tw(int id)
{
    static wchar_t buf[512];
    wchar_t *w = u16(t(id));
    wcsncpy(buf, w ? w : L"", 511);
    buf[511] = 0;
    free(w);
    return buf;
}

/* ---------------- ayarlar ---------------- */
Config g_cfg;
wchar_t g_token[1024];
wchar_t g_email[256];
wchar_t g_username[128];

void cfg_defaults(void)
{
    ZeroMemory(&g_cfg, sizeof g_cfg);
    g_cfg.model = 0;
    g_cfg.mode = 0;
    g_cfg.theme = 0;
    g_cfg.lang = (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_TURKISH) ? 0 : 1;
    g_cfg.send_ctrl_enter = 0;
    g_cfg.auto_approve = 0;
    g_cfg.max_tokens = 4096;
    g_cfg.stream = 1;
    strcpy(g_cfg.endpoint, "https://lux-ai.cz/api/ide/chat/completions");
    wchar_t home[MAX_PATH] = L"";
    if (!SHGetSpecialFolderPathW(NULL, home, CSIDL_PROFILE, FALSE)) wcscpy(home, L"C:\\");
    wcsncpy(g_cfg.workdir, home, MAX_PATH - 1);
}

static void cfg_file(wchar_t *out, size_t cch) { path_appdata(out, cch, L"settings.ini"); }
static void acct_file(wchar_t *out, size_t cch) { path_appdata(out, cch, L"account.json"); }

void cfg_load(void)
{
    cfg_defaults();
    wchar_t p[MAX_PATH];
    cfg_file(p, MAX_PATH);
    char *d = file_read_utf8(p);
    if (!d) return;
    char *line = d, *nl;
    while (line && *line) {
        nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = 0;
            const char *k = line, *v = eq + 1;
            while (*v == ' ') v++;
            if (!strcmp(k, "model")) g_cfg.model = atoi(v);
            else if (!strcmp(k, "mode")) g_cfg.mode = atoi(v);
            else if (!strcmp(k, "lang")) g_cfg.lang = atoi(v);
            else if (!strcmp(k, "theme")) g_cfg.theme = atoi(v);
            else if (!strcmp(k, "send_ctrl_enter")) g_cfg.send_ctrl_enter = atoi(v);
            else if (!strcmp(k, "auto_approve")) g_cfg.auto_approve = atoi(v);
            else if (!strcmp(k, "max_tokens")) g_cfg.max_tokens = atoi(v);
            else if (!strcmp(k, "stream")) g_cfg.stream = atoi(v);
            else if (!strcmp(k, "use_openai")) g_cfg.use_openai = atoi(v);
            else if (!strcmp(k, "endpoint")) { strncpy(g_cfg.endpoint, v, sizeof g_cfg.endpoint - 1); }
            else if (!strcmp(k, "api_key")) { strncpy(g_cfg.api_key, v, sizeof g_cfg.api_key - 1); }
            else if (!strcmp(k, "workdir")) {
                wchar_t *w = u16(v);
                if (w) { wcsncpy(g_cfg.workdir, w, MAX_PATH - 1); free(w); }
            }
        }
        line = nl ? nl + 1 : NULL;
    }
    free(d);
    if (g_cfg.max_tokens < 256) g_cfg.max_tokens = 4096;
}

void cfg_save(void)
{
    Buf b; buf_init(&b);
    char tmp[64];
    _snprintf(tmp, sizeof tmp, "model=%d\n", g_cfg.model); buf_adds(&b, tmp);
    _snprintf(tmp, sizeof tmp, "mode=%d\n", g_cfg.mode); buf_adds(&b, tmp);
    _snprintf(tmp, sizeof tmp, "lang=%d\n", g_cfg.lang); buf_adds(&b, tmp);
    _snprintf(tmp, sizeof tmp, "theme=%d\n", g_cfg.theme); buf_adds(&b, tmp);
    _snprintf(tmp, sizeof tmp, "send_ctrl_enter=%d\n", g_cfg.send_ctrl_enter); buf_adds(&b, tmp);
    _snprintf(tmp, sizeof tmp, "auto_approve=%d\n", g_cfg.auto_approve); buf_adds(&b, tmp);
    _snprintf(tmp, sizeof tmp, "max_tokens=%d\n", g_cfg.max_tokens); buf_adds(&b, tmp);
    _snprintf(tmp, sizeof tmp, "stream=%d\n", g_cfg.stream); buf_adds(&b, tmp);
    _snprintf(tmp, sizeof tmp, "use_openai=%d\n", g_cfg.use_openai); buf_adds(&b, tmp);
    buf_adds(&b, "endpoint="); buf_adds(&b, g_cfg.endpoint); buf_addc(&b, '\n');
    buf_adds(&b, "api_key="); buf_adds(&b, g_cfg.api_key); buf_addc(&b, '\n');
    char *wd = u8(g_cfg.workdir);
    buf_adds(&b, "workdir="); buf_adds(&b, wd); buf_addc(&b, '\n');
    free(wd);
    wchar_t p[MAX_PATH];
    cfg_file(p, MAX_PATH);
    file_write_utf8(p, b.p ? b.p : "", b.n);
    buf_free(&b);
}

void account_load(void)
{
    g_token[0] = g_email[0] = g_username[0] = 0;
    wchar_t p[MAX_PATH];
    acct_file(p, MAX_PATH);
    char *d = file_read_utf8(p);
    if (!d) return;
    JVal *v = json_parse(d);
    free(d);
    if (!v) return;
    const char *tok = json_str(v, "sessionToken");
    if (tok) { wchar_t *w = u16(tok); if (w) { wcsncpy(g_token, w, 1023); free(w); } }
    JVal *u = json_get(v, "user");
    if (u) {
        const char *e = json_str(u, "email"), *n = json_str(u, "name"), *un = json_str(u, "username");
        if (e) { wchar_t *w = u16(e); if (w) { wcsncpy(g_email, w, 255); free(w); } }
        if (un) { wchar_t *w = u16(un); if (w) { wcsncpy(g_username, w, 127); free(w); } }
        else if (n) { wchar_t *w = u16(n); if (w) { wcsncpy(g_username, w, 127); free(w); } }
    }
    json_free(v);
}

void account_save(void)
{
    Buf b; buf_init(&b);
    char *tok = u8(g_token), *em = u8(g_email), *un = u8(g_username);
    buf_adds(&b, "{\"version\":1,\"origin\":\"https://lux-ai.cz\",\"sessionToken\":");
    json_write_str(&b, tok);
    buf_adds(&b, ",\"user\":{\"email\":");
    json_write_str(&b, em);
    buf_adds(&b, ",\"username\":");
    json_write_str(&b, un);
    buf_adds(&b, "},\"client\":\"luxai-win\"}");
    wchar_t p[MAX_PATH];
    acct_file(p, MAX_PATH);
    file_write_utf8(p, b.p, b.n);
    buf_free(&b);
    free(tok); free(em); free(un);
}

void account_clear(void)
{
    g_token[0] = g_email[0] = g_username[0] = 0;
    wchar_t p[MAX_PATH];
    acct_file(p, MAX_PATH);
    DeleteFileW(p);
}

/* ---------------- mesaj listesi ---------------- */
static char *dupz(const char *s) { return s ? _strdup(s) : NULL; }

void msglist_add(MsgList *l, const char *role, const char *content,
                 const char *tool_calls, const char *tool_call_id)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->v = (Msg *)realloc(l->v, (size_t)l->cap * sizeof(Msg));
        if (!l->v) exit(1);
    }
    Msg *m = &l->v[l->n++];
    m->role = dupz(role ? role : "user");
    m->content = dupz(content ? content : "");
    m->tool_calls = dupz(tool_calls);
    m->tool_call_id = dupz(tool_call_id);
}

void msglist_clear(MsgList *l)
{
    for (int i = 0; i < l->n; i++) {
        free(l->v[i].role); free(l->v[i].content);
        free(l->v[i].tool_calls); free(l->v[i].tool_call_id);
    }
    free(l->v);
    l->v = NULL; l->n = l->cap = 0;
}
