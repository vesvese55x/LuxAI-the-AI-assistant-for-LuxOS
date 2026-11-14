#include "luxai.h"
#include <ctype.h>

/* Kucuk, bagimsiz JSON okuyucu (yalnizca ayristirma + sorgu). */

typedef struct { const char *p; int err; } JP;

static JVal *jnew(JType t)
{
    JVal *v = (JVal *)calloc(1, sizeof(JVal));
    if (!v) exit(1);
    v->t = t;
    return v;
}

static void jskip(JP *j)
{
    while (*j->p && (unsigned char)*j->p <= ' ') j->p++;
}

static void jutf8(JVal *v, const char *s, size_t n)
{
    char *out = (char *)calloc(n + 1, 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n) {
            char c = s[++i];
            switch (c) {
            case 'n': out[o++] = '\n'; break;
            case 't': out[o++] = '\t'; break;
            case 'r': out[o++] = '\r'; break;
            case 'b': out[o++] = '\b'; break;
            case 'f': out[o++] = '\f'; break;
            case '/': out[o++] = '/'; break;
            case '\\': out[o++] = '\\'; break;
            case '"': out[o++] = '"'; break;
            case 'u': {
                unsigned cp = 0;
                int k = 0;
                for (; k < 4 && i + 1 < n; k++) {
                    char h = s[++i];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                    else break;
                }
                unsigned c1 = cp;
                /* vekil cift (surrogate pair) */
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 5 < n &&
                    s[i + 1] == '\\' && s[i + 2] == 'u') {
                    unsigned lo = 0;
                    const char *q = s + i + 3;
                    int ok = 1;
                    for (int m = 0; m < 4; m++) {
                        char h = q[m];
                        lo <<= 4;
                        if (h >= '0' && h <= '9') lo |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') lo |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') lo |= (unsigned)(h - 'A' + 10);
                        else { ok = 0; break; }
                    }
                    if (ok) {
                        c1 = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        i += 6;
                    }
                }
                if (c1 < 0x80) out[o++] = (char)c1;
                else if (c1 < 0x800) {
                    out[o++] = (char)(0xC0 | (c1 >> 6));
                    out[o++] = (char)(0x80 | (c1 & 0x3F));
                } else if (c1 < 0x10000) {
                    out[o++] = (char)(0xE0 | (c1 >> 12));
                    out[o++] = (char)(0x80 | ((c1 >> 6) & 0x3F));
                    out[o++] = (char)(0x80 | (c1 & 0x3F));
                } else {
                    out[o++] = (char)(0xF0 | (c1 >> 18));
                    out[o++] = (char)(0x80 | ((c1 >> 12) & 0x3F));
                    out[o++] = (char)(0x80 | ((c1 >> 6) & 0x3F));
                    out[o++] = (char)(0x80 | (c1 & 0x3F));
                }
                break;
            }
            default: out[o++] = c; break;
            }
        } else {
            out[o++] = s[i];
        }
    }
    out[o] = 0;
    v->s = out;
}

static JVal *jvalue(JP *j);

static JVal *jstring(JP *j)
{
    if (*j->p != '"') { j->err = 1; return NULL; }
    j->p++;
    const char *start = j->p;
    while (*j->p && *j->p != '"') {
        if (*j->p == '\\' && j->p[1]) j->p++;
        j->p++;
    }
    JVal *v = jnew(JSTR);
    jutf8(v, start, (size_t)(j->p - start));
    if (*j->p == '"') j->p++;
    return v;
}

static void jpush(JVal *v, char *key, JVal *item)
{
    if (v->n % 8 == 0) {
        size_t cap = (size_t)(v->n + 8);
        v->items = (JVal **)realloc(v->items, cap * sizeof(JVal *));
        if (v->t == JOBJ) v->keys = (char **)realloc(v->keys, cap * sizeof(char *));
    }
    v->items[v->n] = item;
    if (v->t == JOBJ) v->keys[v->n] = key;
    v->n++;
}

static JVal *jvalue(JP *j)
{
    jskip(j);
    if (!*j->p) { j->err = 1; return NULL; }
    char c = *j->p;
    if (c == '{') {
        j->p++;
        JVal *v = jnew(JOBJ);
        jskip(j);
        if (*j->p == '}') { j->p++; return v; }
        for (;;) {
            jskip(j);
            JVal *k = jstring(j);
            if (!k) { json_free(v); return NULL; }
            jskip(j);
            if (*j->p == ':') j->p++;
            JVal *val = jvalue(j);
            if (!val) { free(k->s); free(k); json_free(v); return NULL; }
            jpush(v, k->s, val);
            free(k);
            jskip(j);
            if (*j->p == ',') { j->p++; continue; }
            if (*j->p == '}') { j->p++; break; }
            json_free(v); j->err = 1; return NULL;
        }
        return v;
    }
    if (c == '[') {
        j->p++;
        JVal *v = jnew(JARR);
        jskip(j);
        if (*j->p == ']') { j->p++; return v; }
        for (;;) {
            JVal *val = jvalue(j);
            if (!val) { json_free(v); return NULL; }
            jpush(v, NULL, val);
            jskip(j);
            if (*j->p == ',') { j->p++; continue; }
            if (*j->p == ']') { j->p++; break; }
            json_free(v); j->err = 1; return NULL;
        }
        return v;
    }
    if (c == '"') return jstring(j);
    if (!strncmp(j->p, "true", 4))  { j->p += 4; JVal *v = jnew(JBOOL); v->b = 1; return v; }
    if (!strncmp(j->p, "false", 5)) { j->p += 5; JVal *v = jnew(JBOOL); v->b = 0; return v; }
    if (!strncmp(j->p, "null", 4))  { j->p += 4; return jnew(JNULL); }
    /* sayi */
    {
        char *end = NULL;
        double d = strtod(j->p, &end);
        if (end == j->p) { j->err = 1; return NULL; }
        j->p = end;
        JVal *v = jnew(JNUM);
        v->num = d;
        return v;
    }
}

JVal *json_parse(const char *text)
{
    if (!text) return NULL;
    JP j = { text, 0 };
    JVal *v = jvalue(&j);
    if (j.err) { json_free(v); return NULL; }
    return v;
}

void json_free(JVal *v)
{
    if (!v) return;
    for (int i = 0; i < v->n; i++) {
        json_free(v->items[i]);
        if (v->keys) free(v->keys[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->s);
    free(v);
}

JVal *json_get(const JVal *v, const char *key)
{
    if (!v || v->t != JOBJ || !key) return NULL;
    for (int i = 0; i < v->n; i++)
        if (v->keys[i] && !strcmp(v->keys[i], key)) return v->items[i];
    return NULL;
}

JVal *json_at(const JVal *v, int i)
{
    if (!v || (v->t != JARR && v->t != JOBJ) || i < 0 || i >= v->n) return NULL;
    return v->items[i];
}

const char *json_strv(const JVal *v)
{
    if (!v) return NULL;
    if (v->t == JSTR) return v->s;
    return NULL;
}

const char *json_str(const JVal *v, const char *key)
{
    return json_strv(json_get(v, key));
}

void json_write_str(Buf *b, const char *s)
{
    buf_addc(b, '"');
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
            switch (*p) {
            case '"':  buf_adds(b, "\\\""); break;
            case '\\': buf_adds(b, "\\\\"); break;
            case '\n': buf_adds(b, "\\n"); break;
            case '\r': buf_adds(b, "\\r"); break;
            case '\t': buf_adds(b, "\\t"); break;
            case '\b': buf_adds(b, "\\b"); break;
            case '\f': buf_adds(b, "\\f"); break;
            default:
                if (*p < 0x20) {
                    char tmp[8];
                    _snprintf(tmp, sizeof tmp, "\\u%04x", *p);
                    buf_adds(b, tmp);
                } else {
                    buf_addc(b, (char)*p);
                }
            }
        }
    }
    buf_addc(b, '"');
}
