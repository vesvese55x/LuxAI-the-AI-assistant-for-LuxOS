#include "luxai.h"
#include <shlobj.h>

/* ---------- dynamic buffer ---------- */
void buf_init(Buf *b) { b->p = NULL; b->n = b->cap = 0; }
void buf_free(Buf *b) { free(b->p); buf_init(b); }
void buf_clear(Buf *b) { b->n = 0; if (b->p) b->p[0] = 0; }

void buf_reserve(Buf *b, size_t extra)
{
    if (b->n + extra + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap * 2 : 256;
    while (cap < b->n + extra + 1) cap *= 2;
    char *p = (char *)realloc(b->p, cap);
    if (!p) { fputs("oom\n", stderr); exit(1); }
    b->p = p; b->cap = cap;
    b->p[b->n] = 0;
}

void buf_add(Buf *b, const void *d, size_t n)
{
    if (!n) return;
    buf_reserve(b, n);
    memcpy(b->p + b->n, d, n);
    b->n += n;
    b->p[b->n] = 0;
}

void buf_adds(Buf *b, const char *s) { if (s) buf_add(b, s, strlen(s)); }
void buf_addc(Buf *b, char c) { buf_add(b, &c, 1); }

char *buf_detach(Buf *b)
{
    char *p = b->p ? b->p : (char *)calloc(1, 1);
    buf_init(b);
    return p;
}

/* ---------- utf-8 <-> utf-16 ---------- */
wchar_t *u16(const char *utf8)
{
    if (!utf8) utf8 = "";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (n <= 0) n = 1;
    wchar_t *w = (wchar_t *)calloc((size_t)n + 1, sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
    return w;
}

char *u8(const wchar_t *w)
{
    if (!w) w = L"";
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) n = 1;
    char *s = (char *)calloc((size_t)n + 1, 1);
    if (!s) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

/* ---------- paths ---------- */
void path_appdata(wchar_t *out, size_t cch, const wchar_t *leaf)
{
    wchar_t base[MAX_PATH] = L"";
    if (!SHGetSpecialFolderPathW(NULL, base, CSIDL_APPDATA, TRUE))
        wcscpy(base, L"C:\\");
    if (leaf && *leaf)
        _snwprintf(out, cch, L"%s\\LuxAI\\%s", base, leaf);
    else
        _snwprintf(out, cch, L"%s\\LuxAI", base);
    out[cch - 1] = 0;
}

int mkdir_p(const wchar_t *dir)
{
    wchar_t tmp[MAX_PATH * 2];
    wcsncpy(tmp, dir, MAX_PATH * 2 - 1);
    tmp[MAX_PATH * 2 - 1] = 0;
    for (wchar_t *p = tmp + 3; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            wchar_t save = *p; *p = 0;
            CreateDirectoryW(tmp, NULL);
            *p = save;
        }
    }
    CreateDirectoryW(tmp, NULL);
    return GetFileAttributesW(tmp) != INVALID_FILE_ATTRIBUTES;
}

void path_ensure(wchar_t *path)
{
    wchar_t dir[MAX_PATH * 2];
    wcsncpy(dir, path, MAX_PATH * 2 - 1);
    dir[MAX_PATH * 2 - 1] = 0;
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) { *slash = 0; mkdir_p(dir); }
}

char *file_read_utf8(const wchar_t *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER sz; sz.QuadPart = 0;
    GetFileSizeEx(h, &sz);
    if (sz.QuadPart < 0 || sz.QuadPart > (LONGLONG)64 * 1024 * 1024) { CloseHandle(h); return NULL; }
    size_t n = (size_t)sz.QuadPart;
    char *d = (char *)calloc(n + 1, 1);
    if (!d) { CloseHandle(h); return NULL; }
    DWORD got = 0;
    if (n && (!ReadFile(h, d, (DWORD)n, &got, NULL))) { free(d); CloseHandle(h); return NULL; }
    d[got] = 0;
    CloseHandle(h);
    return d;
}

int file_write_utf8(const wchar_t *path, const char *data, size_t len)
{
    path_ensure((wchar_t *)path);
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD wr = 0;
    BOOL ok = WriteFile(h, data, (DWORD)len, &wr, NULL) && wr == (DWORD)len;
    CloseHandle(h);
    return ok;
}
