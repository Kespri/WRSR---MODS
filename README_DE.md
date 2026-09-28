# Workers & Resources: Soviet Republic â€“ Plugins fÃ¼r den TesmioLoader

[English](README.md) | **Deutsch**

## ðŸ¤– Genosse, Achtung: hier hat eine KI mitgebaut

Diese Plugins wurden mit Hilfe einer kÃ¼nstlichen Intelligenz geschrieben. Die FÃ¼nfjahresplÃ¤ne
dazu hat trotzdem ein Mensch aufgestellt, getestet und beim AbstÃ¼rzen des Spiels geflucht.
Wer keine KI im Code mÃ¶chte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

## Worum es geht

Native Plugins fÃ¼r *Workers & Resources: Soviet Republic* 1.1.1.9, geladen vom
[TesmioLoader](https://github.com/MaxLegend/TesmioLoader) (API 4), vom Soviet Mod Loader
oder von der Workshop Bridge. Jedes Plugin wird als Steam-Workshop-Paket mit einem
Einstellungsschema fÃ¼r den Republic Mod Manager ausgeliefert. Dieses Repository enthÃ¤lt den
Quelltext, wie es die GNU GPL v3 verlangt; fertige DLLs liegen hier nicht.

## Plugins

| Plugin | Version | Was es macht |
|---|---|---|
| [Deposits Plus](plugins/deposits_plus) | 0.4.8 | Neue Vorkommenstypen, natÃ¼rliche Verteilung, sandige Wiese, Arbeitsfahrzeuge |
| [Depletion](plugins/depletion) | 1.1.2 | Vorkommen, die zur Neige gehen, mit Anzeige im Minenfenster |
| [Localization](plugins/localization) | 0.4.0 | Ãœbersetzungsdienst: Textpakete werden zu erweiterten Sprachdateien |
| [Rail Physics Fix](plugins/rail_physics_fix) | 1.3.5 | Zugphysik: Antrieb, Bremsen, Steigungen, Kurven- und Bahnhofslimits, Verbrauch |
| [Research Expansion](plugins/research_expansion) | 0.4.2 | Neue Forschungen und Ã„nderungen am Forschungsbaum |
| [Resources Button Fix](plugins/resources_button_fix) | 0.4.0 | Kompakte Werkzeugraster im GelÃ¤ndeeditor |
| [Technical Service Storage](plugins/technical_service_storage) | 0.3.3 | Streugutlager, MaterialprioritÃ¤ten und Schneepflugtanks |
| [UI Layout Fixes](plugins/ui_layout_fixes) | 0.3.0 | Zeilenabstand im Zollhaus, Textumbruch in Info-Fenstern |
| [Buildings Plus](plugins/buildings_plus) | 0.1.8 | Neue GebÃ¤ude aus einer ErklÃ¤rung: SpendergebÃ¤ude plus geÃ¤nderte Zeilen, beim Spielstart erzeugt |
| [Vanilla Buildings](plugins/vanilla_buildings) | 0.4.4 | VorÃ¼bergehende Ã„nderungen an GebÃ¤udedateien, ohne die Originale anzufassen |
| [Vehicle Materials](plugins/vehicle_materials) | 0.4.0 | ZusÃ¤tzliche Materialien fÃ¼r die Fahrzeugproduktion |
| [Weather Roads](plugins/weather_roads) | 0.3.3 | StraÃŸenschnee, Schmelze und Schutz nach dem RÃ¤umen |
| [Workshop Bridge](plugins/workshop_bridge) | 0.2.0 | LÃ¤dt Workshop-Pakete ohne den Soviet Mod Loader |

Jeder Plugin-Ordner hat eine eigene Anleitung in zwei Sprachen (`README_DE.md`, `README_EN.md`)
und englische Build-Notizen (`BUILD_INFO.md`). Gemeinsame Dateien: `plugins/grit_spreader_api.h`
(Dienst zwischen Technical Service Storage und Weather Roads), `plugins/tesmio_config.h`
(INI-Basis und persÃ¶nliches Overlay), `plugins/tests` (Offline-Tests). `src/` enthÃ¤lt die zwei
SDK-Header `tesmio_api.h` und `tesmio_plugin.h` aus dem TesmioLoader von MaxLegend (GPL v3),
damit die Plugins mit ihren relativen Include-Pfaden aus diesem Repository heraus gebaut werden
kÃ¶nnen. `tesmio_plugin.h` trÃ¤gt eine lokale Ã„nderung: `TsmOpenLog` schreibt die Detail-Protokolle
der Plugins nach `tesmioloader\build\logs\`.

## Werkzeuge

| Werkzeug | Version | Was es macht |
|---|---|---|
| [Republic Mod Manager](tools/republic_mod_manager) | 0.5.9 | Windows-Programm: Einstellungen, Ein- und Ausschalten und Laden der Plugins; sein Workshop-Objekt bringt den Installer und die Workshop Bridge mit |

## Installation

Workshop-Paket abonnieren und das Plugin im Republic Mod Manager einschalten.
Die Plugin-Anleitungen beschreiben die vier Wege, ein Plugin zu laden: klassischer TesmioLoader,
Soviet Mod Loader, Workshop Bridge und Republic Mod Manager.

## Bauen

Visual Studio 2022 oder neuer mit den x64-C++-Werkzeugen, dann in einer Developer Command Prompt
im jeweiligen Plugin-Ordner:

```
cl /nologo /O2 /MT /W3 /EHsc /std:c++17 /LD /Fo"build\\" /Fd"build\\" /Fe"build\<name>.dll" <name>.cpp /link kernel32.lib
```

Rail Physics Fix und Deposits Plus bringen ein eigenes `build.bat` mit. Die Exporte
`TsmPluginApiVersion`, `TsmPluginInit` und `TsmPluginStart` sind der Plugin-Vertrag des Loaders
(API 4). Adressen und Signaturen gelten nur fÃ¼r Spielversion 1.1.1.9 (Steam-Build 23935965).

## Fehlerberichte

Bei Problemen bitte ein Issue mit Spielversion, Loader-Version, aktiven Plugins und den
Protokollen `tesmioloader.log` aus `tesmioloader\build` und `tesmioloader.<plugin>.log` aus `tesmioloader\build\logs` erÃ¶ffnen.

## Credits und Lizenz

GNU GPL v3, siehe [LICENSE](LICENSE). Deposits Plus und Depletion fÃ¼hren Plugins aus dem
TesmioLoader von MaxLegend (Tesmio) weiter. Rail Physics Fix ist eine Ã¼berarbeitete Fassung von
[RailPhysics 1.3.0](https://github.com/TheRealMeowMeow00/WRSR_RailPhysics) von Meow Meow
(TheRealMeowMeow00). `plugins/deposits_plus/third_party` enthÃ¤lt Code von Microsoft unter der
University of Illinois Open Source License (siehe `LICENSE.TXT` dort).
