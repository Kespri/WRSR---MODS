# 🚚 Vehicle Materials 0.4.0

**TesmioLoader-Plugin für zusätzliche Fahrzeugmaterialien**

Frei konfigurierbare Zusatzmaterialien für die Fahrzeugproduktion in *Workers & Resources: Soviet Republic* 1.1.1.9: Glas, Kabel, Kupfer oder jede andere Ressource, die das Resources-Plugin registriert, getrennt einstellbar für Straße, Schiene, Schiff und Flugzeug.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Produktionsgebäude vorbereiten](#-produktionsgebäude-vorbereiten)
- [Konfiguration](#-konfiguration)
- [Wertebereiche](#-wertebereiche)
- [Fahrzeugtypen zuordnen](#-fahrzeugtypen-zuordnen)
- [Verhältnis zu Resources](#-verhältnis-zu-resources)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Pflicht: Resources-Plugin (resources.dll/resources.ini) mit allen Ressourcen, die du als Fahrzeugmaterial verwenden willst. Ohne Resources startet Vehicle Materials nicht, der Grund steht im Log.

### In drei Schritten
1. **Ressourcen registrieren:** Jedes Material muss in `plugins\resources.ini` stehen, zum Beispiel `glass` oder `cable`. In Republic Mod Manager geht das über den Eintrag Resources.
2. **Eine Installationsmethode wählen** (siehe unten), das Plugin aktivieren und die Materialien mit ihren Koeffizienten eintragen, am bequemsten über Republic Mod Manager.
3. **Produktionsgebäude anpassen:** Jede Fahrzeugfabrik braucht ein Lager für das neue Material (siehe [Produktionsgebäude vorbereiten](#-produktionsgebäude-vorbereiten)). Dann Spiel starten.

---

## ✨ Features

### 🎯 Grundfunktion
- ✅ Zusätzliche Materialien für den Bau von Fahrzeugen, ohne die Spieldateien zu verändern
- ✅ Die ursprünglichen Materialanforderungen des Spiels bleiben erhalten
- ✅ Bis zu 32 Materialien, jedes muss durch Resources registriert sein
- ✅ Die Konfiguration wird beim Start komplett geprüft; bei einem Fehler bleibt das Plugin aus und das Spiel läuft wie gewohnt

### 🆕 Einstellmöglichkeiten

#### 1️⃣ **Getrennte Koeffizienten je Fahrzeugklasse** (`[road]`, `[rail]`, `[ship]`, `[airplane]`)
- Ein Wert je Material und Klasse, `0` oder fehlend schaltet das Material für die Klasse ab
- Das Spiel multipliziert den Koeffizienten mit dem internen Produktionswert des Fahrzeugs: größere Fahrzeuge brauchen mehr

#### 2️⃣ **Persönliches Overlay** (`user_overlay = 1`)
- Die DLL liest zuerst ihre normale INI, dann `build\user_config\vehicle_materials.ini` Schlüssel für Schlüssel darüber
- Republic Mod Manager schreibt nur das Overlay; die Original-INI bleibt unangetastet, egal ob die DLL aus `plugins`, aus SML oder über die Workshop Bridge geladen wird

#### 3️⃣ **Zuordnung der Fahrzeugtypen** (`[mapping]`)
- Automatisch: Typ 1 Straße, Typ 6 Schiff, Typ 7 Flugzeug, alle anderen Schiene
- Bei Bedarf je internem Typ 0 bis 15 von Hand übersteuerbar

---

## 💾 Installation

Wähle **eine** der vier Methoden. Dieselbe DLL darf nie zweimal geladen werden. Resources muss in jedem Fall installiert und eingeschaltet sein.

---

### Methode 1️⃣: Klassischer TesmioLoader

```
1. Kopiere vehicle_materials.dll und vehicle_materials.ini aus hooks\
   → tesmioloader\build\plugins\

2. Aktiviere vehicle_materials im TesmioLauncher
3. Prüfe, dass resources aktiviert ist und die Materialien in resources.ini stehen
```

---

### Methode 2️⃣: Soviet Mod Loader (SML)

```
1. Workshop-Objekt abonnieren – SML liest abonnierte Pakete von selbst
2. SML lädt die DLL über soviet.mod.ini aus dem Paket, die INI liegt daneben
3. Die eingetragenen Ressourcen müssen auch in der SML-Konfiguration existieren
4. Eine lokale vehicle_materials.dll in plugins\ vorher entfernen oder abschalten
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
1. Paket in Republic Mod Manager auswählen, Materialien mit + eintragen, Speichern
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

Das Paket enthält im Ordner `config` ein Launcher-Schema. Republic Mod Manager zeigt Vehicle Materials damit in vier Reitern, deutsch und englisch:

- **Allgemein:** Hinweise, „Dateien nur lokal“ und die Diagnose-Einstellungen
- **Ressourcen:** die Materialliste; der Plus-Knopf bietet nur Ressourcen an, die Resources registriert hat, und legt für jede Fahrzeugklasse den Koeffizienten an; der Papierkorb entfernt Material und Koeffizienten wieder
- **Fahrzeugklassen:** die Koeffizienten als Tabelle Material mal Klasse
- **Erweiterte Zuordnung:** die Fahrzeugtypen 0 bis 15

Der Schalter „Plugin aktiv“ im Kopf setzt beim Einschalten auch `enabled = 1`. Persönliche Werte liegen in `user_config\vehicle_materials.ini`; die ausgelieferte INI bleibt unverändert. Wer die INI lieber von Hand bearbeitet, findet alles Weitere unten.

---

## 🏗️ Produktionsgebäude vorbereiten

⚠️ **Jedes neue Material braucht in jeder Fahrzeugfabrik, die damit bauen soll, eine eigene Lagerzeile.** Sonst wird es weder geliefert noch angenommen. Das Plugin ergänzt nur den Materialbedarf des Fahrzeugs, es legt keine Lager an. Die Fabriken kennen von Haus aus die Transportklassen **COVERED** und **OPEN**; die Transportklasse der neuen Zeile muss zur Ressource passen (siehe Resources-Editor, Spalte Transportklasse). Für die Lagerzeile stehen drei Wege offen:

1. **Gebäude kopieren** und die Lagerzeilen von Hand eintragen, zum Beispiel mit dem TesmioLoader-Plugin Buildings.
2. **Vanilla Buildings** aus dem Workshop: fügt die Lagerzeilen den Gebäuden des Spiels hinzu.
3. **Manueller Eintrag** in der `building.ini` des Gebäudes.

In allen drei Fällen ist es dieselbe `$STORAGE_IMPORT_SPECIAL`-Zeile, eine je Material:

```ini
$STORAGE_IMPORT_CARPLANT RESOURCE_TRANSPORT_COVERED 250
$STORAGE_IMPORT_CARPLANT RESOURCE_TRANSPORT_OPEN 300
$STORAGE_EXPORT RESOURCE_TRANSPORT_VEHICLES 15
; neu dazu:
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN 100 glass
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN 50 cable
```

- `100` und `50` sind die Lagerkapazität
- `glass` und `cable` sind die genauen Ressourcen-IDs aus resources.ini
- der Transporttyp muss zur Transportklasse der Ressource passen; im Beispiel sind glass und cable als OPEN registriert

Das gilt für die Fabriken für Straßenfahrzeuge, Schienenfahrzeuge, Schiffe und Flugzeuge. Die Gebäude lassen sich über einen eigenen Gebäudemod oder ein TesmioLoader-Plugin anpassen. Prüfe im Spiel, ob die neuen Lagerplätze am Gebäude erscheinen und beliefert werden.

---

## ⚙️ Konfiguration

### Hauptdatei: `vehicle_materials.ini`

Die DLL liest in dieser Reihenfolge:
- **Basis:** `tesmioloader\build\plugins\vehicle_materials.ini`, falls vorhanden, sonst die INI neben der DLL (im Paket `hooks\vehicle_materials.ini`)
- **Overlay:** `tesmioloader\build\user_config\vehicle_materials.ini`, Schlüssel für Schlüssel darüber; hierhin schreibt Republic Mod Manager

⚠️ **Kommentare nur in eigenen Zeilen mit `;`.** Ein Kommentar hinter dem Wert wird als Teil des Wertes gelesen und lehnt die ganze Konfiguration ab. Die Datei muss UTF-8 ohne BOM sein; alle sieben Abschnitte müssen vorhanden sein, auch leere; unbekannte oder doppelte Abschnitte und Schlüssel, leere Werte und ungültige Zahlen lehnen die ganze Konfiguration ab. Kein Neuladen zur Laufzeit: INI ändern, Spiel neu starten.

### Abschnitt `[general]`

```ini
[general]
; 1 aktiviert das Plugin, 0 schaltet es ab; nur genau 0 oder 1
enabled = 0
; 1 schreibt Materialien und Fahrzeugbauten ins Detail-Log tesmioloader.vehicle_materials.log
debug = 0
; Obergrenze für wiederholte Warnungen und Diagnosemeldungen je Sitzung, 0 unterdrückt sie
debug_limit = 80
```

### Abschnitt `[resources]` und die vier Fahrzeugklassen

Beispiel mit zwei Materialien, so wie Republic Mod Manager es anlegt:

```ini
[resources]
; Anzahl der Einträge resource0, resource1, ...
count = 2
; genaue Ressourcen-ID aus resources.ini, jede nur einmal
resource0 = glass
resource1 = cable

[road]
; Koeffizient je Material; 0 oder fehlend = kein Bedarf in dieser Klasse
glass = 0.020
cable = 0.010

[rail]
glass = 0.030
cable = 0.015

[ship]
glass = 0.010
cable = 0.005

[airplane]
glass = 0.015
cable = 0.020

[mapping]
; -1 = automatisch, 0 = Straße, 1 = Schiene, 2 = Schiff, 3 = Flugzeug
type0 = -1
type1 = -1
type2 = -1
type3 = -1
type4 = -1
type5 = -1
type6 = -1
type7 = -1
type8 = -1
type9 = -1
type10 = -1
type11 = -1
type12 = -1
type13 = -1
type14 = -1
type15 = -1
```

Dabei gilt:
- die Nummerierung beginnt bei `resource0`, `count` muss zur Anzahl passen, Einträge ab `count` werden abgewiesen
- Namen sind ohne Groß- und Kleinschreibung eindeutig und dürfen keine Leerzeichen, Pfadzeichen oder Sonderzeichen enthalten
- ein Schlüssel in einer Fahrzeugklasse muss unter `[resources]` stehen
- ein Material ohne positiven Wert in mindestens einer Klasse wird ignoriert; mit `enabled = 1` verlangt Republic Mod Manager mindestens ein solches Material
- ein Material, das ein Fahrzeug bereits verlangt, wird nicht ein zweites Mal hinzugefügt

Beginne mit kleinen Werten und kontrolliere die Mengen im Spiel.

---

## 📏 Wertebereiche

Die DLL prüft diese Grenzen beim Start. Ein Wert außerhalb lehnt die ganze Konfiguration ab; das Plugin installiert dann keinen Hook, das Spiel läuft mit seinen ursprünglichen Materialien.

| Schlüssel | Bereich | Standard |
|---|---|---|
| `enabled`, `debug` | genau 0 oder 1 | 0 / 0 |
| `debug_limit` | 0 bis 10000 | 80 |
| `count` | 0 bis 32 | 0 |
| `resource0` … `resource31` | ID aus resources.ini, höchstens 63 Zeichen | keine |
| Koeffizienten in `[road]`, `[rail]`, `[ship]`, `[airplane]` | endliche Zahl 0 bis 1000000 | keine |
| `type0` … `type15` | -1 bis 3 | -1 |
| Datei | höchstens 1 MiB, Schlüssel und Werte höchstens 63 Zeichen | |

---

## 🔀 Fahrzeugtypen zuordnen

Das Spiel führt Fahrzeuge unter internen Produktionstypen. Das Plugin ordnet sie so zu:

| Interner Typ | Klasse |
|---|---|
| 1 | Straße |
| 6 | Schiff |
| 7 | Flugzeug |
| alle anderen | Schiene |

`[mapping]` übersteuert das je Typ 0 bis 15:

| Wert | Klasse |
|---|---|
| -1 | automatisch |
| 0 | Straße |
| 1 | Schiene |
| 2 | Schiff |
| 3 | Flugzeug |

Normalerweise bleibt der Abschnitt unverändert. Mit `debug = 1` steht im Detail-Log, welcher Typ welcher Klasse zugeordnet wurde.

---

## 🔗 Verhältnis zu Resources

Vehicle Materials legt keine Ressource an. Es fragt beim Start den Registrierungsdienst des Resources-Plugins und verwendet nur Namen, die dort veröffentlicht sind:

- fehlt Resources oder ist es abgeschaltet, startet Vehicle Materials nicht, der Grund steht im Log
- ein Material, das in resources.ini fehlt, lehnt die Konfiguration ab
- der Name muss in beiden INIs exakt gleich geschrieben sein

Wird ein Material in resources.ini entfernt, bereinigt Republic Mod Manager nach Rückfrage auch die Einträge in Vehicle Materials.

---

## 💾 Kompatibilität

### Spielstände

Das Plugin verändert keine Spieldateien und speichert nichts im Spielstand. Materialien, die Fahrzeuge verlangen, sind Teil der Ressourcenliste des Spielstands: Eine Ressource, die bereits in einem Spielstand verwendet wird, darf in resources.ini nicht mehr entfernt werden.

### Versionskompatibilität

- **0.4.0:** erste veröffentlichte Fassung; die Konfiguration wird als Basis plus persönliches Overlay gelesen

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| Plugin startet nicht | Resources fehlt oder ist abgeschaltet | resources.dll installieren und im Launcher einschalten |
| Konfiguration abgelehnt | Material nicht in resources.ini, Kommentar hinter einem Wert, Wert außerhalb des Bereichs | Log lesen, Namen und Werte korrigieren |
| Fahrzeug verlangt das Material nicht | `enabled = 0` oder Koeffizient 0 in dieser Klasse | „Plugin aktiv“ einschalten, Koeffizient setzen |
| Fabrik nimmt das Material nicht an | kein `$STORAGE_IMPORT_SPECIAL` im Gebäude | Gebäude anpassen (siehe oben) |
| Falsche Fahrzeugklasse | interner Typ anders als erwartet | `debug = 1`, Zuordnung in `[mapping]` setzen |

### Logging

Alle Meldungen stehen in `tesmioloader.log`, mit `debug = 1` zusätzlich in `logs\tesmioloader.vehicle_materials.log`.
- In **Republic Mod Manager** öffnet das Symbol mit dem Dokument unten in der Plugin-Leiste die Protokollansicht mit Filter und Absender.

Erwartet bei erfolgreichem Start:
```
vehicle_materials  Hook active at SOVIET64.exe+0x...
vehicle_materials  Automatic mapping: type1=road, type6=ship, type7=airplane, other=rail
vehicle_materials  v0.4.0 ready
```

Suche nach:
- `vehicle_materials` → alle Meldungen des Plugins
- `FATAL` → Ablehnungen mit Ursache und Handlungsempfehlung
- `Personal overlay applied` → das Overlay aus user_config wurde gelesen

---

## 📦 Dateistruktur

**Workshop-Paket** (Steam-Abo, SML, Workshop Bridge)
```
vehicle_materials\
├── hooks\
│   ├── vehicle_materials.dll       (Plugin)
│   └── vehicle_materials.ini       (Original-INI, ohne Materialien, enabled = 0)
├── config\                         (Launcher-Schema für Republic Mod Manager)
│   ├── vehicle_materials.launcher.ini
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
│   ├── resources.dll               (Pflicht, eigenes Plugin)
│   ├── resources.ini
│   ├── vehicle_materials.dll
│   └── vehicle_materials.ini       (Original-INI)
└── user_config\
    └── vehicle_materials.ini       (persönliche Werte aus Republic Mod Manager)
```

Über die Bridge oder SML liegt in `plugins` nur die Original-INI, die DLL bleibt im Paket. Die persönlichen Werte liegen in jedem Fall in `user_config`.

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Das Plugin enthält keinen fremden Code; der Loader-SDK-Header stammt aus dem TesmioLoader von MaxLegend (GPL v3). Der vollständige Quelltext liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/vehicle_materials.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Kann ich Materialien verwenden, die nicht in resources.ini stehen?**
A: Nein. Vehicle Materials verwendet nur Ressourcen, die das Resources-Plugin registriert hat.

**F: Warum liefert niemand das Material an die Fabrik?**
A: Die Fabrik braucht ein eigenes Lager dafür, eine `$STORAGE_IMPORT_SPECIAL`-Zeile in ihrer building.ini. Das Plugin legt das Lager nicht an.

**F: Werden die ursprünglichen Materialien ersetzt?**
A: Nein. Stahl, mechanische Bauteile und alles andere bleiben; die neuen Materialien kommen dazu.

**F: Was passiert bei einem Fehler in der INI?**
A: Das ganze Plugin wird abgelehnt und installiert keinen Hook. Das Spiel baut Fahrzeuge wie ohne Plugin, der Grund steht im Log.

**F: Muss ich die INI von Hand bearbeiten?**
A: Nein. Republic Mod Manager bietet die Materialien aus resources.ini mit + an, legt die Koeffizienten je Klasse an und prüft die Wertebereiche.

---

**Letzte Aktualisierung:** Vehicle Materials 0.4.0  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
