# 🧰 Resources Button Fix 0.4.0

**TesmioLoader-Plugin für kompakte Werkzeugraster im Geländeeditor**

Ordnet die Werkzeugschaltflächen in zwei Fenstern des Geländeeditors von *Workers & Resources: Soviet Republic* 1.1.1.9 neu: im Fenster **Resources** stehen die Malen-Schaltflächen blockweise über ihren Löschen-Schaltflächen, im Fenster **Rocks/Gravel** wahlweise nebeneinander oder in zwei Reihen. Zusätzliche Werkzeuge aus deposits.dll oder Deposits Plus, etwa Sand, werden automatisch einbezogen.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Wertebereiche](#-wertebereiche)
- [Live-Neuladen](#-live-neuladen)
- [Automatische Skalierung](#-automatische-skalierung)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Optional: deposits.dll oder Deposits Plus – deren zusätzliche Ressourcen- und Geländewerkzeuge werden im jeweils richtigen Fenster mit eingeordnet.

### In drei Schritten
1. **Eine Installationsmethode wählen** (siehe unten) und das Plugin aktivieren.
2. **Spiel starten**, im Geländeeditor das Fenster Resources oder Rocks/Gravel öffnen.
3. **Anpassen**, wenn gewünscht: Spalten, Blöcke, Größe und Abstände stehen in `resources_button_fix.ini`, am bequemsten über Republic Mod Manager. Die meisten Änderungen gelten sofort beim nächsten Öffnen des Fensters.

---

## ✨ Features

### 🎯 Grundfunktion
- ✅ Ressourcenfenster: Malen-Schaltflächen blockweise über den zugehörigen Löschen-Schaltflächen, bis zu 32 Spalten und 4 Blöcke
- ✅ Felsen-/Kiesfenster: Malen und Löschen nebeneinander (P E P E) oder als zwei Reihen (P P über E E)
- ✅ Automatische oder feste Schaltflächengröße, frei einstellbare Abstände und Versätze
- ✅ Fenster wächst bei Bedarf mit; der rote Alles-löschen-Knopf und die Pinselsteuerung rücken nach
- ✅ Baumwerkzeuge und alle anderen Editorfenster bleiben unverändert; keine Spieldatei und keine VFS-Datei wird angefasst

### 🆕 Neu in 0.4.0
- ✅ Alle Texte für Republic Mod Manager im Spieler-Stil: Hinweis-Box auf Allgemein, Karte „Fehlersuche“ mit dem ausführlichen Protokoll, EIN/AUS statt 1/0, „Button“ statt „Schaltfläche“.
- ✅ **INI neben der DLL:** Fehlt `plugins\resources_button_fix.ini`, liest die DLL die INI aus dem eigenen Ordner, also aus dem Workshop-Paket unter Soviet Mod Loader oder der Workshop Bridge. Das Plugin läuft damit direkt aus dem Steam-Abo, ohne dass etwas kopiert werden muss.
- ✅ Die gewählte Konfigurationsdatei steht beim Start im Log; das Live-Neuladen folgt dieser Datei.

---

## 💾 Installation

Wähle **eine** der vier Methoden. Dieselbe DLL darf nie zweimal geladen werden.

---

### Methode 1️⃣: Klassischer TesmioLoader

```
1. Kopiere resources_button_fix.dll und resources_button_fix.ini aus hooks\
   → tesmioloader\build\plugins\

2. Aktiviere resources_button_fix im TesmioLauncher
3. Spiel vollständig neu starten
```

---

### Methode 2️⃣: Soviet Mod Loader (SML)

```
1. Workshop-Objekt abonnieren – SML liest abonnierte Pakete von selbst
2. SML lädt die DLL über soviet.mod.ini aus dem Paket, die INI liegt daneben
3. Eine lokale resources_button_fix.dll in plugins\ vorher entfernen oder abschalten
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

Das Paket enthält im Ordner `config` ein Launcher-Schema. Republic Mod Manager zeigt Resources Button Fix damit in drei Reitern, deutsch und englisch:

- **Allgemein:** Hinweise, „Dateien nur lokal“, Knopf für diese Anleitung und die Fehlersuche (Detail-Log)
- **Ressourcenfenster:** Anordnung (Spalten, Blöcke, Größe, Abstände) sowie Position, Fenster und Alles-löschen-Knopf
- **Felsen-/Kiesfenster:** Anordnung (P E P E oder zwei Reihen, Größe, Abstände) sowie Position und Fenster

Der Schalter „Plugin aktiv“ im Kopf setzt beim Einschalten auch `enabled = 1`. Republic Mod Manager schreibt die wirksame Fassung nach `plugins\resources_button_fix.ini`; persönliche Werte liegen getrennt in `user_config\resources_button_fix.ini`, die INI im Paket bleibt unverändert. Wer die INI lieber von Hand bearbeitet, findet alles Weitere unten.

---

## ⚙️ Konfiguration

### Hauptdatei: `resources_button_fix.ini`

Die DLL liest in dieser Reihenfolge:
- **Zuerst:** `tesmioloader\build\plugins\resources_button_fix.ini`, falls vorhanden (klassische Installation, „Dateien nur lokal“ oder die von Republic Mod Manager geschriebene wirksame INI)
- **Sonst:** die INI neben der DLL, im Paket `hooks\resources_button_fix.ini` (Soviet Mod Loader, Workshop Bridge)

⚠️ **Die Datei wird streng geprüft.** UTF-8 ohne BOM; genau ein Abschnitt `[general]`, `[resources_window]` und `[rocks_window]`; keine unbekannten oder doppelten Abschnitte und Schlüssel; jeder Schlüssel mit Wert; Schalter nur `0` oder `1`; Zahlen im dokumentierten Bereich; Kommentare nur in eigenen Zeilen mit `;` oder `#`, nie hinter einem Wert. Ein fehlender einzelner Schlüssel verwendet den eingebauten Standard; ein fehlender Pflichtabschnitt oder eine anderweitig ungültige Datei verhindert beim Spielstart jede Änderung am Spiel. Die Datei darf höchstens 16 MiB groß sein, ein Wert höchstens 63 Zeichen.

### Abschnitt `[general]`

```ini
[general]
; 1 aktiviert das ganze Plugin, 0 schaltet es ab (Neustart nötig)
enabled = 1
; 1 schreibt Layoutdiagnosen ins Log, nur zur Fehlersuche
debug = 0
```

### Abschnitt `[resources_window]`

```ini
[resources_window]
; 1 ordnet das Ressourcenfenster neu, 0 behält das Originallayout
enabled = 1
; Ressourcenpaare nebeneinander je Block (1 bis 32)
columns_per_block = 6
; Blöcke untereinander (1 bis 4); Kapazität = Spalten × Blöcke
maximum_blocks = 2
; auto oder fester Anteil der Originalgröße 0.25 bis 1.00
button_scale = auto
; Abstand in der Reihe und zwischen Malen- und Löschen-Reihe, 1.00 = normal
horizontal_spacing = 1.00
vertical_spacing = 1.00
; zusätzlicher Abstand zwischen zwei Blöcken
block_gap = 0.35
; Versatz des ganzen Rasters, positiv = rechts bzw. unten
x_offset = 0.00
y_offset = 0.00
; 1 lässt das Fenster wachsen, 0 verkleinert die Schaltflächen passend
expand_window = 1
; below_grid oder original
cancel_position = below_grid
; sichtbare Abstände um den roten Alles-löschen-Knopf
grid_to_cancel_gap = 10.00
cancel_to_brush_gap = 40.00
; Feinkorrektur des roten Knopfs
cancel_x_offset = 0.00
cancel_y_offset = 0.00
```

Die beiden sichtbaren Abstände liegen so:

```
[Ressourcen-Schaltflächen]
       |  grid_to_cancel_gap
   [roter X-Knopf]
       |  cancel_to_brush_gap
     Pinsel
```

### Abschnitt `[rocks_window]`

```ini
[rocks_window]
; 1 ordnet das Felsen-/Kiesfenster neu, 0 behält das Originallayout
enabled = 1
; 1 = P E P E in einer Reihe, 2 = Malen-Reihe über Löschen-Reihe
blocks = 2
; auto oder fester Anteil der Originalgröße 0.25 bis 1.00
button_scale = auto
horizontal_spacing = 1.00
vertical_spacing = 1.00
; Versatz des ganzen Rasters
x_offset = 0.00
y_offset = 0.00
; 1 lässt das Fenster wachsen und rückt die Pinselsteuerung nach unten
expand_window = 1
; sichtbarer Abstand zwischen Raster und Pinsel bei wachsendem Fenster
controls_gap = 20.00
```

Die Einstellung `blocks` erzeugt diese Anordnungen (`P` = Malen, `E` = Löschen):

```
blocks = 1          blocks = 2
P E  P E            P P
                    E E
```

---

## 📏 Wertebereiche

Die DLL prüft diese Grenzen bei jedem Einlesen. Ein Wert außerhalb lehnt die ganze Datei ab: beim Start bleibt das Spiel unverändert, beim Live-Neuladen bleiben die vorherigen Werte aktiv.

| Schlüssel | Bereich | Standard |
|---|---|---|
| `enabled`, `debug`, `expand_window` | genau 0 oder 1 | 1 / 0 / 1 |
| `columns_per_block` | 1 bis 32 | 6 |
| `maximum_blocks` | 1 bis 4 | 2 |
| `blocks` | 1 oder 2 | 2 |
| `button_scale` | `auto` oder 0.25 bis 1.00 | auto |
| `horizontal_spacing`, `vertical_spacing` | 0.50 bis 2.00 | 1.00 |
| `block_gap` | 0.00 bis 2.00 | 0.35 |
| `x_offset`, `y_offset`, `cancel_x_offset`, `cancel_y_offset` | -100.00 bis 100.00 | 0.00 |
| `cancel_position` | `below_grid` oder `original` | below_grid |
| `grid_to_cancel_gap`, `cancel_to_brush_gap`, `controls_gap` | 0.00 bis 100.00 | 10.00 / 40.00 / 20.00 |
| Datei | höchstens 16 MiB, Werte höchstens 63 Zeichen | |

---

## 🔄 Live-Neuladen

Fast alle Einstellungen gelten ohne Neustart:

```
1. INI speichern (oder in Republic Mod Manager Speichern)
2. Das Fenster Resources oder Rocks/Gravel erneut öffnen
3. Beim ersten erkannten Werkzeugknopf wird die geänderte Datei eingelesen
```

Die Datei wird als vollständiger neuer Konfigurationssatz geprüft. Ist nur eine Zeile ungültig, werden **keine** neuen Werte übernommen; sämtliche zuletzt gültigen Einstellungen bleiben aktiv, der Grund steht im Log (`reload-rejected`). Nur `[general] enabled` braucht einen vollständigen Neustart; eine Änderung während des Spiels wird protokolliert, aber nicht angewendet.

---

## 📐 Automatische Skalierung

`button_scale = auto` berechnet eine passende Größe aus der erkannten Werkzeuganzahl, den Spalten und den Abständen. Die sichere Untergrenze ist `0.25`; muss das Plugin sie verwenden, erscheint eine Warnung im Log. Die Werkzeuganzahl stammt aus dem vorherigen vollständigen Fensterdurchlauf; neu hinzugekommene Werkzeuge werden ab dem folgenden Durchlauf stabil berücksichtigt. Überzählige Werkzeuge jenseits der Kapazität `columns_per_block × maximum_blocks` behalten ihre ursprüngliche Position und erzeugen eine einmalige Warnung.

---

## 💾 Kompatibilität

### Spiel und Loader
- Feste, für 1.1.1.9 geprüfte Codepositionen; eine andere Spielversion wird abgelehnt, bevor ein Hook installiert wird
- Vor jeder Änderung werden PE-Signatur, 64-Bit-Maschinentyp, Zeitstempel, Image-Größe, die Bytes des gemeinsamen Schaltflächen-Hooks und die Reichweite aller Sprünge geprüft
- Aufrufe aus Bäumen und anderen Fenstern werden unverändert weitergereicht; ein Laufzeitfehler schaltet die Layoutänderung für die Sitzung sicher ab
- Ohne nahen Speicher für die optionalen Übergänge bleibt die Anordnung aktiv, nur die nachfolgenden Bedienelemente behalten dann ihre Position

### Spielstände
Das Plugin verändert weder Spieldateien noch Spielstände; es ordnet nur Schaltflächen im Speicher des laufenden Spiels.

### Versionskompatibilität
- **0.4.0:** erste veröffentlichte Fassung

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| Plugin tut nichts | `enabled = 0`, Spielversion nicht 1.1.1.9 oder INI ungültig | Log auf `unsupported-build` oder `configuration` prüfen |
| Änderung wird nicht sichtbar | Fenster nicht neu geöffnet oder Live-Neuladen abgewiesen | Fenster schließen und öffnen; Log auf `reload-rejected` prüfen |
| Schaltflächen zu klein | automatische Untergrenze 0.25 erreicht | weniger Spalten, mehr Blöcke oder `expand_window = 1` |
| Werkzeuge fehlen im Raster | Kapazität überschritten | `columns_per_block` oder `maximum_blocks` erhöhen |
| Roter Knopf an falscher Stelle | `cancel_position` oder Abstände | `below_grid` mit `expand_window = 1`, Abstände anpassen |

### Logging

Alle Meldungen stehen in `tesmioloader.log` und im Detail-Log `logs\tesmioloader.resources_button_fix.log`. Jede Meldung nennt Stufe, Quelle und Regelname; Initialisierung und Start enden mit einer Zusammenfassung.
- In **Republic Mod Manager** öffnet das Symbol mit dem Dokument unten in der Plugin-Leiste die Protokollansicht mit Filter und Absender.

Suche nach:
- `Configuration file` → welche INI die DLL gewählt hat
- `configuration`, `unknown-key`, `config-range`, `duplicate-key` → abgelehnte INI
- `reload-rejected` → Live-Änderung verworfen, vorherige Werte aktiv
- `hook-install`, `near-memory`, `layout-fault` → Hook oder Layoutlogik

---

## 📦 Dateistruktur

**Workshop-Paket** (Steam-Abo, SML, Workshop Bridge)
```
resources_button_fix\
├── hooks\
│   ├── resources_button_fix.dll    (Plugin)
│   └── resources_button_fix.ini    (Original-INI)
├── config\                         (Launcher-Schema für Republic Mod Manager)
│   ├── resources_button_fix.launcher.ini
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
├── plugins\
│   ├── resources_button_fix.dll
│   └── resources_button_fix.ini    (wirksame INI)
└── user_config\
    └── resources_button_fix.ini    (persönliche Werte aus Republic Mod Manager)
```

Über die Bridge oder SML liegt in `plugins` nur die wirksame INI, die DLL bleibt im Paket. Ohne Republic Mod Manager liest die DLL die INI direkt aus dem Paket.

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Das Plugin enthält keinen fremden Code; der Loader-SDK-Header stammt aus dem TesmioLoader von MaxLegend (GPL v3). Der vollständige Quelltext liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/resources_button_fix.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Werden auch die Baumwerkzeuge neu angeordnet?**
A: Nein. Nur die Fenster Resources und Rocks/Gravel; alle anderen Editorfenster bleiben unverändert.

**F: Muss ich das Spiel nach jeder Änderung neu starten?**
A: Nein. Nur der Plugin-Schalter braucht einen Neustart; alles andere gilt beim nächsten Öffnen des Fensters.

**F: Was passiert bei einem Fehler in der INI?**
A: Beim Start bleibt das Spiel unverändert, beim Live-Neuladen bleiben die vorherigen Werte aktiv. Der Grund steht im Log.

**F: Funktioniert das Plugin mit Deposits Plus?**
A: Ja. Zusätzliche Werkzeuge aus deposits.dll oder Deposits Plus werden im jeweils richtigen Fenster mit eingeordnet.

**F: Muss ich die INI von Hand bearbeiten?**
A: Nein. Republic Mod Manager zeigt alle Einstellungen mit Beschreibung und prüft die Wertebereiche.

---

**Letzte Aktualisierung:** Resources Button Fix 0.4.0  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
