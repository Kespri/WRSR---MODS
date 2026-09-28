# 🪟 UI Layout Fixes 0.3.0

**TesmioLoader-Plugin für gezielte Korrekturen an einzelnen Infofenstern**

Bündelt fensterbezogene Korrekturen für *Workers & Resources: Soviet Republic* 1.1.1.9. Jede Fensterart ist ein eigenes Modul mit eigener Konfiguration, Zielprüfung und eigenem Schalter. Derzeit enthalten: das Modul **CUSTOMHOUSE** (größerer Zeilenabstand der Ressourcenliste im Zollhaus) und das Modul **TEXT_WRAP** (Spieltexte, die du per Text-ID einträgst, werden umgebrochen und zeilenweise gezeichnet, statt aus ihrem Fenster zu laufen; vorbelegt ist der Routenhinweis im Fahrzeugfenster). Es gibt bewusst keinen globalen Zeilenabstand.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Wertebereiche](#-wertebereiche)
- [Funktionsweise](#-funktionsweise)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Keine Abhängigkeit zu anderen Plugins

### In drei Schritten
1. **Eine Installationsmethode wählen** (siehe unten) und das Plugin aktivieren.
2. **Zeilenabstand einstellen**, wenn gewünscht: `resource_row_pitch` von 25.0 bis 60.0 in `ui_layout_fixes.ini`, am bequemsten über Republic Mod Manager (Standard 30.0).
3. **Spiel vollständig neu starten** und ein Zollhaus oder ein Fahrzeug mit Routenproblem öffnen. Im Detail-Log stehen `[CUSTOMHOUSE] active` und `[TEXT_WRAP] active`.

---

## ✨ Features

### 🎯 Grundfunktion
- ✅ Modul CUSTOMHOUSE: größerer Zeilenabstand der Ressourcenliste in Gebäuden mit `$TYPE_CUSTOMHOUSE`, also den Zollhäusern des Spiels und kompatiblen Mod-Gebäuden mit demselben Infofenster
- ✅ Nur die zwei geprüften Aufrufe der Ressourcenliste werden geändert: der Messdurchlauf (Listenhöhe) und der Zeichendurchlauf (Symbole und Texte); beide verwenden denselben Abstand
- ✅ Position der darunterliegenden Fensterabschnitte und Scrollbereich rechnet weiterhin das native Layout des Spiels; alle anderen Fenster behalten den nativen Abstand von 25,0 logischen Pixeln
- ✅ Nur geprüfter Code und Daten im Speicher des laufenden Spiels werden geändert; Spieldateien, Gebäudedateien und Spielstände bleiben unverändert

### 🆕 Neu in 0.3.0
- ✅ Alle Texte für Republic Mod Manager im Spieler-Stil: Hinweiskästen statt Gruppentexte, Karte Allgemein entfernt, eigener Hilfetext im Hinzufügen-Dialog, Auswahl der Spieltexte über „Text auswählen…“.
- ✅ **Modul TEXT_WRAP:** Lange Spieltexte wie der Hinweis „Zeigt den Bereich an, in dem ein mögliches Problem auf der Route besteht!“ im Fahrzeugfenster werden vom Spiel einzeilig gezeichnet und laufen aus dem Fenster. Das Modul fängt die Textabfrage des Spiels für jede Text-ID aus der Liste `[text_wrap_ids]` ab, liefert eine umgebrochene Kopie und zeichnet sie über die gehookten Zeichenfunktionen der Engine Zeile für Zeile. Regeln: höchstens `max_chars` Zeichen je Zeile (Standard 58, je Text überschreibbar), Wörter werden nie getrennt, die Zeilen werden gleich lang verteilt, die Absätze des Spiels fließen zusammen (`keep_breaks = 1` behält sie), Zeilenabstand `line_spacing` als Vielfaches der Schriftgröße (Standard 1.15). Braucht ein Text mehr als `max_lines` Zeilen (Standard 4), werden die Zeilen schrittweise breiter. Es wird kein Spielcode geändert; das Modul hängt nicht am Spiel-Build und gilt für jede Spielsprache mit Leerzeichen zwischen den Wörtern.
- ✅ **Kandidaten finden:** `log_long_texts = 70` schreibt jede Text-ID, die das Spiel anzeigt und deren längste Zeile über 70 Zeichen hat, einmal ins Detail-Log, mit den ersten 80 Zeichen. In Republic Mod Manager trägst du die ID dann auf dem Reiter Text-IDs ein.
- ✅ Republic Mod Manager: Listen-Editor mit den Reitern Allgemein, Zollhaus, Textumbruch und Text-IDs.
- ✅ **Gemeinsame Konfigurationsregel** (`tesmio_config.h`): Basis ist `plugins\ui_layout_fixes.ini`, sonst die INI neben der DLL (im Workshop-Paket). Liegt `user_config\ui_layout_fixes.ini` im Loader-Ordner, gewinnen dessen Schlüssel einzeln; diese Datei schreibt nur Republic Mod Manager. Beide Pfade stehen beim Start im Detail-Log.

---

## 💾 Installation

Wähle **eine** der vier Methoden. Dieselbe DLL darf nie zweimal geladen werden.

---

### Methode 1️⃣: Klassischer TesmioLoader

```
1. Kopiere ui_layout_fixes.dll und ui_layout_fixes.ini aus hooks\
   → tesmioloader\build\plugins\

2. Aktiviere ui_layout_fixes im TesmioLauncher
3. Spiel vollständig neu starten
```

---

### Methode 2️⃣: Soviet Mod Loader (SML)

```
1. Workshop-Objekt abonnieren – SML liest abonnierte Pakete von selbst
2. SML lädt die DLL über soviet.mod.ini aus dem Paket, die INI liegt daneben
3. Eine lokale ui_layout_fixes.dll in plugins\ vorher entfernen oder abschalten
```

---

### Methode 3️⃣: Workshop Bridge (ohne SML)

```
1. Paket in Republic Mod Manager auswählen
2. „Plugin aktiv“ einschalten
3. workshop_bridge lädt die DLL direkt aus dem Paket
4. Steam-Updates gelten sofort
```

---

### Methode 4️⃣: Republic Mod Manager mit „Dateien nur lokal“

```
1. Paket in Republic Mod Manager auswählen, Einstellungen anpassen, Speichern
2. Reiter „Allgemein“, Karte „Hinweise“: „Dateien nur lokal“ einschalten
3. Bestätigung mit Dateiliste → DLL und INI werden beim Speichern
   nach tesmioloader\build\plugins\ kopiert
4. Die Workshop Bridge überspringt das Paket danach automatisch
```

**„Dateien nur lokal“ im Detail**
- Für alle, die das Plugin ohne Steam-Abo weiterbenutzen wollen
- Steam-Updates gelten bei lokalen Dateien erst nach erneutem Speichern (gelbe Marke „Update“)
- Ausschalten entfernt nur die von Republic Mod Manager kopierten Dateien wieder

---

## 🧰 Republic Mod Manager

Das Paket enthält im Ordner `config` ein Launcher-Schema. Republic Mod Manager zeigt UI Layout Fixes damit in vier Reitern, deutsch und englisch:

- **Allgemein:** Hinweise, „Dateien nur lokal“ und der Knopf für diese Anleitung
- **Zollhaus:** Modulschalter und Zeilenabstand der Ressourcenliste
- **Textumbruch:** Modulschalter, Zeichen je Zeile, Zeilenlimit, Zeilenabstand, Absatzverhalten und die Log-Hilfe für lange Texte
- **Text-IDs:** die Liste der umgebrochenen Texte mit eigener Breite je Eintrag; der Routenhinweis 1970 ist vorbelegt und kann ausgeblendet werden. Der Knopf „Text auswählen…“ im Hinzufügen-Dialog zeigt alle Texte deiner Spielsprache mit Suche und Mindestlänge (braucht RMM 0.4.21)

Der Schalter „Plugin aktiv“ im Kopf setzt beim Einschalten auch `enabled = 1`. Republic Mod Manager schreibt die wirksame INI nach `tesmioloader\build\plugins\ui_layout_fixes.ini` und sichert die ausgelieferte Fassung; eigene Text-IDs liegen getrennt und werden beim Speichern eingemischt. Wer die INI lieber von Hand bearbeitet, findet alles Weitere unten.

---

## ⚙️ Konfiguration

### Hauptdatei: `ui_layout_fixes.ini`

Die DLL liest in dieser Reihenfolge:
- **Basis:** `tesmioloader\build\plugins\ui_layout_fixes.ini`, falls vorhanden, sonst die INI neben der DLL (im Paket `hooks\ui_layout_fixes.ini`); die wirksame INI dort schreibt Republic Mod Manager
- **Overlay:** `tesmioloader\build\user_config\ui_layout_fixes.ini`, Schlüssel für Schlüssel darüber, falls du sie von Hand anlegst (ein `[text_wrap_ids]`-Abschnitt dort ersetzt die Liste komplett)

⚠️ **Kommentare nur in eigenen Zeilen mit `;`.** Die Datei muss UTF-8 ohne BOM sein; Abschnitte und Schlüssel nicht wiederholen. Unbekannte oder falsch geschriebene Namen werden ignoriert, das Plugin verwendet dann unbemerkt den eingebauten Standard; deshalb die dokumentierten Namen unverändert lassen und nach Änderungen das Log prüfen. Die Einstellungen werden nur beim Start gelesen: INI ändern, Spiel vollständig neu starten.

```ini
[general]
; 1 aktiviert die einzeln eingestellten Module, 0 startet kein Modul und schreibt keinen Patch
enabled = 1

[customhouse]
; 1 aktiviert nur das Modul CUSTOMHOUSE; ohne Wirkung, solange [general] enabled = 0 ist
enabled = 1
; Zeilenabstand der Ressourcenliste in logischen Pixeln, 25.0 bis 60.0, Dezimalpunkt
; 25.0 = nativer Spielwert, 30.0 = Plugin-Standard
resource_row_pitch = 30.0

[text_wrap]
; 1 aktiviert nur das Modul TEXT_WRAP; ohne Wirkung, solange [general] enabled = 0 ist
enabled = 1
; Standardbreite in Zeichen für jeden gelisteten Text ohne eigene Breite, 20 bis 200
max_chars = 58
; Zeilenlimit, 0 bis 12; 0 = kein Limit; darüber werden die Zeilen schrittweise breiter
max_lines = 4
; Zeilenabstand als Vielfaches der Schriftgröße, 0.50 bis 3.00, Dezimalpunkt
line_spacing = 1.15
; 0 = Absätze des Spiels fließen zusammen, 1 = Absätze bleiben eigene Zeilen
keep_breaks = 0
; 0 = aus; sonst landet jeder angezeigte Text mit längerer Zeile einmal im Detail-Log
log_long_texts = 0

[text_wrap_ids]
; Text-ID = Zeichen je Zeile; 0 = Standard aus [text_wrap], sonst 20 bis 200; bis 64 Einträge
1970 = 0
```

`enabled` wird als Ganzzahl gelesen: `0` schaltet ab, jeder andere Wert schaltet ein. Verwende trotzdem nur `0` oder `1`.

---

## 📏 Wertebereiche

| Schlüssel | Bereich | Standard |
|---|---|---|
| `[general] enabled` | 0 oder 1 | 1 |
| `[customhouse] enabled` | 0 oder 1 | 1 |
| `resource_row_pitch` | 25.0 bis 60.0 einschließlich, Dezimalpunkt | 30.0 |
| `[text_wrap] enabled` | 0 oder 1 | 1 |
| `max_chars` | 20 bis 200, ganze Zahl | 58 |
| `max_lines` | 0 bis 12, ganze Zahl (0 = kein Limit) | 4 |
| `line_spacing` | 0.50 bis 3.00, Dezimalpunkt | 1.15 |
| `keep_breaks` | 0 oder 1 | 0 |
| `log_long_texts` | 0 bis 400, ganze Zahl (0 = aus) | 0 |
| `[text_wrap_ids] <id>` | ID 1 bis 100000; Wert 0 oder 20 bis 200 | `1970 = 0` |

Für `resource_row_pitch` gilt: `25.0` entspricht dem nativen Spielwert und bewirkt keinen größeren Abstand; nichtnumerische, unendliche oder außerhalb liegende Werte werden verworfen, das Plugin protokolliert eine Warnung (`invalid-config`) und verwendet `30.0`, das Modul bleibt aktiv.

Für die Schlüssel in `[text_wrap]` gilt dasselbe: ungültige oder außerhalb liegende Werte lösen eine Warnung (`invalid-config`) aus und fallen auf den Standard zurück. Ungültige Zeilen in `[text_wrap_ids]` werden mit Warnung übersprungen; fehlt der Abschnitt ganz, gilt der eingebaute Eintrag 1970.

---

## 🔬 Funktionsweise

Das Spiel verwendet für die Ressourcenliste nativ einen Abstand von `25.0f`. Das Plugin leitet nur die zwei geprüften CUSTOMHOUSE-Aufrufe über eine nahe Speicherbrücke um; während eines solchen Aufrufs wird der native Abstand vorübergehend durch den konfigurierten Wert ersetzt und unmittelbar danach wiederhergestellt. Unverändert bleiben dadurch: Ressourcenlisten anderer Fenster, die native Berechnung der Listenhöhe, die Positionierung der nachfolgenden Fensterabschnitte, der Scrollbereich, Warenmengen, Preise, Gebäudeeigenschaften, Spielstände und Dateien.

Vor dem Schreiben eines Patches prüft das Modul die Größe des geladenen Abbilds, den PE-Zeitstempel von `SOVIET64.exe`, die Signatur der CUSTOMHOUSE-Panel-Funktion, Mess- und Zeichenaufruf samt Ziel, die native Instruktion für den Zeilenabstand und die Erreichbarkeit der Speicherbrücke. Schlägt ein Schritt fehl, schreibt das Plugin keinen Patch und das Modul bleibt inaktiv.

Das Modul TEXT_WRAP setzt an zwei Stellen an, beide über die Importtabelle von `SOVIET64.exe`. Erstens hängt es sich in die Textabfrage `C3D_LANGUAGE::GetString` der Engine-DLL: Jede gelistete Text-ID bekommt eine umgebrochene Kopie aus einem eigenen Puffer, jede andere ID läuft unverändert durch; die Kopie wird nur neu gebaut, wenn das Spiel einen anderen Text für die ID liefert (Sprachwechsel). Zweitens leitet es die Zeichenfunktionen der Engine (`PrintLeft/Center/RightUnicode` auf `C3D_FONTMANAGER` und `C3D_FONT`, `PrintLeftUnicodeNoArg`) über kleine erzeugte Sprungstücke um: Zeigt der Textzeiger in die Puffer des Plugins, wird Zeile für Zeile gezeichnet und y je Zeile um Schriftgröße mal `line_spacing` weitergerückt; jeder andere Aufruf springt mit allen Argumenten unverändert ins Original weiter. Die Zeichenfunktionen sind variadisch, deshalb prüft das Sprungstück nur den Zeiger und rührt den Stapel nicht an. Es wird kein Spielcode geändert; das Modul hängt nicht am Spiel-Build, nur die Text-IDs können sich mit einem Spielupdate verschieben. Weitere Plugins, die dieselben Importe hooken (zum Beispiel das Resources-Plugin bei der Textabfrage), reihen sich in Ladereihenfolge ein; jedes beantwortet nur seine eigenen Texte.

Grenze: Kopiert das Spiel einen Text vor dem Zeichnen in einen eigenen Puffer, erkennt die Zeigerprüfung ihn nicht mehr und er bleibt einzeilig. Zeichen sind keine Pixel; für schmale Fenster bekommt der Eintrag eine eigene Breite.

| Eigenschaft | Erwarteter Wert |
|---|---|
| Spielversion | `SOVIET64.exe 1.1.1.9` |
| PE-Zeitstempel | `0x6A3EB6AD` |
| Größe des ausführbaren Abbilds | `0x00A9D000` |

---

## 💾 Kompatibilität

### Spiel und Loader
- Exakt unterstützte Spielversion 1.1.1.9 für CUSTOMHOUSE; andere Versionen werden abgelehnt, weil Speicheradressen und Signaturen versionsabhängig sind. TEXT_WRAP ändert keinen Spielcode und hängt nur an den Text-IDs
- Verträgt sich mit dem Resources-Plugin, das dieselbe Textabfrage hookt; die Hooks reihen sich in Ladereihenfolge
- Nur Sprachen mit Leerzeichen zwischen den Wörtern werden umgebrochen (Chinesisch und Japanisch bleiben unverändert)
- Keine Abhängigkeit zum Localization-Plugin oder zu anderen TesmioLoader-Plugins
- Ein vollständiger Neustart des Spiels entfernt alle aktiven Speicheränderungen

### Spielstände
Das Plugin verändert weder Spieldateien noch Spielstände.

### Versionskompatibilität
- **0.3.0:** erste veröffentlichte Fassung mit den Modulen CUSTOMHOUSE und TEXT_WRAP

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| Kein größerer Abstand | `[general]` oder `[customhouse]` `enabled = 0`, oder `resource_row_pitch = 25.0` | Schalter einschalten, Wert über 25.0 setzen, Spiel neu starten |
| `invalid-config` im Log | ein Wert in `[customhouse]`, `[text_wrap]` oder eine Zeile in `[text_wrap_ids]` ungültig | Wert im dokumentierten Bereich eintragen (Zeilenabstand mit Dezimalpunkt) |
| Gelisteter Text weiter einzeilig | `[text_wrap] enabled = 0`, ID nicht in der Liste, oder das Spiel kopiert den Text vor dem Zeichnen | Modul einschalten, ID prüfen (`log_long_texts`), sonst bleibt dieser Text außen vor |
| Zeilen zu weit auseinander oder überlappend | `line_spacing` passt nicht zur Schrift | Wert ändern; die erste umgebrochene Ausgabe steht mit Schriftgröße und Schritt im Detail-Log |
| `print-import`, `print-hooks` | Zeichen-Importe konnten nicht umgeleitet werden | `tesmioloader.log` prüfen; nur dieses Modul bleibt inaktiv |
| `stub-allocation` | Speicherseite für die Sprungstücke nicht verfügbar | Spiel neu starten, Log aufbewahren |
| `font-size` | Schriftgrößen-Export nicht gefunden | Zeilenabstand rechnet mit 16 mal `line_spacing` |
| `text-length` | Spieltext länger als der Puffer (1023 Zeichen) | Text-ID prüfen; der native Text wird unverändert angezeigt |
| `all window modules are disabled` | Plugin an, aber beide Module `enabled = 0` | mindestens ein Modul einschalten |
| `no text ids listed` | `[text_wrap_ids]` vorhanden, aber leer | eine ID eintragen oder das Modul ausschalten |
| `log-open` | Detail-Log nicht anlegbar | Schreibrechte prüfen; `tesmioloader.log` bleibt verfügbar |

### Logging

Meldungen stehen in `tesmioloader.log` (Warnungen, Fehler, Phasenzusammenfassungen) und im Detail-Log `logs\tesmioloader.ui_layout_fixes.log` (vollständiges strukturiertes Protokoll mit Modul, Prüfregel und Windows-Fehlercode).
- In **Republic Mod Manager** öffnet das Symbol mit dem Dokument unten in der Plugin-Leiste die Protokollansicht mit Filter und Absender.

Erwartet bei erfolgreichem Start:
```
[CUSTOMHOUSE] active
[TEXT_WRAP] active
```

---

## 📦 Dateistruktur

**Workshop-Paket** (Steam-Abo, SML, Workshop Bridge)
```
ui_layout_fixes\
├── hooks\
│   ├── ui_layout_fixes.dll         (Plugin)
│   └── ui_layout_fixes.ini         (Original-INI)
├── config\                         (Launcher-Schema für Republic Mod Manager)
│   ├── ui_layout_fixes.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                  (Manifest für SML, Bridge und Republic Mod Manager)
├── workshopconfig.ini              (Steam-Workshop-Eintrag)
├── previewimage.png
├── README_DE.md
└── README_EN.md
```

**Loader-Ordner** (Methode 1 von Hand oder „Dateien nur lokal“)
```
tesmioloader\build\
├── logs\
│   └── tesmioloader.ui_layout_fixes.log
├── plugins\
│   ├── ui_layout_fixes.dll
│   └── ui_layout_fixes.ini         (Original-INI)
└── user_config\
    └── ui_layout_fixes.ini         (persönliche Werte aus Republic Mod Manager)
```

Über die Bridge oder SML liegt in `plugins` nur die Original-INI, die DLL bleibt im Paket. Die persönlichen Werte liegen in jedem Fall in `user_config`.

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Das Plugin enthält keinen fremden Code; der Loader-SDK-Header stammt aus dem TesmioLoader von MaxLegend (GPL v3). Der vollständige Quelltext liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/ui_layout_fixes.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Gilt der Abstand auch für andere Gebäude?**
A: Nein. Nur für Gebäude mit `$TYPE_CUSTOMHOUSE`; alle anderen Fenster behalten den nativen Abstand.

**F: Muss ich das Spiel nach einer Änderung neu starten?**
A: Ja. Die Einstellungen werden nur beim Start gelesen.

**F: Was passiert bei einem ungültigen Zeilenabstand?**
A: Das Plugin warnt im Log und verwendet 30.0; das Modul bleibt aktiv.

**F: Ein gelisteter Text ist immer noch zu breit?**
A: `Zeichen je Zeile` verkleinern, für diesen Text am besten in der Liste mit eigener Breite; das Modul verteilt die Wörter gleichmäßig auf die Zeilen. Braucht der Text mehr Zeilen als `Zeilenlimit`, macht das Modul die Zeilen wieder breiter; dann das Limit erhöhen oder auf 0 setzen.

**F: Wie finde ich die Text-ID eines Textes?**
A: Am schnellsten über „Text auswählen…“ im Hinzufügen-Dialog von Republic Mod Manager: Suche nach einem Wort aus dem Text oder Filter „Zeilen ab 60 Zeichen“, Doppelklick übernimmt die ID. Ohne RMM: `Lange Texte loggen` auf zum Beispiel 70 stellen, das Spiel starten und das Fenster öffnen; im Detail-Log steht dann jede angezeigte Text-ID mit einer Zeile über 70 Zeichen samt Textanfang. Danach wieder auf 0.

**F: Kommen weitere Fenster dazu?**
A: Das Plugin ist dafür angelegt: jedes Fenster ist ein eigenes Modul mit eigenem Abschnitt in der INI.

**F: Muss ich die INI von Hand bearbeiten?**
A: Nein. Republic Mod Manager zeigt alle Schalter und Werte mit Beschreibung und prüft die Wertebereiche.

---

**Letzte Aktualisierung:** UI Layout Fixes 0.3.0  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
