@echo off
setlocal
cd /d "%~dp0"
if not exist bin mkdir bin
set "TESMIO_CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%TESMIO_CSC%" exit /b 2
"%TESMIO_CSC%" /nologo /target:exe /out:bin\BuildIcon.exe /reference:System.Drawing.dll tests\BuildIcon.cs
if errorlevel 1 exit /b 1
rem Republic Mod Manager (RMM), formerly Tesmio Settings / tesmio_autoload. The
rem embedded resource names (Tesmio.icon, Tesmio.languages.*) are internal and stay.
bin\BuildIcon.exe assets\rmm-icon.png assets\rmm.ico
if errorlevel 1 exit /b 1
set "TESMIO_UI_SOURCES=src\Core.cs src\Content.cs src\Saves.cs src\Startup.cs src\StartCheck.cs src\Reset.cs src\WipBuildings.cs src\WipEdits.cs src\WipEditor.cs src\BuildingStates.cs src\DonorPlan.cs src\DonorLinesEditor.cs src\TargetPlan.cs src\TargetLinesEditor.cs src\OptionsWindow.cs src\ResourceRegistry.cs src\ResourceSettings.cs src\ResourceConsistency.cs src\Catalog.cs src\Presentation.cs src\Visuals.cs src\SettingsForm.cs src\SettingsEditor.cs src\ResourceSettingsEditor.cs src\LogViewer.cs src\Profiles.cs src\ProfilesWindow.cs src\Buildings.cs src\BuildingPicker.cs src\GameTexts.cs src\Research.cs src\References.cs src\TextPack.cs src\GameTextPicker.cs src\ResearchPicker.cs src\ResearchLines.cs src\Cue.cs src\MessageWindow.cs src\App.cs"
set "TESMIO_UI_RESOURCES=/resource:assets\rmm.ico,Tesmio.icon /resource:languages\en.ini,Tesmio.languages.en /resource:languages\de.ini,Tesmio.languages.de"
"%TESMIO_CSC%" /nologo /target:winexe /platform:x64 /optimize+ /warnaserror+ /win32manifest:app.manifest /win32icon:assets\rmm.ico %TESMIO_UI_RESOURCES% /out:bin\rmm.exe /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Core.dll %TESMIO_UI_SOURCES%
if errorlevel 1 exit /b 1
"%TESMIO_CSC%" /nologo /target:exe /platform:x64 /warnaserror+ /out:bin\CoreTests.exe /reference:System.Core.dll src\Core.cs src\Content.cs src\Saves.cs src\Startup.cs src\Reset.cs src\WipBuildings.cs src\WipEdits.cs src\BuildingStates.cs src\DonorPlan.cs src\TargetPlan.cs src\ResourceRegistry.cs src\ResourceSettings.cs src\ResourceConsistency.cs src\Catalog.cs src\Presentation.cs src\Profiles.cs src\Buildings.cs src\GameTexts.cs src\Research.cs src\References.cs src\TextPack.cs tests\CoreTests.cs
if errorlevel 1 exit /b 1
"%TESMIO_CSC%" /nologo /target:exe /platform:x64 /warnaserror+ /main:UiTests %TESMIO_UI_RESOURCES% /out:bin\UiTests.exe /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Core.dll %TESMIO_UI_SOURCES% tests\UiTests.cs
if errorlevel 1 exit /b 1
if not exist bin\settings_schemas mkdir bin\settings_schemas
xcopy /E /I /Y settings_schemas bin\settings_schemas >nul
exit /b %errorlevel%
