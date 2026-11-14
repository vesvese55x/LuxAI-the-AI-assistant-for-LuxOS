/* LuxAI Setup for Windows — native kurucu (WSL GEREKTIRMEZ).
 *
 * LuxAI.exe'yi (RCDATA 201) %LOCALAPPDATA%\LuxAI icine cikarir, Baslat menusu ve
 * masaustu kisayolu olusturur, "Programlar ve Ozellikler" kaydi ekler ve
 * kaldirma betigini yazar. mingw-w64 ile derlenir.
 */
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objidl.h>
#include <stdio.h>

#define IDR_EXE 201
#define IDR_ICO 202

static int g_tr;

static const wchar_t *T(const wchar_t *tr, const wchar_t *en) { return g_tr ? tr : en; }

static void center(HWND h)
{
    RECT r;
    GetWindowRect(h, &r);
    int w = r.right - r.left, ht = r.bottom - r.top;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(h, NULL, (sw - w) / 2, (sh - ht) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static int extract_res(int id, const wchar_t *path)
{
    HRSRC hr = FindResourceW(NULL, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!hr) return 0;
    HGLOBAL hg = LoadResource(NULL, hr);
    DWORD size = SizeofResource(NULL, hr);
    const void *data = hg ? LockResource(hg) : NULL;
    if (!data || !size) return 0;
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD wr = 0;
    BOOL ok = WriteFile(f, data, size, &wr, NULL) && wr == size;
    CloseHandle(f);
    return ok;
}

static int make_shortcut(const wchar_t *target, const wchar_t *workdir,
                         const wchar_t *ico, const wchar_t *lnk_path)
{
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IShellLinkW, (void **)&sl)))
        return 0;
    sl->lpVtbl->SetPath(sl, target);
    sl->lpVtbl->SetWorkingDirectory(sl, workdir);
    sl->lpVtbl->SetIconLocation(sl, ico, 0);
    sl->lpVtbl->SetDescription(sl, L"LuxAI - AI assistant");
    int ok = 0;
    if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
        ok = SUCCEEDED(pf->lpVtbl->Save(pf, lnk_path, TRUE));
        pf->lpVtbl->Release(pf);
    }
    sl->lpVtbl->Release(sl);
    return ok;
}

static void write_uninstaller(const wchar_t *dir, const wchar_t *appdata)
{
    wchar_t path[MAX_PATH * 2];
    _snwprintf(path, MAX_PATH * 2, L"%s\\uninstall.cmd", dir);
    FILE *f = _wfopen(path, L"wb");
    if (!f) return;
    fwprintf(f, L"@echo off\r\n");
    fwprintf(f, L"echo LuxAI kaldiriliyor / uninstalling LuxAI...\r\n");
    fwprintf(f, L"reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LuxAI\" /f >nul 2>&1\r\n");
    fwprintf(f, L"del /q \"%%APPDATA%%\\Microsoft\\Windows\\Start Menu\\Programs\\LuxAI.lnk\" >nul 2>&1\r\n");
    fwprintf(f, L"del /q \"%%USERPROFILE%%\\Desktop\\LuxAI.lnk\" >nul 2>&1\r\n");
    fwprintf(f, L"timeout /t 1 >nul\r\n");
    fwprintf(f, L"rmdir /s /q \"%s\"\r\n", dir);
    fwprintf(f, L"echo Bitti / done.\r\n");
    fclose(f);
}

static void register_uninstall(const wchar_t *dir, const wchar_t *icon)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LuxAI",
                        0, NULL, 0, KEY_WRITE, NULL, &k, NULL) != ERROR_SUCCESS)
        return;
    wchar_t un[MAX_PATH * 2], buf[MAX_PATH * 2];
    _snwprintf(un, MAX_PATH * 2, L"\"%s\\uninstall.cmd\"", dir);
    RegSetValueExW(k, L"DisplayName", 0, REG_SZ, (const BYTE *)L"LuxAI 3.1 (Windows native)", 2 * 30);
    RegSetValueExW(k, L"DisplayVersion", 0, REG_SZ, (const BYTE *)L"3.1.0-win.1", 2 * 12);
    RegSetValueExW(k, L"DisplayIcon", 0, REG_SZ, (const BYTE *)icon, (DWORD)(2 * (wcslen(icon) + 1)));
    RegSetValueExW(k, L"Publisher", 0, REG_SZ, (const BYTE *)L"BELGESELSEMO topluluk yapisi", 2 * 30);
    RegSetValueExW(k, L"InstallLocation", 0, REG_SZ, (const BYTE *)dir, (DWORD)(2 * (wcslen(dir) + 1)));
    RegSetValueExW(k, L"UninstallString", 0, REG_SZ, (const BYTE *)un, (DWORD)(2 * (wcslen(un) + 1)));
    DWORD one = 1;
    RegSetValueExW(k, L"NoModify", 0, REG_DWORD, (const BYTE *)&one, sizeof one);
    _snwprintf(buf, MAX_PATH * 2, L"%s\\LuxAI.exe", dir);
    RegSetValueExW(k, L"DisplayName_hint", 0, REG_SZ, (const BYTE *)buf, (DWORD)(2 * (wcslen(buf) + 1)));
    RegCloseKey(k);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show)
{
    (void)hInst; (void)hPrev; (void)cmd; (void)show;
    g_tr = (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_TURKISH);

    wchar_t appdata[MAX_PATH], dir[MAX_PATH * 2], exe[MAX_PATH * 2], ico[MAX_PATH * 2];
    if (!SHGetSpecialFolderPathW(NULL, appdata, CSIDL_APPDATA, TRUE)) return 1;
    {
        wchar_t local[MAX_PATH] = L"";
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH) && local[0])
            _snwprintf(dir, MAX_PATH * 2, L"%s\\LuxAI", local);
        else
            _snwprintf(dir, MAX_PATH * 2, L"%s\\LuxAI", appdata);
    }
    CreateDirectoryW(dir, NULL);
    _snwprintf(exe, MAX_PATH * 2, L"%s\\LuxAI.exe", dir);
    _snwprintf(ico, MAX_PATH * 2, L"%s\\luxai.ico", dir);

    HWND owner = GetConsoleWindow();
    if (owner) ShowWindow(owner, SW_HIDE);

    if (MessageBoxW(NULL,
            T(L"LuxAI (Windows, native) kurulacak.\n\n"
              "WSL gerekmez: LuxAI dogrudan Windows uygulamasi olarak calisir.\n"
              "Kurulum yeri: %LOCALAPPDATA%\\LuxAI\n\nDevam edilsin mi?",
              L"LuxAI (Windows, native) will be installed.\n\n"
              "No WSL required: LuxAI runs as a native Windows application.\n"
              "Install folder: %LOCALAPPDATA%\\LuxAI\n\nContinue?"),
            T(L"LuxAI Kurulumu", L"LuxAI Setup"),
            MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
        return 0;

    if (!extract_res(IDR_EXE, exe)) {
        MessageBoxW(NULL, T(L"LuxAI.exe cikarilamadi.", L"Cannot extract LuxAI.exe."),
                    L"LuxAI", MB_ICONERROR);
        return 1;
    }
    extract_res(IDR_ICO, ico);

    /* Surum guncellemesi icin eski calisan surec kapansin */
    HWND old = FindWindowW(L"LuxAIWindow", NULL);
    if (old) SendMessageW(old, WM_CLOSE, 0, 0);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    wchar_t startmenu[MAX_PATH] = L"", desktop[MAX_PATH] = L"", lnk[MAX_PATH * 2];
    int ok = 0;
    if (SHGetSpecialFolderPathW(NULL, startmenu, CSIDL_PROGRAMS, TRUE)) {
        _snwprintf(lnk, MAX_PATH * 2, L"%s\\LuxAI.lnk", startmenu);
        ok |= make_shortcut(exe, dir, ico, lnk);
    }
    if (SHGetSpecialFolderPathW(NULL, desktop, CSIDL_DESKTOPDIRECTORY, TRUE)) {
        _snwprintf(lnk, MAX_PATH * 2, L"%s\\LuxAI.lnk", desktop);
        make_shortcut(exe, dir, ico, lnk);
    }
    write_uninstaller(dir, appdata);
    register_uninstall(dir, ico);
    CoUninitialize();

    if (!ok) {
        MessageBoxW(NULL, T(L"Kisayol olusturulamadi, ancak LuxAI kuruldu.",
                            L"Could not create the shortcut, but LuxAI is installed."),
                    L"LuxAI", MB_ICONWARNING);
    }

    if (MessageBoxW(NULL,
            T(L"LuxAI kuruldu!\n\nBaslat menusundeki 'LuxAI' kisayolundan acabilirsiniz.\n"
              "Ilk acilista LuxAI hesabinizla giris yapin (lux-ai.cz).\n\n"
              "Simdi baslatilsin mi?",
              L"LuxAI is installed!\n\nLaunch it from the 'LuxAI' shortcut in the Start Menu.\n"
              "Sign in with your LuxAI account on first start (lux-ai.cz).\n\n"
              "Start it now?"),
            T(L"LuxAI Kurulumu", L"LuxAI Setup"),
            MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        STARTUPINFOW si; PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof si); ZeroMemory(&pi, sizeof pi);
        si.cb = sizeof si;
        if (CreateProcessW(exe, NULL, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    return 0;
}
