/* LuxAI — Windows icin native istemci (giris noktasi).
 * Bagimsizcalisma: GTK yok, WSL yok, harici DLL yok; yalnizca Win32 + WinHTTP.
 */
#include "luxai.h"

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
    ui_run();

    if (mtx) CloseHandle(mtx);
    return 0;
}
