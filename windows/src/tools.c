#include "luxai.h"
#include <stdarg.h>
#include <ctype.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

extern const char *TOOLS_JSON;

const char *tools_json(void) { return TOOLS_JSON; }

/* ---------------- yardimcilar ---------------- */
static wchar_t *arg_path(const char *utf8)
{
    if (!utf8 || !*utf8) return NULL;
    wchar_t *raw = u16(utf8);
    if (!raw) return NULL;
    wchar_t *out = (wchar_t *)calloc(MAX_PATH * 2, sizeof(wchar_t));
    DWORD n = ExpandEnvironmentStringsW(raw, out, MAX_PATH * 2);
    free(raw);
    if (n == 0) return NULL;
    /* tirnaklari temizle */
    wchar_t *s = out;
    while (*s == L'"' || *s == L' ') s++;
    size_t len = wcslen(s);
    while (len && (s[len - 1] == L'"' || s[len - 1] == L' ')) s[--len] = 0;
    if (s != out) memmove(out, s, (len + 1) * sizeof(wchar_t));
    return out;
}

static char *ret(const char *s) { return _strdup(s ? s : ""); }

static char *retf(const char *fmt, ...)
{
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(tmp, sizeof tmp - 1, fmt, ap);
    va_end(ap);
    tmp[sizeof tmp - 1] = 0;
    return _strdup(tmp);
}

static void append_limited(Buf *b, const char *s, size_t limit)
{
    if (!s) return;
    if (b->n >= limit) return;
    size_t n = strlen(s);
    if (b->n + n > limit) n = limit - b->n;
    buf_add(b, s, n);
}

/* ---------------- read_file ---------------- */
static char *tool_read_file(JVal *a)
{
    wchar_t *p = arg_path(json_str(a, "path"));
    if (!p) return ret("error: path is required");
    char *data = file_read_utf8(p);
    free(p);
    if (!data) return ret("error: cannot open the file");
    int start = (int)(json_get(a, "start_line") ? json_get(a, "start_line")->num : 1);
    int count = (int)(json_get(a, "line_count") ? json_get(a, "line_count")->num : 200);
    if (start < 1) start = 1;
    if (count <= 0) count = 200;
    if (count > 2000) count = 2000;
    Buf out; buf_init(&out);
    int line = 1;
    const char *q = data, *nl;
    while (q && *q) {
        nl = strchr(q, '\n');
        size_t len = nl ? (size_t)(nl - q) : strlen(q);
        if (line >= start && line < start + count) {
            char num[16];
            _snprintf(num, sizeof num, "%5d| ", line);
            buf_adds(&out, num);
            buf_add(&out, q, len);
            buf_addc(&out, '\n');
        }
        if (!nl) break;
        q = nl + 1;
        line++;
        if (line >= start + count) break;
    }
    free(data);
    if (!out.n) { buf_free(&out); return ret("(empty range)"); }
    return buf_detach(&out);
}

/* ---------------- write_file ---------------- */
static char *tool_write_file(JVal *a)
{
    const char *content = json_str(a, "content");
    wchar_t *p = arg_path(json_str(a, "path"));
    if (!p) return ret("error: path is required");
    if (!content) content = "";
    int ok = file_write_utf8(p, content, strlen(content));
    free(p);
    return ret(ok ? "ok: file written" : "error: cannot write the file");
}

/* ---------------- edit_file ---------------- */
static char *tool_edit_file(JVal *a)
{
    wchar_t *p = arg_path(json_str(a, "path"));
    const char *old_s = json_str(a, "old_text");
    const char *new_s = json_str(a, "new_text");
    int all = json_get(a, "replace_all") ? json_get(a, "replace_all")->b : 0;
    if (!p || !old_s) { free(p); return ret("error: path and old_text are required"); }
    if (!new_s) new_s = "";
    char *data = file_read_utf8(p);
    if (!data) { free(p); return ret("error: cannot open the file"); }
    size_t ol = strlen(old_s);
    if (!ol) { free(p); free(data); return ret("error: old_text is empty"); }
    /* kac kez gectigini say */
    int hits = 0;
    for (char *q = data; (q = strstr(q, old_s)) != NULL; q += ol) hits++;
    if (hits == 0) { free(p); free(data); return ret("error: old_text was not found"); }
    if (hits > 1 && !all) {
        free(p); free(data);
        return retf("error: old_text occurs %d times; give a longer unique text or set replace_all", hits);
    }
    Buf out; buf_init(&out);
    char *q = data;
    int done = 0;
    while (*q) {
        char *hit = strstr(q, old_s);
        if (!hit || (done && !all)) { buf_adds(&out, q); break; }
        buf_add(&out, q, (size_t)(hit - q));
        buf_adds(&out, new_s);
        q = hit + ol;
        done++;
    }
    free(p); free(data);
    wchar_t *p2 = arg_path(json_str(a, "path"));
    int ok = p2 ? file_write_utf8(p2, out.p ? out.p : "", out.n) : 0;
    free(p2);
    buf_free(&out);
    return ret(ok ? "ok: file edited" : "error: cannot write the file");
}

/* ---------------- delete / move / mkdir ---------------- */
static char *tool_delete_file(JVal *a)
{
    wchar_t *p = arg_path(json_str(a, "path"));
    if (!p) return ret("error: path is required");
    DWORD attr = GetFileAttributesW(p);
    if (attr == INVALID_FILE_ATTRIBUTES) { free(p); return ret("error: path does not exist"); }
    BOOL ok = (attr & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(p) : DeleteFileW(p);
    free(p);
    if (!ok) return ret("error: cannot delete (folder not empty or access denied)");
    return ret("ok: deleted");
}

static char *tool_move_file(JVal *a)
{
    wchar_t *src = arg_path(json_str(a, "path"));
    wchar_t *dst = arg_path(json_str(a, "destination"));
    if (!src || !dst) { free(src); free(dst); return ret("error: path and destination are required"); }
    if (GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES) {
        free(src); free(dst);
        return ret("error: destination already exists");
    }
    path_ensure(dst);
    BOOL ok = MoveFileW(src, dst);
    free(src); free(dst);
    return ret(ok ? "ok: moved" : "error: cannot move");
}

static char *tool_make_directory(JVal *a)
{
    wchar_t *p = arg_path(json_str(a, "path"));
    if (!p) return ret("error: path is required");
    int ok = mkdir_p(p);
    free(p);
    return ret(ok ? "ok: folder created" : "error: cannot create the folder");
}

/* ---------------- list_directory ---------------- */
static char *tool_list_directory(JVal *a)
{
    wchar_t *p = arg_path(json_str(a, "path"));
    if (!p) return ret("error: path is required");
    wchar_t pat[MAX_PATH * 2];
    _snwprintf(pat, MAX_PATH * 2, L"%s\\*", p);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { free(p); return ret("error: cannot list the folder"); }
    Buf out; buf_init(&out);
    int n = 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        char *name = u8(fd.cFileName);
        int isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        unsigned long long size = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        char line[1024];
        if (isdir) _snprintf(line, sizeof line, "[DIR ] %s/\n", name ? name : "?");
        else _snprintf(line, sizeof line, "[%8llu] %s\n", size, name ? name : "?");
        line[sizeof line - 1] = 0;
        buf_adds(&out, line);
        free(name);
        if (++n > 500) { buf_adds(&out, "… (truncated)\n"); break; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    free(p);
    if (!out.n) { buf_free(&out); return ret("(empty folder)"); }
    return buf_detach(&out);
}

/* ---------------- find_files ---------------- */
static int find_walk(const wchar_t *dir, const wchar_t *pattern, Buf *out, int *count, int depth)
{
    if (depth > 8 || *count > 200) return 0;
    wchar_t pat[MAX_PATH * 2];
    _snwprintf(pat, MAX_PATH * 2, L"%s\\*", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t full[MAX_PATH * 2];
        _snwprintf(full, MAX_PATH * 2, L"%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            find_walk(full, pattern, out, count, depth + 1);
        } else if (PathMatchSpecW(fd.cFileName, pattern)) {
            char *f = u8(full);
            buf_adds(out, f ? f : "?");
            buf_addc(out, '\n');
            free(f);
            (*count)++;
        }
    } while (*count <= 200 && FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}

static char *tool_find_files(JVal *a)
{
    wchar_t *p = arg_path(json_str(a, "path"));
    const char *pat_s = json_str(a, "pattern");
    if (!p) return ret("error: path is required");
    if (!pat_s || !*pat_s) pat_s = "*";
    wchar_t *pat = u16(pat_s);
    Buf out; buf_init(&out);
    int count = 0;
    find_walk(p, pat, &out, &count, 0);
    free(p); free(pat);
    if (!count) { buf_free(&out); return ret("(no match)"); }
    return buf_detach(&out);
}

/* ---------------- run_command ---------------- */
static char *tool_run_command(JVal *a)
{
    const char *cmd_s = json_str(a, "command");
    if (!cmd_s || !*cmd_s) return ret("error: command is required");
    int timeout = (int)(json_get(a, "timeout") ? json_get(a, "timeout")->num : 120);
    if (timeout <= 0 || timeout > 1800) timeout = 120;

    wchar_t out_path[MAX_PATH * 2];
    wchar_t tmpdir[MAX_PATH] = L"";
    GetTempPathW(MAX_PATH, tmpdir);
    _snwprintf(out_path, MAX_PATH * 2, L"%sluxai_%lu.txt", tmpdir, (unsigned long)GetCurrentProcessId());

    wchar_t *cmd = u16(cmd_s);
    wchar_t *cwd = arg_path(json_str(a, "cwd"));
    wchar_t *line = (wchar_t *)calloc(MAX_PATH * 8, sizeof(wchar_t));
    _snwprintf(line, MAX_PATH * 8, L"cmd.exe /u /c %s > \"%s\" 2>&1", cmd, out_path);

    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si); ZeroMemory(&pi, sizeof pi);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    BOOL ok = CreateProcessW(NULL, line, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                             NULL, cwd && *cwd ? cwd : (g_cfg.workdir[0] ? g_cfg.workdir : NULL),
                             &si, &pi);
    if (!ok) {
        free(cmd); free(cwd); free(line);
        return ret("error: cannot start cmd.exe");
    }
    DWORD wait = WaitForSingleObject(pi.hProcess, (DWORD)timeout * 1000);
    int timedout = 0;
    DWORD code = 0;
    if (wait == WAIT_TIMEOUT) { TerminateProcess(pi.hProcess, 1); timedout = 1; }
    WaitForSingleObject(pi.hProcess, 5000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    free(cmd); free(cwd); free(line);

    char *data = file_read_utf8(out_path);
    DeleteFileW(out_path);
    Buf out; buf_init(&out);
    char head[256];
    if (timedout) _snprintf(head, sizeof head, "[timeout after %d s, process terminated]\n", timeout);
    else _snprintf(head, sizeof head, "[exit code %lu]\n", (unsigned long)code);
    head[sizeof head - 1] = 0;
    buf_adds(&out, head);
    append_limited(&out, data, 24000);
    if (data && strlen(data) > 24000) buf_adds(&out, "\n… (output truncated)");
    free(data);
    if (!out.n) { buf_free(&out); return ret("(no output)"); }
    return buf_detach(&out);
}

/* ---------------- open_url / clipboard / system_info / notify / screenshot ---------------- */
static char *tool_open_url(JVal *a)
{
    const char *u = json_str(a, "url");
    if (!u || !*u) return ret("error: url is required");
    wchar_t *w = u16(u);
    HINSTANCE r = ShellExecuteW(NULL, L"open", w, NULL, NULL, SW_SHOWNORMAL);
    free(w);
    return ret(((INT_PTR)r > 32) ? "ok: opened" : "error: cannot open");
}

static char *tool_clipboard_read(JVal *a)
{
    (void)a;
    if (!OpenClipboard(g_hMain)) return ret("error: clipboard is busy");
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    char *res = NULL;
    if (h) {
        wchar_t *txt = (wchar_t *)GlobalLock(h);
        if (txt) {
            res = u8(txt);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    if (!res) return ret("(clipboard has no text)");
    if (strlen(res) > 20000) res[20000] = 0;
    return res;
}

static char *tool_system_info(JVal *a)
{
    (void)a;
    Buf b; buf_init(&b);
    OSVERSIONINFOW vi; ZeroMemory(&vi, sizeof vi); vi.dwOSVersionInfoSize = sizeof vi;
    char *wd = u8(g_cfg.workdir);
    wchar_t comp[256] = L"", user[256] = L"";
    DWORD cc = 256, uc = 256;
    GetComputerNameW(comp, &cc);
    GetUserNameW(user, &uc);
    char *c8 = u8(comp), *u8s = u8(user);
    char line[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GetVersionExW(&vi);
#pragma GCC diagnostic pop
    _snprintf(line, sizeof line, "Windows %lu.%lu.%lu (build)\n", (unsigned long)vi.dwMajorVersion,
              (unsigned long)vi.dwMinorVersion, (unsigned long)vi.dwBuildNumber);
    buf_adds(&b, line);
    _snprintf(line, sizeof line, "Computer: %s\nUser: %s\nWorking folder: %s\nShell: cmd.exe (Windows)\n",
              c8 ? c8 : "?", u8s ? u8s : "?", wd ? wd : "?");
    buf_adds(&b, line);
    buf_adds(&b, "Tools: run_command, read_file, write_file, edit_file, delete_file, move_file, "
                 "make_directory, list_directory, find_files, open_url, clipboard_read, screenshot, "
                 "system_info, notify\n");
    _snprintf(line, sizeof line, "LuxAI native Windows client %ls — model: %s, mode: %s\n",
              APP_VERSION, g_cfg.model ? "lite" : "flash", g_cfg.mode ? "agent" : "chat");
    buf_adds(&b, line);
    free(wd); free(c8); free(u8s);
    return buf_detach(&b);
}

static char *tool_notify(JVal *a)
{
    const char *title = json_str(a, "title");
    const char *body = json_str(a, "body");
    if (!title) title = "LuxAI";
    wchar_t *wt = u16(title), *wb = u16(body ? body : "");
    NOTIFYICONDATAW nid;
    ZeroMemory(&nid, sizeof nid);
    nid.cbSize = sizeof nid;
    nid.hWnd = g_hMain;
    nid.uID = 0x4C58;
    nid.uFlags = NIF_INFO | NIF_ICON;
    nid.hIcon = LoadIconW(NULL, IDI_INFORMATION);
    wcsncpy(nid.szInfoTitle, wt ? wt : L"LuxAI", 63);
    wcsncpy(nid.szInfo, wb ? wb : L"", 255);
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_ADD, &nid);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    free(wt); free(wb);
    return ret("ok: notification shown");
}

static char *tool_screenshot(JVal *a)
{
    (void)a;
    HDC screen = GetDC(NULL);
    if (!screen) return ret("error: cannot get the screen");
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, screen, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi;
    ZeroMemory(&bi, sizeof bi);
    bi.biSize = sizeof bi;
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    DWORD row = ((DWORD)w * 3 + 3) & ~3u;
    DWORD size = row * (DWORD)h;
    BYTE *pixels = (BYTE *)malloc(size);
    char *res = NULL;
    if (pixels) {
        GetDIBits(mem, bmp, 0, (UINT)h, pixels, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
        wchar_t dir[MAX_PATH];
        path_appdata(dir, MAX_PATH, L"shots");
        mkdir_p(dir);
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t path[MAX_PATH * 2];
        _snwprintf(path, MAX_PATH * 2, L"%s\\shot_%04d%02d%02d_%02d%02d%02d.bmp", dir,
                   st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        BITMAPFILEHEADER fh;
        ZeroMemory(&fh, sizeof fh);
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof fh + sizeof bi;
        fh.bfSize = fh.bfOffBits + size;
        HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD wr = 0;
            WriteFile(f, &fh, sizeof fh, &wr, NULL);
            WriteFile(f, &bi, sizeof bi, &wr, NULL);
            WriteFile(f, pixels, size, &wr, NULL);
            CloseHandle(f);
            char *p8 = u8(path);
            res = retf("ok: screenshot saved to %s (%dx%d)", p8 ? p8 : "?", w, h);
            free(p8);
        }
        free(pixels);
    }
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    if (!res) return ret("error: cannot save the screenshot");
    return res;
}

/* ---------------- guvenlik ---------------- */
static const char *DANGER[] = {
    "format ", "diskpart", "bcdedit", "vssadmin delete", "cipher /w",
    "del /f /s /q c:\\", "del /s /q c:\\", "rd /s /q c:\\", "rmdir /s /q c:\\",
    "del /f /s /q %systemdrive%", "rd /s /q %systemdrive%",
    "reg delete hklm", "reg delete hkey_local_machine",
    "shutdown /s", "shutdown -s", "net user ", "net localgroup administrators",
    "takeown /f c:\\windows", "icacls c:\\windows", "attrib -r -h -s c:\\",
    "powershell -enc", "powershell -encodedcommand", "mshta http",
    "wmic shadowcopy delete", "wbadmin delete", "sfc /scannow",
    NULL
};

int tool_blocked(const char *name, const char *args_json)
{
    if (!name) return 0;
    if (!strcmp(name, "run_command")) {
        JVal *a = json_parse(args_json);
        const char *cmd = json_str(a, "command");
        int blocked = 0;
        if (cmd) {
            char low[4096];
            size_t i = 0;
            for (; cmd[i] && i < sizeof low - 1; i++) low[i] = (char)tolower((unsigned char)cmd[i]);
            low[i] = 0;
            for (int k = 0; DANGER[k]; k++) {
                if (strstr(low, DANGER[k])) { blocked = 1; break; }
            }
        }
        json_free(a);
        return blocked;
    }
    return 0;
}

int tool_is_write(const char *name)
{
    static const char *W[] = { "run_command", "write_file", "edit_file", "delete_file",
                               "move_file", "make_directory", "open_url", "screenshot",
                               "notify", NULL };
    for (int i = 0; W[i]; i++) if (!strcmp(name, W[i])) return 1;
    return 0;
}

/* ---------------- dagitim ---------------- */
char *tool_execute(const char *name, const char *args_json)
{
    JVal *a = json_parse(args_json && *args_json ? args_json : "{}");
    char *r;
    if (!a) return ret("error: invalid tool arguments (JSON parse failed)");
    if (!strcmp(name, "run_command"))        r = tool_run_command(a);
    else if (!strcmp(name, "read_file"))     r = tool_read_file(a);
    else if (!strcmp(name, "write_file"))    r = tool_write_file(a);
    else if (!strcmp(name, "edit_file"))     r = tool_edit_file(a);
    else if (!strcmp(name, "delete_file"))   r = tool_delete_file(a);
    else if (!strcmp(name, "move_file"))     r = tool_move_file(a);
    else if (!strcmp(name, "make_directory"))r = tool_make_directory(a);
    else if (!strcmp(name, "list_directory"))r = tool_list_directory(a);
    else if (!strcmp(name, "find_files"))    r = tool_find_files(a);
    else if (!strcmp(name, "open_url"))      r = tool_open_url(a);
    else if (!strcmp(name, "clipboard_read"))r = tool_clipboard_read(a);
    else if (!strcmp(name, "screenshot"))    r = tool_screenshot(a);
    else if (!strcmp(name, "system_info"))   r = tool_system_info(a);
    else if (!strcmp(name, "notify"))        r = tool_notify(a);
    else r = retf("error: unknown tool '%s'", name ? name : "?");
    json_free(a);
    return r;
}
