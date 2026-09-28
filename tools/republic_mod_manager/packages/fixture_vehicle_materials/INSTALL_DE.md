# Vehicle Materials 0.4.0

Dieses Paket ist fuer drei alternative Installationswege vorbereitet. Immer nur
einen Weg benutzen; dieselbe Plugin-DLL darf nicht zweimal geladen werden.
Ein Workshop-Abonnement allein aktiviert keine native Plugin-DLL.

## 1. Klassischer TesmioLoader

Bei beendetem Spiel vehicle_materials.dll und vehicle_materials.ini aus hooks
nach tesmioloader\build\plugins kopieren. Vorhandene persoenliche INI zuvor
sichern bzw. beibehalten. Im TesmioLauncher aktivieren. Resources muss installiert
sein und die in der INI eingetragenen Materialien registrieren.

## 2. Soviet Mod Loader (SML)

Der Paketordner enthaelt soviet.mod.ini und hooks\vehicle_materials.dll samt
benachbarter INI. SML kann den Hook ueber sein Manifest laden. Eine zusaetzliche
lokale vehicle_materials.dll vorher entfernen bzw. im TesmioLauncher deaktivieren.
Die eingetragenen Ressourcen muessen auch in der SML-Konfiguration existieren.
SML 0.6.0 liefert seinen eigenen Resources-Dienst. Es werden keine Kopien seiner
Kernplugins mitgeliefert. Der gemeinsame Paketaufbau ist vorbereitet; ein echter
SML-Spieltest ist weiterhin erforderlich.

## 3. Tesmio Settings / Autoload

Paketordner in tesmio_autoload.exe auswaehlen, persoenliche Einstellungen pruefen
und "Bereitstellen" anklicken. Das Programm erstellt eine lokale DLL-Kopie und
eine zusammengefuehrte INI fuer den unveraenderten TesmioLoader. Persoenliche
Abweichungen liegen in build\user_config\vehicle_materials.ini; die Workshop-
Dateien werden nicht veraendert. Vor jedem Spielstart erneut ueber Autoload
bereitstellen, damit Workshop-Updates uebernommen werden.

Autoload und SML sind Alternativen. Die Autoload-Einstellungen werden nicht
automatisch auf SMLs direkt geladene Workshop-INI angewendet.

## Voraussetzungen

Windows x64, WRSR 1.1.1.9, TesmioLoader API 4 und ein Resources-Dienst mit den
konfigurierten Materialien (standardmaessig glass und cable).
Native DLLs koennen Code ausfuehren: nur Pakete vertrauenswuerdiger Autoren nutzen.

Die Version 0.4.0 aendert ausschliesslich die INI-Pfadsuche. Die Berechnung
der Fahrzeugmaterialien ist gegenueber 1.1.0 unveraendert.
