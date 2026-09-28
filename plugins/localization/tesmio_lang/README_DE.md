# 🗂️ tesmio_lang

**Gemeinsames Textpaket für TesmioLoader-Plugins**

Ein Localization-Textpaket mit dem Namensraum `tesmio_lang`, in dem mehrere Plugins ihre wenigen Beschriftungen teilen, ohne je einen eigenen Paketordner zu brauchen. Es wird vom Localization-Plugin beim Spielstart geladen und liefert derzeit die Beschriftung der Zeile „Restvorkommen“ von Depletion.

---

## 📋 Inhaltsverzeichnis

- [Schnellstart](#-schnellstart)
- [Schlüssel](#-schlüssel)
- [Eigene Schlüssel ergänzen](#-eigene-schlüssel-ergänzen)
- [Verwendung durch Plugins](#-verwendung-durch-plugins)
- [Fehlerbehandlung](#-fehlerbehandlung)
- [Dateistruktur](#-dateistruktur)

---

## 🚀 Schnellstart

### Voraussetzungen
- Localization-Plugin (im Paket enthalten oder klassisch installiert)
- Ein Plugin, das den Namensraum benutzt, derzeit Depletion 1.1.2

### In drei Schritten
1. Das Textpaket liegt im Localization-Paket unter `hooks\localization\tesmio_lang` und wird von dort geladen; bei klassischer Installation den Ordner nach `plugins\localization\tesmio_lang` kopieren.
2. Localization aktivieren.
3. Spiel vollständig neu starten. Im Protokoll von Depletion steht `localization key tesmio_lang.depletion.deposit_remaining resolved to …`.

---

## 🔤 Schlüssel

| Schlüssel | Deutsch | Englisch | Verwendet von |
|---|---|---|---|
| `depletion.deposit_remaining` | Restvorkommen | Deposit remaining | Depletion ab 1.1.2, Zeile im Minenfenster |

Vollständiger Schlüssel: `tesmio_lang.depletion.deposit_remaining`. Die Beschriftung steht ohne Doppelpunkt und ohne Zahlen; Depletion ergänzt Rest, Referenz, t/kt/Mt und Prozent selbst, zum Beispiel „Restvorkommen: 11.0 kt / 11.5 kt (95.7 %)“.

Paketkonfiguration:

```ini
[localization]
namespace = tesmio_lang
fallback = sovietEnglish
missingText = [MISSING TEXT: {key}]
```

---

## ✏️ Eigene Schlüssel ergänzen

- Neue Schlüssel nach dem Muster `<plugin>.<textname>` unter dem vorhandenen `[strings]` eintragen; keinen zweiten `[strings]`-Abschnitt anlegen.
- Jeden Schlüssel mindestens in `sovietEnglish.ini` definieren, weil Englisch die Fallback-Sprache ist.
- Weitere Sprachen über `soviet<Sprache>.ini` mit eigenem `[strings]` ergänzen; UTF-8, Umlaute erlaubt.
- Die Depletion-Beschriftung höchstens 63 UTF-16-Codeeinheiten und einzeilig; kurze Titel verhindern ein Überlaufen des Spielfensters.
- Ein INI-Eintrag allein ersetzt keine fest eingebaute Beschriftung: Das jeweilige Plugin muss den Schlüssel ausdrücklich über diesen Namensraum abrufen.

Die übrigen Regeln (Schlüsselzeichen, Escape-Sequenzen `\n` und `\\`, Größen) stehen in der Anleitung von Localization.

---

## 🔌 Verwendung durch Plugins

Ein Plugin holt den Dienst `localization` in `TsmPluginStart()` und löst den vollständigen Schlüssel auf:

```cpp
int id = L->resolveFull("tesmio_lang.depletion.deposit_remaining");
```

Depletion ab 1.1.2 nutzt bei aktivem Localization die Übersetzung der gewählten Spielsprache, sonst den englischen Fallback des Pakets. Fehlt Localization, der Schlüssel oder die native Textabfrage, bleibt die Ausgabe über `[depletion] panel_caption` (Standard „Deposit remaining“). Auch eine leere, zu lange oder mehrzeilige Beschriftung führt zum Ersatztext, nicht zur Abschaltung des Abbaus. Texte werden nie als Formatbefehle ausgeführt; Zahlen und Berechnungen kommen weiterhin aus Depletion.

---

## ⚙️ Fehlerbehandlung

| Meldung | Ursache | Vorgehen |
|---|---|---|
| `localization key tesmio_lang.depletion.deposit_remaining missing/invalid` (Depletion) | Paket nicht geladen oder Schlüssel fehlt | Ordner und `sovietEnglish.ini` prüfen; Localization-Protokoll lesen |
| `Pack was rejected because its localization.ini is invalid` (Localization) | Paketkonfiguration fehlerhaft | genau ein Abschnitt `[localization]` mit `namespace` und `fallback` |
| `namespace-collision` | ein zweites Paket mit Namensraum `tesmio_lang` | eines der beiden entfernen; gleiche Ordnernamen unter `plugins\localization` und neben der DLL werden zusammengeführt, nicht doppelt geladen |
| Beschriftung bleibt englisch | Spielsprache ohne Sprachdatei | `soviet<Sprache>.ini` ergänzen |

Änderungen an Sprachdateien gelten nach einem vollständigen Neustart des Spiels.

---

## 📦 Dateistruktur

```
tesmio_lang\
├── localization.ini      (Namensraum, Fallback, Ersatztext)
├── sovietEnglish.ini     (Fallback-Sprache)
├── sovietGerman.ini
├── README_DE.md
└── README_EN.md
```

---

**Letzte Aktualisierung:** tesmio_lang, Localization 1.2  
**Für:** WRSR 1.1.1.9 | TesmioLoader API 4
