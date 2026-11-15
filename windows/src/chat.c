#include "luxai.h"

/* Sohbet motoru: istek kurma, SSE akisi, ajan arac dongusu. */

static HANDLE        g_thread = NULL;
static volatile LONG g_cancel = 0;
static volatile LONG g_busy = 0;

/* UI'ya aktarilacak yarim UTF-8 dizisi (delta sinirinda bozulmasin) */
static Buf g_pending;

int chat_busy(void) { return g_busy != 0; }

static void post_status(const char *utf8)
{
    char *d = _strdup(utf8 ? utf8 : "");
    if (!d) return;
    if (!PostMessageW(g_hMain, WM_APP_STATUS, 0, (LPARAM)d)) free(d);
}

static size_t valid_utf8_prefix(const char *s, size_t n)
{
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t need = 0;
        if (c < 0x80) need = 1;
        else if ((c & 0xE0) == 0xC0) need = 2;
        else if ((c & 0xF0) == 0xE0) need = 3;
        else if ((c & 0xF8) == 0xF0) need = 4;
        else return i;               /* bozuk bayt: buraya kadar */
        if (i + need > n) return i;  /* yarim kaldi */
        i += need;
    }
    return i;
}

static void emit_delta(const char *utf8, size_t n)
{
    buf_add(&g_pending, utf8, n);
    size_t ok = valid_utf8_prefix(g_pending.p ? g_pending.p : "", g_pending.n);
    if (!ok) return;
    char *tmp = (char *)calloc(ok + 1, 1);
    if (!tmp) return;
    memcpy(tmp, g_pending.p, ok);
    wchar_t *w = u16(tmp);
    free(tmp);
    size_t rest = g_pending.n - ok;
    if (rest) memmove(g_pending.p, g_pending.p + ok, rest);
    g_pending.n = rest;
    if (g_pending.p) g_pending.p[rest] = 0;
    if (!w) return;
    if (!PostMessageW(g_hMain, WM_APP_DELTA, 0, (LPARAM)w)) free(w);
}

/* ---------------- istek govdesi ---------------- */
static void add_message_json(Buf *b, const Msg *m)
{
    buf_adds(b, "{\"role\":");
    json_write_str(b, m->role);
    if (!strcmp(m->role, "assistant") && m->tool_calls && *m->tool_calls) {
        buf_adds(b, ",\"content\":");
        if (m->content && *m->content) json_write_str(b, m->content);
        else buf_adds(b, "null");
        buf_adds(b, ",\"tool_calls\":");
        buf_adds(b, m->tool_calls);
        buf_addc(b, '}');
        return;
    }
    if (!strcmp(m->role, "tool")) {
        buf_adds(b, ",\"tool_call_id\":");
        json_write_str(b, m->tool_call_id ? m->tool_call_id : "");
    }
    buf_adds(b, ",\"content\":");
    json_write_str(b, m->content ? m->content : "");
    buf_addc(b, '}');
}

static char *build_request(MsgList *conv, int mode)
{
    Buf b; buf_init(&b);
    buf_adds(&b, "{\"model\":");
    /* ozel saglayicida kullanicinin girdigi model adi, LuxAI native'de flash/lite */
    json_write_str(&b, (g_cfg.use_openai && g_cfg.model_name[0])
                          ? g_cfg.model_name
                          : (g_cfg.model ? "lite" : "flash"));
    buf_adds(&b, ",\"stream\":true,\"max_tokens\":");
    {
        char tmp[32];
        _snprintf(tmp, sizeof tmp, "%d", g_cfg.max_tokens > 0 ? g_cfg.max_tokens : 4096);
        buf_adds(&b, tmp);
    }
    buf_adds(&b, ",\"messages\":[");
    char *sys = prompt_for(mode, g_cfg.workdir);
    buf_adds(&b, "{\"role\":\"system\",\"content\":");
    json_write_str(&b, sys);
    buf_addc(&b, '}');
    free(sys);
    for (int i = 0; i < conv->n; i++) {
        buf_addc(&b, ',');
        add_message_json(&b, &conv->v[i]);
    }
    buf_addc(&b, ']');
    if (mode == 1) {
        buf_adds(&b, ",\"tools\":");
        buf_adds(&b, tools_json());
        buf_adds(&b, ",\"tool_choice\":\"auto\"");
    }
    buf_addc(&b, '}');
    return buf_detach(&b);
}

/* ---------------- SSE ---------------- */
typedef struct {
    Buf   line;     /* yarim satir */
    Buf   raw;      /* tum govde (SSE degilse yedek) */
    Buf   content;  /* biriken yanit */
    int   saw_data;
    int   error;
    char  err[512];
    struct { char id[96]; char name[96]; Buf args; int used; } tc[8];
    int   ntc;
} Sse;

static void sse_handle_payload(Sse *s, const char *payload)
{
    if (!strcmp(payload, "[DONE]")) return;
    JVal *v = json_parse(payload);
    if (!v) return;
    JVal *err = json_get(v, "error");
    if (err) {
        const char *e = json_strv(err);
        if (!e) e = json_str(err, "message");
        if (!e) e = json_str(err, "code");
        s->error = 1;
        _snprintf(s->err, sizeof s->err, "%s", e ? e : "unknown error");
        json_free(v);
        return;
    }
    JVal *choices = json_get(v, "choices");
    JVal *c0 = json_at(choices, 0);
    if (!c0) { json_free(v); return; }
    JVal *delta = json_get(c0, "delta");
    if (!delta) delta = json_get(c0, "message");
    if (delta) {
        const char *txt = json_str(delta, "content");
        if (txt && *txt) {
            buf_adds(&s->content, txt);
            emit_delta(txt, strlen(txt));
        }
        JVal *tcs = json_get(delta, "tool_calls");
        for (int i = 0; tcs && i < tcs->n && s->ntc < 8; i++) {
            JVal *tc = json_at(tcs, i);
            if (!tc) continue;
            JVal *idx = json_get(tc, "index");
            int slot = idx ? (int)idx->num : s->ntc;
            if (slot < 0) slot = 0;
            if (slot >  7) slot = 7;
            if (!s->tc[slot].used) { s->tc[slot].used = 1; buf_init(&s->tc[slot].args); }
            const char *id = json_str(tc, "id");
            if (id && *id) { strncpy(s->tc[slot].id, id, 95); s->tc[slot].id[95] = 0; }
            JVal *fn = json_get(tc, "function");
            if (fn) {
                const char *nm = json_str(fn, "name");
                if (nm && *nm) { strncpy(s->tc[slot].name, nm, 95); s->tc[slot].name[95] = 0; }
                const char *ar = json_str(fn, "arguments");
                if (ar) buf_adds(&s->tc[slot].args, ar);
            }
            if (slot + 1 > s->ntc) s->ntc = slot + 1;
        }
    }
    json_free(v);
}

static void sse_line(Sse *s, char *line)
{
    size_t n = strlen(line);
    while (n && (line[n - 1] == '\r' || line[n - 1] == '\n')) line[--n] = 0;
    if (n > 5 && !strncmp(line, "data:", 5)) {
        char *p = line + 5;
        while (*p == ' ') p++;
        s->saw_data = 1;
        sse_handle_payload(s, p);
    }
}

static int sse_sink(const char *data, size_t len, void *ud)
{
    Sse *s = (Sse *)ud;
    buf_add(&s->raw, data, len);
    buf_add(&s->line, data, len);
    /* satirlara bol */
    size_t start = 0;
    for (size_t i = 0; i < s->line.n; i++) {
        if (s->line.p[i] == '\n') {
            s->line.p[i] = 0;
            sse_line(s, s->line.p + start);
            start = i + 1;
        }
    }
    if (start) {
        size_t rest = s->line.n - start;
        if (rest) memmove(s->line.p, s->line.p + start, rest);
        s->line.n = rest;
        if (s->line.p) s->line.p[rest] = 0;
    }
    return 0;
}

/* SSE gelmediyse govdeyi normal JSON yaniti gibi degerlendir */
static void sse_fallback(Sse *s)
{
    if (s->saw_data || !s->raw.p || !s->raw.n) return;
    JVal *v = json_parse(s->raw.p);
    if (!v) return;
    JVal *c0 = json_at(json_get(v, "choices"), 0);
    if (c0) {
        JVal *m = json_get(c0, "message");
        if (!m) m = json_get(c0, "delta");
        const char *txt = m ? json_str(m, "content") : NULL;
        if (txt && *txt) { buf_adds(&s->content, txt); emit_delta(txt, strlen(txt)); }
        JVal *tcs = m ? json_get(m, "tool_calls") : NULL;
        for (int i = 0; tcs && i < tcs->n && s->ntc < 8; i++) {
            JVal *tc = json_at(tcs, i);
            JVal *fn = tc ? json_get(tc, "function") : NULL;
            if (!fn) continue;
            int slot = s->ntc;
            s->tc[slot].used = 1;
            buf_init(&s->tc[slot].args);
            const char *id = json_str(tc, "id");
            const char *nm = json_str(fn, "name");
            const char *ar = json_str(fn, "arguments");
            if (id) { strncpy(s->tc[slot].id, id, 95); s->tc[slot].id[95] = 0; }
            if (nm) { strncpy(s->tc[slot].name, nm, 95); s->tc[slot].name[95] = 0; }
            if (ar) buf_adds(&s->tc[slot].args, ar);
            s->ntc++;
        }
    }
    JVal *e = json_get(v, "error");
    if (e) {
        const char *es = json_strv(e);
        if (!es) es = json_str(e, "message");
        s->error = 1;
        _snprintf(s->err, sizeof s->err, "%s", es ? es : "error");
    }
    json_free(v);
}

/* ---------------- yardimcilar ---------------- */
static wchar_t *build_headers(void)
{
    static wchar_t h[2048];
    char *tok = u8(g_token);
    h[0] = 0;
    wcscat(h, L"Content-Type: application/json; charset=utf-8\r\n");
    wcscat(h, L"Accept: text/event-stream, application/json\r\n");
    wcscat(h, L"X-Lux-Client: luxai\r\n");
    if (g_cfg.use_openai) {
        wchar_t *wk = u16(g_cfg.api_key);
        if (wk && *wk) { wcscat(h, L"Authorization: Bearer "); wcscat(h, wk); wcscat(h, L"\r\n"); }
        free(wk);
    } else if (tok && *tok) {
        wcscat(h, L"Cookie: lux_account_session=");
        wchar_t *wt = u16(tok);
        if (wt) { wcscat(h, wt); free(wt); }
        wcscat(h, L"\r\n");
    }
    free(tok);
    return h;
}

static char *tool_calls_json(Sse *s)
{
    Buf b; buf_init(&b);
    buf_addc(&b, '[');
    int first = 1;
    for (int i = 0; i < s->ntc; i++) {
        if (!s->tc[i].used || !s->tc[i].name[0]) continue;
        if (!first) buf_addc(&b, ',');
        first = 0;
        buf_adds(&b, "{\"id\":");
        json_write_str(&b, s->tc[i].id[0] ? s->tc[i].id : "call_luxai");
        buf_adds(&b, ",\"type\":\"function\",\"function\":{\"name\":");
        json_write_str(&b, s->tc[i].name);
        buf_adds(&b, ",\"arguments\":");
        json_write_str(&b, s->tc[i].args.p ? s->tc[i].args.p : "{}");
        buf_adds(&b, "}}");
    }
    buf_addc(&b, ']');
    return buf_detach(&b);
}

static void sse_free(Sse *s)
{
    buf_free(&s->line); buf_free(&s->raw); buf_free(&s->content);
    for (int i = 0; i < 8; i++) if (s->tc[i].used) buf_free(&s->tc[i].args);
}

/* Tek bir model cagrisi: 0 basarili (content veya tool_calls dolu), -1 hata */
static int one_call(MsgList *conv, int mode, Sse *s)
{
    char *body = build_request(conv, mode);
    ZeroMemory(s, sizeof *s);
    buf_init(&s->line); buf_init(&s->raw); buf_init(&s->content);

    wchar_t *url = u16(g_cfg.use_openai && g_cfg.endpoint[0] ? g_cfg.endpoint
                                                             : "https://lux-ai.cz/api/ide/chat/completions");
    Buf err; buf_init(&err);
    int status = 0;
    wchar_t *hdrs = build_headers();
    {
        /* teshis icin son istek/yanit (parola veya anahtar yazilmaz) */
        wchar_t p[MAX_PATH];
        path_appdata(p, MAX_PATH, L"last_request.json");
        file_write_utf8(p, body, strlen(body));
    }
    http_request(url, hdrs, body, strlen(body), sse_sink, s, &status, &err, &g_cancel);
    {
        wchar_t p[MAX_PATH];
        path_appdata(p, MAX_PATH, L"last_response.txt");
        char head[128];
        _snprintf(head, sizeof head, "# HTTP %d\n", status);
        head[sizeof head - 1] = 0;
        Buf dump; buf_init(&dump);
        buf_adds(&dump, head);
        if (s->raw.p) buf_adds(&dump, s->raw.p);
        if (err.p) { buf_adds(&dump, "\n# error: "); buf_adds(&dump, err.p); }
        file_write_utf8(p, dump.p ? dump.p : "", dump.n);
        buf_free(&dump);
    }
    free(url);
    free(body);

    if (g_cancel) { buf_free(&err); return -1; }
    if (status != 200) {
        char *msg = err.p ? _strdup(err.p) : _strdup("network");
        if (status == 401 || status == 403) {
            free(msg);
            msg = _strdup(t(S_LOGIN_FAIL));
            post_status(t(S_NET_ERR));
        } else if (status == 429) {
            free(msg);
            msg = _strdup("429");
        }
        char line[640];
        _snprintf(line, sizeof line, "%s (HTTP %d)", msg ? msg : "error", status);
        line[sizeof line - 1] = 0;
        post_status(line);
        free(msg);
        buf_free(&err);
        return -1;
    }
    buf_free(&err);
    sse_fallback(s);
    if (s->error) {
        if (!strcmp(s->err, "auth") || !strcmp(s->err, "signed_out")) {
            post_status(t(S_LOGIN_FAIL));
        } else {
            char line[600];
            _snprintf(line, sizeof line, "%s: %s", t(S_ERROR), s->err);
            line[sizeof line - 1] = 0;
            post_status(line);
        }
        return -1;
    }
    return 0;
}

/* ---------------- ajan dongusu ---------------- */
static DWORD WINAPI worker(LPVOID p)
{
    (void)p;
    int mode = g_cfg.mode;
    int rounds = 0;
    for (;;) {
        if (g_cancel) break;
        if (rounds++ > 8) { post_status("too many tool rounds"); break; }
        Sse s;
        int r = one_call(&g_conv, mode, &s);
        if (r != 0) { sse_free(&s); break; }
        char *tcs = tool_calls_json(&s);
        int has_tools = (s.ntc > 0 && strcmp(tcs, "[]") != 0);
        if (!has_tools) {
            const char *txt = s.content.p ? s.content.p : "";
            if (!*txt) post_status(t(S_NOMODEL));
            msglist_add(&g_conv, "assistant", txt, NULL, NULL);
            free(tcs);
            sse_free(&s);
            break;
        }
        /* asistanin arac cagrisi mesaji */
        msglist_add(&g_conv, "assistant", s.content.p ? s.content.p : "", tcs, NULL);
        free(tcs);
        for (int i = 0; i < s.ntc; i++) {
            if (!s.tc[i].used || !s.tc[i].name[0]) continue;
            if (g_cancel) break;
            const char *args = s.tc[i].args.p ? s.tc[i].args.p : "{}";
            char *result;
            if (tool_blocked(s.tc[i].name, args)) {
                post_status(t(S_BLOCKED));
                result = _strdup("error: refused by the safety rules of LuxAI; do not retry or work around it");
            } else {
                int allow = 1;
                if (tool_is_write(s.tc[i].name)) {
                    wchar_t *wt = u16(s.tc[i].name);
                    wchar_t detail[4096];
                    wchar_t *wa = u16(args);
                    _snwprintf(detail, 4096, L"%s\n\n%s(%s)", tw(S_ALLOW),
                               wt ? wt : L"?", wa ? wa : L"{}");
                    detail[4095] = 0;
                    allow = ui_confirm(tw(S_CONFIRM), detail, 0);
                    free(wt); free(wa);
                }
                if (!allow) {
                    post_status(t(S_DENIED));
                    result = _strdup("error: the user denied this action; do not try to work around it");
                } else {
                    char msg[256];
                    _snprintf(msg, sizeof msg, "%s: %s", t(S_TOOL), s.tc[i].name);
                    msg[sizeof msg - 1] = 0;
                    post_status(msg);
                    result = tool_execute(s.tc[i].name, args);
                }
            }
            msglist_add(&g_conv, "tool", result ? result : "", NULL,
                        s.tc[i].id[0] ? s.tc[i].id : "call_luxai");
            free(result);
        }
        sse_free(&s);
        if (g_cancel) break;
    }
    g_busy = 0;
    PostMessageW(g_hMain, WM_APP_DONE, 0, 0);
    return 0;
}

void chat_send(const char *user_utf8)
{
    if (g_busy) return;
    if (!user_utf8 || !*user_utf8) return;
    g_cancel = 0;
    g_busy = 1;
    buf_free(&g_pending);
    buf_init(&g_pending);
    g_thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (!g_thread) { g_busy = 0; post_status("cannot start thread"); }
}

void chat_cancel(void)
{
    if (g_busy) { g_cancel = 1; post_status(t(S_STOP)); }
}
