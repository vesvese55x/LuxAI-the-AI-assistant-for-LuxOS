@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul 2>&1
title LuxAI - kaldirma
color 0C

set "DEST=%LOCALAPPDATA%\Programs\LuxAI"
set "CFG=%APPDATA%\LuxAI"

echo.
echo  ============================================================
echo    LuxAI kaldiriliyor
echo  ============================================================
echo.
if not exist "%DEST%\LuxAI.exe" if not exist "%DEST%" (
  echo  i  Kurulum bulunamadi: %DEST%
  goto :end
)

choice /c EY /n /m "  Sohbet gecmisi ve ayarlar da silinsin mi? [E=evet, Y=hayir] "
set "WIPE=%errorlevel%"

echo  LuxAI kapatiliyor...
taskkill /im LuxAI.exe /f >nul 2>&1
timeout /t 2 >nul

echo  Program dosyalari siliniyor...
rmdir /s /q "%DEST%" >nul 2>&1

echo  Kisayollar siliniyor...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p=[Environment]::GetFolderPath('Programs')+'\LuxAI.lnk'; if(Test-Path $p){Remove-Item $p -Force};" ^
  "$d=[Environment]::GetFolderPath('Desktop')+'\LuxAI.lnk'; if(Test-Path $d){Remove-Item $d -Force}" >nul 2>&1

reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\LuxAI" /f >nul 2>&1

if "%WIPE%"=="1" (
  echo  Ayarlar ve sohbetler siliniyor: %CFG%
  rmdir /s /q "%CFG%" >nul 2>&1
) else (
  echo  i  Ayarlar ve sohbetler korundu: %CFG%
)

echo.
echo  LuxAI kaldirildi.
echo.
:end
pause
endlocal
exit /b 0
