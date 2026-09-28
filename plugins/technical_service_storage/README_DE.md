# 🚛 Technical Service Storage 0.3.3

**TesmioLoader-Plugin für Streugutlager, Materialprioritäten und Schneepflugtanks**

Erweitert in *Workers & Resources: Soviet Republic* 1.1.1.9 das Fenster des Technischen Service um zusätzliche Einzellager, anklickbare Materialprioritäten und Warnungen. Schneepflüge erhalten einen plugineigenen Streuguttank, dessen Verbrauch, Nachfüllung und Rückkehr zum Heimatdepot das Plugin verwaltet. Zusammen mit Weather Roads entscheidet das geladene Material, wie viel neuen Schnee eine geräumte Straße fernhält.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Materialien und Depotprioritäten](#-materialien-und-depotprioritäten)
- [Streuguttank, Verbrauch und Rückkehr](#-streuguttank-verbrauch-und-rückkehr)
- [Gebäudelager und Migration](#-gebäudelager-und-migration)
- [Speichern und Laden](#-speichern-und-laden)
- [Lokalisierung](#-lokalisierung)
- [Wertebereiche](#-wertebereiche)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Technische Services mit `$TYPE_GARBAGE_OFFICE` (intern Gebäudetyp 49) und Schneepflüge mit der Fähigkeit 35
- Für den Streubetrieb: registrierte Streugut-Ressourcen und passende Einzellager im Depot (siehe [Gebäudelager und Migration](#-gebäudelager-und-migration))
- **Empfohlen:** das Localization-Plugin mit dem Textpaket `technical_service_storage` für übersetzte Beschriftungen; ohne es gelten eingebaute Ersatztexte
- **Optional:** Weather Roads für materialabhängigen Straßenschutz

### In drei Schritten
1. **Eine Installationsmethode wählen** (siehe unten) und das Plugin aktivieren.
2. **Materialien prüfen:** Die mitgelieferte Liste (`sand`, `gravel`) in Republic Mod Manager oder in `technical_service_storage.ini` an die vorhandenen Ressourcen und Depotlager anpassen. **Spielstand sichern.**
3. **Spiel vollständig neu starten.** Im Depotfenster erscheinen die Streugutlager mit Prioritäten; im Protokoll `logs\tesmioloader.technical_service_storage.log` stehen Konfigurationspfad, Materialliste und Dienststatus.

---

## ✨ Features

### 🎯 Grundfunktion
- ✅ Zusätzliche Lagerzeilen im Fenster des Technischen Service mit der nativen Darstellung
- ✅ Anklickbare Materialprioritäten je Depot: AUS → N → … → 2 → 1 → AUS; die niedrigste aktive Zahl mit Bestand wird verwendet
- ✅ Rote Warnungen für konfigurierte Materialien, die im Gebäude leer oder AUS sind
- ✅ Geordnete Materialliste mit Schutzstärke je Material (bis 32 Materialien)
- ✅ Plugineigener Streuguttank je Schneepflug, bemessen aus Leergewicht und Motorleistung; Verbrauch im Verhältnis zum echten Treibstoffverbrauch
- ✅ Automatische Rückkehr zum Heimatdepot an einer Reserveschwelle; Nachfüllen entnimmt nur die tatsächlich geladene Menge
- ✅ Fehlende Streugutlager werden beim Laden leer an fertige Depots angehängt (nur hinzufügen, nie umbauen)
- ✅ Prioritäten und Tanks werden in Zusatzdateien neben dem Spielstand gespeichert
- ✅ Streugut-Dienst `tss.grit_spreader` für Weather Roads (Materialstärke, Trockenpflügen)
- ✅ Der echte Fahrzeugtreibstoff wird nur gelesen, nie verändert; originale Gebäudedefinitionen bleiben unangetastet

### 🆕 Neu in 0.3.3
- ✅ **Reserveschwelle in Prozent:** `return_threshold_percent = 20` statt Basispunkte; der alte Schlüssel wird weiter gelesen, solange der neue fehlt.
- ✅ **Verfolgte Fahrzeuge:** Die Tabelle fasst jetzt 1024 Fahrzeuge (vorher 256); der Standard von `max_vehicles` ist 512 und lässt sich zwischen 1 und 1024 einstellen.
- ✅ Mitgelieferte Materialliste nur noch `sand` und `gravel`; eigene Ressourcen wie Streusalz trägst du selbst ein.
- ✅ Alle Texte für Republic Mod Manager im Spieler-Stil überarbeitet.

### 🆕 Neu in 0.3.1
- ✅ **INI neben der DLL:** Fehlt `plugins\technical_service_storage.ini`, liest die DLL die INI aus dem eigenen Ordner, also aus dem Workshop-Paket unter Soviet Mod Loader oder der Workshop Bridge. Der gewählte Pfad steht als `Configuration file:` im Protokoll.
- ✅ Editor-Schema für Republic Mod Manager im Paket: Materialliste als Liste mit Plus-Knopf, alle Schalter in Karten auf vier Reitern, deutsch und englisch.
- Parser, Prüfung, Verbrauch, Rückkehr, Tankgrößen, Prioritäten und Speicherformate sind gegenüber 0.3.0 unverändert.

---

## 💾 Installation

Wähle **eine** der vier Methoden. Dieselbe DLL darf nie zweimal geladen werden.

---

### Methode 1️⃣: Klassischer TesmioLoader

```
1. Kopiere technical_service_storage.dll und technical_service_storage.ini aus hooks\
   → tesmioloader\build\plugins\

2. Für übersetzte Beschriftungen: Localization-Plugin installieren; das Textpaket
   liegt im Localization-Paket unter plugins\localization\technical_service_storage\

3. Aktiviere technical_service_storage im TesmioLauncher
4. Spiel vollständig neu starten
```

---

### Methode 2️⃣: Soviet Mod Loader (SML)

```
1. Workshop-Objekt abonnieren – SML liest abonnierte Pakete von selbst
2. SML lädt die DLL über soviet.mod.ini aus dem Paket, die INI liegt daneben
3. Eine lokale technical_service_storage.dll in plugins\ vorher entfernen oder abschalten
```

---

### Methode 3️⃣: Workshop Bridge (ohne SML)

```
1. Paket in Republic Mod Manager auswählen
2. „Plugin aktiv“ einschalten
3. workshop_bridge lädt die DLL direkt aus dem Paket; die INI findet sie neben sich
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

Das Paket enthält im Ordner `config` ein Editor-Schema. Republic Mod Manager (ab 0.34.0) zeigt Technical Service Storage damit in vier Reitern, deutsch und englisch:

- **Allgemein:** Hinweise, „Dateien nur lokal“, Knopf für diese Anleitung, die Karte „Speichern im Spielstand“ (Lagerergänzung, Prioritäten und Tanks speichern, abgerissene Depots vergessen) und darunter „Fehlersuche“ (ausführliches Protokoll, Meldungsgrenze)
- **Streumaterialien:** links die Materialliste (höchstens 32), rechts das gewählte Material mit seiner Schutzstärke; der Plus-Knopf legt ein Material an (interner Ressourcenname als Text, Vorschläge gruppiert nach eigenen Ressourcen aus dem Resources-Plugin und Vanilla-Ressourcen). Originalzeilen lassen sich mit dem Papierkorb ausblenden und wieder anzeigen
- **Depotfenster:** Lagerzeilen und Office-Priority-Beschriftungen, Prioritätsfelder, Trennlinie und Warnungen
- **Streubetrieb:** Verbrauch und Erkennung, Rückkehr zum Heimatdepot, Tankkapazität, Fahrzeuganzeige und Zeiten

Persönliche Materialien liegen in `user_config\technical_service_storage.editor.ini`, die wirksame Datei ist `plugins\technical_service_storage.ini`; die INI im Paket bleibt unverändert. Dezimalwerte werden beim Speichern normalisiert (`0.80` wird zu `0.8`), was das Plugin gleich liest.

---

## ⚙️ Konfiguration

### Hauptdatei: `technical_service_storage.ini`

Die DLL liest in dieser Reihenfolge:
- **Zuerst:** `tesmioloader\build\plugins\technical_service_storage.ini`, falls vorhanden (klassische Installation, „Dateien nur lokal“ oder die von Republic Mod Manager geschriebene wirksame INI)
- **Sonst:** die INI neben der DLL, im Paket `hooks\technical_service_storage.ini` (Soviet Mod Loader, Workshop Bridge)

Empfohlen ist **UTF-8 ohne BOM**; UTF-8 mit BOM und UTF-16 LE werden ebenfalls gelesen. Höchstens 1 MiB. `;` nur in eigenen Zeilen, keine Kommentare hinter Werten. Materialstärken mit Dezimalpunkt, Schalter als `0` oder `1`. Änderungen gelten nach einem vollständigen Neustart.

⚠️ **Die INI zu löschen ist keine Deaktivierung.** Ohne Datei gelten die Standardwerte einschließlich `enabled = 1` und `sand = 0.50`. Zum Abschalten `[general] enabled = 0` setzen und neu starten.

### Abschnitte

| Abschnitt | Aufgabe |
|---|---|
| `[general]` | `enabled`, `debug`, `debug_limit` (nur UI-Debugmeldungen) |
| `[storage_migration]` | fehlende Lager beim Laden ergänzen |
| `[ui]` | Zeilenabstände, Office-Priority-Umbruch, Prioritätsfelder, Streugut-Trennlinie, Materialwarnungen |
| `[priority_persistence]` | Depotprioritäten je Spielstand speichern |
| `[tank_persistence]` | Streuguttanks je Spielstand speichern |
| `[grit_materials]` | geordnete Materialliste, `name = stärke` |
| `[sand_diagnostic]` | Streubetrieb: Verbrauch, Rückkehr, Tankberechnung, Fahrzeuganzeige, Zeiten (alter Abschnittsname, steuert den echten Betrieb) |
| `[tank_probe]` | Depot-Lebenszyklus verfolgen (`building_lifecycle`, eingeschaltet lassen) |

Die INI erklärt jeden Wert direkt an der Einstellung; Republic Mod Manager zeigt dieselben Erklärungen.

### Prüfung und Ersatzverhalten

| Fall | Verhalten |
|---|---|
| Unbekannter oder veralteter Abschnitt/Schlüssel | Warnung, Angabe wird ignoriert |
| Doppelter Abschnitt oder Schlüssel | der erste bleibt maßgeblich |
| Ungültiger Schalter oder fehlerhafte Ganzzahl | Warnung und eingebauter Standardwert |
| Ganzzahl außerhalb ihres Bereichs | Begrenzung auf den Randwert mit Warnung |
| Doppeltes Material | erster gültiger Eintrag bleibt |
| Einzelnes ungültiges Material | Warnung, andere Materialien bleiben |
| Fehlende INI oder fehlender Materialabschnitt | Standardwerte, insbesondere `sand = 0.50` |
| Ausdrücklich leere oder komplett ungültige Materialliste, unlesbare Datei | kein stiller Sand-Ersatz: Streubetrieb, Migration und Prioritäts-/Tankpersistenz werden für diese Sitzung ausgesetzt |

Der alte Abschnittsname `[Streumaterialien]` wird weiterhin als Alias gelesen; bei beiden Namen gilt der erste Abschnitt in der Datei.

---

## 🧂 Materialien und Depotprioritäten

Die mitgelieferte Liste:

```ini
[grit_materials]
sand = 0.30
gravel = 0.45
```

Das sind **interne Ressourcennamen**, keine übersetzten Anzeigenamen. Eine zusätzliche Ressource wie `road_salt` muss vom Spiel oder einem Ressourcen-Plugin registriert sein; `sand` kommt aus dem Resources-Plugin, `gravel` aus dem Grundspiel. Eigene Ressourcen wie Streusalz lieferst du selbst (Ressource, Bilder, Lager) und trägst sie dann hier ein. Unbekannte Namen werden beim Laden einer Welt gemeldet und für diese Welt abgeschaltet. `fuel` ist Fahrzeugtreibstoff und nie Streugut.

Die Stärke von `0.00` bis `1.00` ist der Anteil neuen Schnees, den ein kompatibles Weather Roads verhindert. Sie ist weder Lagermenge noch Verbrauch noch Schutzdauer. `0.00` ist gültig, gewährt aber keinen Schutz; zum Ausschließen eines Materials die Depotpriorität auf AUS stellen.

### Auswahl im Depot
- Die Dateireihenfolge liefert die Anfangsprioritäten: erster Eintrag `1`, dann `2` usw.
- Die niedrigste aktive Zahl mit verfügbarem Bestand wird ausgewählt; `1` ist die höchste Priorität.
- Klickfolge: `AUS → N → … → 2 → 1 → AUS`. Eine bereits belegte Zahl vertauscht die beiden Materialien.
- Sind alle erlaubten Materialien leer oder AUS, wird ohne Streugut geräumt.
- Prioritäten sind depotbezogen und werden gespeichert; eine neue INI-Reihenfolge ersetzt gespeicherte Entscheidungen nicht.

### Wirkung mit Weather Roads
Bei Materialstärke `S` bleibt in der starken Phase der Anteil `1 - S` des neuen Schnees übrig, in der Salzphase mit dem Weather-Roads-Faktor `M` der Anteil `1 - S × (1 - M)`. Beispiel `S = 0.50`: 50 % in der starken Phase, bei `M = 0.50` danach 75 %. Schutzdauer und Phasen gehören in die Weather-Roads-INI. Bestätigtes Trockenpflügen meldet Stärke `0.00`; ob eine ältere Behandlung erhalten bleibt, bestimmt dort `dry_plowing_preserves_treatment`. Schwächeres Material ersetzt keinen stärkeren aktiven Schutz.

---

## ⛽ Streuguttank, Verbrauch und Rückkehr

### Tankkapazität

```text
Grundkapazität in kg =
  Leergewicht in kg × tank_weight_percent / 100
  + Motorleistung in kW × tank_power_kg_per_kw

danach × tank_capacity_multiplier_percent / 100,
auf tank_capacity_step_kg gerundet,
auf tank_minimum_capacity_kg .. tank_maximum_capacity_kg begrenzt
```

Mitgeliefert sind 10 % Gewichtsanteil, 2 kg/kW, Faktor 100 %, Rundung auf 50 kg und die Grenzen 250 bis 5000 kg. Das ergibt etwa 500 kg für einen GZ-53 und 850 kg für einen SKD 706; geänderte Fahrzeugdefinitionen liefern andere Werte.

### Verbrauch und Nachfüllen

```text
Streugutverbrauch in kg =
  Tankkapazität × berücksichtigter Treibstoffverbrauch / Treibstoffkapazität
  × grit_fuel_consumption_factor_percent / 100
```

Nur bestätigte Räumvorgänge zählen; `110` entspricht dem Faktor 1,10. Der Rest fällt höchstens auf null. Bestätigtes Nachfüllen entnimmt nur die tatsächlich geladene Menge aus dem gewählten Depotlager; Teilfüllungen sind möglich. Ein wiederhergestellter Tank belastet das Depot nicht ein zweites Mal.

### Rückkehr zum Heimatdepot
- `return_threshold_percent = 20` bedeutet 20 % der individuellen Kapazität (0 bis 100; der alte Schlüssel `return_threshold_basis_points` gilt nur noch ohne den neuen). Die Schwelle fordert die Rückkehr an; sie füllt oder leert den Tank nicht.
- Route und Fahrt bleiben beim Spiel (natives Heimat-/Betankungsziel). Solange Streugut da ist, verbraucht Räumen auf dem Weg den echten Rest; bei null wird trocken gepflügt.
- Ist kein erlaubtes Material verfügbar, bleibt Trockenpflügen möglich; die Verfügbarkeit wird bei späteren Räumvorgängen und der regelmäßigen Depotbeobachtung erneut geprüft.
- Die Nachfüllbestätigung nutzt die physische Ankunft oder eine passende Treibstoffzunahme bei aktivem Betankungsziel. Ein offenes Depotfenster ist nicht nötig.

Zwei unterstützte Schlüssel stehen nicht in der mitgelieferten INI und bleiben normalerweise unverändert: `return_retry_interval_ms` (250, 250 bis 10000 ms) und `return_arrival_settle_ms` (10, 10 bis 2000 ms). Der alte Schlüssel `consumption_enabled` löst nur noch eine Kompatibilitätswarnung aus.

---

## 🏗️ Gebäudelager und Migration

Die Materialliste erzeugt keine Ressourcen und keine Gebäudelager. Ein Material braucht ein passendes Einzellager im Depot, schematisch:

```text
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_COVERED <Kapazität> <Ressource>
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL  <Kapazität> <Ressource>
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN    <Kapazität> <Ressource>
$STORAGE_FUEL           RESOURCE_TRANSPORT_OIL    <Kapazität>
```

Diese Zeilen gehören in die Gebäudedefinition oder in ein Plugin, das sie ändert, zum Beispiel **Vanilla Buildings** (Regelsatz mit `add`-Zeilen für `buildings_types\technical_services.ini`). Die Transportklasse muss zur Ressource passen; allgemeine Mehrstofflager gehören nicht zum Vertrag.

Mit `[storage_migration] enabled = 1` werden beim Laden fehlende konfigurierte Streugutimporte aus der aktuellen Definition in fertige Depots übernommen:
- Neue Lager werden leer angehängt, in Definitionsreihenfolge.
- Vorhandene Reihenfolge, Kapazität, Bestand und Prioritäten bleiben erhalten; nichts wird verkleinert, vergrößert, umgewandelt oder entfernt.
- Abweichende oder mehrdeutige Zuordnungen werden gemeldet, nicht geraten.
- Baustellen und Gebäude im Abriss oder Einsturz sind keine Migrationsziele.

⚠️ Beim nächsten normalen Speichern können die Ergänzungen im **nativen Spielstand** landen. Ausschalten macht das nicht rückgängig; zum Zurücksetzen einen Spielstand von vor der Migration verwenden.

---

## 💾 Speichern und Laden

Zusätzlich zum nativen Spielstand entstehen im Spielstandordner:

| Datei | Inhalt |
|---|---|
| `tesmioloader.technical_service_storage.priorities.bin` | Materialprioritäten pro Depot |
| `tesmioloader.technical_service_storage.tanks.bin` | zugeordnetes Material und Streugutrest pro Fahrzeug |

Die Dateien werden über die Speicher-/Lade-Hooks zugeordnet und über temporäre Dateien ersetzt; Formate, Größen und Datensätze werden geprüft. Die Fahrzeugzuordnung nutzt Heimatdepotposition, Fahrzeugslot, Motorleistung und Leergewicht; nicht passende Tankdaten werden verworfen, ersatzweise gilt die erste Laufzeitrekonstruktion. Fehlende Prioritätsdaten führen zur Reihenfolge der Materialliste.

- Abriss und Einsturz beenden die Depotidentität; ein Wiederaufbau beginnt mit Standardprioritäten. Dafür `[tank_probe] building_lifecycle = 1` eingeschaltet lassen.
- Ausschalten einer Persistenz lässt bestehende Zusatzdateien liegen.
- Beim Übertragen oder Sichern eines Spielstands den ganzen Ordner mitnehmen. Steam Cloud und ZIP-Autosaves sind für die Zusatzdateien nicht als geprüft bestätigt.
- Die Schutzdatei von Weather Roads ist eine eigene Zusatzdatei dieses anderen Plugins.

---

## 🌐 Lokalisierung

Übersetzte Plugin-Texte kommen aus dem Localization-Plugin mit dem Textpaket `technical_service_storage` (`namespace = technical_service_storage`, `fallback = sovietEnglish`); es wird mit dem Localization-Paket ausgeliefert. Verwendete Schlüssel:

- `vehicle_tank.grit`, `vehicle_tank.none`, `vehicle_tank.dry_plowing`, `vehicle_tank.depot`
- `material_priority.off`
- `building_warning.resource`, `building_warning.empty`, `building_warning.off`

Die aufgelösten IDs müssen zwischen 2.000.000 und 2.999.999 liegen. Fehlende Schlüssel werden je Schlüssel gemeldet; eingebaute Ersatztexte bleiben verfügbar. Ressourcennamen und die Prioritätsüberschrift verwenden die nativen Spieltexte.

---

## 📏 Wertebereiche

| Größe | Grenze |
|---|---|
| Materialien | höchstens 32; Namen bis 63 Bytes, Zeile bis 511 Bytes |
| Schutzstärke | 0.00 bis 1.00 |
| `debug_limit` | 0 bis 10000 |
| `[ui]` Abstände und Größen | `resource_gap` 0–50, Beschriftungsbreite 120–400, Zeilenhöhe 12–28, Versatz X 100–800, Versatz Y -50–100, Breite 60–300, Höhe 12–60, Abstand zur Trennlinie 48–100 |
| Verbrauchsfaktor | 1 bis 1000 Prozent |
| Reserveschwelle | 0 bis 100 % |
| Tank | Gewichtsanteil 0–50 %, 0–20 kg/kW, Faktor 10–500 %, Schritt 1–1000 kg, Minimum 1–10000 kg, Maximum 1–20000 kg |
| Zeiten | Doppelmeldungsfenster 0–60000 ms, Zusammenfassungen 100–60000 ms, Räumprotokoll 0–60000 ms, Depotsuche 250–10000 ms |
| Verfolgte Fahrzeuge | 1 bis 1024 (Standard 512) |
| Anzeigeversatz | -1000 bis 1000 |
| INI | höchstens 1 MiB |

---

## 💾 Kompatibilität

### Spielstände
Ergänzte Lager können Teil des nativen Spielstands werden. Prioritäten und Tanks liegen in Zusatzdateien; ohne sie lädt der Spielstand normal, die exakten früheren Prioritäten und Tankreste sind dann nicht garantiert.

### Andere Plugins
- **Weather Roads** nutzt den Streugut-Dienst dieses Plugins; ohne ihn rechnet es mit Stärke 1.00.
- **Localization** liefert die übersetzten Beschriftungen (empfohlen, nicht zwingend).
- **Vanilla Buildings** oder das Buildings-Plugin ergänzen die nötigen Einzellager in den Depots.
- Ressourcen-Plugins (Resources, Deposits Plus) registrieren zusätzliche Materialien wie `road_salt`.

### Versionskompatibilität
- **0.3.3:** Reserveschwelle in Prozent (alter Schlüssel bleibt lesbar), Fahrzeugtabelle 1024 (Standard 512), Standardliste ohne `road_salt`, Texte; Speicherformate unverändert
- **0.3.1:** INI-Fallback neben der DLL, Editor-Schema im Paket; Betrieb und Speicherformate unverändert
- **0.3.0:** einheitliche Versionsnummer für Plugin und Streugutkomponente

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| `ini-unknown-key` / `ini-unknown-section` | Schreibweise oder alte Einstellung | Angabe entfernen oder korrigieren |
| `ini-value` / `ini-range` | Wert ungültig oder außerhalb des Bereichs | Standard bzw. Randwert wurde verwendet; Wert prüfen |
| `grit-material-list` | Name, Stärke, Duplikat, Limit oder leere Liste | Materialliste prüfen |
| `validation=not-found` | Ressource in dieser Welt nicht registriert | Ressourcen-Plugin und exakten Namen prüfen |
| `requires-single-resource-import` | Gebäudelager erfüllt den Einzellagervertrag nicht | Gebäudedefinition anpassen |
| `already-present-with-different-layout-or-capacity` | vorhandenes Lager weicht ab | wird nicht umgebaut; Definition und Bestand vergleichen |
| `vehicle-tank-key` | Textschlüssel fehlt oder ID außerhalb des Bereichs | Textpaket prüfen |
| `no-sidecar` | noch keine Zusatzdatei | normal beim ersten Laden |
| `invalid-sidecar-defaults-used` | Zusatzdaten abgelehnt | zusammengehörigen Spielstand verwenden |
| Keine Nachfüllung | Dienst, Material, Bestand, AUS-Priorität oder Ankunft | Depotfenster und Protokoll prüfen |

### Logging

Alle Meldungen stehen in `tesmioloader.log` und im Detail-Log `logs\tesmioloader.technical_service_storage.log`, mit Bereich, Regelkennung und Kontext. `debug = 0` unterdrückt nur Routinedetails; Warnungen, Fehler und Start-/Speichermeldungen bleiben. `debug = 1` ergänzt Details und den lesenden Strg+F8-Lebenszyklus-Schnappschuss.
- In **Republic Mod Manager** öffnet das Symbol mit dem Dokument unten in der Plugin-Leiste die Protokollansicht mit Filter und Absender.

Suche nach:
- `Configuration file` → welche INI gewählt wurde
- `grit-material-list` → die übernommene Materialliste
- `completed with restrictions` → Teilfunktionen prüfen; installierte Hooks bleiben, nicht jede Funktion ist aktiv

---

## 📦 Dateistruktur

**Workshop-Paket** (Steam-Abo, SML, Workshop Bridge)
```
technical_service_storage\
├── hooks\
│   ├── technical_service_storage.dll   (Plugin)
│   └── technical_service_storage.ini   (Original-INI mit Erklärungen)
├── config\                             (Editor-Schema für Republic Mod Manager)
│   ├── technical_service_storage.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                      (Manifest für SML, Bridge und Republic Mod Manager)
├── workshopconfig.ini                  (Steam-Workshop-Eintrag)
├── previewimage.png
├── README_DE.md
└── README_EN.md
```

**Loader-Ordner** (Methode 1 von Hand oder „Dateien nur lokal“)
```
tesmioloader\build\
├── plugins\
│   ├── technical_service_storage.dll
│   ├── technical_service_storage.ini   (wirksame INI)
│   ├── localization.dll                (empfohlen, eigenes Plugin)
│   └── localization\technical_service_storage\   (Textpaket, aus dem Localization-Paket)
├── user_config\
│   └── technical_service_storage.editor.ini (persönliche Materialien aus Republic Mod Manager)
├── logs\
│   └── tesmioloader.technical_service_storage.log
└── tesmioloader.log
```

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Das Plugin enthält keinen fremden Code; der Loader-SDK-Header stammt aus dem TesmioLoader von MaxLegend (GPL v3). Der vollständige Quelltext liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/technical_service_storage. Der Service-Header `grit_spreader_api.h` verbindet es mit Weather Roads.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Verändert das Plugin meine Gebäudedateien?**
A: Nein. Es ändert den laufenden Depot- und Fahrzeugzustand; ergänzte Lager können aber im nativen Spielstand gespeichert werden.

**F: Warum sehe ich kein Streugutlager im Depot?**
A: Meist fehlt das Einzellager in der Gebäudedefinition oder die Ressource ist nicht registriert. Das Protokoll nennt `requires-single-resource-import` oder `validation=not-found`.

**F: Der Pflug fährt nicht zum Nachfüllen zurück.**
A: Die Rückkehr wird erst an der Reserveschwelle angefordert und nur, wenn ein erlaubtes Material mit Bestand und aktiver Priorität vorhanden ist. Route und Fahrt bestimmt das Spiel.

**F: Brauche ich Weather Roads?**
A: Nein. Ohne es verwaltet das Plugin Lager, Prioritäten und Tanks; die Wirkung auf den Straßenschnee liefert erst Weather Roads.

**F: Kann ich die INI von Hand bearbeiten?**
A: Ja, mit den Regeln aus [Konfiguration](#-konfiguration). Republic Mod Manager bietet dieselben Werte mit Beschreibung und Bereichsprüfung.

---

**Letzte Aktualisierung:** Technical Service Storage 0.3.3  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
