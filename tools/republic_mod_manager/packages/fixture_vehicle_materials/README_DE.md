# Vehicle Materials Plugin

Mit diesem Plugin kannst du **zusätzliche, benutzerdefinierte Ressourcen** für die Fahrzeugproduktion in *Workers & Resources: Soviet Republic* festlegen – zum Beispiel Glas, Kabel oder Kupfer.

Die Ressourcen selbst werden vom **Resources-Plugin** bereitgestellt. Das Vehicle-Materials-Plugin legt anschließend fest, welche dieser Ressourcen für Straßenfahrzeuge, Schienenfahrzeuge, Schiffe und Flugzeuge benötigt werden und wie stark sie in die Produktionskosten einfließen.

Die originale Spiel-EXE wird nicht dauerhaft verändert. Das Plugin ergänzt die Materialanforderungen nur während der laufenden Spielsitzung.

---

## Wichtige Abhängigkeit

Das Vehicle-Materials-Plugin benötigt zwingend:

- das Resources-Plugin (`resources.dll`);
- eine passende `resources.ini`;
- alle dort definierten benutzerdefinierten Ressourcen, die du in `vehicle_materials.ini` verwenden möchtest.

Das Resources-Plugin erzeugt und registriert die zusätzlichen Ressourcen. Vehicle Materials greift auf diesen Dienst zu und kann keine Ressource selbst anlegen.

Fehlt das Resources-Plugin oder ist es nicht aktiv, wird Vehicle Materials nicht gestartet. Der genaue Grund wird in `tesmioloader.log` eingetragen.

---

## Voraussetzungen

- TesmioLoader mit API **4**
- aktiviertes Resources-Plugin
- *Workers & Resources: Soviet Republic* **1.1.1.9**
- geeignete Importlager in allen Produktionsgebäuden, die zusätzliche Fahrzeugmaterialien verarbeiten sollen

> Nur Ressourcen, die vom Resources-Plugin bereitgestellt werden, können für die Fahrzeugproduktion verwendet werden. Achte darauf, den Ressourcennamen in beiden INI-Dateien exakt gleich zu schreiben.

---

## Installation und Ordnerstruktur

Nach der Installation sollte die Ordnerstruktur mindestens so aussehen:

```text
SovietRepublic\
└── tesmioloader\
    └── build\
        └── plugins\
            ├── resources.dll
            ├── resources.ini
            ├── vehicle_materials.dll
            └── vehicle_materials.ini
```

### Benötigte Dateien

- `resources.dll`
- `resources.ini`
- `vehicle_materials.dll`
- `vehicle_materials.ini`

Kopiere diese Dateien in `tesmioloader\build\plugins\` und aktiviere beide Plugins im TesmioLauncher.

Eine Änderung an `vehicle_materials.ini` erfordert **kein erneutes Kompilieren** der DLL. Starte das Spiel nach einer Änderung jedoch vollständig neu, damit die Konfiguration neu eingelesen wird.

---

## Produktionsgebäude vorbereiten

Ein Fahrzeug kann ein zusätzliches Material nur verbrauchen, wenn das zuständige Produktionsgebäude diese Ressource auch annehmen und lagern kann.

Das betrifft beispielsweise Produktions-Fabriken für:

- Straßenfahrzeuge;
- Schienenfahrzeuge;
- Schiffe;
- Flugzeuge.

Für jedes zusätzliche Material benötigt das Gebäude eine passende `$STORAGE_IMPORT_SPECIAL`-Zeile. Beispiel:

```ini
$STORAGE_IMPORT_CARPLANT RESOURCE_TRANSPORT_COVERED 250
$STORAGE_IMPORT_CARPLANT RESOURCE_TRANSPORT_OPEN 300
$STORAGE_EXPORT RESOURCE_TRANSPORT_VEHICLES 15

--> neu dazu kommt:
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN 100 glass
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN 50 cable
```

Dabei gilt:

- `100` beziehungsweise `50` ist die Lagerkapazität;
- `glass` beziehungsweise `cable` ist die genaue Ressourcen-ID;
- der Transporttyp muss zur jeweiligen Ressource passen.

Du kannst die Gebäude über einen eigenen Gebäudemod oder ein geeignetes TesmioLoader-Plugin virtuell anpassen. Prüfe anschließend im Spiel, ob die zusätzlichen Lagerplätze am Gebäude angezeigt werden und beliefert werden können.

> Vehicle Materials ergänzt nur den Materialbedarf für den Bau der Fahrzeuge. Das Plugin fügt den Produktionsgebäuden nicht automatisch neue Lagerplätze hinzu.

---

## Konfiguration: `vehicle_materials.ini`

Die Konfiguration besteht aus sieben Abschnitten:

```ini
[general]
[resources]
[road]
[rail]
[ship]
[airplane]
[mapping]
```

Die vier Fahrzeugkategorien verwenden die Ressourcennamen aus `[resources]`.

Die Datei muss als **UTF-8 ohne BOM** gespeichert sein. Nur vollständige
Kommentarzeilen mit `;` oder `#` sind erlaubt; Inline-Kommentare werden als Teil
des Wertes gelesen. Alle sieben Abschnitte müssen vorhanden sein. Unbekannte
oder mehrfach vorkommende Abschnitte und Schlüssel, leere Werte sowie ungültige
Zahlen führen dazu, dass die gesamte Konfiguration abgelehnt wird. Reine
Trennlinien aus `-` sind erlaubt.

---

## Allgemeine Einstellungen: `[general]`

```ini
[general]
enabled = 0
debug = 0
debug_limit = 80
```

| Schlüssel | Bedeutung | Standardwert |
|-----------|-----------|--------------|
| `enabled` | `1` aktiviert das Plugin, `0` deaktiviert es | `0` |
| `debug` | `1` schreibt zusätzliche Informationen zu Fahrzeugtypen und Kategorien ins Log | `0` |
| `debug_limit` | Begrenzt zusätzliche Diagnose- und Ressourcenmeldungen pro Spielsitzung | `80` |

Für den normalen Spielbetrieb kannst du `debug = 0` verwenden. Aktiviere die zusätzlichen Meldungen nur, wenn du eine Zuordnung oder einen Materialverbrauch prüfen möchtest.

`enabled` und `debug` akzeptieren ausschließlich `0` oder `1`.
`debug_limit` muss zwischen `0` und `10000` liegen.

---

## Ressourcen auswählen: `[resources]`

In diesem Abschnitt legst du fest, welche Ressourcen für Fahrzeuge verwendet werden dürfen.

```ini
[resources]
count = 0
```

| Schlüssel | Bedeutung |
|-----------|-----------|
| `count` | Anzahl der nachfolgenden `resource0`, `resource1`, … Einträge |
| `resource0`, `resource1`, … | Genaue Ressourcen-ID aus `resources.ini` |

Das ausgelieferte Plugin enthält bewusst keine feste Anfangsressource. In
Tesmio Settings werden die gewünschten Einträge mit `+` aus der aktiven
`resources.ini` gewählt; die notwendigen Schlüssel entstehen automatisch.

### Beispiel mit einer Ressource

```ini
[resources]
count = 1
resource0 = copper
```

Wichtig:

- Die Nummerierung beginnt bei `resource0`.
- `count` muss zur Anzahl der Einträge passen.
- Es können höchstens **32 Ressourcen** eingelesen werden; größere Werte werden abgelehnt.
- Für jeden Index kleiner als `count` muss ein nicht leerer Eintrag vorhanden sein.
- Einträge mit einem Index ab `count` werden abgelehnt.
- Doppelte Namen werden unabhängig von Groß- und Kleinschreibung abgelehnt.
- Jeder Name muss vom Resources-Plugin aus `resources.ini` veröffentlicht werden.
- Eine Ressource ohne positiven Wert in mindestens einer Fahrzeugkategorie wird ignoriert.

---

## Materialverbrauch nach Fahrzeugkategorie

Für jede eingetragene Ressource kannst du einen Koeffizienten pro Fahrzeugkategorie festlegen:

| Abschnitt | Gilt für |
|-----------|----------|
| `[road]` | Straßenfahrzeuge |
| `[rail]` | Schienenfahrzeuge und Züge |
| `[ship]` | Schiffe |
| `[airplane]` | Flugzeuge |

Beispiel:

```ini
[road]
copper = 0.030

[rail]
copper = 0.025

[ship]
copper = 0.005

[airplane]
copper = 0.015
```

Der eingetragene Wert ist ein **Koeffizient**. Das Spiel multipliziert ihn mit dem internen Produktionswert des jeweiligen Fahrzeugs. Größere oder aufwendigere Fahrzeuge können dadurch mehr Material benötigen als kleinere Fahrzeuge derselben Kategorie.

Es gilt:

- ein größerer Wert erzeugt einen höheren Materialbedarf;
- `0` oder ein fehlender Eintrag deaktiviert die Ressource für diese Kategorie;
- Werte müssen endlich sein und zwischen `0` und `1.000.000` liegen;
- negative, unvollständige oder anderweitig ungültige Werte lehnen die gesamte Konfiguration ab;
- ein Schlüssel in einem Kategorieabschnitt muss unter `[resources]` aufgeführt sein;
- das Plugin fügt eine Ressource nicht erneut hinzu, wenn sie bereits im Materialbedarf des Fahrzeugs vorhanden ist.

Beginne am besten mit kleinen Werten und kontrolliere die tatsächliche Menge im Spiel. So kannst du die Balance anschließend schrittweise anpassen.

---

## Fahrzeugtypen zuordnen: `[mapping]`

Normalerweise erkennt das Plugin die Fahrzeugkategorie automatisch:

- Typ `1` → Straße
- Typ `6` → Schiff
- Typ `7` → Flugzeug
- alle anderen Produktionstypen → Schiene

Die automatische Zuordnung ist in der Regel ausreichend:

```ini
[mapping]
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

### Werte für eine manuelle Zuordnung

| Wert | Kategorie |
|------|-----------|
| `-1` | automatische Erkennung |
| `0` | Straße |
| `1` | Schiene |
| `2` | Schiff |
| `3` | Flugzeug |

Beispiel:

```ini
[mapping]
type2 = 0
```

Damit wird der Fahrzeugtyp `2` manuell als Straßenfahrzeug behandelt. Andere
Werte als `-1` bis `3` werden abgelehnt; ein fehlender `typeN`-Schlüssel verwendet
weiterhin die automatische Zuordnung.

Ändere diesen Abschnitt nur, wenn ein Fahrzeug nachweislich der falschen Kategorie zugeordnet wird. Mit `debug = 1` kannst du den erkannten Typ und die verwendete Kategorie im Log kontrollieren.

---

## Protokolldatei

Die Meldungen des Plugins findest du hier:

```text
tesmioloader\build\tesmioloader.log
```

Zusätzlich schreibt das Plugin ein eigenes Detailprotokoll:

```text
tesmioloader\build\tesmioloader.vehicle_materials.log
```

Beim Start protokolliert das Plugin unter anderem:

- die Plugin- und API-Version;
- die Anzahl der aktiven Materialien;
- bei `debug = 1` die Koeffizienten jeder eingelesenen Ressource;
- ungültige, fehlende oder doppelte Konfigurationseinträge;
- eine nicht unterstützte Spielversion;
- einen fehlenden Resources-Dienst.

Mit `debug = 1` werden zusätzlich erkannte Fahrzeugtypen, Kategorien und die Anzahl der ergänzten Materialien ausgegeben. `debug_limit` verhindert, dass das Log durch wiederholte Meldungen unnötig groß wird.

---

## Kurzanleitung

1. Definiere deine benutzerdefinierten Ressourcen in `resources.ini`.
2. Aktiviere `resources.dll` und `vehicle_materials.dll` im TesmioLauncher.
3. Füge die gewünschten Ressourcen in Tesmio Settings mit `+` hinzu (oder trage sie manuell unter `[resources]` ein).
4. Setze für jede benötigte Fahrzeugkategorie passende Koeffizienten.
5. Ergänze in den betroffenen Produktionsgebäuden passende Importlager für diese Ressourcen.
6. Starte das Spiel vollständig neu über den TesmioLauncher.
7. Kontrolliere `tesmioloader.log` und teste die Materialmengen im Spiel.

---

## Fehlerbehebung

| Meldung oder Problem | Ursache und Lösung |
|----------------------|--------------------|
| `[resources-service]` | Das Resources-Plugin fehlt, ist deaktiviert oder konnte nicht gestartet werden. |
| `[resource-not-registered]` | Die Ressource wird vom Resources-Plugin nicht veröffentlicht. Prüfe Namen und Definition in `resources.ini`. |
| `[duplicate-key]` oder `[duplicate-resource]` | Ein Abschnitt, Schlüssel oder Ressourcenname kommt mehrfach vor. Entferne das Duplikat. |
| `[missing-value]` oder `[missing-resource]` | Ein erforderlicher Wert oder ein durch `count` erwarteter `resourceN`-Eintrag fehlt. |
| `[zero-coefficient]` | Die Ressource besitzt in keiner Kategorie einen positiven Wert und wird ignoriert. |
| `[config-range]`, `[coefficient-range]` oder `[mapping-range]` | Ein Wert liegt außerhalb des dokumentierten Bereichs oder ist keine vollständige Zahl. |
| `[unsupported-build]` oder `[builder-prologue]` | Die installierte Spielversion entspricht nicht **1.1.1.9** oder ein anderes Plugin belegt bereits die Hook-Stelle. |
| Das Fahrzeug verlangt das Material, aber die Fabrik kann es nicht annehmen | Im Produktionsgebäude fehlt ein passendes Importlager für die Ressource. |
| Eine Änderung der INI erscheint nicht im Spiel | Beende das Spiel vollständig und starte es erneut über den TesmioLauncher. |

Wenn etwas nicht funktioniert, öffne zuerst `tesmioloader.log`. Suche dort nach Zeilen, die mit `vehicle_materials` beginnen.

---

## Sicherheit und Verhalten

- Die originale `SOVIET64.exe` wird nicht auf der Festplatte verändert.
- Das Plugin arbeitet nur während der laufenden Spielsitzung im Speicher des Spiels.
- Die Konfiguration wird vollständig geprüft, bevor ein Hook installiert wird.
- Spielversion, PE-Struktur, Bildgröße, Zeitstempel, Adressbereiche und Hook-Prolog werden vor der Änderung kontrolliert.
- Ein abgefangener Laufzeitfehler deaktiviert weitere benutzerdefinierte Zusätze für die Sitzung; der originale Materialaufbau läuft weiter.
- Wird das Spiel ohne TesmioLoader gestartet oder das Plugin deaktiviert, werden keine zusätzlichen Fahrzeugmaterialien ergänzt.
- Deine vorhandenen Spiel- und Gebäudedateien werden von Vehicle Materials nicht automatisch geändert.

### Umstieg von 1.0.0

Version 1.1.0 verwendet einheitlich den Namen `vehicle_materials`. Entferne die
alten Dateien `vehiclematerials.dll` und `vehiclematerials.ini`, damit nicht
beide Plugin-Versionen gleichzeitig geladen werden. Übernimm deine Werte in die
neue `vehicle_materials.ini`; ungültige Werte werden nicht mehr stillschweigend
korrigiert oder übersprungen.

---

## Version

- Plugin-Version: **0.4.0**
- erstellt für TesmioLoader API: **4**
- unterstützte Spielversion: **WRSR 1.1.1.9**

### Neu in 0.4.0: Workshop-Paket

Die INI wird neben der eigenen DLL gelesen, auch wenn SML einen anderen
Loader-Basisordner übergibt. Fehlt sie dort, wird nicht auf eine fremde lokale
INI zurückgegriffen. Die Materialberechnung bleibt unverändert.

Ein gemeinsames Paket enthält `soviet.mod.ini`, `hooks/vehicle_materials.dll`,
`hooks/vehicle_materials.ini` und `config/vehicle_materials.launcher.ini`.
Die separate Schema-INI wird nicht als Plugin-Konfiguration eingelesen.
Tesmio Settings stellt eine lokale Kopie und persönliche
INI-Overrides bereit. SML und Autoload nicht gleichzeitig verwenden.
Ein echter SML-Spieltest steht noch aus; Offline-Prüfungen ersetzen diesen nicht.
