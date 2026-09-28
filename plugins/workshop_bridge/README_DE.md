# 🌉 Workshop Bridge 0.2.0

**TesmioLoader-Plugin: lädt Workshop-Pakete ohne Soviet Mod Loader**

Lädt die Hook-DLLs deiner abonnierten Workshop-Pakete direkt in TesmioLoader für *Workers & Resources: Soviet Republic* 1.1.1.9. Kein Soviet Mod Loader, kein Kopieren von Hand, Steam-Updates gelten sofort.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Wertebereiche](#-wertebereiche)
- [Ablauf & Log](#-ablauf--log)
- [Was die Bridge nie tut](#-was-die-bridge-nie-tut)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)
- [Lizenz & Credits](#-lizenz--credits)
- [FAQ](#-faq)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader b0.3.6 (API 4)
- Republic Mod Manager (empfohlen, pflegt die Paketliste für dich)

### In drei Schritten
1. **Bridge in den Loader-Ordner:** Sie kommt mit dem Republic-Mod-Manager-Paket nach `tesmioloader\build\plugins\` (Installer oder Ordner „Manual Installation“, siehe Installation).
2. **Bridge einschalten:** In `tesmioloader.ini` steht `workshop_bridge = 1` unter `[plugins]`. Republic Mod Manager setzt das beim ersten Speichern, im TesmioLauncher ist es das Häkchen.
3. **Pakete freigeben:** Im Republic Mod Manager bei jedem Workshop-Paket „Plugin aktiv“ einschalten. Beim nächsten Spielstart lädt die Bridge dessen Hooks.

---

## ✨ Features

- **Lädt direkt aus dem Paket:** Die DLL bleibt im Workshop-Ordner, nichts wird kopiert. Ein Steam-Update ist beim nächsten Start sofort drin.
- **Gleiche Sicht wie unter SML:** Jeder geladene Hook bekommt dieselbe Host-Tabelle wie die Bridge selbst, also denselben Loader-Ordner, dieselbe `user_config`, dieselben Dienste.
- **Zwei Phasen wie im Loader:** Alle Hooks werden im Init der Bridge initialisiert, ihre Dienste stehen damit vor jedem Start bereit; die Starts laufen im Start der Bridge.
- **Paketliste per Schalter:** Welche Pakete laden, entscheidet je Paket der Schalter „Plugin aktiv“ im Republic Mod Manager. Ohne Manager gilt die Regel `policy`.
- **Hält sich raus, wo es schon jemand macht:** Mit Soviet Mod Loader bleibt sie untätig, eine lokale Kopie in `plugins\` hat immer Vorrang, eine doppelt geladene DLL gibt es nicht.
- **Kein Spielcode:** Die Bridge hookt nichts und patcht nichts. Ein Spiel-Update kann sie nicht brechen.

---

## 💾 Installation

Die Bridge wird zusammen mit Republic Mod Manager verteilt. Wähle **eine** Methode.

### Methode 1️⃣: Installer des Republic-Mod-Manager-Pakets

```
1. Republic-Mod-Manager-Paket abonnieren oder herunterladen
2. Install-RMM.ps1 (oder die .bat) starten
3. Der Installer legt rmm.exe, die Ordner und workshop_bridge.dll + .ini
   in tesmioloader\build\plugins\ an und trägt die Bridge in tesmioloader.ini ein
```

### Methode 2️⃣: Ordner „Manual Installation“

```
1. Im Paket den Ordner "Manual Installation" öffnen
2. Darin liegt die komplette Struktur ab tesmioloader\
3. Den Ordner tesmioloader über den im Spielordner ziehen (zusammenführen)
4. Im TesmioLauncher das Häkchen bei workshop_bridge setzen
   oder Republic Mod Manager einmal speichern
```

### Methode 3️⃣: Selbst gebaut (Entwickler)

```
1. workshop_bridge.cpp mit der Bauzeile von der Startseite des Repositorys bauen
2. workshop_bridge.dll und workshop_bridge.ini nach <Spiel>\tesmioloader\build\plugins\ kopieren
3. In tesmioloader.ini unter [plugins] workshop_bridge=1 setzen
```

**Wichtig:** Zusammen mit Soviet Mod Loader brauchst du die Bridge nicht. Ist SML geladen oder in `tesmioloader.ini` eingeschaltet, bleibt die Bridge von selbst untätig (Logzeile `bridge   idle`).

---

## 🧰 Republic Mod Manager

Republic Mod Manager kennt die Bridge als eigenen Eintrag „Workshop Bridge“ mit einer Karte für ihre vier Schalter. Die Paketliste ist dort bewusst kein Feld: Sie entsteht aus dem Schalter „Plugin aktiv“ der einzelnen Workshop-Pakete und landet in `user_config\workshop_bridge.ini`. Die Statuszeile jedes Pakets zeigt mit „Ladeweg: Workshop Bridge“, dass es über die Bridge läuft, und „Zuletzt im Spiel geladen“ liest die Bridge-Zeile aus `tesmioloader.log`.

Persönliche Werte der Bridge selbst stehen ebenfalls in `user_config\workshop_bridge.ini`; die Basis `plugins\workshop_bridge.ini` bleibt unverändert.

---

## ⚙️ Konfiguration

Die Basis ist `plugins\workshop_bridge.ini`, darüber liegt `user_config\workshop_bridge.ini` Schlüssel für Schlüssel. Republic Mod Manager schreibt nur die Overlay-Datei.

```ini
[bridge]
; 0 = Bridge untätig, kein Paket wird geladen
enabled = 1
; list = nur Pakete mit 1 unter [packages]; all = jedes Paket mit Hooks, außer 0
policy = list
; auto = Steam-Bibliothek des Spiels, sonst absoluter Pfad mit einem Unterordner je Paket
workshop_root = auto
; 1 = jedes übersprungene Paket bekommt eine Logzeile
log_verbose = 0

[packages]
; Workshop-Nummer oder Ordnername = 1 lädt die Hooks, 0 überspringt sie
3794994476 = 1
```

Ein Paket mit `enabled = 0` in seiner eigenen `soviet.mod.ini` wird immer übersprungen, egal was hier steht.

---

## 📏 Wertebereiche

| Schlüssel | Bereich | Standard |
|---|---|---|
| `enabled` | 0 oder 1 | 1 |
| `policy` | `list` oder `all` | `list` |
| `workshop_root` | `auto` oder absoluter Pfad | `auto` |
| `log_verbose` | 0 oder 1 | 0 |
| `[packages] <Name>` | 0 oder 1 | – |

---

## 🔄 Ablauf & Log

1. Beim Init liest die Bridge INI und Overlay, prüft, ob Soviet Mod Loader da ist, und geht die Paketordner unter `workshop_root` durch.
2. Jedes freigegebene Paket mit `[hooks] dll` wird geladen, sein Init läuft sofort. Dienste, die ein Hook per `provide` anmeldet, stehen damit vor jedem Start bereit.
3. Im Start der Bridge laufen die Starts aller Hooks.

So sieht ein normaler Start in `tesmioloader.log` aus:

```
workshop_bridge  config base=...\plugins\workshop_bridge.ini (loader folder) overlay=...\user_config\workshop_bridge.ini
bridge   Workshop C:\...\steamapps\workshop\content\784150, policy list
plugin   service "tss.grit_spreader" v1 from workshop_bridge.dll
bridge   hook technical_service_storage 0.3.3    from <Item-Nummer>\hooks\technical_service_storage.dll
bridge   10 hook(s) loaded, 0 skipped, 10 package(s) with hooks
plugin   workshop_bridge  0.2.0 from workshop_bridge.dll
```

Der Loader schreibt die Dienste eines Hooks seinem Log nach `workshop_bridge.dll` gut, die Bridge nennt daneben den echten Namen und die Version.

---

## 🚫 Was die Bridge nie tut

- **Soviet Mod Loader** ist geladen oder installiert und eingeschaltet: SML lädt die Hooks selbst, die Bridge bleibt untätig.
- **`plugins\<name>.dll` existiert:** Diese Kopie gehört dem Loader, ein- oder ausgeschaltet, wie der Launcher es sagt. Das Paket wird übersprungen, eine Logzeile sagt warum. Willst du das Paket, lösch die lokale Kopie oder schalte im Republic Mod Manager „Dateien nur lokal“ aus.
- **Doppelt laden:** Eine DLL, die unter demselben Dateinamen schon im Prozess ist, wird nicht noch einmal geladen.
- **Aus dem Paket ausbrechen:** Hook-Pfade mit `..` oder absolutem Pfad werden abgewiesen.
- **Spielcode anfassen:** Kein Hook, kein Patch, keine Adresse.

---

## 🔗 Kompatibilität

- WRSR 1.1.1.9, TesmioLoader b0.3.6 mit API 4 (mindestens 3). Da die Bridge keine Spieladressen kennt, übersteht sie Spiel-Updates; ob die geladenen Hooks das tun, entscheidet jeder Hook selbst.
- Soviet Mod Loader: verträglich, die Bridge weicht ihm aus.
- Republic Mod Manager ab 0.4.12 (Statuszeile mit Ladeweg).

---

## 🛠️ Fehlerbehandlung

| Symptom | Ursache | Abhilfe |
|---|---|---|
| `bridge   0 hook(s) loaded` | Kein Paket freigegeben oder `policy = list` ohne Einträge | Im Republic Mod Manager „Plugin aktiv“ beim Paket einschalten |
| `bridge   idle` | Soviet Mod Loader ist geladen oder eingeschaltet | Gewollt. Entweder SML oder Bridge |
| Paket wird übersprungen, Logzeile nennt `plugins\<name>.dll` | Lokale Kopie hat Vorrang | Kopie löschen oder „Dateien nur lokal“ ausschalten |
| Paket wird übersprungen, Logzeile nennt `enabled = 0` | Das Paket ist in seiner `soviet.mod.ini` abgeschaltet | Manifest des Pakets prüfen |
| Statuszeile „Beim letzten Spielstart nicht geladen“ | Das Spiel lief seit dem Einschalten noch nicht | Spiel einmal starten |
| Log ohne `bridge`-Zeilen | `workshop_bridge = 0` in `tesmioloader.ini` oder DLL fehlt | Häkchen im TesmioLauncher oder Installation prüfen |

Mit `log_verbose = 1` bekommt jedes angesehene und nicht geladene Paket eine eigene Zeile.

---

## 📁 Dateistruktur

```
tesmioloader\build\
├── plugins\
│   ├── workshop_bridge.dll
│   └── workshop_bridge.ini           (Basis)
├── user_config\
│   └── workshop_bridge.ini           (Overlay: Paketliste und persönliche Werte)
└── tesmioloader.ini                  ([plugins] workshop_bridge = 1)
```

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE`. Die Bridge nutzt die TesmioLoader-API von MaxLegend (Tesmio), https://github.com/MaxLegend/TesmioLoader. Der vollständige Quelltext liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/workshop_bridge.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Brauche ich die Bridge, wenn ich Soviet Mod Loader habe?**
A: Nein. Mit SML bleibt sie untätig. Sie ist für alle, die nur TesmioLoader nutzen.

**F: Muss ich nach einem Steam-Update etwas tun?**
A: Nein. Die DLL wird beim nächsten Start direkt aus dem aktualisierten Paket geladen.

**F: Warum sehe ich die Dienste eines Pakets im Log bei workshop_bridge.dll?**
A: Der Loader kennt nur die Bridge als Plugin. Die Zeile `bridge   hook …` direkt daneben nennt den echten Namen.

**F: Kann ich ein Paket lokal behalten, ohne Abo?**
A: Ja, über „Dateien nur lokal“ im Republic Mod Manager. Die Bridge überspringt das Paket dann automatisch.

**F: Ich habe kein Republic Mod Manager. Geht die Bridge trotzdem?**
A: Ja. Setz `policy = all` oder trag die Pakete unter `[packages]` von Hand ein.

---

**Letzte Aktualisierung:** Workshop Bridge 0.2.0  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
