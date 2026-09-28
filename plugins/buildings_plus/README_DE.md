# 🏭 Buildings Plus 0.1.10

**TesmioLoader-Plugin für neue Gebäude aus einer Konfigurationsdatei**

Legt in *Workers & Resources: Soviet Republic* 1.1.1.9 neue Gebäude an, ohne dass du Modelle bauen oder Dateien kopieren musst: Du nennst ein Spendergebäude des Spiels (etwa die Bauxitmine) und die Zeilen der building.ini, die anders sein sollen (etwa `$PRODUCTION raw_salt 1.0`). Bei jedem Spielstart schreibt das Plugin daraus ein vollständiges Workshop-Objekt nach `media_soviet\workshop_wip`, mit Modell, Texturen, Kollisionsdaten, Icon und der angepassten building.ini. Typischer Einsatz: die Salzmine und die Salzraffinerie für neue Ressourcen, ohne die Kohlemine des Spiels anzufassen.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Ersetzungsregeln](#-ersetzungsregeln)
- [Wertebereiche](#-wertebereiche)
- [Sicherheitsregeln](#-sicherheitsregeln)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9 (Referenzversion; das Plugin liest keine Spieladresse, ein Spielupdate kann höchstens eine building.ini-Zeile ungültig machen)
- TesmioLoader API 4
- Ressourcen, die ein Gebäude verbraucht oder herstellt, müssen im Spiel existieren, zum Beispiel über das Resources-Plugin.

### In drei Schritten
1. **Eine Installationsmethode wählen** (siehe unten) und das Plugin aktivieren.
2. **Gebäude anlegen:** In Republic Mod Manager auf dem Reiter Gebäude „Hinzufügen“ drücken und im Fenster Objektname, Spender, Name im Spiel und die Zeilen eintragen (die Workshop-ID darf leer bleiben). Von Hand: ein Abschnitt in `buildings_plus.ini`, wie das mitgelieferte Beispiel `[example_pharmacy]` (ausgeschaltet).
3. **Spiel vollständig neu starten.** Das Gebäude erscheint im Baumenü unter dem Namen aus `name`; im Log steht je Abschnitt eine Zeile mit Ordner und Spender.

---

## ✨ Features

### 🎯 Grundfunktion
- ✅ Ein Abschnitt je Gebäude: Spender, geänderte Zeilen, fertig
- ✅ Vollständiges Workshop-Objekt beim Spielstart: Modell, Material (Texturpfade umgeschrieben), Leuchtmaterial des Spenders, Kollisionsbox, Feuerpunkte, Icon, workshopconfig.ini, renderconfig.ini, building.ini
- ✅ Die Geometrie des Spenders bleibt vollständig (Anschlüsse, Bauphasen, Partikel, Fahrzeugplätze); ersetzt wird nur, was du erklärst
- ✅ Ordner werden nur neu geschrieben, wenn sich Abschnitt, Plugin oder Spenderdateien geändert haben
- ✅ Ein Ordner ohne den Stempel des Plugins wird nie angefasst; echte Workshop-Abos sind sicher
- ✅ `prune` räumt Ordner ausgeschalteter oder gelöschter Abschnitte wieder weg
- ✅ Dasselbe Abschnittsformat, das Soviet Mod Loader aus `tesmio\buildings.ini` eines Mods liest

---

## 💾 Installation

Buildings Plus wird mit Republic Mod Manager ausgeliefert: Der Installer des RMM-Pakets legt `buildings_plus.dll` und `buildings_plus.ini` unter `tesmioloader\build\plugins\` ab, der Ordner „Manual Installation“ enthält beide zum Drüberziehen. Danach schaltest du das Plugin in Republic Mod Manager ein. Wer das Plugin einzeln bekommt, wählt **eine** der Methoden unten. Dieselbe DLL darf nie zweimal geladen werden.

---

### Methode 1️⃣: Klassischer TesmioLoader

```
1. Kopiere buildings_plus.dll und buildings_plus.ini aus hooks\
   → tesmioloader\build\plugins\

2. Aktiviere buildings_plus im TesmioLauncher
3. Abschnitte in der INI anlegen, Spiel vollständig neu starten
```

---

### Methode 2️⃣: Soviet Mod Loader (SML)

```
SML bringt einen eigenen Gebäude-Generator mit und liest tesmio\buildings.ini
aus jedem Mod. Buildings Plus wird dort nicht gebraucht; ein Mod mit
tesmio\buildings.ini läuft unter SML und unter Republic Mod Manager gleich.
```

---

### Methode 3️⃣: Workshop Bridge (ohne SML)

```
1. Paket in Republic Mod Manager auswählen
2. „Plugin aktiv“ einschalten
3. workshop_bridge lädt die DLL direkt aus dem Paket
```

---

### Methode 4️⃣: Republic Mod Manager mit „Dateien nur lokal“

```
1. Paket in Republic Mod Manager auswählen, Gebäude anlegen, Speichern
2. Reiter „Allgemein“, Karte „Hinweise“: „Dateien nur lokal“ einschalten
3. Bestätigung mit Dateiliste → DLL und INI werden beim Speichern
   nach tesmioloader\build\plugins\ kopiert
```

---

## 🧰 Republic Mod Manager

Das Paket enthält im Ordner `config` ein Editor-Schema. Republic Mod Manager zeigt Buildings Plus in drei Reitern, deutsch und englisch:

- **Allgemein:** Hinweise, Anleitungen, die Karte „Erzeugte Ordner“ mit den Schaltern Besitzer-ID nachtragen und Ordner aufräumen (prune), dazu die Karte „Fehlersuche“ mit Immer neu schreiben (always) und Ausführliches Protokoll
- **Gebäude:** eine Zeile je Gebäude mit Schalter, Name, Nummer, Spender, Zustand und den Knöpfen Ändern, Öffnen und Löschen. „Hinzufügen“ und „Ändern…“ öffnen dasselbe Fenster mit zwei Reitern: **Angaben** (Objektname, Workshop-ID, Spender, Name im Spiel, Beschreibung, Lebensdauer) und **Zeilen** (die building.ini des Spenders neben deinen Zeilen, samt Anzeige, welche Spenderzeilen jede deiner Zeilen wegnimmt)
- **SML-Gebäude:** was Soviet Mod Loader selbst nach `media_soviet\workshop_wip` erzeugt hat, mit dem Knopf, der eine fehlende Besitzer-ID nachträgt

Den Schlüssel `object` bietet Republic Mod Manager nicht an — dort ist der Objektname immer der Abschnittsname. Von Hand geschrieben gilt `object` weiterhin.

Persönliche Gebäude liegen in `user_config\buildings_plus.editor.ini`, die wirksame Datei ist `plugins\buildings_plus.ini`; die INI im Paket bleibt unverändert.

---

## ⚙️ Konfiguration

### Hauptdatei: `buildings_plus.ini`

Die DLL liest in dieser Reihenfolge:
- **Zuerst:** `tesmioloader\build\plugins\buildings_plus.ini`, falls vorhanden (klassische Installation, „Dateien nur lokal“ oder die von Republic Mod Manager geschriebene wirksame INI)
- **Sonst:** die INI neben der DLL, im Paket `hooks\buildings_plus.ini`

⚠️ **Kommentare nur in eigenen Zeilen mit `;`.** Die Datei muss UTF-8 ohne BOM sein. In einem Gebäudeabschnitt nie ein `$TOKEN` in eine Kommentarzeile schreiben. Änderungen gelten nach einem vollständigen Neustart des Spiels.

### Abschnitt `[buildings_plus]`

```ini
[buildings_plus]
; 1 erzeugt die Gebäude beim Spielstart, 0 lädt das Plugin nicht (Ordner bleiben)
enabled = 1
; 1 entfernt Ordner von gelöschten oder ausgeschalteten Abschnitten (nur mit Stempel)
prune = 0
; 1 schreibt jeden Ordner bei jedem Start neu
always = 0
; 1 protokolliert jede kopierte Datei und jede entfernte Spenderzeile
verbose = 0
; 1 trägt die Steam-ID auch in erzeugte Ordner anderer Generatoren nach
repair_owner_ids = 1
```

### Gebäude: ein Abschnitt je Gebäude

```ini
[salt_mine]
enabled = 1
; Workshop-ID, optional: fehlt sie, vergibt das Plugin eine ab 9300000000 und merkt sie sich
; id = 9300000001
; Spender: Name unter media_soviet\buildings_types ohne .ini
donor = bauxite_mine
; Objektname (Ordner und $OBJECT_BUILDING), Standard = Abschnittsname
object = SaltMine
; Name im Baumenü und im Workshop-Eintrag, ohne Anführungszeichen
name = Salt Mine
; Beschreibung, je Zeile eine desc-Zeile
desc = Mines raw salt from a salt deposit.
; Zeilen der building.ini, in dieser Reihenfolge vor den Spenderzeilen
line = $PRODUCTION raw_salt 1.0
line = $STORAGE_EXPORT RESOURCE_TRANSPORT_OPEN 20
; Spenderzeilen, die ersatzlos wegfallen
strip = $WORKERS_NEEDED
```

**Mehrzeilige Angaben.** Manche Token der building.ini tragen ihre Werte in den Zeilen darunter — ein Anschluss seine Punkte, eine Warenanzeige ihre Lage. Eine `line` ohne `$TOKEN` gehört zu der Zeile darüber, genau wie in der Datei des Spiels:

```ini
line = $CONNECTION_WATERPIPE_INPUT
line = -92.5 -3.2 5.0
line = -91.5 -3.2 5.0
line = $RESOURCE_VISUALIZATION 0
line = position 15.2912 0.0 -51.6213
line = rotation 0.0
line = scale 1 1 1
line = numstepx 3.0 12
line = numstept 4.0 3
```

Die erste `line` eines Abschnitts muss ein `$TOKEN` tragen — sonst hätte sie nichts, wozu sie gehören könnte, und ein Tippfehler würde stillschweigend zur Datenzeile.

Der Spender gibt die Form vor: Eine Mine will eine Mine als Spender (Förderband, Animation), eine Fabrik eine Fabrik, ein Laden einen Laden. `life` (Standard 3000) ist die Lebensdauer in der renderconfig.ini.

**Ein Workshop-Gebäude als Spender.** Statt eines Grundspiel-Namens darfst du ein Gebäude aus einem abonnierten Workshop-Objekt nehmen:

```ini
donor = 1872558150\gravelmine
```

Links die Objektnummer, rechts der Ordner des Gebäudes im Objekt — dieselbe Schreibweise, die Vanilla Buildings für seine Zieldateien nutzt. Gesucht wird im Workshop-Ordner des Spiels und in `media_soviet\workshop_wip`, dein eigenes, noch nicht veröffentlichtes Gebäude geht also auch.

Was dabei passiert: Der gewählte Ordner wird kopiert, dazu alle losen Dateien des Objekts und jeder Unterordner, der kein eigenes Gebäude enthält — Texturordner heissen je nach Autor `mtl`, `Textures` oder `materials`, deshalb entscheidet der Inhalt und nicht der Name. Die anderen Gebäude eines Sammel-Objekts bleiben liegen: ein Abschnitt ist ein Gebäude. Die `workshopconfig.ini` des Spenders wird nicht übernommen, dein Klon bekommt eine eigene mit genau deinem Gebäude darin. Die `renderconfig.ini` des Spenders bleibt, weil sie seine Dateinamen kennt.

Zwei Dinge dazu: Das Objekt muss abonniert sein, sonst wird der Abschnitt mit einer Fehlerzeile übersprungen — und mit diesem Plugin wird nichts davon ausgeliefert, die Kopie entsteht auf deinem eigenen Rechner. Willst du das Ergebnis selbst im Workshop veröffentlichen, brauchst du die Erlaubnis des ursprünglichen Autors.

**Workshop-ID:** Lass `id` einfach weg. Beim Spielstart sucht das Plugin die höchste Nummer zwischen 9300000000 und 9399999999 (im Katalog, in der INI und unter den Ordnern in workshop_wip, auch fremden) und vergibt die nächste. Die Nummer steht danach in `plugins\buildings_plus.ids.ini` unter dem Abschnittsnamen und bleibt dort für immer, weil Spielstände das Gebäude über den Ordner `workshop_wip\<Nummer>` kennen. Ein umbenannter Abschnitt ist ein neues Gebäude mit neuer Nummer; ein gelöschter Abschnitt gibt seine Nummer nicht frei. Eine eigene `id` (9000000000 bis 9999999999) gilt weiterhin und geht vor. Sichere die Katalogdatei zusammen mit deinen Spielständen; Profile im Republic Mod Manager nehmen sie mit.

**Name aus dem Textpaket.** Normalerweise schreibst du den Namen einfach hin, `name = Salt Mine`. Er steht dann fest in der Datei und ist in jeder Spielsprache gleich.

Läuft das Plugin Localization mit, darfst du stattdessen einen Übersetzungsschlüssel eintragen:

```ini
name = localization.lang.salt_mine
```

Buildings Plus schlägt den Schlüssel beim Spielstart nach und schreibt die gefundene Nummer in die building.ini, genau so, wie das Grundspiel es bei seinen eigenen Gebäuden macht. Den Text liefert danach das Localization-Paket: in der Spielsprache, wenn das Paket sie mitbringt, sonst in seiner Rückfallsprache. Als Schlüssel gilt alles, was mindestens einen Punkt enthält und nur aus Buchstaben, Ziffern, Punkt, Unterstrich und Bindestrich besteht. Ein Name mit Leerzeichen kann also nie versehentlich als Schlüssel gelten.

Fehlt Localization oder kennt es den Schlüssel nicht, nimmt Buildings Plus den Teil nach dem letzten Punkt als Namen, hier also `salt_mine`, und schreibt eine Warnung ins Protokoll. Das Gebäude funktioniert trotzdem.

---

## 🛠️ Ersetzungsregeln

Die building.ini des Spenders wird Zeile für Zeile übernommen. Eine Spenderzeile fällt nur weg, wenn eine deiner Zeilen sie ersetzt oder `strip` sie nennt.

| Deine Zeile beginnt mit | Beim Spender fällt weg |
|---|---|
| dasselbe Token | dieselbe Zeile |
| `$NAME_STR` (aus `name`) oder `$NAME` | jede `$NAME`-Zeile |
| ein `$TYPE_*` | jede `$TYPE_*`-Zeile, es gilt nur ein Typ |
| ein `$STORAGE*` | jede `$STORAGE*`-Zeile und `$RESOURCE_VISUALIZATION`, weil die Darstellung die Lager von null zählt |
| `$PRODUCTION`, `$CONSUMPTION` oder `$CONSUMPTION_PER_SECOND` | alle drei: ein Rezept wird als Ganzes ersetzt |

`$PRODUCTION_SEWAGE_POLLUTION` und `$CONSUMPTION_WATER_REQUIRED_QUALITY` sind Einstellungen, kein Rezept, und bleiben. Zählt wird das erste `$TOKEN` irgendwo in der Zeile, so wie es der Parser des Spiels macht.

---

## 📏 Wertebereiche

| Größe | Grenze |
|---|---|
| `enabled`, `prune`, `always`, `verbose`, `repair_owner_ids` | genau 0 oder 1 |
| Gebäudeabschnitte | höchstens 256 |
| `id` | optional; Zahl von 9000000000 bis 9999999999, in der Datei eindeutig; ohne Angabe automatisch ab 9300000000 |
| `object`, Abschnittsname | Buchstaben, Ziffern, `_` und `-`, höchstens 64 Zeichen |
| `donor` | Grundspiel-Name wie `object`; ein Workshop-Spender `<Objektnummer>\<Ordner>`, je Teil höchstens 96 Zeichen, kein `..` |
| `name` | höchstens 128 Zeichen, keine Anführungszeichen; mit Punkten ein Übersetzungsschlüssel |
| `desc` | höchstens 4096 Zeichen, keine Anführungszeichen |
| `life` | 1 bis 1000000 |
| `line` je Abschnitt | höchstens 512, je höchstens 4096 Zeichen, jede mit einem `$TOKEN` |
| `strip` | genau ein `$TOKEN` je Zeile |
| gelesene Textdatei | höchstens 4 MiB |

Ein Abschnitt mit einem Fehler wird übersprungen und im Log genannt; die anderen Abschnitte laufen weiter.

---

## 🔒 Sicherheitsregeln

- Es wird nur unter `media_soviet\workshop_wip` geschrieben; keine Datei des Spiels wird verändert, die Steam-Prüfung bleibt zufrieden.
- Jeder erzeugte Ordner trägt `tesmioloader.stamp`. Ein Ordner ohne diesen Stempel wird nie angefasst, auch nicht bei gleicher ID; der Abschnitt wird abgewiesen.
- `prune` löscht nur Ordner mit dem Stempel dieses Plugins, nie Abos und nie Ordner eines anderen Generators.
- Im Ordner eines anderen Generators wird höchstens eine fehlende `$OWNER_ID` ergänzt — eine einzelne Zahl, sonst kein Byte. Der Stempel bleibt unangetastet: Soviet Mod Loader schließt das Spiel, wenn in seinem Nummernbereich ein Ordner ohne Stempel liegt.
- IDs unter 9000000000 werden abgewiesen, damit keine echte Steam-Nummer getroffen wird.
- Automatisch vergebene Nummern stehen in `plugins\buildings_plus.ids.ini` und werden nie ein zweites Mal vergeben, auch nicht nach dem Löschen eines Abschnitts.
- Dateien werden erst unter einem Hilfsnamen geschrieben und dann ersetzt; ein Absturz hinterlässt keine halbe building.ini.
- Das Plugin prüft die Form der Deklaration, nicht die fachlichen Werte der Spiel-Direktiven. Eine falsche Zeile meldet das Spiel in seinem eigenen Log.

---

## 💾 Kompatibilität

### Spielstände
Generierte Gebäude sind Workshop-Objekte mit fester ID. Ein Spielstand, in dem eines gebaut ist, braucht den Ordner beim Laden; entferne Abschnitte deshalb erst, wenn kein Spielstand das Gebäude mehr nutzt.

In die `workshopconfig.ini` schreibt das Plugin deine **Steam-ID** — die des Spielers, auf dessen Rechner das Gebäude entsteht. Das Spiel prüft beim Laden eines Spielstands, wem die verwendeten Workshop-Objekte gehören; steht dort eine 0, meldet es „Die in diesem Speicherstand verwendeten Workshop-Objekte wurden nicht gefunden". Der Spielstand lädt trotzdem, aber die Meldung kommt bei jedem Laden. Die Nummer holt sich das Plugin aus der Registry des angemeldeten Steam-Kontos, sonst aus `loginusers.vdf` deiner Steam-Installation; findet es beides nicht, bleibt die 0 stehen und der nächste Start mit angemeldetem Steam trägt sie nach. Nichts davon reist im Paket mit — jede Kopie bekommt die ID dessen, der sie erzeugt hat.

**Auch für Gebäude anderer Generatoren.** Soviet Mod Loader bringt seinen eigenen Gebäude-Teil mit und schreibt dort immer `$OWNER_ID 0` — die Meldung trifft also jeden, der Gebäude aus einem Inhaltspaket baut, und abstellen lässt sie sich von Hand kaum. Mit `repair_owner_ids = 1` (Vorgabe) sieht Buildings Plus beim Start auch in erzeugte Ordner, die es nicht selbst geschrieben hat, und trägt dort **nur die fehlende Nummer** nach; jedes andere Byte der Datei bleibt, wie es war, Zeilenenden inbegriffen. Dafür müssen alle vier Bedingungen stimmen: Ordnername ist eine erzeugte Nummer (9000000000 bis 9999999999), im Ordner liegt ein `tesmioloader.stamp`, die Datei nennt gar keinen Besitzer, und deine eigene ID war zu ermitteln. Ein Ordner, der schon jemanden nennt, wird nie angefasst, der Stempel ebenso wenig. Der Zeitpunkt passt: SML erzeugt in seiner Startphase, Buildings Plus läuft danach — ein gerade neu geschriebener Ordner ist im selben Start wieder in Ordnung.

### Soviet Mod Loader
Ein Mod mit `[content] buildings = tesmio\buildings.ini` in seiner soviet.mod.ini läuft unter SML mit dessen eigenem Generator und unter Republic Mod Manager mit Buildings Plus. Das Abschnittsformat ist dasselbe.

### Andere Plugins
Ressourcen aus `$PRODUCTION`, `$CONSUMPTION` und `$STORAGE_*` müssen im Spiel existieren (Grundspiel oder Resources-Plugin). Vorkommen für Minen kommen aus Deposits Plus. Vanilla Buildings ändert bestehende Gebäude, Buildings Plus legt neue an.

### Versionskompatibilität
- **0.1.10:** Sicherheitsrunde: ein Ordner mit dem Stempel eines fremden Generators wird nicht mehr übernommen; ein Abschnitt, den die Prüfung ablehnt, behält seinen Ordner auch bei `prune = 1` (ein Tippfehler löscht kein Gebäude); beim Neuaufbau aus einem Workshop-Spender bleibt der Stempel erhalten, ein fehlgeschlagener Schreibvorgang sperrt den Ordner nicht mehr dauerhaft; führende Nullen in `id` werden entfernt; Steuerzeichen in einer `line` werden abgewiesen, Anführungszeichen bleiben erlaubt; höchstens 512 `line`- und 64 `strip`-Zeilen, 4096 Dateien und 16 Ordnerebenen je Workshop-Spender, 256 MB je Datei; eine fehlende Referenz wird immer gemeldet; jeder Schreibfehler nennt den Windows-Fehlercode
- **0.1.9:** Hat Soviet Mod Loader ein Gebäude gleichen Abschnittsnamens schon nummeriert (`soviet_mod_loader\catalog.ini`, `[building_ids]`), übernimmt das Plugin diese Nummer und schreibt denselben Ordner — beide Ladewege erzeugen dann dasselbe Gebäude, ein Spielstand findet es unter beiden, und im Baumenü steht es nur einmal. Eine ausdrückliche `id` im SML-Bereich ist erlaubt, wenn SMLs Katalog genau diese Nummer für den Abschnitt führt. Ein älterer eigener Ordner bleibt liegen (`prune = 1` räumt ihn weg).
- **0.1.8:** Die fehlende Steam-ID wird auch in erzeugten Ordnern anderer Generatoren nachgetragen — Soviet Mod Loader lässt dort immer eine 0 stehen; Schalter `repair_owner_ids`, Vorgabe an
- **0.1.7:** In die `workshopconfig.ini` des erzeugten Gebäudes kommt die Steam-ID des Spielers, der es erzeugt hat — ohne sie meldet das Spiel beim Laden eines Spielstands „Workshop-Objekte wurden nicht gefunden"
- **0.1.6:** Eine `line` ohne `$TOKEN` gehört zur Zeile darüber — damit lassen sich Anschlüsse, Warenanzeigen und alles andere anlegen, was Datenzeilen braucht
- **0.1.5:** Nummern zwischen 9100000000 und 9199999999 sind gesperrt — den Bereich behält sich Soviet Mod Loader für seine eigenen Gebäude vor, und ein fremder Ordner darin hindert das Spiel am Start
- **0.1.4:** `donor` darf ein Gebäude eines abonnierten Workshop-Objekts sein, geschrieben `<Objektnummer>\<Ordner>`; der Klon entsteht auf dem eigenen Rechner
- **0.1.3:** `name` darf ein Übersetzungsschlüssel sein; der Name wird dann als `$NAME` mit der aufgelösten Nummer geschrieben und der Text kommt aus dem Localization-Paket
- **0.1.2:** erste veröffentlichte Fassung

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| Gebäude fehlt im Baumenü | Abschnitt `enabled = 0`, Plugin aus oder Fehler im Abschnitt | Log lesen, Abschnitt einschalten, Spiel neu starten |
| `id must lie between ...` | ID außerhalb 9000000000 bis 9999999999 | ID ändern |
| `exists and was not written by this plugin` | Ordner ohne Stempel unter workshop_wip | andere ID wählen oder den fremden Ordner prüfen |
| Gebäude nach dem Umbenennen eines Abschnitts doppelt | neuer Abschnittsname = neue Nummer, der alte Ordner bleibt | `prune` einschalten oder den alten Ordner löschen |
| `donor "..." has no ...` | Spendername falsch oder nicht installiert | Name aus `media_soviet\buildings_types` ohne `.ini` |
| Spiel stürzt beim ersten Bild ab | Spender mit Leuchtmaterial, aber `material_e.mtl` fehlt | Ordner löschen, Spiel neu starten; im Log auf „emissive material“ achten |
| `name must be plain text` | Anführungszeichen in `name` oder `desc` | Anführungszeichen entfernen |

### Logging

Ergebnisse stehen in `tesmioloader.log` (Absender `buildings_plus`), alles Weitere im Detail-Log `logs\tesmioloader.buildings_plus.log`. Mit `verbose = 1` steht dort jede kopierte Datei und jede entfernte Spenderzeile.
- In **Republic Mod Manager** öffnet das Symbol mit dem Dokument unten in der Plugin-Leiste die Protokollansicht mit Filter und Absender.

Suche nach:
- `configuration:` → welche INI die DLL gewählt hat und wie viele Abschnitte
- `-> <id>\<object> from "<donor>"` → ein Gebäude wurde geschrieben
- `up to date` → nichts hat sich geändert
- `prune:` → ein Ordner wurde entfernt
- `ERROR:` → ein Abschnitt wurde übersprungen, mit Zeile und Grund

---

## 📦 Dateistruktur

**Workshop-Paket** (Steam-Abo, Workshop Bridge)
```
buildings_plus\
├── hooks\
│   ├── buildings_plus.dll          (Plugin)
│   └── buildings_plus.ini          (Original-INI, Beispiel ausgeschaltet)
├── config\                         (Editor-Schema für Republic Mod Manager)
│   ├── buildings_plus.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                  (Manifest für SML, Bridge und Republic Mod Manager)
├── workshopconfig.ini              (Steam-Workshop-Eintrag)
├── previewimage.png
├── README_DE.md
└── README_EN.md
```

**Loader-Ordner** (mit Republic Mod Manager ausgeliefert, oder Methode 1 von Hand)
```
tesmioloader\build\
├── plugins\
│   ├── buildings_plus.dll
│   ├── buildings_plus.ini          (wirksame INI)
│   └── buildings_plus.ids.ini      (vergebene Workshop-IDs, schreibt das Plugin)
├── user_config\
│   └── buildings_plus.editor.ini   (persönliche Gebäude aus Republic Mod Manager)
└── logs\
    └── tesmioloader.buildings_plus.log
```

**Erzeugte Gebäude**
```
media_soviet\workshop_wip\<id>\
├── tesmioloader.stamp              (Kennung des Plugins und Prüfsumme)
├── workshopconfig.ini
├── previewimage.png
├── material.mtl                    (Texturpfade umgeschrieben)
├── material_e.mtl                  (nur wenn der Spender eines hat)
└── <object>\
    ├── building.ini                (deine Zeilen, dann der Spender ohne die ersetzten Zeilen)
    ├── renderconfig.ini
    ├── model.nmf
    ├── building.bbox
    ├── building.fire
    └── imagegui.png
```

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Buildings Plus ist ein Fork des Plugins `buildings` aus dem TesmioLoader von MaxLegend (GPL v3); Aufbau und Ersetzungsregeln stammen von dort, die Prüfungen, das Aufräumen, das Detail-Log und die Pfadbehandlung sind neu. Der vollständige Quelltext liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/buildings_plus.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Werden meine Spieldateien verändert?**
A: Nein. Es entstehen nur neue Ordner unter `media_soviet\workshop_wip`, die das Spiel wie unveröffentlichte Workshop-Objekte liest.

**F: Kann ich das Modell des Spenders austauschen?**
A: Nicht über das Plugin. Lege dafür ein eigenes Workshop-Objekt an; Buildings Plus ist für Gebäude, die wie ein Spendergebäude aussehen und sich anders verhalten.

**F: Warum darf die ID nicht kleiner sein?**
A: Echte Steam-Objekte haben Nummern um 3,8 Milliarden. Ab 9 Milliarden kann es nie eine Überschneidung geben.

**F: Muss ich eine ID angeben?**
A: Nein. Ohne `id` vergibt das Plugin die nächste freie Nummer ab 9300000000 und merkt sie sich in `plugins\buildings_plus.ids.ini`. Der Bereich liegt bewusst neben dem von Soviet Mod Loader (9100000000 aufwärts).

**F: Was passiert beim Löschen eines Abschnitts?**
A: Mit `prune = 1` verschwindet der erzeugte Ordner beim nächsten Start, sonst bleibt er liegen und das Spiel lädt das Gebäude weiter.

---

**Letzte Aktualisierung:** Buildings Plus 0.1.10  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
