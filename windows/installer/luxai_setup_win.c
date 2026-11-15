/* LuxAI Setup for Windows — native kurucu (WSL GEREKTIRMEZ).
 *
 * LuxAI.exe'yi (RCDATA 201) %ProgramFiles%\LuxAI icine cikarir (yönetici
 * hakki ister; yükseltme reddedilirse %LOCALAPPDATA%\Programs\LuxAI'ye kurar),
 * Baslat menusu + masaustu kisayolu olusturur, "Programlar ve Ozellikler"
 * kaydini (HKLM/HKCU) ekler ve kaldirma betigini yazar. mingw-w64 ile derlenir.
 */
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objidl.h>
#include <stdio.h>

#define IDR_EXE 201
#define IDR_ICO 202

static int g_tr;

/* yönetici (admin) mi? */
static int is_admin(void)
{
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return 0;
    TOKEN_ELEVATION el;
    DWORD n = 0;
    int adm = 0;
    if (GetTokenInformation(tok, TokenElevation, &el, sizeof el, &n)) adm = el.TokenIsElevated ? 1 : 0;
    CloseHandle(tok);
    return adm;
}

/* Program Files'e yazilabiliyor mu? (admin degilse genelde hayir) */
static int dir_writable(const wchar_t *dir)
{
    wchar_t probe[MAX_PATH * 2];
    _snwprintf(probe, MAX_PATH * 2, L"%s\\_luxai.tmp", dir);
    HANDLE h = CreateFileW(probe, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(h);
    DeleteFileW(probe);
    return 1;
}

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
    fwprintf(f, L"setlocal\r\n");
    fwprintf(f, L"echo LuxAI kaldiriliyor / uninstalling LuxAI...\r\n");
    /* Program Files altindaysa yonetici hakki gerekir: kendini yukselt */
    fwprintf(f, L"net session >nul 2>&1 || (powershell -NoProfile -Command \"Start-Process -Verb RunAs -FilePath '%~f0'\" & exit /b)\r\n");
    fwprintf(f, L"reg delete \"HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LuxAI\" /f >nul 2>&1\r\n");
    fwprintf(f, L"reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LuxAI\" /f >nul 2>&1\r\n");
    fwprintf(f, L"del /q \"%%APPDATA%%\\Microsoft\\Windows\\Start Menu\\Programs\\LuxAI.lnk\" >nul 2>&1\r\n");
    fwprintf(f, L"del /q \"%%USERPROFILE%%\\Desktop\\LuxAI.lnk\" >nul 2>&1\r\n");
    fwprintf(f, L"timeout /t 1 >nul\r\n");
    fwprintf(f, L"rmdir /s /q \"%s\"\r\n", dir);
    fwprintf(f, L"echo Bitti / done.\r\n");
    fclose(f);
}

static int g_machine;

static void register_uninstall(const wchar_t *dir, const wchar_t *icon)
{
    HKEY k;
    if (RegCreateKeyExW(g_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LuxAI",
                        0, NULL, 0, KEY_WRITE, NULL, &k, NULL) != ERROR_SUCCESS)
        return;
    wchar_t un[MAX_PATH * 2], buf[MAX_PATH * 2];
    _snwprintf(un, MAX_PATH * 2, L"\"%s\\uninstall.cmd\"", dir);
    {
        const wchar_t *dn = L"LuxAI 3.1 (Windows native)";
        RegSetValueExW(k, L"DisplayName", 0, REG_SZ, (const BYTE *)dn, (DWORD)(2 * (wcslen(dn) + 1)));
    }
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

    int silent = 0, launch = 0, elevated = 0, forcemachine = 0, forceuser = 0;
    {
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i < argc; i++) {
            if (!lstrcmpiW(argv[i], L"/S") || !lstrcmpiW(argv[i], L"-s") || !lstrcmpiW(argv[i], L"--silent"))
                silent = 1;
            else if (!lstrcmpiW(argv[i], L"/LAUNCH") || !lstrcmpiW(argv[i], L"--launch"))
                launch = 1;
            else if (!lstrcmpiW(argv[i], L"/ELEVATED") || !lstrcmpiW(argv[i], L"--elevated"))
                elevated = 1;
            else if (!lstrcmpiW(argv[i], L"/MACHINE") || !lstrcmpiW(argv[i], L"--machine"))
                forcemachine = 1;
            else if (!lstrcmpiW(argv[i], L"/USER") || !lstrcmpiW(argv[i], L"--user"))
                forceuser = 1;
        }
        if (argv) LocalFree(argv);
    }
    (void)elevated; (void)forcemachine;

    /* 1) %ProgramFiles%\LuxAI  (tüm kullanicilar, yönetici hakki) */
    wchar_t pfiles[MAX_PATH] = L"";
    if (!SHGetSpecialFolderPathW(NULL, pfiles, CSIDL_PROGRAM_FILES, TRUE) || !pfiles[0]) {
        wchar_t env[MAX_PATH] = L"";
        if (GetEnvironmentVariableW(L"ProgramFiles", env, MAX_PATH) && env[0]) wcscpy(pfiles, env);
    }
    wchar_t machine_dir[MAX_PATH * 2] = L"", user_dir[MAX_PATH * 2] = L"";
    if (pfiles[0]) _snwprintf(machine_dir, MAX_PATH * 2, L"%s\\LuxAI", pfiles);
    {
        wchar_t local[MAX_PATH] = L"";
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH) && local[0])
            _snwprintf(user_dir, MAX_PATH * 2, L"%s\\Programs\\LuxAI", local);
        else
            _snwprintf(user_dir, MAX_PATH * 2, L"%s\\LuxAI", appdata);
    }

    if (!forceuser && machine_dir[0]) {
        CreateDirectoryW(machine_dir, NULL);
        /* yönetici degilsek ve Program Files yazilamiyorsa: yükseltmeyi dene */
        if (!is_admin() && !dir_writable(machine_dir)) {
            if (!silent) {
                wchar_t self[MAX_PATH * 2], args[MAX_PATH * 4];
                GetModuleFileNameW(NULL, self, MAX_PATH * 2);
                _snwprintf(args, MAX_PATH * 4, L"\"%s\" /ELEVATED /MACHINE%s%s",
                           self, launch ? L" /LAUNCH" : L"", silent ? L" /S" : L"");
                HINSTANCE r = ShellExecuteW(NULL, L"runas", self, args, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)r > 32) return 0;      /* yükseltilmiş kopya kurulumu yapar */
                /* kullanici UAC'yi reddetti -> bu kullanici icin kur */
            }
            wcscpy(dir, user_dir);
        } else {
            wcscpy(dir, machine_dir);
        }
    } else {
        wcscpy(dir, user_dir);
    }
    int machine = (!wcsncmp(dir, machine_dir, wcslen(machine_dir)) && machine_dir[0]);
    CreateDirectoryW(dir, NULL);
    _snwprintf(exe, MAX_PATH * 2, L"%s\\LuxAI.exe", dir);
    _snwprintf(ico, MAX_PATH * 2, L"%s\\luxai.ico", dir);

    HWND owner = GetConsoleWindow();
    if (owner) ShowWindow(owner, SW_HIDE);

    if (!silent && MessageBoxW(NULL,
            T(L"LuxAI (Windows, native) kurulacak.\n\n"
              "WSL gerekmez: LuxAI dogrudan Windows uygulamasi olarak calisir.\n"
              "Kurulum yeri: %ProgramFiles%\\LuxAI  (bu kullanici icin: %%LOCALAPPDATA%%\\Programs\\LuxAI)\n\nDevam edilsin mi?",
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
    g_machine = machine;
    register_uninstall(dir, ico);
    CoUninitialize();

    if (!ok && !silent) {
        MessageBoxW(NULL, T(L"Kisayol olusturulamadi, ancak LuxAI kuruldu.",
                            L"Could not create the shortcut, but LuxAI is installed."),
                    L"LuxAI", MB_ICONWARNING);
    }

    int start_app = launch;
    if (!silent) {
        if (MessageBoxW(NULL,
                T(L"LuxAI kuruldu!\n\nBaslat menusundeki 'LuxAI' kisayolundan acabilirsiniz.\n"
                  "Ilk acilista LuxAI hesabinizla giris yapin (lux-ai.cz).\n\n"
                  "Simdi baslatilsin mi?",
                  L"LuxAI is installed!\n\nLaunch it from the 'LuxAI' shortcut in the Start Menu.\n"
                  "Sign in with your LuxAI account on first start (lux-ai.cz).\n\n"
                  "Start it now?"),
                T(L"LuxAI Kurulumu", L"LuxAI Setup"),
                MB_YESNO | MB_ICONINFORMATION) == IDYES)
            start_app = 1;
    }
    if (start_app) {
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
