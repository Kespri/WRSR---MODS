# 🚆 Rail Physics Fix 1.3.5

**TesmioLoader-Plugin für physikalische Zugdynamik**

Erweitert in *Workers & Resources: Soviet Republic* 1.1.1.9 die Zugphysik: Antrieb, Widerstände, Bremsen, Steigung, Kurvenlimits, Bahnhofs- und Zollanfahrten sowie Diesel- und Stromverbrauch. Es ist die Windows-/TesmioLoader-Portierung von **RailPhysics 1.3.0** (GPL v3). Alle Änderungen wirken über Hooks im Arbeitsspeicher; keine Spieldatei wird auf der Festplatte verändert, es entstehen keine VFS-Ersatzdateien und kein eigenes Spielstandformat.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Funktionsschalter](#-funktionsschalter)
- [Antrieb, Widerstand und Bremsen](#-antrieb-widerstand-und-bremsen)
- [Kurven, Bahnhöfe und Zoll](#-kurven-bahnhöfe-und-zoll)
- [Verbrauch und Stromnetz](#-verbrauch-und-stromnetz)
- [Wertebereiche](#-wertebereiche)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9, `SOVIET64.exe` Build 23935965 (PE-Kennung, Zeitstempel, Größe, Signaturen und Befehle werden geprüft; andere Stände erhalten keine Hooks)
- TesmioLoader API 4
- Die ursprüngliche `railphysics.dll` darf **nicht** gleichzeitig geladen sein
- Kein Localization-Plugin, kein Sprachpaket, keine VFS-Inhalte nötig

### In drei Schritten
1. **Eine Installationsmethode wählen** (siehe unten) und das Plugin aktivieren; eine alte `railphysics.dll` vorher abschalten.
2. **Werte prüfen:** Die mitgelieferte INI ist eine abgestimmte Konfiguration (unter anderem Leistung 1.5, Betriebsbremse 1.6 m/s², Bahnhofslimit 60 km/h, Netzmultiplikator 2.0). Für den ersten Test unverändert lassen und **eine Kopie des Spielstands** verwenden.
3. **Spiel vollständig neu starten.** `logs\tesmioloader.rail_physics_fix.log` nennt Version, Konfigurationsdatei, Vorprüfung und `9 subsystem(s) patched` bei allen gelieferten Funktionen.

---

## ✨ Features

### 🎯 Grundfunktion
- ✅ Physikalische Beschleunigung aus Motorleistung, Haftung, Davis-Widerstand und Steigung
- ✅ Eigene Betriebs- und Notbremsraten mit Anpassung geplanter Bremsungen
- ✅ Kurvenlimits aus seitlicher Beschleunigung und Radius, Streckenvorschau bis 8000 m; native Limits werden nie angehoben
- ✅ Bahnhofszonen, sanfte Bahnhofshalte und Zollanfahrten aus Zonen, Routenende und Gleiskorridoren
- ✅ Lastabhängiger Diesel- und Stromverbrauch über die Buchungen des Spiels
- ✅ Optionaler netzweiter Multiplikator der Übertragungsgrenze
- ✅ Strenge Vorprüfung der EXE, Signaturprüfung jeder Hookstelle, geprüfte Brücken und Speicherreservierungen
- ✅ Drei unabhängige Diagnoseschalter; Warnungen bleiben immer sichtbar

### 🆕 Neu in 1.3.5
- ✅ Alle Texte für Republic Mod Manager im Spieler-Stil: Hinweis und Info-Box auf Allgemein, Karte „Fehlersuche“, Bremsmarge bei den Bremsen, überall „Kurven- und Streckenvorschau“.
- ✅ Spieltest im Log bestätigt: 9 Teilsysteme gepatcht, Kurvenlimits, Bahnhofshalt, Bahnhofszone und Verbrauchsmodell arbeiten.
- ✅ **Gegenüber dem Original RailPhysics 1.3.0 behoben:** drei Fehler bei der Argumentübergabe der Brücken, ein Zugriff auf bereits freigegebenen Speicher im Korridoraufbau, ungeprüfte Speicherzugriffe und das sprachabhängige Einlesen von Dezimalzahlen. Physik, Werte und Hooks sind die des Originals; bei zerrissenen Daten, inaktiven Wagen und Speicherfehlern verhält sich das Plugin wie das Original (Teilvorschau statt Abbruch, unlesbare Kette überspringen, letzte Zonentabellen behalten).
- ✅ **INI neben der DLL:** Fehlt `plugins\rail_physics_fix.ini`, liest die DLL die INI aus dem eigenen Ordner, also aus dem Workshop-Paket unter Soviet Mod Loader oder der Workshop Bridge. Der klassische Weg über den Loader ist unverändert; der gewählte Pfad steht als `configuration file:` im Protokoll.
- ✅ Schema für Republic Mod Manager im Paket: vier Reiter mit allen 30 Einstellungen, deutsch und englisch.
- ✅ Offline-Testsuite mit 17 Prozessen, darunter das Szenario `beside_dll`.

---

## 💾 Installation

Wähle **eine** der vier Methoden. Dieselbe DLL darf nie zweimal geladen werden, und die ursprüngliche `railphysics.dll` muss abgeschaltet sein (das Plugin verweigert sonst mit `RP404`).

---

### Methode 1️⃣: Klassischer TesmioLoader

```
1. Kopiere rail_physics_fix.dll und rail_physics_fix.ini aus hooks\
   → tesmioloader\build\plugins\

2. Aktiviere rail_physics_fix im TesmioLauncher (railphysics abschalten)
3. Spiel vollständig neu starten
```

---

### Methode 2️⃣: Soviet Mod Loader (SML)

```
1. Workshop-Objekt abonnieren – SML liest abonnierte Pakete von selbst
2. SML lädt die DLL über soviet.mod.ini aus dem Paket, die INI liegt daneben
3. Eine lokale rail_physics_fix.dll in plugins\ vorher entfernen oder abschalten
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

Das Paket enthält im Ordner `config` ein Darstellungsschema. Republic Mod Manager zeigt Rail Physics Fix damit in vier Reitern, deutsch und englisch:

- **Allgemein:** Hinweise (Warnung und Info-Box), „Dateien nur lokal“, Knopf für diese Anleitung; Funktionsschalter und Fehlersuche
- **Antrieb und Bremsen:** Haftung, Leistung, Davis-Koeffizienten, Steigungsfaktor, Bremsraten, Bremsmarge
- **Kurven- und Streckenvorschau:** Bahnhofslimit, seitliche Beschleunigung, Weite der Vorschau, Zoll-Einfahrt
- **Treibstoff und Strom:** Leerlaufanteil, Lastfaktor, elektrischer Bedarf, Netzmultiplikator

Die Wertebereiche des Schemas sind Editorgrenzen; das Plugin selbst prüft nur `power_scale` (0.1 bis 5.0) und `grid_boost` (höchstens 10). Republic Mod Manager schreibt die wirksame Datei `plugins\rail_physics_fix.ini`; die INI im Paket bleibt unverändert.

---

## ⚙️ Konfiguration

### Hauptdatei: `rail_physics_fix.ini`

Die DLL liest in dieser Reihenfolge:
- **Zuerst:** `tesmioloader\build\plugins\rail_physics_fix.ini`, falls vorhanden (klassische Installation, „Dateien nur lokal“ oder die von Republic Mod Manager geschriebene wirksame INI), über die Leser des Loaders wie bisher
- **Sonst:** die INI neben der DLL, im Paket `hooks\rail_physics_fix.ini` (Soviet Mod Loader, Workshop Bridge)

Der Abschnitt heißt aus Kompatibilitätsgründen weiterhin **`[railphysics]`**; nicht umbenennen. **UTF-8 ohne BOM**, Dezimalpunkt (`0.30`), `;` nur in eigenen Zeilen, jeden Schlüssel genau einmal, Schalter als `0` oder `1`. Änderungen gelten nach einem vollständigen Neustart.

⚠️ **Die INI zu löschen deaktiviert das Plugin nicht.** Fehlende Schlüssel nehmen die Code-Standardwerte, darunter `enabled = 1`. Sieben gelieferte Werte weichen vom Code-Standard ab: `station_limit_kmh`, `curve_lateral_ms2`, `power_scale`, `grade_scale`, `service_brake_ms2`, `emergency_brake_ms2` und `grid_boost`. Zum Abschalten `enabled = 0` setzen und neu starten; eine DLL mit aktiven Hooks darf im laufenden Spiel nicht entladen werden.

### Prüfung und Ersatzverhalten
- Nicht leere, ungültige oder nicht endliche Fließkommawerte werden mit `RP201` gemeldet und durch den Code-Standard ersetzt.
- `power_scale` außerhalb von 0.1 bis 5.0 wird mit `RP202` auf 1.0 gesetzt.
- `grid_boost` bis 1 setzt keinen Netz-Hook; über 1 bis 10 aktiviert ihn; über 10 wird protokolliert und nicht angewendet.
- Unbekannte Schlüssel, Duplikate und Zahlenbeziehungen werden **nicht** vollständig geprüft. Zahlen kurz halten (64-Byte-Lesepuffer).

---

## 🔀 Funktionsschalter

| Schlüssel | Geliefert | Code-Standard | Bedeutung |
|---|---:|---:|---|
| `enabled` | 1 | 1 | gesamtes Plugin |
| `accel` | 1 | 1 | Antrieb, Widerstand und Steigung in der Beschleunigung |
| `brake` | 1 | 1 | eigene Bremsraten und Anpassung geplanter Bremsungen |
| `slope` | 1 | 1 | separate Steigungs-Hooks, bestehendes 1.1.1.9-Zweigverhalten |
| `fuel` | 1 | 1 | lastabhängiger Diesel-/Stromverbrauch |
| `curves` | 1 | 1 | Kurvenlimit und gemeinsame Vorschau |
| `stations` | 1 | 1 | Bahnhofszonen, braucht `curves` |
| `smoothstop` | 1 | 1 | sanfte Bahnhofshalte, braucht `curves` |
| `customstop` | 1 | 1 | Zollanfahrten, braucht `curves` |

Die Schalter wählen Hookbereiche, keine unabhängigen Modelle: `slope = 0` entfernt die Steigung nicht aus `accel` und `fuel`; `grid_boost` ist ein eigener Eingriff und hängt nicht an `fuel`. `curves = 0` schaltet auch `stations`, `smoothstop` und `customstop` aus.

---

## 🚂 Antrieb, Widerstand und Bremsen

| Schlüssel | Geliefert | Code-Standard | Bedeutung |
|---|---:|---:|---|
| `adhesion_mu` | 0.30 | 0.30 | Haftungskoeffizient, begrenzt die Zugkraft bei niedriger Geschwindigkeit |
| `power_scale` | 1.5 | 1.0 | Multiplikator der ermittelten Motorleistung |
| `davis_a` | 1.5 | 1.5 | konstanter Widerstand, N je Tonne |
| `davis_b` | 0.006 | 0.006 | geschwindigkeitsabhängiger Widerstand, N je Tonne und km/h |
| `davis_c` | 0.40 | 0.40 | quadratischer Widerstand des ganzen Zuges, N je (km/h)² |
| `grade_scale` | 0.06 | 0.12 | Umrechnung der mittleren internen Steigung |
| `service_brake_ms2` | 1.6 | 0.8 | Betriebsbremse und Bremsbudget der Vorschau |
| `emergency_brake_ms2` | 2.6 | 1.3 | Notbremse |
| `brake_min_vanilla_ratio` | 1.0 | 1.0 | Anteil der nativen Bremsrate als Untergrenze außerhalb geplanter Limitbremsungen |

Widerstand: `R = Masse_t × (A + B × v_kmh) + C × v_kmh²`. `grade_scale = 0.06` halbiert den Steigungsbeitrag gegenüber 0.12, ohne das Gelände zu ändern. Geplante Bremsungen werden situationsabhängig moduliert; `brake_min_vanilla_ratio = 1.0` heißt nicht, dass jede geplante Bremsung mindestens so stark wie die native ist.

---

## 🛤️ Kurven, Bahnhöfe und Zoll

| Schlüssel | Geliefert | Code-Standard | Bedeutung |
|---|---:|---:|---|
| `station_limit_kmh` | 60 | 30 | Limit erkannter Bahnhofszonen |
| `curve_lateral_ms2` | 1.4 | 0.9 | seitliche Beschleunigung: Kurvengeschwindigkeit = √(a × Radius) |
| `curve_brake_margin` | 1.25 | 1.25 | Teiler der Betriebsbremse im Vorschau-Budget (1.25 = 80 %) |
| `curve_lookahead_m` | 1200 | 1200 | Weite der Kurven- und Streckenvorschau, wächst mit der Anhaltestrecke + 150 m, höchstens 8000 m |
| `customs_entry_kmh` | 50 | 50 | Zielgeschwindigkeit an der Zoll-Einfahrt |

Bahnhofszonen umfassen erkannte Fracht-, Personen- und Wartebahnhofsgleise; das Limit folgt dem Zugkopf. Sanfte Halte gleichen das Routenende mit erkannten Bahnhofsknoten ab und übergeben die letzten etwa 25 m an die native Logik. Zollanfahrten verbinden Zonen, Routenende und vorberechnete Gleiskorridore. Bahnhofs- und Zolltabellen werden alle 30 Sekunden neu aufgebaut; neue Anlagen wirken deshalb verzögert.

---

## ⚡ Verbrauch und Stromnetz

| Schlüssel | Geliefert | Code-Standard | Bedeutung |
|---|---:|---:|---|
| `idle_load` | 0.05 | 0.05 | Leerlaufanteil im Lastmodell |
| `fuel_load_max` | 1.0 | 1.0 | Obergrenze des Lastfaktors |
| `electric_load_scale` | 1.0 | 1.0 | verringert die elektrische Anforderung bei Werten unter 1 |
| `grid_boost` | 2.0 | 1.0 | Multiplikator der Netz-Übertragungsgrenze, **netzweit** |

Der Lastfaktor kommt aus dem mechanischen Leistungsbedarf und geht an die Verbrauchsfunktionen des Spiels; Tank, Betankung und Buchungen bleiben beim Spiel. `grid_boost = 2.0` wirkt auf das gesamte Stromnetz, erzeugt aber keine Kraftwerksleistung; `1.0` lässt den Hook weg.

---

## 📏 Wertebereiche

| Größe | Grenze |
|---|---|
| `power_scale` | 0.1 bis 5.0 (vom Plugin geprüft, sonst 1.0) |
| `grid_boost` | bis 1 aus, über 1 bis 10 aktiv, über 10 abgewiesen |
| Streckenvorschau | höchstens 8000 m wirksam |
| Zahlenwerte | Lesepuffer 64 Bytes, kurz schreiben |
| Editorgrenzen im Schema | Bremsen 0.05–10 m/s², Haftung 0.01–1, Davis A 0–20, B 0–1, C 0–10, Steigungsfaktor 0–1, Limits 1–200 km/h, seitlich 0.1–5 m/s², Marge 1–5, Vorschau 100–8000 m, Leerlauf 0–1, Lastfaktor 0.05–10, elektrisch 0–1 |

Das Plugin selbst erzwingt außer `power_scale` und `grid_boost` keine dieser Grenzen.

---

## 💾 Kompatibilität

### Spielstände
Kein eigenes Spielstandformat. Bereits beeinflusste Positionen, Geschwindigkeiten und Verbrauchsbuchungen bleiben im normalen Spielstand; ein Neustart ohne Plugin setzt sie nicht rückwirkend zurück. Vergleiche auf einer Kopie fahren.

### Andere Plugins
- Die ursprüngliche `railphysics.dll` nie parallel laden (`RP404`).
- Native Fahrzeug- und Gleislimits werden nicht angehoben; der Entwickler nennt `railspeed` als optionale Ergänzung, ohne bestätigte Kompatibilität jeder Variante.
- Andere Eingriffe an denselben Hookstellen werden durch die Signaturprüfung erkannt und lösen `RP401` bis `RP406` aus.

### Versionskompatibilität
- **1.3.5:** erste veröffentlichte Fassung des Ports; Physik und Einstellungen wie RailPhysics 1.3.0, INI-Abschnitt `[railphysics]` bleibt kompatibel

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| keine Wirkung, kein aktuelles Detail-Log | Plugin nicht aktiv oder `enabled = 0` | Aktivierung, DLL-Pfad und INI prüfen, Hauptlog lesen |
| INI-Änderung wirkt nicht | kein vollständiger Neustart oder falsche INI | neu starten; `configuration file` im Log zeigt die gelesene Datei |
| `RP201` / `RP202` | ungültige Zahl bzw. `power_scale` außerhalb des Bereichs | Wert korrigieren; Code-Standard war aktiv |
| `RP401` bis `RP406` | EXE-, Signatur-, Befehls- oder Helferprüfung fehlgeschlagen | Spielversion und konkurrierende Eingriffe prüfen |
| `RP404` | `railphysics.dll` ebenfalls geladen | nur eine Fassung aktivieren |
| `RP303` / `RP304` | Hooks nur teilweise installiert | nicht entladen; Logs sichern, Spiel beenden, Ursache prüfen |
| `RP100` bis `RP105` | ungültige Fahrzeug-/Wagendaten, Zonen- oder Speicheraufbau | wiederkehrende Fälle mit Log melden |
| `RP900` / `RP901` | Detail-Log nicht verfügbar | Schreibrechte und freien Platz prüfen |
| Stromnetz verhält sich anders | `grid_boost` wirkt netzweit | für den Vergleich 1.0 verwenden |

### Logging

Meldungen stehen in `tesmioloader.log` und im Detail-Log `logs\tesmioloader.rail_physics_fix.log` (Zeitstempel, Schweregrad, Bereich, Regelkennung). Das Detail-Log wird bei aktiviertem Plugin neu geöffnet und überschrieben. Wiederholte Laufzeitwarnungen sind je Regel auf eine Ausgabe alle 30 Sekunden begrenzt.
- In **Republic Mod Manager** öffnet das Symbol mit dem Dokument unten in der Plugin-Leiste die Protokollansicht mit Filter und Absender.

Suche nach:
- `configuration file` → welche INI gewählt wurde
- `subsystem(s) patched` → Anzahl der Patchstellen; `9` mit allen gelieferten Funktionen und `grid_boost` über 1
- `RP` → alle Regelkennungen des Plugins

---

## 📦 Dateistruktur

**Workshop-Paket** (Steam-Abo, SML, Workshop Bridge)
```
rail_physics_fix\
├── hooks\
│   ├── rail_physics_fix.dll        (Plugin)
│   └── rail_physics_fix.ini        (Original-INI mit Erklärungen)
├── config\                         (Schema für Republic Mod Manager)
│   ├── rail_physics_fix.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                  (Manifest für SML, Bridge und Republic Mod Manager)
├── workshopconfig.ini              (Steam-Workshop-Eintrag)
├── previewimage.png
├── LICENSE                         (GPL v3)
├── README_DE.md
└── README_EN.md
```

**Loader-Ordner** (Methode 1 von Hand oder „Dateien nur lokal“)
```
tesmioloader\build\
├── plugins\
│   ├── rail_physics_fix.dll
│   └── rail_physics_fix.ini        (wirksame INI)
├── logs\
│   └── tesmioloader.rail_physics_fix.log
└── tesmioloader.log
```

---

## 📜 Lizenz & Credits

**GPL v3**, siehe `LICENSE`. Rail Physics Fix ist eine überarbeitete Fassung von **RailPhysics 1.3.0** von **Meow Meow** (TheRealMeowMeow00): Original im Workshop unter https://steamcommunity.com/sharedfiles/filedetails/?id=3776784867, Quelltext des Originals unter https://github.com/TheRealMeowMeow00/WRSR_RailPhysics. Physik und Einstellungen sind unverändert übernommen; die Änderungen dieser Fassung (Argumentübergabe an die Hook-Stellen, Speicherzugriffe, Konfigurationsprüfung, degradierendes Verhalten bei zerrissenen Daten, Diagnose, Paketunterstützung) sind in den Abschnitten „Neu in …“ und in `BUILD_INFO.md` beschrieben. Der vollständige Quelltext von Rail Physics Fix (Quelltext, Brücken, Offline-Tests mit 17 Prozessen, Build-Nachweis) liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/rail_physics_fix. Der Loader-SDK-Header stammt aus dem TesmioLoader von MaxLegend (GPL v3).

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Verändert das Plugin Spieldateien?**
A: Nein. Es setzt Hooks im Arbeitsspeicher und prüft vorher die EXE; ohne passende Version passiert nichts.

**F: Warum meldet das Log weniger als neun Patches?**
A: Abgeschaltete Funktionen oder `grid_boost = 1.0` fordern weniger Patchstellen an. `only N/9 requested patches installed` dagegen ist eine Warnung.

**F: Kann ich nur die Bremsen oder nur den Verbrauch nutzen?**
A: Ja, über die Funktionsschalter. Die Steigung bleibt aber Teil des Beschleunigungs- und Verbrauchsmodells, auch mit `slope = 0`.

**F: Warum erreicht mein Zug die Höchstgeschwindigkeit nicht?**
A: Native Fahrzeug- und Gleislimits, Leistung, Masse, Widerstand, Steigung und Stromversorgung begrenzen weiterhin; das Plugin hebt keine Limits an.

**F: Kann ich die INI von Hand bearbeiten?**
A: Ja, mit den Regeln aus [Konfiguration](#-konfiguration). Republic Mod Manager bietet dieselben Werte mit Beschreibung und Bereichsprüfung.

---

**Letzte Aktualisierung:** Rail Physics Fix 1.3.5  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
