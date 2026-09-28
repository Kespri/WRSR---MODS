# Republic Mod Manager - installer of the Steam Workshop package.
#
# Copies rmm.exe, its settings schemas and the Workshop Bridge from
# "Manual Installation\tesmioloader\build" into <game>\tesmioloader\build,
# switches the bridge on in tesmioloader.ini and can create a desktop shortcut.
# It also replaces tesmioloader.dll with the corrected build that ships in this
# package (b0.3.6-rmm.1, source in the GitHub repository); the previous file goes
# into the backup folder. The launcher (tesmiolauncher.exe) and tesmioloader.ini
# must already be there - the loader itself comes from MaxLegend's Workshop item.
# -Uninstall removes what this script installed, leaves the loader DLL and your
# personal settings (user_config) alone.
#
# Start it through Install-RMM.bat (double-click). Options for the console:
#   -GamePath "C:\...\SovietRepublic"   game folder when it is not found automatically
#   -Shortcut / -NoShortcut             desktop shortcut without asking
#   -Uninstall                          remove the installed files
param(
    [string]$GamePath = '',
    [switch]$Uninstall,
    [switch]$Shortcut,
    [switch]$NoShortcut
)
$ErrorActionPreference = 'Stop'
try { [Console]::OutputEncoding = [Text.Encoding]::UTF8 } catch { }

$german = $false
try { $german = (Get-UICulture).TwoLetterISOLanguageName -eq 'de' } catch { }
function T([string]$de, [string]$en) { if ($german) { return $de } else { return $en } }
function Step([string]$text) { Write-Host ('  ' + $text) }
function Ok([string]$text) { Write-Host ('  [OK] ' + $text) -ForegroundColor Green }
function Note([string]$text) { Write-Host ('  [--] ' + $text) -ForegroundColor DarkGray }
function Fail([string]$what, [string]$why) {
    Write-Host ''
    Write-Host ((T 'FEHLER: ' 'ERROR: ') + $what) -ForegroundColor Red
    Write-Host ((T 'Grund:  ' 'Reason: ') + $why) -ForegroundColor Yellow
    exit 1
}
function Hash([string]$path) { return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function Interactive() {
    try { return ([Environment]::UserInteractive -and -not [Console]::IsInputRedirected) } catch { return $false }
}

# ---------------------------------------------------------------- package
$source = Join-Path $PSScriptRoot 'Manual Installation\tesmioloader\build'
if (-not (Test-Path -LiteralPath (Join-Path $source 'rmm.exe') -PathType Leaf)) {
    Fail (T 'Das Paket ist unvollständig.' 'The package is incomplete.') `
         (T ('rmm.exe fehlt unter ' + $source + '. Bitte das Workshop-Objekt in Steam einmal abbestellen und neu abonnieren.') `
            ('rmm.exe is missing under ' + $source + '. Please unsubscribe and resubscribe the Workshop item in Steam.'))
}
$files = Get-ChildItem -LiteralPath $source -Recurse -File | ForEach-Object { $_.FullName.Substring($source.Length + 1) }
# Files with the player's own values: written once, never overwritten by an update.
$keep = @('rmm.ini', 'plugins\workshop_bridge.ini', 'plugins\buildings_plus.ini')

# ---------------------------------------------------------------- game folder
function SteamLibraries() {
    $result = @()
    try {
        $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath
        if ($steam) {
            $steam = $steam.Replace('/', '\')
            $result += $steam
            $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
            if (Test-Path -LiteralPath $vdf) {
                foreach ($line in Get-Content -LiteralPath $vdf) {
                    if ($line -match '^\s*"path"\s+"(.+)"\s*$') { $result += $Matches[1].Replace('\\', '\') }
                }
            }
        }
    } catch { }
    return $result
}
$game = ''
if ($GamePath -ne '') {
    $game = $GamePath.TrimEnd('\')
} else {
    # The Workshop item lives in <library>\steamapps\workshop\content\784150\<item>;
    # the game in <library>\steamapps\common\SovietRepublic.
    $steamapps = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
    $candidate = Join-Path $steamapps 'common\SovietRepublic'
    if (Test-Path -LiteralPath (Join-Path $candidate 'SOVIET64.exe') -PathType Leaf) { $game = $candidate }
    if ($game -eq '') {
        foreach ($library in SteamLibraries) {
            $candidate = Join-Path $library 'steamapps\common\SovietRepublic'
            if (Test-Path -LiteralPath (Join-Path $candidate 'SOVIET64.exe') -PathType Leaf) { $game = $candidate; break }
        }
    }
    if ($game -eq '' -and (Interactive)) {
        Write-Host (T 'Der Spielordner wurde nicht gefunden.' 'The game folder was not found.') -ForegroundColor Yellow
        $game = (Read-Host (T 'Bitte den Ordner mit SOVIET64.exe eingeben (z. B. C:\Program Files (x86)\Steam\steamapps\common\SovietRepublic)' `
                              'Please enter the folder with SOVIET64.exe (e.g. C:\Program Files (x86)\Steam\steamapps\common\SovietRepublic)')).Trim().Trim('"').TrimEnd('\')
    }
}
if ($game -eq '' -or -not (Test-Path -LiteralPath (Join-Path $game 'SOVIET64.exe') -PathType Leaf)) {
    Fail (T 'Der Spielordner wurde nicht gefunden.' 'The game folder was not found.') `
         (T ('Unter "' + $game + '" liegt keine SOVIET64.exe. Starte Install-RMM.bat aus dem abonnierten Workshop-Ordner oder gib den Ordner an: Install-RMM.bat -GamePath "C:\...\SovietRepublic".') `
            ('There is no SOVIET64.exe under "' + $game + '". Run Install-RMM.bat from the subscribed Workshop folder or pass the folder: Install-RMM.bat -GamePath "C:\...\SovietRepublic".'))
}
$build = Join-Path $game 'tesmioloader\build'
Ok ((T 'Spielordner: ' 'Game folder: ') + $game)

# ---------------------------------------------------------------- loader
$loaderExe = Join-Path $build 'tesmiolauncher.exe'
$loaderIni = Join-Path $build 'tesmioloader.ini'
if (-not (Test-Path -LiteralPath $loaderExe -PathType Leaf) -or -not (Test-Path -LiteralPath $loaderIni -PathType Leaf)) {
    Fail (T 'Der TesmioLoader ist nicht installiert.' 'The TesmioLoader is not installed.') `
         (T ('Es fehlt ' + $build + '\tesmiolauncher.exe oder tesmioloader.ini. Republic Mod Manager braucht den TesmioLoader von MaxLegend: im Steam-Workshop abonnieren und nach seiner Anleitung installieren (Quelle: https://github.com/MaxLegend/TesmioLoader). Danach Install-RMM.bat erneut starten.') `
            ('Missing ' + $build + '\tesmiolauncher.exe or tesmioloader.ini. Republic Mod Manager needs the TesmioLoader by MaxLegend: subscribe to it in the Steam Workshop and install it as its guide says (source: https://github.com/MaxLegend/TesmioLoader). Then run Install-RMM.bat again.'))
}
Ok (T 'TesmioLoader gefunden.' 'TesmioLoader found.')

# ---------------------------------------------------------------- running programs
$running = @(Get-Process -Name rmm, tesmiolauncher, SOVIET64 -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    Fail (T 'Das Spiel oder der Republic Mod Manager läuft noch.' 'The game or Republic Mod Manager is still running.') `
         (T ('Bitte zuerst beenden: ' + (($running | ForEach-Object { $_.ProcessName } | Sort-Object -Unique) -join ', ') + '. Dateien, die gerade benutzt werden, lassen sich nicht ersetzen.') `
            ('Please close first: ' + (($running | ForEach-Object { $_.ProcessName } | Sort-Object -Unique) -join ', ') + '. Files in use cannot be replaced.'))
}

# ---------------------------------------------------------------- tesmioloader.ini
# Sets one key of the [plugins] section of tesmioloader.ini; returns $true when the file changed.
function SetPluginSwitch([string]$path, [string]$name, [string]$value) {
    $text = [IO.File]::ReadAllText($path)
    $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($l in ($text -split "`r?`n")) { $lines.Add($l) }
    # drop a trailing empty element produced by a final newline
    if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq '') { $lines.RemoveAt($lines.Count - 1) }
    $section = -1; $end = -1; $found = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $l = $lines[$i]
        if ($l -match '^\s*\[(.+)\]\s*$') {
            if ($section -ge 0 -and $end -lt 0) { $end = $i }
            if ($Matches[1].Trim().ToLowerInvariant() -eq 'plugins') { $section = $i; $end = -1 }
            continue
        }
        if ($section -ge 0 -and $end -lt 0 -and $l -match ('^\s*' + [regex]::Escape($name) + '\s*=')) { $found = $i }
    }
    if ($found -ge 0) {
        if ($lines[$found] -match ('^\s*' + [regex]::Escape($name) + '\s*=\s*' + [regex]::Escape($value) + '\s*$')) { return $false }
        $lines[$found] = $name + '=' + $value
    } elseif ($section -ge 0) {
        if ($end -lt 0) { $end = $lines.Count }
        # insert after the last non-empty line of the section
        $at = $end
        while ($at -gt $section + 1 -and $lines[$at - 1].Trim() -eq '') { $at-- }
        $lines.Insert($at, $name + '=' + $value)
    } else {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1].Trim() -ne '') { $lines.Add('') }
        $lines.Add('[plugins]'); $lines.Add($name + '=' + $value)
    }
    # UTF-8 without a BOM: the loader reads the file byte by byte.
    [IO.File]::WriteAllText($path, (($lines -join $nl) + $nl), (New-Object Text.UTF8Encoding($false)))
    return $true
}

# ================================================================ uninstall
if ($Uninstall) {
    Write-Host (T 'Entferne Republic Mod Manager ...' 'Removing Republic Mod Manager ...')
    $removed = 0
    foreach ($rel in $files) {
        # The loader DLL stays: without it the game has no loader at all. The file it
        # replaced is in tesmioloader\rmm_install_backup\<time> should anyone want it back.
        if ($rel -ieq 'tesmioloader.dll') { Note (T 'tesmioloader.dll bleibt (die Fassung aus diesem Paket); das Original liegt in tesmioloader\rmm_install_backup.' 'tesmioloader.dll stays (the build from this package); the original is in tesmioloader\rmm_install_backup.'); continue }
        $target = Join-Path $build $rel
        if (Test-Path -LiteralPath $target -PathType Leaf) { Remove-Item -LiteralPath $target -Force; $removed++ }
    }
    foreach ($dir in @('settings_schemas\languages', 'settings_schemas')) {
        $d = Join-Path $build $dir
        if ((Test-Path -LiteralPath $d) -and -not (Get-ChildItem -LiteralPath $d -Force | Select-Object -First 1)) { Remove-Item -LiteralPath $d -Force }
    }
    Ok ((T 'Dateien entfernt: ' 'Files removed: ') + $removed)
    if (SetPluginSwitch $loaderIni 'workshop_bridge' '0') { Ok (T 'tesmioloader.ini: workshop_bridge=0' 'tesmioloader.ini: workshop_bridge=0') }
    if (SetPluginSwitch $loaderIni 'resources_plus' '0') { Ok (T 'tesmioloader.ini: resources_plus=0' 'tesmioloader.ini: resources_plus=0') }
    if (Test-Path -LiteralPath (Join-Path $build 'plugins\resources.dll') -PathType Leaf) {
        if (SetPluginSwitch $loaderIni 'resources' '1') { Ok (T 'tesmioloader.ini: das Original-Plugin resources ist wieder eingeschaltet (resources=1).' 'tesmioloader.ini: the original resources plugin is switched back on (resources=1).') }
    }
    Note (T 'plugins\resources_plus.ini bleibt liegen - dort steht deine Ressourcenliste.' 'plugins\resources_plus.ini is kept - it holds your resource list.')
    if (SetPluginSwitch $loaderIni 'needs_plus' '0') { Ok (T 'tesmioloader.ini: needs_plus=0' 'tesmioloader.ini: needs_plus=0') }
    if (Test-Path -LiteralPath (Join-Path $build 'plugins\needs.dll') -PathType Leaf) {
        if (SetPluginSwitch $loaderIni 'needs' '1') { Ok (T 'tesmioloader.ini: das Original-Plugin needs ist wieder eingeschaltet (needs=1).' 'tesmioloader.ini: the original needs plugin is switched back on (needs=1).') }
    }
    Note (T 'plugins\needs_plus.ini bleibt liegen - dort steht deine Bedürfnisliste.' 'plugins\needs_plus.ini is kept - it holds your needs list.')
    $lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Republic Mod Manager.lnk'
    if (Test-Path -LiteralPath $lnk) { Remove-Item -LiteralPath $lnk -Force; Ok (T 'Desktop-Verknüpfung entfernt.' 'Desktop shortcut removed.') }
    Note (T ('Deine persönlichen Einstellungen unter ' + $build + '\user_config bleiben erhalten.') ('Your personal settings under ' + $build + '\user_config are kept.'))
    Write-Host ''
    Write-Host (T 'Republic Mod Manager wurde entfernt.' 'Republic Mod Manager has been removed.') -ForegroundColor Green
    exit 0
}

# ================================================================ install
Write-Host (T 'Installiere Republic Mod Manager ...' 'Installing Republic Mod Manager ...')
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$backup = Join-Path $game ('tesmioloader\rmm_install_backup\' + $stamp)
$copied = 0; $kept = 0; $replaced = 0
foreach ($rel in $files) {
    $from = Join-Path $source $rel
    $to = Join-Path $build $rel
    $exists = Test-Path -LiteralPath $to -PathType Leaf
    if ($exists -and ($keep -contains $rel)) { $kept++; Note ((T 'behalten: ' 'kept: ') + $rel); continue }
    if ($exists -and (Hash $from) -eq (Hash $to)) { Note ((T 'unverändert: ' 'unchanged: ') + $rel); continue }
    $parent = Split-Path -Parent $to
    if (-not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    if ($exists) {
        $bparent = Split-Path -Parent (Join-Path $backup $rel)
        if (-not (Test-Path -LiteralPath $bparent)) { New-Item -ItemType Directory -Path $bparent -Force | Out-Null }
        Copy-Item -LiteralPath $to -Destination (Join-Path $backup $rel) -Force
        $replaced++
    }
    try {
        Copy-Item -LiteralPath $from -Destination $to -Force
    } catch {
        Fail ((T 'Datei konnte nicht kopiert werden: ' 'File could not be copied: ') + $rel) `
             ((T 'Windows meldet: ' 'Windows says: ') + $_.Exception.Message + (T ' Läuft das Spiel noch, oder fehlen Schreibrechte im Spielordner?' ' Is the game still running, or is the game folder write-protected?'))
    }
    if ((Hash $from) -ne (Hash $to)) {
        Fail ((T 'Datei ist nach dem Kopieren nicht identisch: ' 'File differs after copying: ') + $rel) `
             (T 'Der Kopiervorgang wurde gestört (Virenscanner, volle Platte?). Bitte erneut starten.' 'The copy was disturbed (antivirus, full disk?). Please run again.')
    }
    $copied++
}
Ok ((T 'Dateien kopiert: ' 'Files copied: ') + $copied + (T ', behalten: ' ', kept: ') + $kept + (T ', ersetzt: ' ', replaced: ') + $replaced)
if ($replaced -gt 0) { Note ((T 'Sicherung der ersetzten Dateien: ' 'Backup of the replaced files: ') + $backup) }
Note (T 'tesmioloader.dll ist jetzt die Fassung aus diesem Paket (b0.3.6-rmm.1, gleiche API wie MaxLegends b0.3.6).' 'tesmioloader.dll is now the build from this package (b0.3.6-rmm.1, same API as the b0.3.6 by MaxLegend).')

if (SetPluginSwitch $loaderIni 'workshop_bridge' '1') { Ok (T 'tesmioloader.ini: Workshop Bridge eingeschaltet (workshop_bridge=1).' 'tesmioloader.ini: Workshop Bridge switched on (workshop_bridge=1).') }
else { Ok (T 'tesmioloader.ini: Workshop Bridge war schon eingeschaltet.' 'tesmioloader.ini: Workshop Bridge was already on.') }

# Resources Plus replaces the original resources plugin: same list, same saves, without the
# crash on exit. The original is switched off, never deleted; -Uninstall switches it back on.
if (SetPluginSwitch $loaderIni 'resources_plus' '1') { Ok (T 'tesmioloader.ini: Resources Plus eingeschaltet (resources_plus=1).' 'tesmioloader.ini: Resources Plus switched on (resources_plus=1).') }
else { Ok (T 'tesmioloader.ini: Resources Plus war schon eingeschaltet.' 'tesmioloader.ini: Resources Plus was already on.') }
if (Test-Path -LiteralPath (Join-Path $build 'plugins\resources.dll') -PathType Leaf) {
    if (SetPluginSwitch $loaderIni 'resources' '0') {
        Ok (T 'tesmioloader.ini: das Original-Plugin resources ist ausgeschaltet (resources=0). Resources Plus übernimmt seine Liste beim nächsten Spielstart nach plugins\resources_plus.ini.' 'tesmioloader.ini: the original resources plugin is switched off (resources=0). Resources Plus takes over its list into plugins\resources_plus.ini at the next game start.')
        Note (T 'Zurück zum Original: im Republic Mod Manager Resources Plus aus- und resources einschalten, oder in tesmioloader.ini resources=1 und resources_plus=0 setzen. Die DLL wurde nicht angefasst.' 'Back to the original: switch Resources Plus off and resources on in Republic Mod Manager, or set resources=1 and resources_plus=0 in tesmioloader.ini. The DLL was not touched.')
    } else { Note (T 'tesmioloader.ini: das Original-Plugin resources war schon ausgeschaltet.' 'tesmioloader.ini: the original resources plugin was already off.') }
}

# Needs Plus replaces the original needs plugin the same way: same list, same saves, its own file.
if (SetPluginSwitch $loaderIni 'needs_plus' '1') { Ok (T 'tesmioloader.ini: Needs Plus eingeschaltet (needs_plus=1).' 'tesmioloader.ini: Needs Plus switched on (needs_plus=1).') }
else { Ok (T 'tesmioloader.ini: Needs Plus war schon eingeschaltet.' 'tesmioloader.ini: Needs Plus was already on.') }
if (Test-Path -LiteralPath (Join-Path $build 'plugins\needs.dll') -PathType Leaf) {
    if (SetPluginSwitch $loaderIni 'needs' '0') {
        Ok (T 'tesmioloader.ini: das Original-Plugin needs ist ausgeschaltet (needs=0). Needs Plus übernimmt seine Liste beim nächsten Spielstart nach plugins\needs_plus.ini.' 'tesmioloader.ini: the original needs plugin is switched off (needs=0). Needs Plus takes over its list into plugins\needs_plus.ini at the next game start.')
        Note (T 'Zurück zum Original: im Republic Mod Manager Needs Plus aus- und needs einschalten, oder in tesmioloader.ini needs=1 und needs_plus=0 setzen. Die DLL wurde nicht angefasst.' 'Back to the original: switch Needs Plus off and needs on in Republic Mod Manager, or set needs=1 and needs_plus=0 in tesmioloader.ini. The DLL was not touched.')
    } else { Note (T 'tesmioloader.ini: das Original-Plugin needs war schon ausgeschaltet.' 'tesmioloader.ini: the original needs plugin was already off.') }
}

# ---------------------------------------------------------------- shortcut
$want = $false
if ($Shortcut) { $want = $true }
elseif ($NoShortcut) { $want = $false }
elseif (Interactive) {
    $answer = Read-Host (T 'Verknüpfung zu rmm.exe auf dem Desktop anlegen? (J/N)' 'Create a shortcut to rmm.exe on the desktop? (Y/N)')
    $want = ($answer.Trim().ToLowerInvariant() -in @('j', 'ja', 'y', 'yes'))
}
if ($want) {
    try {
        $lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Republic Mod Manager.lnk'
        $shell = New-Object -ComObject WScript.Shell
        $sc = $shell.CreateShortcut($lnk)
        $sc.TargetPath = Join-Path $build 'rmm.exe'
        $sc.WorkingDirectory = $build
        $sc.IconLocation = (Join-Path $build 'rmm.exe') + ',0'
        $sc.Description = 'Republic Mod Manager'
        $sc.Save()
        Ok ((T 'Desktop-Verknüpfung angelegt: ' 'Desktop shortcut created: ') + $lnk)
    } catch {
        Note ((T 'Desktop-Verknüpfung konnte nicht angelegt werden: ' 'Desktop shortcut could not be created: ') + $_.Exception.Message)
    }
}

Write-Host ''
Write-Host (T 'Republic Mod Manager ist installiert.' 'Republic Mod Manager is installed.') -ForegroundColor Green
Write-Host ((T 'Starten: ' 'Start: ') + (Join-Path $build 'rmm.exe'))
Write-Host (T 'Nach einem Update des Workshop-Objekts einfach Install-RMM.bat erneut ausführen.' 'After an update of the Workshop item just run Install-RMM.bat again.')
exit 0
