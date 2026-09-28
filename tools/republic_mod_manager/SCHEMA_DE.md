# Republic Mod Manager – Paket- und Schema-Referenz

[English](SCHEMA_EN.md) | **Deutsch**

Das ist die Referenz für Plugin-Autoren: was ein Workshop-Paket braucht, damit Republic Mod Manager (RMM) es anzeigt, und wie eine Einstellungsseite beschrieben wird. Weder das Manifest noch ein Schema enthält ausführbare Befehle; alles ist einfacher INI-Text.

---

## Inhalt

1. [Das Manifest (`soviet.mod.ini`)](#1-das-manifest-sovietmodini)
2. [Einstellungsseite ohne Schema](#2-einstellungsseite-ohne-schema)
3. [Presentation-Schema (`<name>.launcher.ini`)](#3-presentation-schema)
4. [Sammlungen](#4-sammlungen)
5. [Lokale Schemas für installierte Plugins](#5-lokale-schemas-für-installierte-plugins)
6. [Editor-Schemas: Listen und Abschnitte](#6-editor-schemas)
7. [Verweisprüfung](#7-verweisprüfung)
8. [Sprachen](#8-sprachen)
9. [Symbole und Pfadsicherheit](#9-symbole-und-pfadsicherheit)
10. [Wie RMM ein Paket bereitstellt](#10-wie-rmm-ein-paket-bereitstellt)
11. [Inhaltspakete (`[content]`)](#11-inhaltspakete-content)

---

## 1. Das Manifest (`soviet.mod.ini`)

Das kleinste gültige Manifest ist das des Soviet Mod Loaders:

```ini
[mod]
id = example.my_plugin
name = My Plugin
version = 1.0.0

[hooks]
dll = hooks\my_plugin.dll
```

Daraus leitet RMM ab: das Ziel `my_plugin` (der DLL-Dateiname), die Standard-INI `hooks\my_plugin.ini` neben der DLL (falls vorhanden), die persönliche Konfiguration `user_config\my_plugin.ini` und das Schema `config\my_plugin.launcher.ini` (falls vorhanden, sonst aus der INI erzeugt).

Genau eine `dll`-Zeile ist erlaubt. Wiederholte `dll`-Zeilen, Pfadausbrüche, doppelte IDs und ein `target`, das nicht zum DLL-Namen passt, werden mit genauer Ursache abgewiesen.

Optionale Übersteuerungen:

```ini
[mod]
enabled = 1
tesmio_api_min = 4
tesmio_api_max = 4

[configuration]
defaults = hooks\my_plugin.ini          ; Standard-INI
launcher_schema = config\my_plugin.launcher.ini
user_config = my_plugin.ini             ; nur Dateiname, liegt neben der DLL
user_overlay = 0                        ; 1: die DLL liest user_config\<target>.ini selbst
local_copy = 0                          ; 1: bietet "Dateien nur lokal" an
replaced_by_sml =                       ; resources | deposits | needs | buildings

[assets]
dir = hooks\my_plugin                   ; Ordner, der bei "Dateien nur lokal" mit der DLL reist

[autoload]
format = 1
kind = plugin
target = my_plugin
requires_local = resources|other_plugin
conflicts_local = old_name|duplicate_name

[dependencies]
other.plugin = >=1.2.0
```

- `tesmio_api_min` / `tesmio_api_max`: wenn angegeben, muss API 4 im Bereich liegen.
- `user_overlay = 1`: Die DLL legt `user_config\<target>.ini` selbst über ihre INI. RMM stellt dann die Original-INI unverändert bereit und schreibt persönliche Werte nur nach `user_config`.
- `replaced_by_sml = deposits`: Dein Plugin macht dasselbe wie eine der vier Fähigkeiten, die Soviet Mod Loader eingebaut mitbringt (`resources`, `deposits`, `needs`, `buildings`), und tritt beim Spielstart zur Seite, solange SML läuft. RMM zeigt den Eintrag dann mit orangem Punkt statt grau und schreibt in die Karte „Hinweise“, wer gerade zuständig ist. Ein anderer Wert als die vier weist das Paket ab. Dein Plugin muss selbst zur Seite treten — der Schlüssel beschreibt nur, was es tut.
- `local_copy = 1`: Die Karte „Hinweise“ bietet bei Paketen unter der Workshop Bridge den Schalter „Dateien nur lokal“ an. RMM kopiert DLL, INI und den Ordner aus `[assets] dir` nach `plugins\` (den Ordner unter seinem eigenen Namen, `plugins\my_plugin\...`). Im Assets-Ordner sind keine `.dll`- oder `.exe`-Dateien erlaubt, höchstens 512 Dateien zu je 64 MB, keine Reparse-Punkte.
- `[dependencies]`: `mod.id = <Bedingung>` mit `>=`, `>`, `=`, `<=`, `<`, einer bloßen Version (mindestens) oder `*`. Wird gegen die Pakete im Workshop-Ordner aufgelöst; ein klassisch installiertes Plugin `plugins\<name>.dll` zählt auch, wenn `<name>` der letzte Teil der Kennung ist (Version ungeprüft). Ein Hook-Paket in der Bridge-Liste gilt als erfüllt. Nicht erfüllte Abhängigkeiten verhindern die Bereitstellung.
- `[content]` ohne `[hooks] dll` ist ein reines Inhaltspaket: Soviet Mod Loader führt es beim Spielstart selbst zusammen, ohne ihn stellt RMM es über den Schalter „Im Spiel bereitstellen“ in die Editoren der Zielplugins (Kapitel 11).
- `[content] allow_settings = 1` ist ein Schalter des Soviet Mod Loaders: ohne ihn nimmt er aus einem Paket nur `[list]`, `[base_price]` und `[price]` (bei Bedürfnissen nur `[list]`) und verwirft jeden `[custom:…]`-Abschnitt mit einer Konfliktnotiz. Mit ihm gelten sie — dafür lässt er dann auch den Einstellungsabschnitt des Plugins durch, in die Fragmentdatei gehören also nur `[list]` und `[custom:…]`. RMM liest die Abschnitte ohnehin und beschwert sich über den Schlüssel nicht.

---

## 2. Einstellungsseite ohne Schema

Ohne Schema baut RMM eines aus der Standard-INI:

- jeder INI-Abschnitt wird eine Karte auf dem Reiter „Allgemein“;
- jeder Schlüssel wird ein Feld, sein Name die Beschriftung;
- der Kommentarblock direkt über dem Schlüssel und ein Kommentar hinter dem Wert werden zur Beschreibung, Zeilenumbrüche bleiben erhalten;
- der Typ folgt dem Wert: `0`/`1` wird `boolean`, außer der Schlüsselname nennt eine Menge (`count`, `days`, `frequency`, `size`, `limit`, `level`, `scale`, `mode` und ähnliche) oder der Kommentar nennt andere ganze Zahlen oder einen Bereich (`2-12`, `1..6`, „up to 400“); Versionsangaben wie `v1.6` zählen nicht. Namen wie `enabled`, `debug`, `use_*`, `*_enabled` oder ein Kommentar, der beide Zustände nennt, erzwingen `boolean`. Andere Ganzzahlen werden `integer`, Dezimalzahlen `decimal`, alles andere `text`;
- ein Schlüssel `enabled` vom Typ `boolean` wird zum Aktivierungsfeld;
- der Kommentarblock am Dateianfang wird zur Paketbeschreibung.

In diesem Modus darf eine lokale INI Schlüssel enthalten, die die Paket-INI nicht kennt; die strenge Schlüsselprüfung gilt nur mit Schema. Ein Kommentar hinter dem Wert wie `130 ; (stock 121)` bleibt in der Datei, bis der Wert geändert wird.

---

## 3. Presentation-Schema

`config\<name>.launcher.ini` beschreibt Reiter, Karten, Felder und Texte. Jeder wirksame Schlüssel der INI muss als Feld oder Sammlungselement beschrieben sein.

### 3.1 `[launcher]`

```ini
[launcher]
layout_version = 1
visible = 1
id = example.my_plugin
name = My Plugin
config = my_plugin.ini
description = Rückfalltext
description_key = plugin.description
language_directory = config\languages
icon = builtin:gear
enabled_field = general/enabled        ; optional; boolesches Feld für den Aktivierungsschalter
default_tab = general
maximum_value_length = 63              ; Standard 4096
notice = ...                           ; paketweiter Kasten in der Karte Hinweise des ersten Reiters
notice_key = plugin.notice
notice_style = warning                 ; warning (gelb) oder info (blau)
info = ...                             ; zweiter, immer blauer Kasten unter dem Hinweis
info_key = plugin.info
```

`\n` in jeder Beschreibung, jedem Hinweis und jedem Hilfetext ist ein Zeilenumbruch. Ein Pfad wie `plugins\needs.ini` in so einem Text wird deshalb zerrissen; dort `/` schreiben.

### 3.2 Reiter und Gruppen

```ini
[tab:general]
label = Allgemein
label_key = tab.general
order = 10

[group:general]
tab = general
label = Allgemeine Einstellungen
label_key = group.general
description = ...
description_key = group.general.description
layout = fields                        ; oder matrix
order = 10
```

Es gibt keine feste Reiterzahl. Felder ohne Gruppe landen im gemeinsamen Bereich `settings`, angezeigt als „Allgemein“. Jeder ausdrückliche Reiter- und Gruppenverweis muss existieren. Eine Gruppe ohne sichtbare Felder und ohne Texte wird nicht gezeichnet.

Eine Gruppe mit `layout = matrix` ordnet ihre Felder über `row`, `column`, `row_label`, `column_label`, `unit` und `icon` an; die `_key`-Varianten übersetzen. Doppelte Zellen sind ungültig.

### 3.3 Felder

```ini
[field:limit]
section = general
key = limit
label = Grenze
label_key = field.limit
description = Was es im Spiel verändert.
description_key = field.limit.description
type = integer
minimum = 0
maximum = 10000
step = 1
group = general
order = 20
```

Typen:

| Typ | Wert |
|---|---|
| `boolean` | `0` oder `1`, als Schalter gezeigt |
| `integer` | Ganzzahl innerhalb `minimum`/`maximum`; `step` ist die Schrittweite der Plus/Minus-Knöpfe (Standard 1) |
| `decimal` | endliche Dezimalzahl innerhalb der Grenzen; `step` Standard 0.1 |
| `choice` | genau ein Wert aus `choices = a|b|c` |
| `text` | freier Text |
| `readonly` | wird gezeigt, nicht bearbeitet |

Jedes `[field:]` bindet genau einen vorhandenen Wert der Standard-INI. Das Aktivierungsfeld erscheint nicht in einer Karte; es gehört zum Schalter „Plugin aktiv“ im Kopf.

### 3.4 Knöpfe, die Paketdateien öffnen

```ini
[links]
tab = general
label_key = plugin.links
root = ..                              ; relativ zum Schema-Ordner

[link:readme_de]
file = README_DE.md                    ; .md, .txt, .html oder .pdf, kein ..
label_key = plugin.link.readme
language = de                          ; optional: nur in dieser Oberflächensprache
order = 10
```

### 3.5 Aktionszeile

```ini
[action:prune]
group = bridge                         ; Karte, in der die Zeile steht; sortiert mit den Feldern nach order
label = Bridge-Liste aufräumen
label_key = bridge.prune
description = ...
description_key = bridge.prune.description
button = Jetzt aufräumen
button_key = bridge.prune.button
command = bridge_prune                 ; bisher einziger Befehl
order = 35
```

`bridge_prune` entfernt Einträge aus der Paketliste der Bridge, deren Paket nicht in der RMM-Liste steht. Der Knopf wird nie gedehnt.

### 3.6 Ordnerliste

```ini
[folder_list:packs]
group = packs
paths = package:hooks\localization | build:plugins\localization   ; jeder Unterordner wird eine Zeile
description_prefix = loc.pack          ; Kurztext je Ordner aus der Sprachdatei: <prefix>.<ordnername>
note = ...
note_key = loc.packs.note
order = 10
```

Jede Zeile nennt den Ordner, seinen Kurztext und wo er liegt: „im Paket“ (`package:`-Pfad), „lokal“ (`build:`/`vfs:`-Pfad) oder „lokal, überlagert das Paket“ (beides). Pfadpräfixe: `package:` = `[links] root`, `build:` = Loader-Ordner, `vfs:` = VFS-Wurzel des Loaders (neben `build`, sonst `build\vfs`).

---

## 4. Sammlungen

Eine Sammlung baut eine benutzerverwaltete Liste und eine Wertematrix aus einem lokalen Plugin-Katalog auf (Vehicle Materials nutzt das):

```ini
[collection:materials]
source = local-plugin:resources/list   ; liest nur diesen Abschnitt aus build\plugins\resources.ini
source_ready_section = resources       ; optionale Bereitschaftsbedingung
source_ready_key = hook
source_ready_value = 2

resource_group = resources             ; Gruppe mit der Liste
matrix_group = vehicles                ; Gruppe mit der Matrix
count_section = resources              ; für diesen Schlüssel muss ein readonly-Feld existieren
count_key = count
item_prefix = resource                 ; resource0, resource1, ...
item_label = Material
item_label_key = material_number
item_description_key = field.resource.description

target_sections = road|rail|ship|airplane
target_labels = Straßenfahrzeuge|Schienenfahrzeuge|Schiffe|Flugzeuge
target_label_keys = row.road|row.rail|row.ship|row.airplane
target_icons = builtin:truck|builtin:rail|builtin:ship|builtin:plane
unit = Koeffizient
unit_key = unit.coefficient
coefficient_description_key = coefficient.description

empty_notice = Noch keine Einträge. Füge zuerst einen hinzu.
empty_notice_key = collection.empty_notice
empty_notice_style = warning           ; warning oder info; erscheint in matrix_group, solange die Liste leer ist

type = decimal
minimum = 0
maximum = 1000000
step = 0.001
default = 0
maximum_items = 32
ownership = user                       ; eine vorhandene Liste überlebt eine Änderung der Paketstandards
allow_remove_defaults = 1              ; vom Paket gelieferte Einträge dürfen gelöscht werden
require_positive_when_enabled = 1      ; mindestens ein positiver Wert, solange das Plugin an ist
```

Die vier `target_*`-Listen müssen gleich lang sein. Hinzufügen erzeugt lückenlose `item_prefix0`, `item_prefix1`, … plus einen Wert je Zielabschnitt; Entfernen löscht alle Werte und verdichtet die Liste. `require_positive_when_enabled` schaltet das Plugin aus, wenn der letzte positive Eintrag gelöscht wird; die Fehlermeldung nennt den sichtbaren Reiter und die Gruppe.

---

## 5. Lokale Schemas für installierte Plugins

Ein Plugin, das nur als `plugins\<name>.dll` im Loader-Ordner liegt, bekommt sein Schema aus `settings_schemas\<name>.launcher.ini` neben `rmm.exe`, falls diese Datei existiert, sonst aus seiner INI (Abschnitt 2). Ein solches lokales Schema ist ein gewöhnliches Presentation-Schema:

```ini
[launcher]
layout_version = 1
visible = 1
id = local.walking
name = Walking Distance
config = walking.ini                   ; muss <name>.ini sein
enabled_field = walking/enabled
language_directory = languages         ; relativ zu settings_schemas
```

RMM liefert solche Schemas für accumulator, cities, daynight, depletion, easystart, walking und die Workshop Bridge mit; die deutschen Texte stehen in `settings_schemas\languages\de.ini` unter dem Präfix `<name>.`, die englischen Rückfalltexte im Schema selbst. Ein lokales Schema beschreibt, es verbietet nicht: Werte dürfen einen Kommentar tragen (er bleibt, bis der Wert geändert wird), und Schlüssel, die das Schema nicht kennt, werden in den Hinweisen genannt und unverändert zurückgeschrieben. Freie Werte wie `auto` oder Jahreszahlen mit `off`/`always` verwenden `type = text`.

Installierte Plugins werden als geschützte Basis geführt: Original unter `user_config\.autoload\<name>.upstream.ini`, wirksame INI in `plugins\`, persönliche Werte in `user_config\<name>.ini`. Eine außerhalb von RMM geänderte `plugins\<name>.ini` wird zum neuen Original.

---

## 6. Editor-Schemas

Für INIs, die Listen statt einzelner Werte halten, gibt es drei Editor-Typen. Das Schema liegt in `settings_schemas\` (installierte Plugins) oder im Paket unter `config\<name>.launcher.ini`; im Paket müssen `[editor] plugin` und `config` DLL-Name und INI des Pakets nennen. Die INI des Pakets ist die Originalbasis, persönliche Einträge liegen in `user_config\<name>.editor.ini`, die wirksame Datei ist `plugins\<name>.ini`. DLL, Loader-Eintrag, Bridge-Liste und „Dateien nur lokal“ laufen wie bei jedem Paket.

| `editor_type` | Aufbau der INI | Beispiel |
|---|---|---|
| `keyed_resources` | Listenabschnitt plus ein Abschnitt je Eintrag (`[custom:<id>]`) und Schlüsselwerte | Resources |
| `keyed_list` | eine Zeile je Eintrag: `<id> = <Spalte 1>, <Spalte 2>, ...` | Needs, Technical Service Storage, UI Layout Fixes |
| `keyed_sections` | ein Abschnitt je Eintrag | Deposits, Deposits Plus, Research Expansion, Vanilla Buildings |

### 6.1 `[editor]`, `[activity]`, `[source]`

```ini
[launcher]
layout_version = 1
editor_type = keyed_list
id = tesmio.needs.editor
name = Needs
language_directory = languages
default_tab = needs
notice = ...                           ; wie im Presentation-Schema
info = ...

[editor]
plugin = needs
config = needs.ini
list_section = list                    ; keyed_list / keyed_resources
item_section_prefix = custom:          ; keyed_resources
reserved_sections = deposits           ; keyed_sections: Abschnitte, die KEIN Eintrag sind
section_prefix = modify:               ; keyed_sections: nur Abschnitte mit diesem Präfix sind Einträge
maximum_items = 8

[activity]                             ; das Ein/Aus-Feld des Plugins; nicht in einer Karte gezeigt,
section = needs                        ; es gehört zum Schalter "Plugin aktiv"
key = enabled
values = 1

[source]                               ; Kennungen für den +-Dialog
plugin = resources                     ; Schlüssel des Abschnitts aus plugins\resources.ini
section = list
ready_section = resources              ; optional: Quelle gilt nur mit [resources] hook = 2
ready_key = hook
ready_value = 2
```

Ohne `[source]` fragt der +-Dialog die Kennung als editierbares Feld ab und schlägt die Grundspiel-Ressourcen plus die Liste des Resources-Plugins vor. `[list] id_suggestions = 0` lässt diese Vorschläge weg (für Text-IDs und Ähnliches). Kennungen folgen den üblichen Regeln: Buchstaben, Ziffern, `_`, `-`, `.`.

### 6.2 Reiter, Listentexte, Dialogtexte

```ini
[tab:general]
label_key = tab.general
order = 10

[list]
label_key = needs.list                 ; Listentitel
add_label_key = needs.add              ; +-Knopf
select_help_key = needs.select_help    ; Hilfetext, solange nichts gewählt ist
id_label_key = needs.resource          ; Beschriftung der Kennung
id_help_key = needs.resource.help      ; Hilfe unter der Kennung im Detailbereich
add_id_help_key = needs.add_help       ; Hilfe unter der Kennung im +-Dialog (sonst id_help)
save_warning_key = needs.save_warning  ; gelbe Spielstandwarnung auf der Listenkarte
note_key = needs.note                  ; kleiner Text links unter der Liste, neben +
summary = type|map                     ; Schlüssel, die in der Liste unter dem Namen stehen
id_picker = game_texts                 ; game_texts: Knopf "Text auswählen..."; game_research: "Forschung wählen..."
id_suggestions = 0
remove_label_key = re.remove           ; Knopf "nur Eintrag" im Löschdialog von Einträgen mit Anhang

[new]                                  ; +-Dialog bei keyed_sections
name_label_key = deposits.new_name
token_key = token                      ; Feld, das der Dialog aus dem Namen vorschlägt
token_template = $TYPE_MINE_{NAME}
hint_key = deposits.new_hint           ; Text am Ende des Dialogs

[group:materials]                      ; die Listenkarte
tab = materials
label_key = tss.group.materials
description_key = tss.group.materials.description
notice_key = tss.group.notice          ; blauer Kasten zwischen Beschreibung und Spielstandwarnung
id_reference = resources               ; jede eigene Kennung muss existieren (Abschnitt 7)

[global]                               ; Karte für plugin-weite Werte (scope = global)
tab = general
label_key = group.needs.plugin
description_key = ...
notice_key = ...
order = 10                             ; sortiert unter die [card:]-Karten des Reiters; 0 = zuerst

[card:log]                             ; weitere Karten für plugin-weite Felder
tab = general
label_key = tss.card.log
notice_key = ...
notice_style = info                    ; blau statt gelb
order = 20
```

**Kartenliste statt Liste und Detailbereich.** Mit `style = cards` in `[list]` wird aus der Liste eine Zeile je Eintrag, und die Felder wandern in einen Dialog hinter „Ändern…":

```ini
[list]
style = cards
title = bp_name                 ; Feld für die Überschrift der Zeile
subtitle = bp_id|bp_object|bp_donor   ; die graue Zeile darunter
toggle = bp_enabled             ; Schalter links, muss type = boolean sein
state = buildings_plus          ; eingebauter Zustandsgeber, sonst keine Zustandsspalte
actions = edit|open|delete      ; Ändern… / Öffnen / Papierkorb
```

`title`, `subtitle` und `toggle` nennen Feld-Kennungen; ist eine davon unbekannt, wird das Schema abgewiesen. Ein Suchfeld erscheint ab acht Einträgen, sortiert wird nach der Überschrift. Ein Original (aus der wirksamen INI oder aus einem Inhaltspaket) trägt ein Schloss und wird ausgeblendet statt gelöscht.

Im Dialog liegt jedes Feld mit `card = global` auf dem ersten Reiter; jeder weitere `card`-Wert wird ein eigener Reiter, beschriftet aus `[card:<id>]`. Eine solche Karte kann statt Feldern den Zeilen-Editor tragen:

```ini
[card:lines]
tab = buildings
label_key = bp.card.lines
editor = donor_lines            ; Spenderdatei links, deine Zeilen rechts
lines = bp_line                 ; das Feld mit den building.ini-Zeilen
strip = bp_strip                ; das Feld mit den zu entfernenden Token
```

Der Editor zeigt links die `building.ini` des Spenders (umschaltbar auf das Ergebnis), streicht durch, was deine Deklaration wegwirft, und nennt unter jeder deiner Zeilen, welche Spenderzeilen sie umbringt — mit einem Knopf, der sie als eigene Zeilen zurückholt. In der Ergebnisansicht trägt jede `$STORAGE`-Zeile ihre Nummer für `$RESOURCE_VISUALIZATION`, und ein Wasser- oder Abwasserlager vor einem angezeigten Lager wird gemeldet. Die Ergebnisansicht ist nur lesend. Zwei deiner Zeilen, die dieselbe Einstellung setzen, werden gemeldet und beim Hinzufügen abgefragt.

Der zweite Zeilen-Editor arbeitet auf echten Zieldateien statt auf einem Spender:

```ini
[card:commands]
tab = buildings
label_key = vb.card.commands
editor = target_lines           ; Zieldatei links, die Befehle rechts
targets = target                ; das Feld mit den Zieldateien
add = add                       ; die vier Zeilenbefehle
replace = replace
remove = remove
insert = insert
add_connection = add_connection ; die drei Anschlussbefehle, gleiche Karte
replace_connection = replace_connection
remove_connection = remove_connection
```

Oben wählst du die Zieldatei — ein Regelsatz kann viele haben, und jeder Befehl wird gegen jede einzeln geprüft. Links stehen die Zeilen der unveränderten Datei, angeklickt werden sie zum Anker; die Knöpfe **Anhängen, Ersetzen, Davor, Danach, Entfernen** schreiben daraus den fertigen Befehl. Rechts stehen die Befehle, ein Befehl, der in dieser Datei nicht greift, steht rot, und darunter steht warum (nicht gefunden, mehrfach vorhanden, `$COST_`-Zeile, bereits vorhanden …). Eine Zieldatei, die es auf diesem Rechner nicht gibt, schaltet die Knöpfe ab.

Anschlüsse sind Blöcke, keine Zeilen. Klickst du links auf eine Punktzeile, nimmt der Editor den ganzen Anschluss (Token-Zeile plus eine oder zwei Punktzeilen, oder die einzeilige Form `$TOKEN x y z`), und die Knöpfe schreiben von selbst den passenden Anschlussbefehl statt `add`/`replace`/`remove`. Die Punkte werden wie im Plugin als Zahlen verglichen, `5` und `5.0` sind also derselbe Punkt; ein `*_ALLOWPASS`-Token lehnt das Plugin ab, und der Editor sagt es dir sofort. Davor und Danach gibt es für Anschlüsse nicht — sie landen immer hinter dem letzten.

Das Kontrollkästchen **Gegen alle Zieldateien prüfen** rechnet den gewählten Befehl gegen jede Zieldatei des Regelsatzes und sagt, in wie vielen er greift. Das ist wichtiger, als es klingt: Ein Befehl, der in einer einzigen Zieldatei nicht passt, macht dort den **ganzen Regelsatz** ungültig — das Plugin schreibt in diesem Fall "No rule for this target was applied" ins Protokoll, und keine deiner Änderungen kommt an. Passt ein Befehl nur in einen Teil, nennt der Editor die erste Datei, in der er danebengeht; dann gehört er in einen eigenen Regelsatz.

Der Editor hat zwei Ansichten. **Original** ist die unveränderte Zieldatei, in der du deine Anker anklickst. **Ergebnis** zeigt schreibgeschützt die Datei, die das Spiel nach den Befehlen lesen wird, und markiert jede Zeile, die ein Befehl anlegt (`+`) oder ersetzt (`>`); ein Klick darauf nennt den Befehl, der sie geschrieben hat. Solange ein Befehl rot ist, steht dort das unveränderte Original — genau das passiert dann auch im Spiel.

Mehrere Listen in einem `keyed_sections`-Schema: Ein `[group:<id>]` mit eigenem `section_prefix` ist eine Zusatzliste; ihre Texte kommen aus `[list:<id>]` und `[new:<id>]`, ihre Felder tragen `group = <id>`. Die Standardliste ist das erste `[group:]` ohne Präfix. Kennungen der Zusatzlisten sind volle Abschnittsnamen (`research:quartz`), angezeigt ohne Präfix.

### 6.3 Spalten (`keyed_list`)

```ini
[column:donor]
type = choice                          ; text, integer, decimal, choice
choices = food|meat|clothes|eletronics|alcohol
allow_other = 1                        ; Auswahl mit freier Eingabe
required = 1
default = auto                         ; füllt fehlende Spalten
minimum = 0
maximum = 1
step = 0.1
label_key = needs.donor
description_key = needs.donor.description
heading_key = ...                      ; Trennlinie mit Titel über dem Feld
reference = resources                  ; Abschnitt 7
order = 10
```

Zeilen werden beim Schreiben normalisiert (`1.0` wird `1`, Auswahlwerte in der Schreibweise des Schemas).

### 6.4 Detailfelder (`keyed_sections`, `keyed_resources`, plugin-weite Werte)

```ini
[detail:type]
scope = item                           ; item: Schlüssel im Abschnitt des Eintrags
                                       ; global: [section] key der Plugin-INI, gezeigt auf [global] oder einer [card:]
                                       ; custom / keyed: keyed_resources
group = research                       ; Zusatzliste (keyed_sections)
card = log                             ; Karte bei scope = global
section = deposits                     ; scope = global
key = type
type = integer                         ; text, integer, decimal, choice, boolean (global), lines, pair, triple
minimum = 10
maximum = 127
step = 1
choices = technical|soviet|medical
choices_source = registry              ; choice: Kennungen aus [source] plus Grundspiel
allow_other = 1
unique = 1                             ; kein Wert doppelt
auto_increment = 1                     ; Dialog schlägt Maximum + 1 vor
dialog = 1                             ; Teil des +-Dialogs
default = {name}                       ; Vorgabe im Dialog; {name} = Name des neuen Eintrags
maximum_length = 7
length_rule = map=terrain:4            ; kürzer, solange ein anderer Schlüssel diesen Wert hat
empty_hint = leer = Spieleigene ID     ; steht fett unter dem leeren Feld als "Standard: <Text>";
empty_hint_key = x.feld.empty          ; verschwindet, sobald ein Wert drinsteht
suffix = .name                         ; gesperrtes Kästchen mit dem Suffix hinter dem Feld, Kennung als Platzhalter
position = above_id                    ; Feld über der Kennungszeile
heading_key = ...
label_key = ...
description_key = ...
reference = game_research              ; Abschnitt 7
reference_format = requires
reference_own = research
picker = game_buildings                ; lines: Gebäude-Auswahl; game_research: Forschungs-Auswahl;
                                       ; research_lines: Zeilen des Vanilla-Blocks des Eintrags;
                                       ; donor_lines: Zeilen der building.ini des Spenders (siehe game_donor);
                                       ; files: Dateiliste; text: game_donor (Grundspiel-Gebäude, ein Name)
assigned_from = build:plugins\x.ids.ini|ids   ; text: leer = das Plugin vergibt den Wert selbst und merkt
                                       ; ihn sich in dieser INI unter der Kennung des Eintrags; RMM zeigt
                                       ; ihn als grauen Platzhalter im Feld
picker_format = line_edit              ; research_lines: line, line_edit, line_anchor, anchor_edit, edit
picker_folders = package:hooks\x\assets | build:plugins\x\assets   ; files: erster vorhandener Ordner
picker_pattern = *.dds
count_label_key = ...                  ; lines: Zählerbeschriftung (Standard "Anzahl Zeilen")
maximum_lines = 64                     ; lines: Obergrenze, darüber wird der Zähler rot
order = 20
```

`type = lines` (scope = item) beschreibt einen Schlüssel, den die INI im Abschnitt wiederholt (`target = …`, `add = …`): eine Zeile je Vorkommen, leere Zeilen entfallen, geschrieben als ein Block an der Stelle des ersten Vorkommens, im Override als `field.<id>.0`, `field.<id>.1`, … Zeilenfelder haben eine feste Höhe mit Bildlaufleiste, einen Griff zum Aufziehen und Plus/Minus zum Auf- und Zuklappen. Für `[list] summary` zeigt ein lines-Feld seine erste Zeile mit Zähler `(+n)`.

Originaleinträge sind gesperrt, können aber ausgeblendet werden (`suppressed = 1` in `user_config\<plugin>.editor.ini`): Sie fehlen in der wirksamen Datei, bleiben in der Originalbasis und lassen sich wieder anzeigen. Persönliche Einträge stehen als `[item:<id>] owned = 1` mit `field.<id>`-Werten (bei keyed_list `list = <Tupel>`) und werden als neuer Abschnitt in Feldreihenfolge geschrieben; plugin-weite Werte als `[global] field.<id> = <Wert>`. Ein ausgeblendetes Original verliert seinen ganzen Abschnitt in der wirksamen Datei; ein überschriebenes Original behält Abschnitt und Kommentare.

### 6.5 Zeilen auf Karten: Ordner, Datei, Bild, Vanilla-Block

```ini
[folder:icon_store]                    ; Ordnerzeile auf einer Karte
card = icons                           ; Karte oder global
path = vfs:media_soviet\research
missing_key = re.icon_store.missing    ; gelber Kasten, solange der Ordner fehlt
create = 1                             ; Knopf "Ordner erstellen", solange er fehlt
label_key = re.icon_store              ; Zeile mit Pfadfeld, "Öffnen" und "Aktualisieren", sobald er existiert
order = 10

[file:noimage]                         ; Dateizeile: erster vorhandener Kandidat wird gezeigt
card = icons
paths = workshop=package:hooks\x\noimage.png | local=build:plugins\x\noimage.png
label_key = re.noimage
order = 20

[picture:icon]                         ; Bildvorschau je Eintrag mit "Bild einfügen..."
group = research
folder = vfs:media_soviet\research
file = {id}.png
size = 128                             ; Pflichtgröße in Pixeln, 0 = beliebig
order = 15

[research_block:block]                 ; schreibgeschützter Kasten mit dem Block des Eintrags aus media_soviet\research\research.ini
label_key = re.block
description_key = re.block.description
group = modify
order = 12
```

Einträge mit Anhang (Texte, Bild) bekommen einen Löschdialog mit drei Knöpfen: „Alles löschen“ entfernt den Eintrag, seine Texte aus allen Sprachdateien und das Bild; der `remove_label`-Knopf nur den Eintrag.

### 6.6 Textpaket-Reiter

```ini
[tab:localization]
label = Localization

[textpack]
tab = localization
folder = build:plugins\localization\research_expansion   ; lokales Paket, dort schreibt RMM
seed = dependency:tesmio.localization|hooks\localization\research_expansion   ; Startbestand aus dem Paket der Abhängigkeit
namespace = research_expansion         ; für ein neues Paket ohne Startbestand
keys_from = research                   ; Listengruppe, deren Kennungen die Zeilen <id>.name / <id>.desc liefern
label_key = ...
description_key = ...
missing_key = re.textpack.missing      ; gelber Kasten mit "Lokal anlegen", solange der Ordner fehlt
required = 1                           ; rote Box auf jedem Reiter, roter Reiter und Speichern gesperrt (bei eigenen Eintraegen), solange der Ordner fehlt
required_notice_key = re.textpack.required   ; Text der roten Box (required_notice = Rueckfalltext); ohne Angabe ein Standardtext
```

Der Reiter zeigt die Rückfallsprache, die Sprachdateien (mit „+“ aus den Spielsprachen), je eigenem Eintrag Name und Beschreibung sowie weitere Schlüssel mit einem Papierkorb, der einen Schlüssel aus allen Sprachdateien entfernt.

### 6.7 Reiter für erzeugte Workshop-Ordner

```ini
[tab:sml]
label = SML buildings
label_key = bp.tab.sml
order = 30

[wip_buildings]
tab = sml
range = sml                  ; sml = von einem anderen Generator erzeugt (auch der Bereich 9100000000..9199999999)
                             ; own = die Ordner mit dem Stempel des eigenen Plugins
                             ; all = jeder Ordner unter media_soviet\workshop_wip
label_key = bp.wip
description_key = bp.wip.description
empty_key = bp.wip.empty     ; Text, wenn kein Ordner passt
```

Der Reiter liest `media_soviet\workshop_wip` und zeigt je Ordner Name (aus `$ITEM_NAME`, sonst aus `$NAME_STR` der building.ini), Nummer, Objektordner und Herkunft. Die Herkunft steht im `tesmioloader.stamp`: Buildings Plus, der Gebäude-Teil des TesmioLoaders (den Soviet Mod Loader mitbringt), oder kein Stempel — dann stammt der Ordner aus dem Spiel-Editor.

Gemeldet wird von selbst: ein Ordner mit `$OWNER_ID 0` (der Knopf trägt die Steam-ID nach, und **nur** diese Zahl) und ein Ordner, den kein Paket mehr erklärt (Soviet Mod Loader beendet deswegen das Spiel beim Start). Der Stempel wird nie angefasst.

Über „Ändern…" bearbeitet der Spieler die erzeugte `building.ini`. Gespeichert werden **Änderungen**, nicht die Datei: je Gebäude liegen in `user_config\.autoload\wip\` ein Beleg `<id>.receipt.ini` mit `replace = alte Zeile | neue Zeile`, `remove = Zeile` und `add = Zeile` sowie `<id>.baseline.ini`, die wörtliche Fassung des Generators.

Ein Anker darf **mehrere Zeilen** umfassen. Ein Token und die Zeilen darunter ohne eigenes Token sind ein Block, und erst der macht eine Zeile eindeutig: `rotation 0.0` steht in einer erzeugten Datei oft dreimal, der Block, zu dem sie gehört, genau einmal. Ein Klick auf eine beliebige Zeile wählt den ganzen Block. Sobald eine Änderung ein Block ist, schreibt RMM den Beleg als Abschnitte, damit die Reihenfolge erhalten bleibt (beide Formen werden gelesen):

```ini
[op:4]
kind = replace
old = $RESOURCE_VISUALIZATION 3
old = rotation 0.0
new = $RESOURCE_VISUALIZATION 3
new = rotation 90.0
``` Schreibt der Generator neu (der Hash im `tesmioloader.stamp` ändert sich), wird dessen frische Ausgabe die neue Grundlage und die Änderungen werden erneut angewandt — was das Paket verbessert hat, bleibt dabei erhalten. Ein Anker, der keine oder mehr als eine Zeile trifft, wird gemeldet und ausgelassen. Ein Schema braucht dafür nichts weiter als `[wip_buildings]`.

---

## 7. Verweisprüfung

Werte, die etwas benennen müssen, was das Spiel kennt, prüft RMM vor dem Speichern. Ein Treffer macht die Fußzeile rot („Konfiguration ungültig“) mit Eintrag und Feld; Speichern und „Speichern + Starten“ brechen mit derselben Meldung ab. Geprüft werden nur eigene Einträge und persönlich gesetzte Werte. Fehlt der Spielordner neben dem Loader, entfällt die Prüfung.

```ini
[group:modify]
id_reference = game_research           ; die Kennung jedes eigenen Eintrags muss eine aktive Vanilla-Forschung sein

[detail:r_requires]
type = lines
reference = game_research              ; game_research, game_buildings, game_donor, game_texts, resources, files
reference_format = requires            ; id (Standard): ganzer Wert / jede Zeile
                                       ; requires: "<Vorgänger> | before/after | <Anker>", beide geprüft
                                       ; directive:$UNLOCK_RESEARCH: nur Zeilenteile mit dieser Direktive
                                       ; file: Datei relativ zu media_soviet oder zum Workshop-Ordner muss existieren
                                       ; exists | dds_dxt1 | dds_dxt5: bei reference = files
reference_own = research               ; eigene, eingeschaltete Einträge dieser Gruppe gelten als bekannt
```

Mengen: `game_research` = aktive Blöcke der `research.ini`; `game_buildings` = Gebäudedateien des Spiels, der DLCs und der abonnierten Workshop-Objekte; `game_texts` = IDs der `.btf` der Spielsprache; `resources` = die 57 Grundspiel-Namen plus `plugins\resources.ini [list]`, wenn das Resources-Plugin bereit ist; `files` = die Picker-Ordner. Die DDS-Formen prüfen zusätzlich Kompression, quadratische Zweierpotenz 256..4096 und die vollständige Mipmap-Kette.

---

## 8. Sprachen

```ini
[language]
name = Deutsch

[strings]
plugin.description = Beschreibung
field.enabled = Plugin aktivieren
```

`language_directory` ist relativ zur Paketwurzel (Presentation-Schema) oder zu `settings_schemas` (lokales Schema); Dateien heißen `<code>.ini`. Reihenfolge: gewählte Sprache, Englisch, der einfache Text im Schema. Abschnitts-, Schlüssel-, Gruppen-, Reiter- und Eintragskennungen werden nie übersetzt. Sprachdateien werden wörtlich gelesen: Ein Strichpunkt im Text ist kein Kommentar.

---

## 9. Symbole und Pfadsicherheit

Eingebaute Symbole sind unter anderem `builtin:gear`, `builtin:truck`, `builtin:rail`, `builtin:ship` und `builtin:plane`. Alternativ ein sicherer relativer PNG-/ICO-Pfad im Paket. Absolute Pfade, `..`, URLs, Reparse-Punkte, übergroße Dateien und ungültige Bildmaße werden abgewiesen oder als optional protokolliert und durch ein Standardsymbol ersetzt. Asset-Namen dürfen Leerzeichen und `. _ + ( ) & , ' -` enthalten, aber keinen führenden Punkt, keinen Punkt oder Leerzeichen am Ende und kein `..`.

---

## 10. Wie RMM ein Paket bereitstellt

Jedes Speichern hinterlässt unter `user_config\.autoload\` einen Beleg mit der Paketversion und den Prüfsummen der bereitgestellten Dateien. Sein `mode` sagt, wer die DLL lädt:

| `mode` | Bedeutung |
|---|---|
| `package` | RMM hat DLL und INI nach `plugins\` kopiert (TesmioLoader klassisch) |
| `bridge` | nur die INI; die Workshop Bridge lädt die DLL; `[packages] <Ordner> = 1` in `user_config\workshop_bridge.ini` entscheidet |
| `sml` | nur die INI; der Soviet Mod Loader lädt die DLL |
| `installed` | Plugin ohne Paket, geschützte Basis |

Ein geändertes Paket (neue Version, neue DLL oder Standard-INI) zeigt die gelbe Marke „Update“; Speichern übernimmt die neue Fassung und behält persönliche Werte. Unter Bridge oder SML zählt nur die Standard-INI, weil das Spiel die DLL ohnehin aus dem Paket lädt. Vor jedem Speichern hält RMM je Plugin genau einen rollierenden Wiederherstellungspunkt unter `user_config\.autoload\backups\<Paket>\previous`; ein Marker schützt nach einem Absturz vor einem halb geschriebenen Stand.

Für das Plugin `workshop_bridge` selbst überleben die `[packages]`-Zeilen in seiner Overlay-Datei jedes Speichern und „Original wiederherstellen“.

---

## 11. Inhaltspakete (`[content]`)

Ein Paket ohne `[hooks] dll`, aber mit `[content]`, ist ein Inhaltspaket: es bringt keine DLL mit, sondern Abschnitte für Plugins, die es schon gibt. Das ist dasselbe Format, das der Soviet Mod Loader liest, also läuft ein solches Paket unter beiden Ladern.

```ini
[mod]
id = example.salt
name = Salt Resources
version = 1.0.0
enabled = 1

[content]
resources = tesmio\resources.ini       ; [list] und [custom:<id>] wie in plugins\resources.ini
deposits  = tesmio\deposits.ini        ; ein Abschnitt je Vorkommen, wie in deposits_plus.ini
needs     = tesmio\needs.ini           ; [list] wie in needs.ini
buildings = tesmio\buildings.ini       ; ein Abschnitt je Gebäude, wie in buildings_plus.ini
assets    = assets                     ; Ordner, dessen Aufbau ab media_soviet\... gilt
```

Alle fünf Zeilen sind einzeln optional; mindestens eine Inhaltsdatei oder ein Asset-Ordner muss da sein. Die Dateien liest RMM tolerant (wiederholte Schlüssel sind erlaubt), eine kaputte Datei weist das ganze Paket ab.

**Was RMM daraus macht.** Die Paketseite hat keine Einstellungen, nur den Schalter „Im Spiel bereitstellen“. Beim Speichern:

- Jede Inhaltsdatei geht an das erste Plugin, dessen wirksame INI in `plugins\` liegt: `resources` → resources, `deposits` → deposits_plus, sonst deposits, `needs` → needs, `buildings` → buildings_plus, sonst buildings. Fehlt das Plugin, sagt die Seite das und lässt diesen Teil weg.
- Die Einträge erscheinen im Editor des Plugins als Originale: überschreibbar, aber nicht löschbar. Eine Kennung, die es dort schon gibt, bleibt unangetastet und wird auf der Paketseite als übersprungen genannt.
- `assets` wird nach `tesmioloader\vfs` kopiert, Pfad für Pfad.
- Bei Vorkommen vergibt RMM `type` selbst (10 bis 127, frei gegenüber allem, was das Plugin schon kennt) und setzt `map = auto`; `component` entfällt. Eine einmal vergebene Nummer bleibt beim Abschnitt.
- Ein Beleg unter `user_config\.autoload\content\<mod-id>\` hält fest, was bereitgestellt ist. Ändert sich das Paket im Workshop, erscheint die Marke „Update“; Speichern übernimmt den neuen Stand.

Ausschalten und Speichern nimmt Einträge und Dateien wieder heraus; persönliche Werte und eigene Einträge bleiben.
