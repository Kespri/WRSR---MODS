# 📦 Resources Plus 0.1.2

**Erweiterung des TesmioLoader-Plugins resources**

Eigene Ressourcen für *Workers & Resources: Soviet Republic* 1.1.1.9: neue Güter mit Symbol, Frachtmodellen, Preis und Handel im Zollhaus, ganz aus einer INI heraus.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Verhältnis zum Original-Plugin](#-verhältnis-zum-original-plugin)
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
- TesmioLoader API 4

### In drei Schritten
1. **Installieren:** über das Paket Republic Mod Manager (`Install-RMM.bat`). Der Installer schaltet Resources Plus ein und ein vorhandenes Original-Plugin resources aus und sagt beides in seiner Ausgabe.
2. **Spiel einmal starten:** Resources Plus legt `plugins\resources_plus.ini` an und übernimmt dabei deine vorhandene `plugins\resources.ini` unverändert. Deine Ressourcen sind sofort da.
3. **Weiter pflegen:** im Republic Mod Manager unter „Resources Plus“, oder direkt in `plugins\resources_plus.ini`.

---

## ✨ Features

### 🎯 Originalfunktionen (erhalten)
- ✅ Neue Ressourcen als Kopie eines Vorbilds (`= steel, Cable`) oder ganz eigen (`= custom` plus `[custom:<name>]`)
- ✅ Symbol, Frachtmodelle, Transportklasse, Preisart und Marktwerte je Ressource
- ✅ Preise aus der Produktionskette, `[base_price]` und `[price]` zum Übersteuern
- ✅ Zollhäuser bekommen neue Ressourcen auch nachträglich als Handelsplatz
- ✅ Beliebig viele Einträge, das Plugin vergrößert die Ressourcentabelle des Spiels selbst

### 🆕 Verbesserungen (neu in Resources Plus)

#### 1️⃣ **Kein Absturz mehr beim Beenden**
Das Original ließ das Spiel beim Beenden abstürzen, sobald mehr als sechs eigene Ressourcen eingetragen waren (Windows meldete `ucrtbase.dll`, Code `c0000409`). Spielstände waren nie betroffen, aber jeder Ausstieg hinterließ einen Absturzbericht. Resources Plus legt die vergrößerte Tabelle so an, wie das Spiel sie beim Beenden wieder freigibt.

#### 2️⃣ **Eigene Datei, automatisch übernommen**
Resources Plus liest `plugins\resources_plus.ini`. Fehlt sie, entsteht sie beim ersten Start aus dem, was du schon hast: aus `plugins\resources.ini` des Original-Plugins, in derselben Reihenfolge, mit einem Kopfkommentar, woher sie stammt. Das Original wird dabei nur gelesen, nie verändert. Stammt die `resources.ini` vom Soviet Mod Loader (der schreibt sie beim Spielstart mit allen Paketen zusammen neu), nimmt Resources Plus stattdessen SMLs Grundfassung, damit keine Paketressource doppelt erscheint.

#### 3️⃣ **Doppelte Namen werden abgewiesen**
Steht ein Name zweimal in `[list]`, zählt der erste, der zweite wird mit einer Zeile im Protokoll übergangen. Das Original hätte ihn zweimal angemeldet.

#### 4️⃣ **Tritt von selbst zur Seite**
Solange das Original eingeschaltet ist oder der Soviet Mod Loader läuft, bleibt Resources Plus untätig und schreibt eine Zeile ins Protokoll. Es gibt nie zwei Plugins, die dieselbe Ressource anmelden.

---

## 💾 Installation

Resources Plus und das Original-Plugin resources laufen nie gleichzeitig. Der Installer des Republic-Mod-Manager-Pakets regelt das für dich; von Hand geht es genauso.

### Methode 1️⃣: Republic Mod Manager (Paket, empfohlen)

```
1. Das Paket Republic Mod Manager abonnieren und Install-RMM.bat ausführen
2. resources_plus.dll und die Vorlage resources_plus.template.ini landen in
   tesmioloader\build\plugins\ (eine vorhandene resources_plus.ini bleibt unangetastet)
3. Der Installer setzt in tesmioloader.ini: resources_plus=1 und, falls
   plugins\resources.dll vorhanden ist, resources=0 - und sagt es dir
4. Spiel starten: die Liste aus plugins\resources.ini wird übernommen
```

**Zurück zum Original:** im Republic Mod Manager Resources Plus aus- und resources einschalten, oder in `tesmioloader.ini` `resources=1` und `resources_plus=0` setzen. `Uninstall-RMM.bat` macht das ebenfalls. Die DLL des Originals wird nie gelöscht oder überschrieben.

### Methode 2️⃣: Von Hand

```
1. Kopiere resources_plus.dll → tesmioloader\build\plugins\
2. Aktiviere resources_plus im TesmioLauncher (oder resources_plus=1 in tesmioloader.ini)
3. Deaktiviere das Original-Plugin resources (resources=0)
4. Spiel starten: plugins\resources_plus.ini entsteht aus deiner resources.ini;
   ohne resources.ini nimmt das Plugin die Vorlage resources_plus.template.ini neben der DLL
```

### Methode 3️⃣: Soviet Mod Loader (SML)

⚠️ **Solange SML läuft, bleibt Resources Plus untätig.** SML bringt seine eigene Fassung des Original-Plugins mit und beendet das Spiel, wenn ein zweites Plugin Ressourcen anmeldet. Willst du Resources Plus, schalte `soviet_mod_loader` in der `tesmioloader.ini` aus (im Republic Mod Manager der Schalter „Plugin aktiv“) und nimm Methode 1 oder 2. Beim nächsten Start übernimmt Resources Plus deine Liste aus SMLs Grundfassung, die Pakete stellt der Republic Mod Manager danach bereit.

---

## 🧰 Republic Mod Manager

- Der Eintrag **Resources Plus** bearbeitet `plugins\resources_plus.ini`: Liste, Vorbild, Anzeigename, Transportklasse, Preisart, Marktwerte. Fehlt die Datei noch, legt RMM sie beim Öffnen der Seite nach derselben Regel an wie das Plugin und sagt es in einem blauen Kasten. Der alte Eintrag „Resources“ bleibt als ausgeschaltetes Plugin in der Liste stehen, so wie „Deposits“ neben Deposits Plus.
- **Inhaltspakete** aus dem Workshop (`[content] resources`) landen in `plugins\resources_plus.ini`, sobald sie da ist, und lassen sich wieder zurücknehmen.
- Der Schalter **„Plugin aktiv“** bei Resources Plus schreibt den Eintrag in die `tesmioloader.ini`.
- Läuft SML, leuchtet Resources Plus **orange**: pausiert, nicht kaputt. Die Seite ist dann gesperrt, bis du „Trotzdem bearbeiten“ wählst.
- Das Protokollfenster zeigt `logs\tesmioloader.resources_plus.log`.

---

## ⚙️ Konfiguration

### Hauptdatei: `plugins\resources_plus.ini`

| Abschnitt | Inhalt |
|---|---|
| `[list]` | Eine Zeile je Ressource: `<name> = <Vorbild oder custom>[, <Anzeigename>]` |
| `[custom:<name>]` | Feinabstimmung: `transport`, `kind`, `cargo`, `price`, `market_rub`, `market_usd` und mehr |
| `[base_price]` / `[price]` | Preise vor bzw. nach der Preisrechnung des Spiels übersteuern |
| `[resources]` | Wie das Plugin das Spiel erreicht: `hook`, `resource_capacity`, `price_hook`, `price_report`, `custom_report` |
| `[customs]` | Zollhaus-Anbindung: `hook`, `probe` |

Das Format ist das des Original-Plugins, jeder Schlüssel ist in der Datei selbst erklärt. Die Vorbildressource entscheidet über Transportklasse und Frachtform, darum: `open` für Stückgut (Vorbild steel), Silogüter über alumina, Schüttgut über rawgravel oder gravel, Nahrungsmittel über food.

### Wichtig zu wissen
- **Spielstände hängen an `[list]`.** Ein Spielstand mit zwei eigenen Ressourcen lädt nicht ohne sie, ein Spielstand ohne eigene Ressourcen nicht mit ihnen. Änderungen also nur mit einem passenden Spielstand oder einem neuen Spiel. Die Übernahme aus `resources.ini` behält die Reihenfolge bei, deine Spielstände laden also weiter.
- Symbol: `media_soviet\resources\<name>.png` (48×48, RGBA), am besten über `tesmioloader\vfs\`.
- Frachtmodelle: `resources\<name>.nmf` bei Stückgut, `<name>1..4.nmf` plus `<name>_vehicle.nmf` bei Schüttgut.

---

## 🔗 Verhältnis zum Original-Plugin

### Koexistenz mit `resources.dll`

Resources Plus und das Original-Plugin `resources` nutzen:
- **denselben Registrierungsdienst:** `resources`
- **dasselbe Dateiformat**, nur getrennte Dateien: `plugins\resources_plus.ini` und `plugins\resources.ini`
- **dieselben Text-Nummern** für die Anzeigenamen (ab 1000000)
- **Kompatibilität:** Spielstände bleiben gültig, solange `[list]` in derselben Reihenfolge dieselben Einträge hat

### Falls beide installiert sind

**Resources Plus lädt nur, wenn das Original ausgeschaltet ist.**

Ist `plugins\resources.dll` vorhanden und in tesmioloader.ini eingeschaltet:
- Resources Plus bleibt untätig, bevor es irgendetwas patcht
- Im Log: `resources_plus  idle - plugins\resources.dll is present and enabled`
- Das Original lädt normal

Läuft der Soviet Mod Loader, gilt dasselbe: `resources_plus  idle - soviet_mod_loader.dll brings its own resources`.

**Lösung:** Original oder SML im TesmioLauncher oder in Republic Mod Manager ausschalten. Löschen ist nicht notwendig.

---

## 💾 Kompatibilität

### Speicherformat
Unverändert gegenüber dem Original: Die Anzahl der Ressourcen ist Teil des Spielstands, die Anzeigenamen bekommen dieselben Text-Nummern. Ein Spielstand, der mit dem Original oder unter SML entstand, lädt mit Resources Plus, solange `[list]` dieselben Einträge in derselben Reihenfolge hat. Genau das stellt die automatische Übernahme sicher.

### Versionskompatibilität
- 0.1.2: Sicherheitsrunde ohne neue Funktion: das Zollhaus-Abgleichen läuft unter demselben Lock wie das Verschieben der Ressourcentabelle und prüft die Push-Funktion des Spiels vor dem ersten Aufruf auf ihre Bytes; Transportklassen ausserhalb der 18 des Datensatzes werden nicht mehr gelesen; das Anmelden der Ressourcen und das Preis-Bracket schalten sich nach einem Fehler ab, statt ihn zu wiederholen; `packed = auto` tut wieder, was der Kommentar sagt; eine UTF-8-BOM in resources_plus.ini versteckt `[list]` nicht mehr; `resource_capacity`, `resource_vector_rva`, `packed` und eine Zahl bei `family` werden geprüft und mit WARN abgewiesen; Schreibfehler nennen den Windows-Fehlercode
- 0.1.1: Die Bremse gegenüber Soviet Mod Loader greift auch, wenn Resources Plus vor SML geladen wird (alphabetische Reihenfolge in `plugins`): `pluginssoviet_mod_loader.dll` vorhanden und eingeschaltet reicht. Ohne diesen Fix blockierte SML den Spielstart mit „resources registration hook was not installed".
- 0.1.0: erste Fassung als Fork von resources 1.7. Gleiche Funktionen, dazu der Fix beim Beenden, die eigene Datei mit automatischer Übernahme, die Prüfung doppelter Namen und die Bremse gegenüber Original und SML.

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| Keine eigenen Ressourcen im Spiel | Original oder SML eingeschaltet | Log lesen: `resources_plus  idle …`, dann das andere abschalten |
| „Spielstand braucht Mods“ beim Laden | `[list]` passt nicht zum Spielstand | Dieselben Einträge in derselben Reihenfolge eintragen |
| Eine Ressource fehlt, im Log steht „listed twice“ | Name zweimal in `[list]` | Zweite Zeile entfernen |
| Lager zeigt 0.00 von 0.00 t | Transportklasse des Vorbilds passt nicht zum `$STORAGE` des Gebäudes | Anderes Vorbild wählen oder `transport =` in `[custom:<name>]` setzen |
| Ressource nicht im Zollhaus handelbar | Vorbild `custom` ohne Marktwerte | Vorbild mit Marktwerten nehmen (z. B. food) oder `market_rub`/`market_usd` setzen |
| Kein Symbol | PNG fehlt | `media_soviet\resources\<name>.png`, 48×48 RGBA |

### Logging
- Zusammenfassung in `tesmioloader.log`: `plugin resources_plus 0.1.2`, beim ersten Start `created plugins\resources_plus.ini from …`, dann je Ressource `published as index …`
- Details in `logs\tesmioloader.resources_plus.log` (Datensätze, Preise)

---

## 📦 Dateistruktur

```
tesmioloader\build\
├── plugins\
│   ├── resources_plus.dll
│   ├── resources_plus.ini              (deine Liste; beim ersten Öffnen im RMM oder beim ersten Spielstart aus resources.ini übernommen)
│   ├── resources_plus.template.ini     (Vorlage aus dem Paket, nur ohne resources.ini benutzt)
│   └── resources.ini                   (Original-Plugin, bleibt unangetastet)
└── logs\
    └── tesmioloader.resources_plus.log
```

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Resources Plus ist ein Fork des Plugins `resources` aus dem TesmioLoader von MaxLegend (Tesmio), GPL v3, https://github.com/MaxLegend/TesmioLoader; Servicename, Text-Nummern und Dateiformat sind bewusst mit dem Original identisch geblieben. Der vollständige Quelltext von Resources Plus liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/resources_plus.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Kann ich beide Plugins gleichzeitig benutzen?**
A: Nein. Entweder das Original ODER Resources Plus. Solange das Original eingeschaltet ist, bleibt Resources Plus untätig.

**F: Muss ich meine resources.ini von Hand übertragen?**
A: Nein. Beim ersten Start kopiert Resources Plus sie nach `resources_plus.ini`, in derselben Reihenfolge. Danach ist `resources_plus.ini` die Datei, die zählt.

**F: Was ist mit dem Soviet Mod Loader?**
A: SML bringt das Original eingebaut mit. Unter SML pausiert Resources Plus, der Fix beim Beenden wirkt dann nicht. Willst du ihn, schalte SML aus und lade deine Pakete über die Workshop Bridge.

**F: Warum hat der Installer mein resources ausgeschaltet?**
A: Damit die fehlerbereinigte Fassung läuft und nicht beide gleichzeitig. Er sagt es in seiner Ausgabe, löscht nichts, und ein Schalter bringt das Original zurück.
