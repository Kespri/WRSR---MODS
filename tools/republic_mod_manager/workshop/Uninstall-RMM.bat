@echo off
setlocal
chcp 65001 >nul
title Republic Mod Manager - Deinstallation / Uninstall
echo.
echo Republic Mod Manager - Deinstallation / Uninstall
echo ===================================================
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-RMM.ps1" -Uninstall %*
set RC=%ERRORLEVEL%
echo.
if not "%RC%"=="0" (
    echo ------------------------------------------------------------------
    echo Die Deinstallation wurde NICHT abgeschlossen.
    echo Was fehlgeschlagen ist und warum, steht in der roten Meldung oben.
    echo.
    echo The uninstall did NOT finish.
    echo What failed and why is in the red message above.
    echo ------------------------------------------------------------------
    echo.
    pause
    exit /b %RC%
)
echo Fertig. Dieses Fenster schliesst sich in 5 Sekunden.
echo Done. This window closes in 5 seconds.
ping -n 6 127.0.0.1 >nul
exit /b 0
