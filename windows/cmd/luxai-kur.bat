@echo off
setlocal EnableDelayedExpansion
if /i "%~1"=="/elevated" set "ELEVATED=1"
chcp 65001 >nul 2>&1
title LuxAI 2.0.2 - Windows kurulumu (cmd)
color 0B

set "VER=2.0.2"
set "BASE=https://github.com/vesvese55x/LuxAI-the-AI-assistant-for-LuxOS/releases/download/v%VER%"
set "ZIP=LuxAI-%VER%-Windows-native-x64.zip"
set "DEST=%ProgramFiles%\LuxAI"
set "DEST_USER=%LOCALAPPDATA%\Programs\LuxAI"
set "HK=HKLM"
set "TMP=%TEMP%\luxai-kur"
set "EXE=%DEST%\LuxAI.exe"

echo.
echo  ============================================================
echo    LuxAI %VER%  -  native Windows kurulumu (WSL gerekmez)
echo    Windows 7 SP1 / 8 / 8.1 / 10 / 11  -  x64
echo  ============================================================
echo.

rem --- yonetici hakki (Program Files icin) -----------------------------------
net session >nul 2>&1
if errorlevel 1 (
  echo  i  Program Files'a kurmak icin yonetici hakki gerekiyor - yukseltme istenecek...
  powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "Start-Process -Verb RunAs -FilePath '%~f0' -ArgumentList '/elevated'"
  if errorlevel 1 goto :usermode
  exit /b 0
)
goto :arch

:usermode
echo  [!] Yukseltme reddedildi - bu kullanici icin kurulacak:
echo      %DEST_USER%
set "DEST=%DEST_USER%"
set "HK=HKCU"

:arch
rem --- mimari denetimi -------------------------------------------------------
if /i not "%PROCESSOR_ARCHITECTURE%"=="AMD64" if /i not "%PROCESSOR_ARCHITEW6432%"=="AMD64" (
  echo  [!] Bu yapi 64 bit (x64) Windows icin. 32 bit sistem desteklenmiyor.
  goto :fail
)

rem --- daha once kurulu mu ---------------------------------------------------
tasklist /fi "imagename eq LuxAI.exe" 2>nul | find /i "LuxAI.exe" >nul && (
  echo  [!] LuxAI calisiyor, kurulumdan once kapatiliyor...
  taskkill /im LuxAI.exe /f >nul 2>&1
  timeout /t 2 >nul
)

if exist "%EXE%" (
  echo  i  LuxAI zaten kurulu: %DEST%
  echo     Guncelleniyor...
)

rem --- indir ----------------------------------------------------------------
if not exist "%TMP%" mkdir "%TMP%" >nul 2>&1
set "URL=%BASE%/%ZIP%"
echo  [1/4] Indiriliyor: %ZIP%
where curl.exe >nul 2>&1
if errorlevel 1 goto :ps_download
curl.exe -L --fail --retry 3 --retry-delay 2 -o "%TMP%\%ZIP%" "%URL%"
if errorlevel 1 goto :ps_download
goto :unzip

:ps_download
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ProgressPreference='SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%URL%' -OutFile '%TMP%\%ZIP%'"
if errorlevel 1 (
  echo  [X] Indirme basarisiz. Internet baglantisini denetleyin.
  echo      Adres: %URL%
  goto :fail
)

:unzip
for %%F in ("%TMP%\%ZIP%") do if %%~zF LSS 100000 (
  echo  [X] Indirilen dosya cok kucuk - indirme bozuk.
  goto :fail
)

rem --- ac -------------------------------------------------------------------
echo  [2/4] Aciliyor: %DEST%
if not exist "%DEST%" mkdir "%DEST%" >nul 2>&1
if not exist "%DEST%" (
  echo  [!] %DEST% olusturulamadi - kullanici klasorune geciliyor
  set "DEST=%DEST_USER%"
  set "HK=HKCU"
  set "EXE=%DEST%\LuxAI.exe"
  mkdir "%DEST%" >nul 2>&1
)
taskkill /im LuxAI.exe /f >nul 2>&1
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath '%TMP%\%ZIP%' -DestinationPath '%DEST%' -Force"
if errorlevel 1 (
  echo  [X] Arsiv acilamadi.
  goto :fail
)

rem zip kokunde klasor varsa icerigi yukari tasi
if not exist "%EXE%" (
  for /d %%D in ("%DEST%\*") do (
    if exist "%%D\LuxAI.exe" (
      echo     ... klasor duzeni duzeltiliyor
      xcopy "%%D\*" "%DEST%\" /e /h /y >nul 2>&1
      rmdir /s /q "%%D" >nul 2>&1
    )
  )
)
if not exist "%EXE%" (
  echo  [X] LuxAI.exe bulunamadi - arsiv beklenenden farkli.
  goto :fail
)

rem --- kisayollar -----------------------------------------------------------
echo  [3/4] Kisayollar olusturuluyor (Baslat menusu + masaustu)
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$s=(New-Object -ComObject WScript.Shell);" ^
  "$lnk=$s.CreateShortcut([Environment]::GetFolderPath('Programs')+'\LuxAI.lnk');" ^
  "$lnk.TargetPath='%EXE%'; $lnk.WorkingDirectory='%DEST%'; $lnk.IconLocation='%EXE%,0';" ^
  "$lnk.Description='LuxAI - the AI assistant for LuxOS (native Windows)'; $lnk.Save();" ^
  "$d=[Environment]::GetFolderPath('Desktop');" ^
  "if($d){$l2=$s.CreateShortcut($d+'\LuxAI.lnk'); $l2.TargetPath='%EXE%'; $l2.WorkingDirectory='%DEST%'; $l2.IconLocation='%EXE%,0'; $l2.Save()}"

rem --- kaldirma bilgisi (Ayarlar > Uygulamalar) ------------------------------
reg add "%HK%\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /v "DisplayName" /t REG_SZ /d "LuxAI %VER%" /f >nul 2>&1
reg add "%HK%\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /v "DisplayVersion" /t REG_SZ /d "%VER%" /f >nul 2>&1
reg add "%HK%\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /v "Publisher" /t REG_SZ /d "BELGESELSEMO" /f >nul 2>&1
reg add "%HK%\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /v "DisplayIcon" /t REG_SZ /d "%EXE%" /f >nul 2>&1
reg add "%HK%\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /v "InstallLocation" /t REG_SZ /d "%DEST%" /f >nul 2>&1
reg add "%HK%\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /v "UninstallString" /t REG_SZ /d "\"%DEST%\luxai-kaldir.bat\"" /f >nul 2>&1
reg add "%HK%\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /v "NoModify" /t REG_DWORD /d 1 /f >nul 2>&1
reg add "%HK%\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /v "NoRepair" /t REG_DWORD /d 1 /f >nul 2>&1

rem kaldirma betigini de yanina koy
if exist "%~dp0luxai-kaldir.bat" copy /y "%~dp0luxai-kaldir.bat" "%DEST%\luxai-kaldir.bat" >nul 2>&1

echo  [4/4] Bitti.
echo.
echo  ============================================================
echo    LuxAI %VER% kuruldu.
echo      Konum      : %DEST%
echo      Baslat     : Baslat menusu - LuxAI  (veya masaustu kisayolu)
echo      Kaldirma   : luxai-kaldir.bat  ya da Ayarlar ^> Uygulamalar
echo.
echo    Ilk acilista LuxAI hesabinizla giris yapin
echo    (hesap yoksa lux-ai.cz uzerinden ucretsiz olusturabilirsiniz).
echo  ============================================================
echo.
choice /c EY /n /m "  LuxAI simdi acilsin mi? [E=evet, Y=hayir] "
if errorlevel 2 goto :done
start "" "%EXE%"
goto :done

:done
rmdir /s /q "%TMP%" >nul 2>&1
endlocal
exit /b 0

:fail
echo.
echo  Kurulum tamamlanamadi. Yardim:
echo    https://github.com/vesvese55x/LuxAI-the-AI-assistant-for-LuxOS
echo.
pause
endlocal
exit /b 1
