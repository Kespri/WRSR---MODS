# 🌐 Localization 0.4.0

**TesmioLoader-Plugin für eigene Texte und Übersetzungen**

Fügt *Workers & Resources: Soviet Republic* 1.1.1.9 eigene Namen, Beschreibungen und Beschriftungen hinzu, ohne die originalen Sprachdateien anzufassen. Textpakete sind einfache INI-Dateien; beim Spielstart erzeugt das Plugin daraus erweiterte `soviet*.btf`-Sprachdateien im virtuellen Dateisystem (VFS) des TesmioLoaders und stellt anderen Plugins einen Übersetzungsdienst bereit. Das Paket enthält die Textpakete von Research Expansion, Technical Service Storage und das gemeinsame Paket `tesmio_lang`.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Konfiguration](#-konfiguration)
- [Textpakete](#-textpakete)
- [Sprachdateien](#-sprachdateien)
- [Verwendung in anderen Plugins](#-verwendung-in-anderen-plugins)
- [Wertebereiche](#-wertebereiche)
- [Kompatibilität](#-kompatibilität)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4 mit aktiviertem VFS
- Mindestens ein gültiges Textpaket (drei sind im Paket enthalten)

### In drei Schritten
1. **Eine Installationsmethode wählen** (siehe unten) und das Plugin aktivieren.
2. **Textpakete prüfen:** Die mitgelieferten Pakete brauchen keine Einstellung. Eigene Pakete kommen als Ordner nach `plugins\localization\<paketname>` mit `localization.ini` und mindestens der Fallback-Sprachdatei.
3. **Spiel vollständig neu starten.** `logs\tesmioloader.localization.log` nennt Konfigurationsdatei, Paketordner, geladene Pakete und die erzeugten Sprachdateien unter `tesmioloader\vfs\media_soviet`.

---

## ✨ Features

### 🎯 Grundfunktion
- ✅ Textpakete mit Namensraum: je Plugin ein Ordner mit `localization.ini` und `soviet<Sprache>.ini`-Dateien
- ✅ Fallback-Sprache je Paket und sichtbarer Platzhalter `[MISSING TEXT: …]` für fehlende Schlüssel
- ✅ Text-IDs im reservierten Bereich 2.000.000 bis 2.999.999, bei jedem Start deterministisch neu vergeben
- ✅ Dienst `localization` für andere Plugins: Schlüssel zu Text-ID
- ✅ Strenge Prüfung von Kodierung, Abschnitten, Schlüsseln, Escape-Sequenzen und BTF-Format; ein fehlerhaftes Paket wird allein abgewiesen
- ✅ Erzeugte Dateien werden vor jedem Start und beim Ausschalten wieder entfernt; die Originale unter `media_soviet` werden nie verändert

### 🆕 Neu in 0.4.0
- ✅ **INI und Textpakete neben der DLL:** Fehlt `plugins\localization.ini`, liest die DLL die INI aus dem eigenen Ordner, also aus dem Workshop-Paket unter Soviet Mod Loader oder der Workshop Bridge. Textpakete werden aus `plugins\localization` **und** aus dem Ordner `localization` neben der DLL geladen. Beide Pfade stehen im Log.
- ✅ **Lokale Textpakete werden zusammengeführt statt zu ersetzen:** Ein Paketordner unter `plugins\localization` legt sich Schlüssel für Schlüssel über den gleichnamigen Ordner neben der DLL. Deine eigenen Texte und Sprachen bleiben, neue Schlüssel und Sprachen aus einem Workshop-Update kommen trotzdem an; bei gleichem Schlüssel gilt dein lokaler Text. Die lokale `localization.ini` bestimmt Namensraum, Fallback und missingText.
- ✅ Die Textpakete `research_expansion`, `technical_service_storage` und `tesmio_lang` reisen im Paket mit.
- ✅ Republic Mod Manager: Reiter Allgemein (Hinweise, Anleitung, Fehlersuche) und Plugin („Erweiterung anwenden“ mit Abschaltreihenfolge, Textpakete als Liste mit Kurztext und Herkunft).

---

## 💾 Installation

Wähle **eine** der vier Methoden. Dieselbe DLL darf nie zweimal geladen werden, und der Dienstname `localization` darf nur einmal registriert sein.

---

### Methode 1️⃣: Klassischer TesmioLoader

```
1. Kopiere localization.dll und localization.ini aus hooks\
   → tesmioloader\build\plugins\

2. Kopiere den Ordner hooks\localization (die Textpakete)
   → tesmioloader\build\plugins\localization\

3. Aktiviere localization im TesmioLauncher
4. Spiel vollständig neu starten
```

---

### Methode 2️⃣: Soviet Mod Loader (SML)

```
1. Workshop-Objekt abonnieren – SML liest abonnierte Pakete von selbst
2. SML lädt die DLL über soviet.mod.ini aus dem Paket; INI und Textpakete liegen daneben
3. Eine lokale localization.dll in plugins\ vorher entfernen oder abschalten
```

---

### Methode 3️⃣: Workshop Bridge (ohne SML)

```
1. Paket in Republic Mod Manager auswählen
2. „Plugin aktiv“ einschalten
3. workshop_bridge lädt die DLL direkt aus dem Paket; INI und Textpakete findet sie neben sich
4. Steam-Updates gelten sofort
```

---

### Methode 4️⃣: Republic Mod Manager mit „Dateien nur lokal“

```
1. Paket in Republic Mod Manager auswählen, Einstellungen anpassen, Speichern
2. Reiter „Allgemein“, Karte „Hinweise“: „Dateien nur lokal“ einschalten
3. Bestätigung mit Dateiliste → DLL, INI und der Ordner localization mit den
   Textpaketen werden beim Speichern nach tesmioloader\build\plugins\ kopiert
4. Die Workshop Bridge überspringt das Paket danach automatisch
```

**„Dateien nur lokal“ im Detail**
- Für alle, die das Plugin ohne Steam-Abo weiterbenutzen wollen
- Steam-Updates gelten bei lokalen Dateien erst nach erneutem Speichern (gelbe Marke „Update“)
- Ausschalten entfernt nur die von Republic Mod Manager kopierten Dateien wieder

---

## 🧰 Republic Mod Manager

Das Paket enthält im Ordner `config` ein Darstellungsschema. Republic Mod Manager zeigt Localization damit auf einem Reiter, deutsch und englisch: Hinweise, „Dateien nur lokal“, Knopf für diese Anleitung, die beiden Schalter (Plugin aktiv, ausführliches Protokoll) und eine Übersicht der mitgelieferten Textpakete. Textpakete selbst werden nicht im Manager bearbeitet; sie sind Ordner mit INI-Dateien.

Republic Mod Manager schreibt die wirksame Datei `plugins\localization.ini` als UTF-8 ohne BOM; die INI im Paket bleibt unverändert.

---

## ⚙️ Konfiguration

### Hauptdatei: `localization.ini`

Die DLL liest in dieser Reihenfolge:
- **Zuerst:** `tesmioloader\build\plugins\localization.ini`, falls vorhanden (klassische Installation, „Dateien nur lokal“ oder die von Republic Mod Manager geschriebene wirksame INI)
- **Sonst:** die INI neben der DLL, im Paket `hooks\localization.ini` (Soviet Mod Loader, Workshop Bridge)

Die Hauptdatei muss **UTF-8 ohne BOM** sein. Erlaubt ist genau ein Abschnitt `[localization]` mit den Schlüsseln `enabled` und `verbose`, beide genau `0` oder `1`. Unbekannte oder doppelte Abschnitte und Schlüssel werden abgelehnt; dann bleibt das Plugin inaktiv und die Vanilla-Sprachdateien gelten.

```ini
[localization]
; 1 = Textpakete laden, Dienst bereitstellen, Sprachdateien erzeugen; 0 = erzeugte Dateien entfernen, inaktiv
enabled = 1
; 1 = technische Pfade im Detailprotokoll
verbose = 0
```

⚠️ **Vor dem Abschalten der DLL** erst `enabled = 0` setzen und das Spiel einmal starten: Nur so entfernt das Plugin die erzeugten `soviet*.btf`-Dateien aus dem VFS. Wird die DLL direkt im Launcher abgeschaltet, bleiben die erweiterten Sprachdateien liegen.

### Paketordner

Textpakete werden aus zwei Ordnern geladen:
- `tesmioloader\build\plugins\localization\<paketname>` (klassische Installation, „Dateien nur lokal“, eigene Pakete)
- der Ordner `localization` neben der DLL, im Paket `hooks\localization` (Soviet Mod Loader, Workshop Bridge)

Ein Paketordner unter `plugins\localization` legt sich Schlüssel für Schlüssel über den gleichnamigen Ordner neben der DLL: lokale Texte gewinnen, alles andere kommt weiter aus dem Paket, auch Sprachen und Schlüssel, die ein Workshop-Update neu bringt. So kannst du ein mitgeliefertes Textpaket lokal anpassen, ohne den Anschluss zu verlieren. Zusammen höchstens 256 Paketordner.

---

## 📦 Textpakete

Jedes Textpaket ist ein Ordner mit einer `localization.ini` und mindestens der Fallback-Sprachdatei:

```text
research_expansion\
├── localization.ini
├── sovietEnglish.ini
└── sovietGerman.ini
```

```ini
[localization]
namespace = research_expansion
fallback = sovietEnglish
missingText = [MISSING TEXT: {key}]
```

| Einstellung | Bedeutung |
|---|---|
| `namespace` | eindeutiger Name des Pakets: Kleinbuchstaben, Ziffern, `_`, `-`, bis 80 Zeichen |
| `fallback` | Sprache ohne `.ini`, die gilt, wenn die Spielsprache oder ein Text darin fehlt; die Datei muss vorhanden und gültig sein |
| `missingText` | Ersatztext, `{key}` wird durch den vollständigen Schlüssel ersetzt; ohne Angabe `[MISSING TEXT: {key}]` |

Paketkonfiguration und Sprachdateien dürfen UTF-8 mit oder ohne BOM sein. Zwei Pakete mit demselben Namensraum werden beide abgewiesen.

**Mitgelieferte Pakete**

| Paket | Namensraum | Inhalt |
|---|---|---|
| `research_expansion` | `research_expansion` | Namen und Beschreibungen neuer Forschungen von Research Expansion |
| `technical_service_storage` | `technical_service_storage` | Beschriftungen des Streuguttanks und der Depotwarnungen von Technical Service Storage |
| `tesmio_lang` | `tesmio_lang` | gemeinsame Texte, zum Beispiel `depletion.deposit_remaining` für die Vorkommen-Überschrift von Depletion |

---

## 🔤 Sprachdateien

Sprachdateien heißen wie die Sprachdateien des Spiels: `sovietEnglish.ini`, `sovietGerman.ini`, `sovietFrench.ini` und so weiter. Jede hat genau einen Abschnitt `[strings]`:

```ini
[strings]
quartz_smasher.name = Quarzbrecher
quartz_smasher.desc = Zerkleinert Quarz für die Glasproduktion.\nBraucht zusätzliche Arbeiter.
```

- Schlüssel: Kleinbuchstaben, Ziffern, `_`, `-` und einzelne Punkte, bis 160 Zeichen, je Datei nur einmal; der Namensraum wird automatisch vorangestellt (`research_expansion.quartz_smasher.name`)
- Escape-Sequenzen: nur `\n` (Zeilenumbruch) und `\\` (Backslash); jede andere macht die ganze Sprachdatei ungültig
- Ein Text darf `=` enthalten, getrennt wird am ersten Gleichheitszeichen; leere Texte sind nicht erlaubt; Kommentare nur in eigenen Zeilen mit `;` oder `#`

**Fallback:** Für jeden Text sucht das Plugin erst in der aktiven Spielsprache, dann in der Fallback-Sprache, zuletzt im Ersatztext. Ein Schlüssel, der in keiner Sprachdatei vorkommt, wird nicht registriert und liefert die ID 0.

---

## 🔌 Verwendung in anderen Plugins

Das Plugin stellt den TesmioLoader-Dienst `localization` in Version 1 bereit. Ein anderes Plugin fragt ihn in `TsmPluginStart()` ab:

```cpp
static const TsmLocalizationApi* L = nullptr;

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    L = (const TsmLocalizationApi*)H->consume(TSM_SERVICE_LOCALIZATION, TSM_LOCALIZATION_VERSION);
    if (!L) return 1;
    int nameId = L->resolveFull("research_expansion.quartz_smasher.name");
    int descId = L->resolve("research_expansion", "quartz_smasher.desc");
    return (nameId && descId) ? 0 : 1;
}
```

Die IDs liegen zwischen 2.000.000 und 2.999.999 und passen zu `$NAME` und `$DESC` in Spieldateien. Ungültiger Namensraum, ungültiger Schlüssel oder ein Schlüssel ohne Sprachdatei liefern 0.

---

## 📏 Wertebereiche

| Größe | Grenze |
|---|---|
| `enabled`, `verbose` | genau 0 oder 1 |
| Größe je INI-Datei | 16 MiB |
| Größe je gelesener oder erzeugter BTF-Datei | 256 MiB |
| Paketordner (beide Pfade zusammen) | 256 |
| registrierte Textschlüssel | 100.000 |
| Textlänge | 65.535 UTF-16-Codeeinheiten |
| Namensraum / Schlüssel | 80 / 160 Zeichen |
| reservierte IDs | 2.000.000 bis 2.999.999 |

---

## 💾 Kompatibilität

### Spielstände
Sprachdateien sind nicht Teil des Spielstands. Texte einer Forschung oder Ressource, die ein Spielstand kennt, brauchen nach einem Update denselben Schlüssel, damit die ID stabil bleibt: IDs werden in fester Reihenfolge der Pakete und Schlüssel vergeben.

### Andere Plugins
- **Research Expansion** braucht diesen Dienst zwingend.
- **Technical Service Storage** nutzt ihn für übersetzte Beschriftungen, läuft aber auch ohne (eingebaute Ersatztexte).
- **Depletion** ab 1.1.2 nutzt `tesmio_lang.depletion.deposit_remaining`, wenn der Dienst da ist.
- Andere Plugins, die `soviet*.btf` im VFS ersetzen, werden nicht zusammengeführt.

### Versionskompatibilität
- **0.4.0:** erste veröffentlichte Fassung; Research Expansion verlangt `tesmio.localization >= 0.4.0`

---

## ⚙️ Fehlerbehandlung

### Häufige Probleme

| Problem | Ursache | Lösung |
|---|---|---|
| `main-config`, `unknown-section`, `unknown-key`, `invalid-value` | Hauptdatei fehlerhaft | nur `[localization]` mit `enabled`/`verbose` als 0 oder 1 |
| `utf8-bom` | Hauptdatei mit BOM gespeichert | als UTF-8 ohne BOM speichern |
| `pack-root` | weder `plugins\localization` noch ein Ordner `localization` neben der DLL | Textpakete bereitstellen |
| `Pack was rejected because its localization.ini is invalid` | Pflichtangabe fehlt oder Wert ungültig | Paketkonfiguration prüfen |
| `Fallback ... has no valid language file` | Fallback-Datei fehlt oder ist fehlerhaft | Datei anlegen oder korrigieren |
| `Invalid escape sequence` | andere Sequenz als `\n` oder `\\` | Text korrigieren |
| `namespace-collision` | zwei Pakete mit demselben Namensraum | eines umbenennen; unter `plugins\localization` und neben der DLL zählt nur der Ordnername, nicht der Namensraum |
| `missing-key` | ein Plugin fragt einen Schlüssel ab, der in keiner Sprachdatei steht | Schlüssel in der Fallback-Datei anlegen |
| `id-collision` | vergebene Text-ID kollidiert mit einer vorhandenen | Log prüfen; alle erzeugten Dateien werden entfernt |
| `cleanup`, `cleanup-scan` | erzeugte VFS-Datei konnte nicht entfernt werden | Spiel und zugreifende Programme schließen |
| Texte fehlen im Spiel | Plugin nicht aktiv oder Paket abgewiesen | Log lesen: `Ready: N pack(s)` und die Fehler davor |

### Logging

Alle Meldungen stehen in `tesmioloader.log` und im Detail-Log `logs\tesmioloader.localization.log` mit Datei, Zeile, Regel und Begründung.
- In **Republic Mod Manager** öffnet das Symbol mit dem Dokument unten in der Plugin-Leiste die Protokollansicht mit Filter und Absender.

Suche nach:
- `Configuration file` und `Pack folders` → welche INI und welche Paketordner gewählt wurden
- `Ready:` → Anzahl geladener Pakete und Schlüssel
- `is skipped; plugins\localization has a folder of that name` → ein lokales Paket hat die Paketkopie überstimmt

---

## 📦 Dateistruktur

**Workshop-Paket** (Steam-Abo, SML, Workshop Bridge)
```
localization\
├── hooks\
│   ├── localization.dll            (Plugin)
│   ├── localization.ini            (Original-INI)
│   └── localization\               (Textpakete)
│       ├── research_expansion\
│       ├── technical_service_storage\
│       └── tesmio_lang\
├── config\                         (Schema für Republic Mod Manager)
│   ├── localization.launcher.ini
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
tesmioloader\
├── build\
│   ├── plugins\
│   │   ├── localization.dll
│   │   ├── localization.ini        (wirksame INI)
│   │   └── localization\           (Textpakete, eigene und mitgelieferte)
│   ├── logs\
│   │   └── tesmioloader.localization.log
│   └── tesmioloader.log
└── vfs\media_soviet\               (erzeugte soviet*.btf)
```

---

## 📜 Lizenz & Credits

**GNU GPL v3**, siehe `LICENSE` im Paket. Das Plugin enthält keinen fremden Code; der Loader-SDK-Header stammt aus dem TesmioLoader von MaxLegend (GPL v3). Der vollständige Quelltext liegt unter https://github.com/Kespri/WRSR-MODS/tree/main/plugins/localization.

**Genosse, Achtung:** Dieses Plugin wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

---

## ❓ FAQ

**F: Werden die Sprachdateien des Spiels verändert?**
A: Nein. Die erweiterten Dateien entstehen im VFS des Loaders; die Originale unter `media_soviet` werden nur gelesen.

**F: Wie füge ich eigene Texte hinzu?**
A: Ordner unter `plugins\localization\<name>` mit `localization.ini` (Namensraum, Fallback) und mindestens der Fallback-Sprachdatei anlegen, Schlüssel unter `[strings]`, Spiel neu starten.

**F: Kann ich ein mitgeliefertes Textpaket anpassen?**
A: Ja: den Ordner nach `plugins\localization` kopieren und dort ändern. Deine Schlüssel gewinnen gegen die Paketkopie, der Rest kommt weiter aus dem Paket, und Workshop-Updates lassen deine Dateien in Ruhe.

**F: Wie werde ich die erweiterten Sprachdateien wieder los?**
A: Plugin ausschalten, Spiel einmal starten, dann die DLL abschalten.

**F: Muss ich die INI von Hand bearbeiten?**
A: Nein. Die beiden Schalter bietet Republic Mod Manager an; Textpakete sind Ordner mit INI-Dateien.

---

**Letzte Aktualisierung:** Localization 0.4.0  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
