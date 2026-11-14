/* LuxAI Setup for Windows — installs LuxAI into WSL(WSLg) and creates a Start Menu shortcut.
 *BELGESELSEMO v3.1 — compiled with mingw-w64.
 *
 * Flow:
 *  1. localized welcome (system UI language: tr/en/de/fr/ru/zh/ja)
 *  2. checks WSL (`wsl.exe --status`); offers `wsl --install -d Debian --no-launch`
 *  3. extracts embedded luxai .deb to %LOCALAPPDATA%\LuxAI
 *  4. runs `wsl -d Debian -- bash -lc 'apt-get ... install /mnt/c/.../luxai.deb'`
 *  5. creates Start Menu shortcut -> `wsl.exe -d Debian -- luxai`
 */


#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

/* embedded resources */
#define IDB_DEB     101
#define IDB_ICO     102

static LANGID g_lang;

static int langindex(void)
{
    LANGID id = g_lang & 0xFF;
    if (id == 0x1F) return 0;   /* Turkish */
    if (id == 0x07) return 1;   /* German */
    if (id == 0x0C) return 2;   /* French */
    if (id == 0x19) return 3;   /* Russian */
    if (id == 0x04) return 4;   /* Chinese */
    if (id == 0x11) return 5;   /* Japanese */
    return 6;                   /* English */
}

static const wchar_t *MSG_TITLE[] = {
    L"LuxAI Kurulumu",
    L"LuxAI-Installation",
    L"Installation de LuxAI",
    L"Установка LuxAI",
    L" LuxAI 安装",
    L"LuxAI インストール",
    L"LuxAI Setup",
};
static const wchar_t *MSG_WELCOME[] = {
    L"LuxAI (Windows) kurulacak. Bu kurucu, LuxAI'yi Windows'un WSL ortamina kurar ve Başlat menüsü kısayolu oluşturur (Windows 11 / WSLg gerekir).\n\nDevam edilsin mi?",
    L"LuxAI (Windows) wird installiert. Dieses Setup installiert LuxAI in der Windows-WSL-Umgebung und erstellt eine Startmenü-Verknüpfung (Windows 11 / WSLg erforderlich).\n\nFortfahren?",
    L"LuxAI (Windows) va être installé. Ce programme installe LuxAI dans l'environnement WSL de Windows et crée un raccourci dans le menu Démarrer (Windows 11 / WSLg requis).\n\nContinuer ?",
    L"LuxAI (Windows) будет установлен. Установщик развернёт LuxAI в среде WSL Windows и создаст ярлык в меню «Пуск» (нужны Windows 11 / WSLg).\n\nПродолжить?",
    L"将安装 LuxAI（Windows 版）。安装程序会把 LuxAI 安装到 Windows 的 WSL 环境中，并在开始菜单创建快捷方式（需要 Windows 11 / WSLg）。\n\n是否继续？",
    L"LuxAI（Windows 版）をインストールします。Windows の WSL 環境に LuxAI をインストールし、スタートメニューにショートカットを作成します（Windows 11 / WSLg が必要）。\n\n続行しますか？",
    L"LuxAI (Windows) will be installed. This setup installs LuxAI into the Windows WSL environment and creates a Start Menu shortcut (requires Windows 11 / WSLg).\n\nContinue?",
};
static const wchar_t *MSG_DEBIAN_MISSING[] = {
    L"WSL kurulu değil. Kurucu artık Debian dağıtımını kuracak — bu birkaç dakika sürebilir ve bilgisayar yeniden başlatılmasını isteyebilir.\n\nDevam edilsin mi?",
    L"WSL ist nicht installiert. Das Setup installiert jetzt Debian — das kann einige Minuten dauern und einen Neustart erfordern.\n\nFortfahren?",
    L"WSL n'est pas installé. Le programme va maintenant installer Debian — cela peut prendre plusieurs minutes et demander un redémarrage.\n\nContinuer ?",
    L"WSL не установлен. Сейчас будет установлен Debian — это может занять несколько минут и потребовать перезагрузку.\n\nПродолжить?",
    L"未安装 WSL。安装程序现在将安装 Debian——可能需要几分钟，并可能要求重启电脑。\n\n是否继续？",
    L"WSL がインストールされていません。これから Debian をインストールします。数分かかることがあり、再起動が必要になる場合があります。\n\n続行しますか？",
    L"WSL is not installed. The setup will now install Debian — this can take several minutes and may require a restart.\n\nContinue?",
};
static const wchar_t *MSG_INSTALLING[] = {
    L"LuxAI Debian'a kuruluyor (birkaç dakika sürebilir)…",
    L"LuxAI wird in Debian installiert (kann Minuten dauern)…",
    L"Installation de LuxAI dans Debian (peut prendre plusieurs minutes)…",
    L"Установка LuxAI в Debian (может занять несколько минут)…",
    L"正在将 LuxAI 安装到 Debian（可能需要几分钟）…",
    L"Debian に LuxAI をインストールしています（数分かかることがあります）…",
    L"Installing LuxAI into Debian (may take a few minutes)…",
};
static const wchar_t *MSG_DONE[] = {
    L"LuxAI kuruldu! Başlat menüsündeki 'LuxAI' kısayolundan çalıştırın.\n\nİlk açılışta LuxAI hesabınızla oturum açın. Hoş geldiniz — BELGESELSEMO · belgeselsemo.com.tr",
    L"LuxAI wurde installiert! Starten Sie es über die Verknüpfung 'LuxAI' im Startmenü.\n\nMelden Sie sich beim ersten Start mit Ihrem LuxAI-Konto an. Willkommen — BELGESELSEMO · belgeselsemo.com.tr",
    L"LuxAI est installé ! Lancez-le via le raccourci 'LuxAI' du menu Démarrer.\n\nConnectez-vous avec votre compte LuxAI au premier démarrage. Bienvenue — BELGESELSEMO · belgeselsemo.com.tr",
    L"LuxAI установлен! Запускайте через ярлык 'LuxAI' в меню «Пуск».\n\nПри первом запуске войдите в свою учётную запись LuxAI. Добро пожаловать — BELGESELSEMO · belgeselsemo.com.tr",
    L"LuxAI 安装完成！请从开始菜单的“LuxAI”快捷方式启动。\n\n首次启动时请登录您的 LuxAI 账户。欢迎——BELGESELSEMO · belgeselsemo.com.tr",
    L"LuxAI をインストールしました！スタートメニューの「LuxAI」ショートカットから起動してください。\n\n初回起動時に LuxAI アカウントでサインインしてください。ようこそ — BELGESELSEMO · belgeselsemo.com.tr",
    L"LuxAI installed! Launch it from the 'LuxAI' shortcut in the Start Menu.\n\nSign in with your LuxAI account on first start. Welcome — BELGESELSEMO · belgeselsemo.com.tr",
};
static const wchar_t *MSG_ERROR[] = {
    L"Kurulum tamamlanamadı. Ayrıntılar için konsol çıktısına bakın (wsl.exe): %ls",
    L"Installation fehlgeschlagen. Details in der Konsolenausgabe (wsl.exe): %ls",
    L"Échec de l'installation. Voir la sortie console (wsl.exe) : %ls",
    L"Не удалось завершить установку. Подробности в выводе консоли (wsl.exe): %ls",
    L"安装失败。详情请查看控制台输出（wsl.exe）：%ls",
    L"インストールに失敗しました。詳細はコンソール出力（wsl.exe）を確認してください：%ls",
    L"Installation failed. See console output (wsl.exe) for details: %ls",
};

/* run a command, capture exit code; output goes to stderr of parent (console) */
static int run_cmd(const wchar_t *cmdline, BOOL show_window)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = -1;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = show_window ? SW_SHOWNORMAL : SW_HIDE;
    wchar_t *cmd = _wcsdup(cmdline);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                        show_window ? 0 : CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        fwprintf(stderr, L"[luxai-setup] CreateProcess failed: %ls\n", cmdline);
        free(cmd);
        return -1;
    }
    WaitForSingleObject(pi.hProcess, 60 * 60 * 1000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(cmd);
    return (int)code;
}

static BOOL extract_resource(int resid, const wchar_t *path)
{
    HRSRC hr = FindResourceW(NULL, MAKEINTRESOURCEW(resid), RT_RCDATA);
    if (!hr) return FALSE;
    HGLOBAL hg = LoadResource(NULL, hr);
    if (!hg) return FALSE;
    DWORD size = SizeofResource(NULL, hr);
    const void *data = LockResource(hg);
    if (!data || !size) return FALSE;
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    DWORD written = 0;
    BOOL ok = WriteFile(f, data, size, &written, NULL) && written == size;
    CloseHandle(f);
    return ok;
}

static BOOL make_shortcut(const wchar_t *exe_path, const wchar_t *ico_path,
                          const wchar_t *lnk_dir, const wchar_t *name)
{
    wchar_t lnk[MAX_PATH];
    swprintf(lnk, MAX_PATH, L"%ls\\%ls.lnk", lnk_dir, name);
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl);
    if (FAILED(hr)) return FALSE;
    sl->lpVtbl->SetPath(sl, exe_path);
    sl->lpVtbl->SetArguments(sl, L"-d Debian -- luxai");
    sl->lpVtbl->SetIconLocation(sl, ico_path, 0);
    sl->lpVtbl->SetDescription(sl, name);
    hr = sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
    if (SUCCEEDED(hr)) {
        hr = pf->lpVtbl->Save(pf, lnk, TRUE);
        pf->lpVtbl->Release(pf);
    }
    sl->lpVtbl->Release(sl);
    return SUCCEEDED(hr);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show)
{
    g_lang = GetUserDefaultUILanguage();
    int li = langindex();
    wchar_t appdata[MAX_PATH], dir[MAX_PATH], deb_path[MAX_PATH], ico_path[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata)))
        wcscpy(appdata, L"C:\\");
    swprintf(dir, MAX_PATH, L"%ls\\LuxAI", appdata);
    CreateDirectoryW(dir, NULL);
    swprintf(deb_path, MAX_PATH, L"%ls\\luxai.deb", dir);
    swprintf(ico_path, MAX_PATH, L"%ls\\luxai.ico", dir);

    if (MessageBoxW(NULL, MSG_WELCOME[li], MSG_TITLE[li], MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
        return 0;

    /* WSL present? */
    int rc = run_cmd(L"wsl.exe --status", FALSE);
    if (rc != 0) {
        if (MessageBoxW(NULL, MSG_DEBIAN_MISSING[li], MSG_TITLE[li], MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            return 0;
        run_cmd(L"wsl.exe --install -d Debian --no-launch", TRUE);
        MessageBoxW(NULL, MSG_INSTALLING[li], MSG_TITLE[li], MB_OK | MB_ICONINFORMATION);
        rc = run_cmd(L"wsl.exe -d Debian -- true", FALSE);
        if (rc != 0) {
            wchar_t buf[64]; swprintf(buf, 64, L"%d", rc);
            wchar_t msg[512];
            swprintf(msg, 512, MSG_ERROR[li], buf);
            MessageBoxW(NULL, msg, MSG_TITLE[li], MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    if (!extract_resource(IDB_DEB, deb_path) || !extract_resource(IDB_ICO, ico_path)) {
        MessageBoxW(NULL, L"Resource extraction failed.", MSG_TITLE[li], MB_OK | MB_ICONERROR);
        return 1;
    }

    /* convert C:\ path to /mnt/c path for WSL */
    wchar_t linux_path[MAX_PATH * 2];
    swprintf(linux_path, MAX_PATH * 2,
             L"/mnt/%lc/%ls", towlower(deb_path[0]), deb_path + 3);

    wchar_t cmdline[1024];
    swprintf(cmdline, 1024,
             L"wsl.exe -d Debian -- bash -lc \"sudo -n true 2>/dev/null || true; "
             L"sudo apt-get update -qq && sudo apt-get install -y %ls\"",
             linux_path);
    MessageBoxW(NULL, MSG_INSTALLING[li], MSG_TITLE[li], MB_OK | MB_ICONINFORMATION);
    rc = run_cmd(cmdline, TRUE);

    if (rc == 0) {
        wchar_t startmenu[MAX_PATH];
        SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startmenu);
        make_shortcut(L"C:\\Windows\\System32\\wsl.exe", ico_path, startmenu, L"LuxAI");
        MessageBoxW(NULL, MSG_DONE[li], MSG_TITLE[li], MB_OK | MB_ICONINFORMATION);
        run_cmd(L"wsl.exe -d Debian -- luxai", TRUE);
    } else {
        wchar_t buf[64]; swprintf(buf, 64, L"%d", rc);
        wchar_t msg[512];
        swprintf(msg, 512, MSG_ERROR[li], buf);
        MessageBoxW(NULL, msg, MSG_TITLE[li], MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}
