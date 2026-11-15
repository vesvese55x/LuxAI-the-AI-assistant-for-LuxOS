/* LuxAI — Windows icin native istemci (giris noktasi).
 * Bagimsizcalisma: GTK yok, WSL yok, harici DLL yok; yalnizca Win32 + WinHTTP.
 */
#include "luxai.h"
#include <shellapi.h>
#include <stdlib.h>
#include <wchar.h>

wchar_t g_startup_ask[2048];
int g_startup_account;   /* --account : acilista LuxAI Hesabi penceresi */

/* --ask "metin"  : acilista mesaji otomatik gonderir (CI/otomasyon testi)
 * --theme 0|1|2  : tema gecersiz kilma        --mode 0|1 : sohbet/ajan
 * --lang 0..17   : arayuz dili (18 dil)       --version   : surum yazdir
 * --account      : acilista LuxAI Hesabi penceresini acar (test/ekran goruntusu)
 */
static void parse_cli(void)
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"--ask") && i + 1 < argc) {
            wcsncpy(g_startup_ask, argv[++i], 2047);
            g_startup_ask[2047] = 0;
        } else if (!wcscmp(argv[i], L"--theme") && i + 1 < argc) {
            g_cfg.theme = _wtoi(argv[++i]);
        } else if (!wcscmp(argv[i], L"--mode") && i + 1 < argc) {
            g_cfg.mode = _wtoi(argv[++i]);
        } else if (!wcscmp(argv[i], L"--lang") && i + 1 < argc) {
            g_cfg.lang = _wtoi(argv[++i]);
        } else if (!wcscmp(argv[i], L"--account")) {
            g_startup_account = 1;
        } else if (!wcscmp(argv[i], L"--version")) {
            MessageBoxW(NULL, L"LuxAI " APP_VERSION L"\nWindows native (Win32, WSL'siz)", L"LuxAI",
                        MB_OK | MB_ICONINFORMATION);
        }
    }
    LocalFree(argv);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show)
{
    (void)hInst; (void)hPrev; (void)cmd; (void)show;

    /* tek ornek */
    HANDLE mtx = CreateMutexW(NULL, TRUE, L"LuxAI-Windows-SingleInstance");
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"LuxAIWindow", NULL);
        if (other) { ShowWindow(other, SW_RESTORE); SetForegroundWindow(other); }
        return 0;
    }

    wchar_t dir[MAX_PATH];
    path_appdata(dir, MAX_PATH, NULL);
    mkdir_p(dir);

    cfg_load();
    account_load();
    parse_cli();
    ui_run();

    if (mtx) CloseHandle(mtx);
    return 0;
}
