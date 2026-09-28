# 🏭 Deposits Plus 0.4.10

**Erweiterung des TesmioLoader-Plugins deposits**

Vollständig konfigurierbare Rohstoffvorkommen für *Workers & Resources: Soviet Republic* 1.1.1.9 mit natürlicher Verteilung, visueller Bodentextur und Fahrzeugintegration.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Wertebereiche](#-wertebereiche)
- [Häufigkeit & Größe](#-häufigkeit--größe)
- [Natürliche Verteilung](#-natürliche-verteilung)
- [Sandige Wiese](#-sandige-wiese-visuelle-textur)
- [Arbeitsfahrzeuge](#-arbeitsfahrzeuge)
- [Verhältnis zum Original-Plugin](#-verhältnis-zum-original-plugin)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Empfohlen: Resources-Plugin (resources.dll/resources.ini) – nur nötig, damit der Minimap-Knopf eines Vorkommens das Symbol einer dort registrierten Ressource zeigen kann (`icon`). Die Vorkommen selbst brauchen es nicht.

### In drei Schritten
1. **Original deaktivieren:** Falls `plugins\deposits.dll` vorhanden ist, in Republic Mod Manager „Plugin aktiv“ bei deposits ausschalten oder im TesmioLauncher das Häkchen entfernen. Löschen ist nicht nötig.
2. **Eine Installationsmethode wählen** (siehe unten) und das Plugin aktivieren.
3. **Spiel starten** und einen Spielstand laden. Die Einstellungen stehen in `deposits_plus.ini`, am bequemsten über Republic Mod Manager.

---

## ✨ Features

### 🎯 Originalfunktionen (erhalten)
- ✅ Benutzerdefinierte Vorkommenstypen (nicht im Originalspiel vorhanden)
- ✅ Vollständig INI-gesteuert: Typen, Minikarte, Editor-Pinsel
- ✅ Code-Patch für neue Mineraltypen

### 🆕 Erweiterungen (neu in Deposits Plus)

#### 1️⃣ **Natürliche Verteilung** (`generation`)
- Längliche, verzweigte Vorkommensfelder statt Rasterpunkte
- Auslaufende Ergiebigkeit (reich → schwach → leer)
- Automatische Anpassung an Landesgrenze und Wasserläufe
- Keine Überschneidung mit anderen Vorkommen
- Zufälliger oder fester Seed je Spielwelt

#### 2️⃣ **Sandige Wiese** (`sand_surface`)
- Visuelle Bodentextur auf Sandvorkommen
- Stärke mit Ergiebigkeit skaliert (reiche Felder deutlicher)
- Kacheltabelle je Bodentextur: Sets Vanilla, Siberia (Sommer und Schneeherbst), Asia - Jungle und Ultimate Vanilla +; eigene DDS-Dateien in eigenen Ordnern möglich
- Rein optisch, kein Gameplay-Effekt
- Wüstenkarten: `desert_fill` macht die ganze Landfläche zum Sandvorkommen, organisch mit 60 bis 100 % im Tiefland und abnehmend mit der Höhe; die Erze finden trotzdem Platz

#### 3️⃣ **Arbeitsfahrzeuge** (`working_vehicle_skill`)
- Vorkommen können die Fahrzeugfreigabe des Kiesabbaus übernehmen
- Beispiel: Sand mit `gravelmining`
- Arbeitet mit den vorhandenen Fahrzeugfähigkeiten des Spiels

---

## 💾 Installation

Wähle **eine** der vier Methoden. Dieselbe DLL darf nie zweimal geladen werden, und Deposits Plus und das Original-Plugin deposits laufen nie gleichzeitig.

### ⚠️ Wichtig: Originales Plugin deaktivieren

Falls `plugins\deposits.dll` existiert:
- **Republic Mod Manager:** „Plugin aktiv“ bei deposits ausschalten
- **TesmioLauncher:** das Häkchen entfernen
- **Löschen ist nicht nötig** – solange das Original eingeschaltet ist, bleibt Deposits Plus von selbst untätig (Logzeile `deposits_plus  idle`)

---

### Methode 1️⃣: Klassischer TesmioLoader

```
1. Kopiere deposits_plus.dll und deposits_plus.ini aus hooks\
   → tesmioloader\build\plugins\

2. Kopiere den Ordner hooks\deposits_plus (die Texturen)
   → tesmioloader\build\plugins\deposits_plus\

3. Aktiviere deposits_plus im TesmioLauncher
4. Deaktiviere das Original-Plugin deposits
```

---

### Methode 2️⃣: Soviet Mod Loader (SML)

⚠️ **Solange SML läuft, bleibt Deposits Plus untätig.** SML bringt seine eigenen Vorkommen mit; das Plugin lädt, tritt zur Seite und schreibt eine Zeile ins Protokoll. Willst du Deposits Plus, schalte `soviet_mod_loader` in der `tesmioloader.ini` aus (im Republic Mod Manager der Schalter „Plugin aktiv") und nimm Methode 1 oder 3. Der Weg unten lohnt nur, wenn du das Paket für später bereithalten willst.

```
1. Workshop-Objekt abonnieren – SML liest abonnierte Pakete von selbst
2. SML lädt die DLL über soviet.mod.ini aus dem Paket
3. Die Texturen findet die DLL neben sich im Paket
4. Eine lokale deposits_plus.dll in plugins\ vorher entfernen oder abschalten
```

---

### Methode 3️⃣: Workshop Bridge (ohne SML)

```
1. Paket in Republic Mod Manager auswählen
2. „Plugin aktiv“ einschalten
3. workshop_bridge lädt die DLL direkt aus dem Paket
4. Die Texturen findet die DLL neben sich im Paket
5. Steam-Updates gelten sofort
```

---

### Methode 4️⃣: Republic Mod Manager mit „Dateien nur lokal“

```
1. Paket in Republic Mod Manager auswählen, Einstellungen anpassen, Speichern
2. Reiter „Allgemein“, Karte „Hinweise“: „Dateien nur lokal“ einschalten
3. Bestätigung mit Dateiliste → DLL, INI und Texturordner werden beim
   Speichern nach tesmioloader\build\plugins\ kopiert
4. Die Workshop Bridge überspringt das Paket danach automatisch
```

**„Dateien nur lokal“ im Detail**
- Für alle, die das Plugin ohne Steam-Abo weiterbenutzen wollen
- Steam-Updates gelten bei lokalen Dateien erst nach erneutem Speichern (gelbe Marke „Update“)
- Ausschalten entfernt nur die von Republic Mod Manager kopierten Dateien wieder

---

## 🧰 Republic Mod Manager

Das Paket enthält im Ordner `config` ein Editor-Schema. Republic Mod Manager (ab Version 0.4.0) zeigt Deposits Plus damit in drei Reitern, deutsch und englisch:

- **Allgemein:** Hinweise, „Dateien nur lokal“, Knöpfe für diese Anleitung und die Plugin-Schalter Code-Patch, Minimap-Ebenen, Editor-Pinsel
- **Sand-Struktur:** Sandige Wiese und Natürliche Verteilung
- **Sand-Texturen:** die Kacheltabelle, je Bodentextur ein Eintrag mit Farb- und Normalmap-Datei
- **Vorkommen:** Liste links, alle Einstellungen des gewählten Vorkommens rechts, Plus-Knopf für neue Vorkommen mit Ressourcen aus resources.ini und automatisch vorgeschlagener Typnummer

Persönliche Änderungen liegen in `user_config\deposits_plus.editor.ini`, die wirksame Datei ist `plugins\deposits_plus.ini`. Die INI im Paket bleibt unverändert; ein Steam-Update ist sofort die neue Originalbasis. Wer die INI lieber von Hand bearbeitet, findet alles Weitere unten.

---

## ⚙️ Konfiguration

### Hauptdatei: `deposits_plus.ini`

Die Original-INI liegt:
- **Klassischer Loader:** `tesmioloader\build\plugins\deposits_plus.ini`
- **Workshop/SML/Bridge:** im Paketordner unter `hooks\deposits_plus.ini`; Republic Mod Manager schreibt die wirksame Fassung nach `plugins\deposits_plus.ini`

⚠️ **Kommentare nur in eigenen Zeilen mit `;`.** Der Parser nimmt alles hinter `=` als Wert. Ein Kommentar hinter dem Wert macht Token unbrauchbar und schaltet die Verteilung ab („generation disabled for safety“ im Log). Kein Neuladen zur Laufzeit: INI ändern, Spiel neu starten, Spielstand laden.

### Abschnitt `[deposits_plus]` (globale Schalter)

```ini
[deposits_plus]
; Code- und UI-Patches
; 1 trägt die neuen Vorkommenstypen in die Spieldatei ein (Pflicht für alles Weitere)
code_patch = 1
; 1 fügt je Vorkommen einen Minimap-Knopf und eine Overlay-Ebene hinzu (Vorschau beim Überfahren, fest per Klick)
minimap = 1
; 1 fügt je Vorkommen ein Mal-/Lösch-Paar im Terrain-Editor hinzu
editor = 1

; Natürliche Verteilung
; 1 prüft nach jedem Laden einer Welt alle Vorkommenstypen und verteilt die noch leeren
generation = 1
; 1 füllt auch Kanäle, die früher als leer erfasst wurden; 0 schützt alte leere Kanäle
generate_existing_empty = 1
; 0 = zufälliger Seed je neuer Welt; jede andere Zahl = immer dieselbe Verteilung
generation_seed = 0

; Abstände
; Mindestabstand zwischen einem neuen Feld und jedem anderen Vorkommen (Meter)
generation_gap_m = 40
; Mindestabstand zwischen einem neuen Feld und Wasser (Meter)
generation_shore_m = 40
; Höhe, die das Gelände in einer Zelle über dem Wasserspiegel liegen muss (Meter)
generation_water_clearance_m = 2
; 1 = auch die Kies-/Felszone des Geländes gilt als belegt (auf Gebirgskarten wie Siberia bleibt dann kaum Platz)
generation_block_gravel = 0

; Sandige Wiese (optional)
; 1 = an, 0 = aus
sand_surface = 1
; Stärke 0 bis 1; 1.0 = volles Bild, kleinere Werte blenden aus
sand_surface_strength = 1.0
; Token des Vorkommens, dem die Bodendarstellung folgt
sand_surface_token = $TYPE_MINE_SAND
```

### Pro Vorkommen: ein eigener Abschnitt

Der Abschnittsname ist der Name des Vorkommens; `deposits_plus` ist der einzige verbotene Name. Beispiel Sand, so wie ausgeliefert, mit allen möglichen Schlüsseln:

```ini
[sand]
; Pflicht: das Token in der building.ini der Mine
token = $TYPE_MINE_SAND
; Pflicht: Typnummer 10 bis 127, keine doppelt (0 bis 9 gehören dem Spiel)
type = 11
; auto, resourcemap, resourcemap2 ... resourcemap10 oder terrain
map = terrain
; Farbkanal 0 bis 3 der Karte; Pflicht, außer bei map = auto
component = 1
; 1 legt einen eigenen Ressourcenkanal an und kopiert vorhandenen Sand dorthin
independent_map = 1
; 1 = auf Wüstenkarten ($TYPE_DESERT in der script.ini) ist beim ersten Anlegen die ganze Landfläche dieses Vorkommen; sperrt die anderen Vorkommen nicht
desert_fill = 1
; 1 = Ergiebigkeit nimmt mit der Höhe ab (volles Band im Tiefland, 0 auf den höchsten Erhebungen), 0 = Band überall
desert_fill_relief = 1
; Ergiebigkeitsband in Prozent im Tiefland, per Rauschen variiert
desert_fill_min = 60
desert_fill_max = 100
; 7 = Mine (Standard), 92 = Wasserbrunnen
building_type = 7
; Suchradius der Mine: oil, ore, bauxite, gravel, wood, water, watersurface oder eine Zahl in Metern
radius = gravel
; Ressource, deren Symbol der Minimap-Knopf zeigt (leer = kein Symbol)
icon = sand
; 1 = Minimap-Knopf und Ebene (Überfahren zeigt sie kurz, Klick hält sie), 0 = keine
minimap = 1
; Name des Editor-Pinsels, höchstens 31 Zeichen (leer = kein Pinsel)
editor = sand
; Nur für das Plugin depletion: Tonnen je gesättigtem Texel, 0 = unendlich, leer = globaler Wert
; deplete = 500

; Natürliche Verteilung
; 1 verteilt dieses Vorkommen einmalig, 0 verhindert nur die Erstverteilung
generation = 1
; Häufigkeit 1 bis 6 (siehe Tabelle), Standard 3
generation_frequency = 5
; Größenklasse 1 bis 3 (siehe Tabelle), Standard 2
generation_size = 3
; Ergiebigkeit 0.001 bis 1, unabhängig von den Voreinstellungen
; generation_richness_min = 0.45
; generation_richness_max = 1.00

; Arbeitsfahrzeuge: gravelmining oder none; braucht building_type = 7
working_vehicle_skill = gravelmining
```

Weitere Vorkommen aus der ausgelieferten INI: copper (Typ 10), clay (Typ 12) und gas (Typ 13). Ein Vorkommen wird abgewiesen und im Log gemeldet bei fehlendem Token, Typnummer außerhalb 10 bis 127, unbekannter Karte, doppelter Typnummer, doppeltem Token, doppeltem Kanal oder unbrauchbarem Radius.

---

## 📏 Wertebereiche

Die DLL prüft diese Grenzen. Ein Wert außerhalb schaltet die Verteilung für das Vorkommen (Abschnitt) oder für alle (Abschnitt `[deposits_plus]`) ab und schreibt „generation WARN … generation disabled for safety“ ins Log.

| Schlüssel | Bereich | Standard |
|---|---|---|
| `generation`, `generate_existing_empty`, `independent_map` | 0 oder 1 | 1 / 1 / 0 |
| `generation_seed` | 0 bis 4294967295, ganzzahlig | 0 |
| `generation_gap_m`, `generation_shore_m` | 0 bis 500 m | 40 / 40 |
| `generation_water_clearance_m` | 0 bis 50 m | 2 |
| `generation_block_gravel` | 0 oder 1 | 0 |
| `generation_frequency` | 1 bis 6 | 3 |
| `generation_size` | 1 bis 3 | 2 |
| `generation_richness_min`, `_max` | 0.001 bis 1 | 0.45 / 1.00 |
| `sand_surface_strength` | 0 bis 1, sonst Bodendarstellung aus | 1.0 |
| `type` | 10 bis 127 | Pflicht |
| `component` | 0 bis 3 | Pflicht, außer bei auto |
| `editor` | höchstens 31 Zeichen | leer |

---

## 📊 Häufigkeit & Größe

### Häufigkeitsstufen (`generation_frequency`)

| Wert| Häufigkeit        | Anzahl Felder |
|-----|-------------------|---------------|
|  1  | extrem selten     |   1    |
|  2  | selten            |   2    |
|  3  | normal (Standard) |   3    |
|  4  | häufig            |   4    |
|  5  | sehr häufig       |   5    |
|  6  | extrem häufig     |   6    |

### Feldgrößen (`generation_size`)

|Wert| Größe             | Grundradius |
|----|-------------------|----------------|
| 1  | klein             |   150–350 m    |
| 2  | mittel (Standard) |   350–550 m    |
| 3  | groß              |   550–750 m    |

**Hinweis:** Die Größe skaliert das ganze Feld, nicht einzelne Punkte. Felder sind länglich und verzweigt, keine Kreise; getrennte Teilflächen zählen als ein Feld. Entlang seiner Hauptachse reicht ein Feld etwa über das 3,4- bis 4,6-Fache des Grundradius, große Felder brauchen entsprechend Platz.

---

## 🌍 Natürliche Verteilung

### Wie funktioniert es?

1. **Beim Laden:** Das Plugin vergleicht alle Vorkommen der INI anhand ihrer Token mit der Historie des Spielstands
2. **Neue Vorkommen:** werden einmalig verteilt, beim ersten Zeichnen des Geländes nach dem Laden
3. **Bestehende:** bleiben erhalten, auch nach vollständigem Abbau; nichts wird nachgefüllt
4. **Form:** längliche, verzweigte Felder mit auslaufender Ergiebigkeit

### Regeln

✅ **Erlaubt:**
- Langgestreckte Felder mit unregelmäßigen Rändern
- Gelegentliche Verzweigungen
- Echte Lücken und graduell sinkende Ergiebigkeit
- Mehrere getrennte Teilflächen pro Feld

❌ **Nicht erlaubt:**
- Platzierung im Wasser
- Weniger Abstand zum Ufer als `generation_shore_m`
- Überschneidung mit anderen Vorkommen, auch mit Öl, Eisen, Kohle, Uran und Bauxit des Spiels; die Kies-/Felszone nur mit `generation_block_gravel = 1`
- Verlassen der Landesgrenze (ohne Grenzpolygon gelten die rechteckigen Baugrenzen)

Ein Feld zählt nur, wenn nach dem Beschneiden mindestens 60 % seiner Fläche übrig bleiben. Reicht die Fläche nicht, entstehen weniger oder keine Felder; das Log nennt Ziel, Ergebnis und Ablehnungsgründe.

### Erstes Laden eines alten Spielstands

⚠️ **Wichtig: Alte Spielstände sicher übernehmen**

```
1. Sicherheitskopie des kompletten Spielstandordners anlegen
2. Die bisherigen Vorkommen in der INI NICHT entfernen oder umsortieren
   (alte Spielstände ohne Zuordnungsliste brauchen die Reihenfolge)
3. Spielstand laden → unter neuem Namen speichern
   → tesmio_deposits.bin wird angelegt
4. Ab dem zweiten Laden zählt nur noch das Token, die Reihenfolge ist egal
```

### Neues Vorkommen hinzufügen

```
1. Spielstand speichern
2. Neuen Abschnitt in deposits_plus.ini mit eigenem token und eigener type anlegen
   (in Republic Mod Manager: Reiter Vorkommen, Plus-Knopf)
3. Spiel komplett neu starten (Laden aus dem Hauptmenü reicht nicht)
4. Spielstand laden → nur das neue Vorkommen wird verteilt
5. Speichern
```

---

## 🏞️ Sandige Wiese (Visuelle Textur)

### Was macht es?

Blendet eine sandig-fleckige Bodentextur über Sandvorkommen ein, deutlicher bei reichen Vorkommen. **Rein optisch – kein Gameplay-Effekt.** Spielstand und Karten werden nicht verändert.

- **Wiese:** Sandflecken auf grüner Wiese im Sommer, auf brauner im Herbst
- **Siberia:** eigene Einträge für Sommer und den angezuckerten Herbst (`grass2snow.dds`)
- **Dschungel:** Sommer mit Sand, der Herbst läuft über die normale Herbstwiese
- **Wüste und Winter:** immer nativ, Wüste ist schon Sand, Schnee deckt alles
- **Mit Abbau:** die Textur verschwindet mit der Ressource

Voraussetzung: Das Vorkommen aus `sand_surface_token` hat einen eigenen Kanal ab resourcemap3, bei Sand über `independent_map = 1`. Sonst meldet das Log „no independent map; disabled“.

### Einstellung

```ini
[deposits_plus]
; 1 = an, 0 = aus
sand_surface = 1
; 0 bis 1, 1.0 = volles Bild
sand_surface_strength = 1.0
sand_surface_token = $TYPE_MINE_SAND
```

### Kacheltabelle (seit 0.4.1)

Je Bodentextur des Geländes ein Abschnitt `[sand_tile:<name>]`: `base` ist die Textur, die die material.mtl der Karte auf Platz 5 nennt, mit Ordner; `color` und `normal` sind deine DDS-Dateien unter `deposits_plus\assets`, den die DLL zuerst neben sich (Paket: `hooks\deposits_plus\assets`) und dann unter `plugins\deposits_plus\assets` sucht. Seit 0.4.2 dürfen die Dateien in Set-Ordnern liegen (`Siberia/sand_meadow_siberia_color.dds`); `..` und absolute Pfade sind tabu. Im Republic Mod Manager pflegst du die Tabelle auf dem Reiter Sand-Texturen: Die Dateifelder zeigen alle DDS-Dateien nach Set-Ordner gruppiert, und vor dem Speichern prüft der RMM, ob die Datei existiert und das richtige Format hat.

```ini
[sand_tile:meadow]
base   = tiles_normal/grass2.dds
color  = Vanilla/sand_meadow_color.dds
normal = Vanilla/sand_meadow_normal.dds

[sand_tile:siberia_autumn]
base   = dlc2/tiles_siberia/grass2snow.dds
color  = Siberia/sand_meadow_autumn_siberia_color.dds
normal = Siberia/sand_meadow_autumn_siberia_normal.dds
```

Ausgeliefert sind die Sets `Vanilla` (Wiese Sommer/Herbst), `Siberia` (Sommer und Schneeherbst), `Asia - Jungle` (Sommer) und `Ultimate Vanilla +` (Wiesenpaar passend zu diesem Texturpaket, nicht voreingestellt: trag seine Dateien bei den Wieseneinträgen ein, wenn du das Paket nutzt). Eine Bodentextur ohne Eintrag bleibt nativ, ebenso ein Eintrag, dessen Dateien fehlen (Logzeile `sand surface WARN tile`). Bekannte Bodentexturen: `tiles_normal/grass2.dds`, `tiles_normal/grass2_fall.dds`, `dlc2/tiles_siberia/grass2.dds`, `dlc2/tiles_siberia/grass2snow.dds`, `dlc2/tiles_asia/jungle_swamp_dm.dds`.

### Dateien

```
Vanilla\sand_meadow_color.dds                          (Wiese Sommerfarbe)
Vanilla\sand_meadow_normal.dds                         (Wiese Sommer-Normalmap)
Vanilla\sand_meadow_autumn_color.dds                   (Wiese Herbstfarbe)
Vanilla\sand_meadow_autumn_normal.dds                  (Wiese Herbst-Normalmap)
Siberia\sand_meadow_siberia_color.dds                  (Siberia Sommer)
Siberia\sand_meadow_siberia_normal.dds
Siberia\sand_meadow_autumn_siberia_color.dds           (Siberia Schneeherbst)
Siberia\sand_meadow_autumn_siberia_normal.dds
Asia - Jungle\sand_meadow_jungle_color.dds             (Dschungel Sommer)
Asia - Jungle\sand_meadow_jungle_normal.dds
Ultimate Vanilla +\sand_meadow_color_ultimatevanilla.dds          (Wiese Sommer für Ultimate Vanilla+)
Ultimate Vanilla +\sand_meadow_normal_ultimatevanilla.dds
Ultimate Vanilla +\sand_meadow_autumn_color_ultimatevanilla.dds   (Wiese Herbst für Ultimate Vanilla+)
Ultimate Vanilla +\sand_meadow_autumn_normal_ultimatevanilla.dds
```

**Format:** quadratisch, Zweierpotenz 256 bis 4096 (1024 oder 2048 empfohlen), vollständige Mipmap-Kette
- **Farbe:** BC1/DXT1
- **Normal:** BC3/DXT5

### Spieltest

```
1. Spiel komplett neu starten
2. Sandvorkommen auf grüner Wiese ansehen → Flecken sichtbar?
3. Herbst → braune Wiese mit Sandflecken?
4. Schnee → Schneedecke überlagert alles?
5. Aus dem Hauptmenü neu laden → gleiche Darstellung?
```

Erwartet in `tesmioloader.log`:
```
sand surface assets from <Ordner>
sand surface shader preparation: 13/13 verified native programs augmented
```

---

## 🚜 Arbeitsfahrzeuge

### Was macht es?

Ein Vorkommen kann die Fahrzeugfähigkeit des Kiesabbaus übernehmen. Beispiel: Sand mit den **Baggern, die auch Kies fördern**.

### Einstellung

```ini
[sand]
token                 = $TYPE_MINE_SAND
working_vehicle_skill = gravelmining
building_type         = 7
```

**Unterstützte Werte:**
- `none` (Standard, keine Fahrzeugfreigabe)
- `gravelmining` (Fähigkeit des Kiesabbaus)

Ein anderer Wert ergibt eine Warnung im Log und wirkt wie `none`. Groß- und Kleinschreibung ist egal.

**Voraussetzung**
- `building_type = 7` (Mine). Sonst wird die Fahrzeugzuordnung mit Warnung abgeschaltet, das Vorkommen bleibt erhalten.

### Wie funktioniert es?

- Die `building.ini` der Mine behält ihr Token, zum Beispiel `$TYPE_MINE_SAND`
- Fahrzeuge nutzen ihre vorhandene Fähigkeit `$SKILL_GRAVELMINING`
- **Kein neuer Fahrzeugtyp** – nur bestehende Bagger werden zugewiesen
- Braucht wie immer `$WORKING_VEHICLES_NEEDED` und Parkplätze in der `building.ini`
- Ressourcen, Produktion, Treibstoff: alles wie gehabt

### Gameplay-Test

```
1. Sandmine mit Arbeitsfahrzeugplätzen öffnen
2. Bagger kaufen und zuweisen
3. Ohne Arbeiter → fördert der Bagger Sand?
4. Treibstoff leer → stoppt er korrekt?
5. Alte Kies- und Bauxitminen → keine Änderung
```

---

## 🔗 Verhältnis zum Original-Plugin

### Koexistenz mit `deposits.dll`

Deposits Plus und das Original-Plugin `deposits` nutzen:
- **denselben Registrierungsdienst:** `deposits`
- **dieselbe Speicherdatei:** `tesmio_deposits.bin`
- **Kompatibilität:** bestehende Spielstände und Typnummern bleiben erhalten, solange Typnummern und Kanäle nicht verändert werden

### Falls beide installiert sind

**Deposits Plus lädt nur, wenn das Original ausgeschaltet ist.**

Ist `plugins\deposits.dll` vorhanden und in tesmioloader.ini eingeschaltet:
- Deposits Plus bleibt untätig, bevor es irgendetwas patcht
- Im Log: `deposits_plus  idle - plugins\deposits.dll is present and enabled`
- Das Original lädt normal

**Lösung:** Original im TesmioLauncher oder in Republic Mod Manager ausschalten. Löschen ist nicht notwendig.

---

## 💾 Kompatibilität

### Speicherformat

- **DDS-Karten des Spielstands** enthalten den Abbauzustand wie immer; die vom Plugin angelegten Karten resourcemap3 und höher werden mit dem Terrain gespeichert
- **`tesmio_deposits.bin`** speichert zusätzlich:
  - Vorkommen-Identitäten (Token)
  - Kanal-Zuordnungen
  - Seed der Verteilung (gleicher Seed, gleiche Verteilung)
  - Liste der entfernten Vorkommen

### Spielstandsicherung

Beim Kopieren oder Sichern **immer den kompletten Spielstandordner nehmen**:
```
MeinSpiel\
├── dds\                    (Terrain und Karten)
├── buildings.bin
├── ...
└── tesmio_deposits.bin     (← nicht vergessen)
```

### Versionskompatibilität

- **Innerhalb der 0.4-Reihe:** Updates übernehmen Spielstände und `tesmio_deposits.bin` automatisch; Typnummern, Kartenplätze und Token laufender Spielstände bleiben tabu (siehe oben)
- **Mit Deposit Depletion:** funktioniert weiter wie immer, der Schlüssel `deplete` wird an das Plugin depletion durchgereicht
- **Mit Soviet Mod Loader:** SML bringt Vorkommen selbst mit. Läuft er, pausiert dieses Plugin und schreibt eine Zeile ins Protokoll; willst du stattdessen Deposits Plus, schalte `soviet_mod_loader` in der `tesmioloader.ini` aus (im Republic Mod Manager der Schalter „Plugin aktiv")
- **0.4.10:** Sicherheitsrunde ohne neue Funktion: die Zusatzkarten werden erst eingehängt, wenn der Typ-Patch steht; der Geländeeditor-Overlay und der Pinsel stellen ihren Zustand auch nach einem Fehler wieder her und schalten sich ab; die Zusatzkarten-Tabelle wird unter dem Lock gelesen; eine leere DDS-Datei, die nicht vollständig geschrieben werden konnte, wird mit Fehlercode gemeldet und entfernt; `building_type` muss 7 oder 92 sein und `minimap` 0 oder 1, sonst WARN und Standardwert; die Kopfzeilen der Spiel-EXE werden vor dem Lesen geprüft. Im RMM: höchstens 32 Vorkommen und 16 Sand-Kacheln (die Grenzen des Plugins), die Karte „Natürliche Verteilung“ liegt jetzt auf dem Reiter Allgemein
- **0.4.9:** Die Bremse gegenüber Soviet Mod Loader greift auch dann, wenn Deposits Plus vor SML geladen wird (lokale Kopie in `plugins`, alphabetische Reihenfolge): es reicht, dass `plugins\soviet_mod_loader.dll` liegt und in der `tesmioloader.ini` eingeschaltet ist.
- **0.4.8:** Das Plugin tritt unter Soviet Mod Loader von selbst zur Seite
- **0.4.7:** Der Pinselname darf jetzt bis zu 31 Zeichen lang sein. Bestehende Namen und die zwei PNG-Dateien bleiben, wie sie sind

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem                     | Ursache                                                     | Lösung                                                                           |
|-----------------------------|-------------------------------------------------------------|----------------------------------------------------------------------------------|
| Deposits Plus tut nichts    | Original deposits eingeschaltet                             | Log auf `deposits_plus  idle` prüfen, Original ausschalten                       |
| Vorkommen nicht verteilt    | `generation = 0` oder ungültiger Wert                       | Log auf `generation WARN` prüfen, Wert in den Bereich bringen                    |
| Texturen nicht sichtbar     | Assets-Ordner fehlt oder kein eigener Kanal                 | `deposits_plus\assets` neben der DLL oder unter `plugins`; `independent_map = 1` |
| Bagger arbeiten nicht       | `working_vehicle_skill` falsch oder `building_type` nicht 7 | Log auf `vehicles WARN` prüfen                                                   |
| Alte Vorkommen weg          | INI stark umgebaut                                          | Spielstand wiederherstellen, neu übernehmen                                      |

### Logging

Alle Meldungen stehen in `tesmioloader.log`.
- In **Republic Mod Manager** öffnet das Symbol mit dem Dokument unten in der Plugin-Leiste die Protokollansicht mit Filter und Absender.

Suche nach:
- `generation` → Verteilung
- `vehicles` → Arbeitsfahrzeuge
- `sand surface` → Bodentextur
- `deposits_plus` → allgemeine Meldungen, Ablehnungen einzelner Vorkommen

---

## 📦 Dateistruktur

**Workshop-Paket** (Steam-Abo, SML, Workshop Bridge)
```
deposits_plus\
├── hooks\
│   ├── deposits_plus.dll           (Plugin)
│   ├── deposits_plus.ini           (Original-INI)
│   └── deposits_plus\assets\       (Texturen der sandigen Wiese)
│       ├── Vanilla\                (sand_meadow_*.dds)
│       ├── Siberia\                (sand_meadow_siberia_*.dds, sand_meadow_autumn_siberia_*.dds)
│       ├── Asia - Jungle\          (sand_meadow_jungle_*.dds)
│       └── Ultimate Vanilla +\     (sand_meadow_*_ultimatevanilla.dds)
├── config\                         (Editor-Schema für Republic Mod Manager)
│   ├── deposits_plus.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                  (Manifest für SML, Bridge und Republic Mod Manager)
├── workshopconfig.ini              (Steam-Workshop-Eintrag)
├── previewimage.png
├── README_DE.md
├── README_EN.md
└── THIRD-PARTY-LICENSE.txt         (Lizenz des enthaltenen Shader-Compiler-Codes)
```

**Loader-Ordner** (Methode 1 von Hand oder „Dateien nur lokal“)
```
tesmioloader\build\
├── plugins\
│   ├── deposits_plus.dll
│   ├── deposits_plus.ini           (wirksame INI)
│   └── deposits_plus\assets\
│       ├── Vanilla\
│       ├── Siberia\
│       ├── Asia - Jungle\
│       └── Ultimate Vanilla +\
└── user_config\
    └── deposits_plus.editor.ini    (persönliche Werte aus Republic Mod Manager)
```

Über die Bridge oder SML liegt in `plugins` nur die wirksame INI, DLL und Texturen bleiben im Paket.

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Deposits Plus ist ein Fork des Plugins `deposits` aus dem TesmioLoader von MaxLegend (Tesmio), GPL v3, https://github.com/MaxLegend/TesmioLoader; Servicename und Spielstand-Datei sind bewusst mit dem Original identisch geblieben. **Code aus dem DirectX Shader Compiler** (DXIL-Signatur der angepassten Shader) – siehe `THIRD-PARTY-LICENSE.txt`. Der vollständige Quelltext von Deposits Plus liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/deposits_plus.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Kann ich beide Plugins gleichzeitig nutzen?**
A: Nein. Nur das Original ODER Deposits Plus. Ist das Original eingeschaltet, bleibt Deposits Plus untätig.

**F: Verliere ich meine Spielstände?**
A: Nein. Alte Spielstände funktionieren weiter. Beim ersten Laden wird `tesmio_deposits.bin` angelegt.

**F: Kann ich die Verteilung später noch ändern?**
A: Ja, aber nur noch nicht verteilte Vorkommen werden verteilt. Vorhandene Felder bleiben bestehen.

**F: Brauche ich alle Funktionen?**
A: Nein. Der Code-Patch für die Vorkommenstypen ist das Minimum. Verteilung, Sandtextur und Arbeitsfahrzeuge sind optional.

**F: Funktioniert Deposit Depletion noch?**
A: Ja, vollständig kompatibel. Der Abbau funktioniert wie immer.

**F: Muss ich die INI von Hand bearbeiten?**
A: Nein. Republic Mod Manager zeigt alle Einstellungen mit Beschreibung, prüft die Wertebereiche und schlägt für neue Vorkommen die nächste freie Typnummer vor.

---

**Letzte Aktualisierung:** Deposits Plus 0.4.10  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
