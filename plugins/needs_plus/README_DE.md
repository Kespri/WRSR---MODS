# 📦 Needs Plus 0.1.1

**Erweiterung des TesmioLoader-Plugins needs**

Eigene Bedürfnisse für *Workers & Resources: Soviet Republic* 1.1.1.9: Deine Bürger wollen eine weitere Ware, die Läden führen sie, alles aus einer INI heraus.

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
- Eine Ressourcenliste, in der die gewünschte Ware steht (Resources Plus oder das Original-Plugin resources)

### In drei Schritten
1. **Installieren:** über das Paket Republic Mod Manager (`Install-RMM.bat`). Der Installer schaltet Needs Plus ein und ein vorhandenes Original-Plugin needs aus und sagt beides in seiner Ausgabe.
2. **Spiel einmal starten:** Needs Plus legt `plugins\needs_plus.ini` an und übernimmt dabei deine vorhandene `plugins\needs.ini` unverändert. Deine Bedürfnisse sind sofort da.
3. **Weiter pflegen:** im Republic Mod Manager unter „Needs Plus“, oder direkt in `plugins\needs_plus.ini`.

---

## ✨ Features

### 🎯 Originalfunktionen (erhalten)
- ✅ Ein neues Bedürfnis je Zeile: `<ressource> = <spender>[, <faktor>[, <kategorie>[, <chance>[, <unzufriedenheit>]]]]`
- ✅ Das Bedürfnis wird vom Spender kopiert (food, meat, clothes, eletronics) und landet in genau den Läden, die den Spender führen
- ✅ Ladenkategorie wählbar: wie der Spender, nur Lebensmittel, nur Kaufhaus, keine
- ✅ Wahrscheinlichkeit und Unzufriedenheit je Bedürfnis
- ✅ Bis zu acht Bedürfnisse, Obergrenze je Bürger einstellbar

### 🆕 Verbesserungen (neu in Needs Plus)

#### 1️⃣ **Eigene Datei, automatisch übernommen**
Needs Plus liest `plugins\needs_plus.ini`. Fehlt sie, entsteht sie beim ersten Start aus dem, was du schon hast: aus `plugins\needs.ini` des Original-Plugins, in derselben Reihenfolge, mit einem Kopfkommentar, woher sie stammt. Das Original wird dabei nur gelesen, nie verändert. Stammt die `needs.ini` vom Soviet Mod Loader (der schreibt sie beim Spielstart mit allen Paketen zusammen neu), nimmt Needs Plus stattdessen SMLs Grundfassung, damit kein Paketbedürfnis doppelt erscheint.

#### 2️⃣ **Doppelte Namen werden abgewiesen**
Steht eine Ressource zweimal in `[list]`, zählt die erste Zeile, die zweite wird mit einer Zeile im Protokoll übergangen. Das Original hätte den Bürgern das Bedürfnis zweimal gegeben.

#### 3️⃣ **Tritt von selbst zur Seite**
Solange das Original eingeschaltet ist oder der Soviet Mod Loader läuft, bleibt Needs Plus untätig und schreibt eine Zeile ins Protokoll. Kein Bürger bekommt ein Bedürfnis doppelt.

---

## 💾 Installation

Needs Plus und das Original-Plugin needs laufen nie gleichzeitig. Der Installer des Republic-Mod-Manager-Pakets regelt das für dich; von Hand geht es genauso.

### Methode 1️⃣: Republic Mod Manager (Paket, empfohlen)

```
1. Das Paket Republic Mod Manager abonnieren und Install-RMM.bat ausführen
2. needs_plus.dll und die Vorlage needs_plus.template.ini landen in
   tesmioloader\build\plugins\ (eine vorhandene needs_plus.ini bleibt unangetastet)
3. Der Installer setzt in tesmioloader.ini: needs_plus=1 und, falls
   plugins\needs.dll vorhanden ist, needs=0 - und sagt es dir
4. Spiel starten: die Liste aus plugins\needs.ini wird übernommen
```

**Zurück zum Original:** im Republic Mod Manager Needs Plus aus- und needs einschalten, oder in `tesmioloader.ini` `needs=1` und `needs_plus=0` setzen. `Uninstall-RMM.bat` macht das ebenfalls. Die DLL des Originals wird nie gelöscht oder überschrieben.

### Methode 2️⃣: Von Hand

```
1. Kopiere needs_plus.dll → tesmioloader\build\plugins\
2. Aktiviere needs_plus im TesmioLauncher (oder needs_plus=1 in tesmioloader.ini)
3. Deaktiviere das Original-Plugin needs (needs=0)
4. Spiel starten: plugins\needs_plus.ini entsteht aus deiner needs.ini;
   ohne needs.ini nimmt das Plugin die Vorlage needs_plus.template.ini neben der DLL
```

### Methode 3️⃣: Soviet Mod Loader (SML)

⚠️ **Solange SML läuft, bleibt Needs Plus untätig.** SML bringt seine eigene Fassung des Original-Plugins mit. Willst du Needs Plus, schalte `soviet_mod_loader` in der `tesmioloader.ini` aus (im Republic Mod Manager der Schalter „Plugin aktiv“) und nimm Methode 1 oder 2. Beim nächsten Start übernimmt Needs Plus deine Liste aus SMLs Grundfassung, die Pakete stellt der Republic Mod Manager danach bereit.

---

## 🧰 Republic Mod Manager

- Der Eintrag **Needs Plus** bearbeitet `plugins\needs_plus.ini`: Liste, Spender, Faktor, Kategorie, Chance, Unzufriedenheit und die Plugin-Schalter. Fehlt die Datei noch, legt RMM sie beim Öffnen der Seite nach derselben Regel an wie das Plugin und sagt es in einem blauen Kasten. Der alte Eintrag „Needs“ bleibt als ausgeschaltetes Plugin in der Liste stehen.
- Der +-Dialog bietet nur Ressourcen an, die in der Ressourcenliste stehen (Resources Plus oder das Original).
- **Inhaltspakete** aus dem Workshop (`[content] needs`) landen in `plugins\needs_plus.ini`, sobald sie da ist, und lassen sich wieder zurücknehmen.
- Der Schalter **„Plugin aktiv“** bei Needs Plus schreibt den Eintrag in die `tesmioloader.ini`.
- Läuft SML, leuchtet Needs Plus **orange**: pausiert, nicht kaputt. Die Seite ist dann gesperrt, bis du „Trotzdem bearbeiten“ wählst.
- Das Protokollfenster zeigt `logs\tesmioloader.needs_plus.log`.

---

## ⚙️ Konfiguration

### Hauptdatei: `plugins\needs_plus.ini`

| Abschnitt | Inhalt |
|---|---|
| `[list]` | Eine Zeile je Bedürfnis: `<ressource> = <spender>[, <faktor>[, <kategorie>[, <chance>[, <unzufriedenheit>]]]]` |
| `[needs]` | Wie das Plugin das Spiel erreicht: `enabled`, `demand`, `storage`, `max_demands`, `when_full`, `probe`, `log_seconds` |

| Feld | Bedeutung |
|---|---|
| Spender | Vorhandenes Bürgerbedürfnis: `food`, `meat`, `clothes` oder `eletronics` (Schreibweise des Spiels) |
| Faktor | Menge im Verhältnis zum Spender, `1.0` = gleich viel |
| Kategorie | `auto` (wie der Spender), `grocery`, `advanced` (Kaufhaus), `none` (nur eigene Läden mit `$STORAGE_SPECIAL`) |
| Chance | 0 bis 1, wie oft ein Bürger das Bedürfnis überhaupt aufnimmt |
| Unzufriedenheit | 0 bis 1, was der Bürger je vergeblichem Einkauf an Zufriedenheit verliert |

Das Format ist das des Original-Plugins, jeder Schlüssel ist in der Datei selbst erklärt.

### Wichtig zu wissen
- **Die Ressource muss existieren.** Sie steht in der Ressourcenliste (Resources Plus: `plugins\resources_plus.ini`), und ihre Transportklasse muss zu den Läden des Spenders passen. Am einfachsten: die Ressource dort vom selben Spender klonen.
- **Spielstände hängen an der Liste.** Jedes Bedürfnis legt in den betroffenen Läden einen Lagerplatz an, und die Lagerliste ist Teil des Spielstands. Änderungen zuerst auf einer Kopie testen.

---

## 🔗 Verhältnis zum Original-Plugin

### Koexistenz mit `needs.dll`

Needs Plus und das Original-Plugin `needs` nutzen:
- **dieselben Einhakpunkte** im Spiel (Tagesplan der Bürger, Lagerplätze, Ladentakt)
- **dasselbe Dateiformat**, nur getrennte Dateien: `plugins\needs_plus.ini` und `plugins\needs.ini`
- **Kompatibilität:** Spielstände bleiben gültig, solange `[list]` dieselben Einträge hat

### Falls beide installiert sind

**Needs Plus lädt nur, wenn das Original ausgeschaltet ist.**

Ist `plugins\needs.dll` vorhanden und in tesmioloader.ini eingeschaltet:
- Needs Plus bleibt untätig, bevor es irgendetwas patcht
- Im Log: `needs_plus  idle - plugins\needs.dll is present and enabled`
- Das Original lädt normal

Läuft der Soviet Mod Loader, gilt dasselbe: `needs_plus  idle - soviet_mod_loader.dll brings its own needs`.

**Lösung:** Original oder SML im TesmioLauncher oder in Republic Mod Manager ausschalten. Löschen ist nicht notwendig.

---

## 💾 Kompatibilität

### Speicherformat
Unverändert gegenüber dem Original: Die Lagerplätze der Läden sind Teil des Spielstands. Ein Spielstand, der mit dem Original oder unter SML entstand, lädt mit Needs Plus, solange `[list]` dieselben Einträge hat. Genau das stellt die automatische Übernahme sicher.

### Versionskompatibilität
- 0.1.1: Sicherheitsrunde ohne neue Funktion: eigenes Protokoll `logs\tesmioloader.needs_plus.log` für `probe`, die Zusammenfassung bleibt in `tesmioloader.log`; Faktor, Chance und Unzufriedenheit einer Zeile sowie `max_demands`, `when_full` und `log_seconds` werden geprüft und mit einer WARN-Zeile auf den erlaubten Wert gesetzt; eine UTF-8-BOM in needs_plus.ini versteckt `[list]` nicht mehr; mehr als 16 Bedürfnisse werden einmal gemeldet statt still abgeschnitten; die Lagerplatz-Korrektur der Läden schaltet sich nach einem Fehler ab, statt ihn zu wiederholen
- 0.1.0: erste Fassung als Fork von needs 1.0. Gleiche Funktionen, dazu die eigene Datei mit automatischer Übernahme, die Prüfung doppelter Namen und die Bremse gegenüber Original und SML.

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| Keine eigenen Bedürfnisse im Spiel | Original oder SML eingeschaltet | Log lesen: `needs_plus  idle …`, dann das andere abschalten |
| Im Log „no resource named …“ | Ressource fehlt in der Ressourcenliste | Sie in Resources Plus anlegen, dann neu starten |
| Im Log „cannot be stored in transport class …“ | Transportklasse passt nicht zu den Läden des Spenders | Ressource in der Ressourcenliste vom selben Spender klonen |
| Ein Bedürfnis fehlt, im Log steht „listed twice“ | Name zweimal in `[list]` | Zweite Zeile entfernen |
| Bürger wollen die Ware, Läden haben sie nicht | `storage = 0` | `storage = 1` setzen oder die Läden selbst bestücken |

### Logging
- Zusammenfassung in `tesmioloader.log`: `plugin needs_plus 0.1.1`, beim ersten Start `created plugins\needs_plus.ini from …`, dann je Bedürfnis eine Zeile mit Spender, Faktor und Kategorie
- Mit `probe = 1` schreibt das Plugin die Bedürfnislisten der Bürger und die Lagerplätze der Läden in sein eigenes Protokoll `logs\tesmioloader.needs_plus.log`; ungültige Werte in der INI stehen dort als WARN-Zeile

---

## 📦 Dateistruktur

```
tesmioloader\build\
├── logs\
│   └── tesmioloader.needs_plus.log   (ausführliches Protokoll, entsteht beim ersten Start)
└── plugins\
    ├── needs_plus.dll
    ├── needs_plus.ini              (deine Liste; beim ersten Öffnen im RMM oder beim ersten Spielstart aus needs.ini übernommen)
    ├── needs_plus.template.ini     (Vorlage aus dem Paket, nur ohne needs.ini benutzt)
    └── needs.ini                   (Original-Plugin, bleibt unangetastet)
```

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Needs Plus ist ein Fork des Plugins `needs` aus dem TesmioLoader von MaxLegend (Tesmio), GPL v3, https://github.com/MaxLegend/TesmioLoader; Einhakpunkte und Dateiformat sind bewusst mit dem Original identisch geblieben. Der vollständige Quelltext von Needs Plus liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/needs_plus.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Kann ich beide Plugins gleichzeitig benutzen?**
A: Nein. Entweder das Original ODER Needs Plus. Solange das Original eingeschaltet ist, bleibt Needs Plus untätig.

**F: Muss ich meine needs.ini von Hand übertragen?**
A: Nein. Beim ersten Start kopiert Needs Plus sie nach `needs_plus.ini`. Danach ist `needs_plus.ini` die Datei, die zählt.

**F: Was ist mit dem Soviet Mod Loader?**
A: SML bringt das Original eingebaut mit. Unter SML pausiert Needs Plus. Willst du es, schalte SML aus und lade deine Pakete über die Workshop Bridge.

**F: Warum hat der Installer mein needs ausgeschaltet?**
A: Damit nicht beide gleichzeitig laufen. Er sagt es in seiner Ausgabe, löscht nichts, und ein Schalter bringt das Original zurück.
