# Workers & Resources: Soviet Republic – Plugins für den TesmioLoader

[English](README.md) | **Deutsch**

## 🤖 Genosse, Achtung: hier hat eine KI mitgebaut

Diese Plugins wurden mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne
dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht.
Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

## Worum es geht

Native Plugins für *Workers & Resources: Soviet Republic* 1.1.1.9, geladen vom
[TesmioLoader](https://github.com/MaxLegend/TesmioLoader) (API 4), vom Soviet Mod Loader
oder von der Workshop Bridge. Jedes Plugin wird als Steam-Workshop-Paket mit einem
Einstellungsschema für den Republic Mod Manager ausgeliefert. Dieses Repository enthält den
Quelltext, wie es die GNU GPL v3 verlangt; fertige DLLs, Texturen und Modelle liegen hier nicht -
die kommen mit den Workshop-Paketen.

## Plugins

| Plugin | Version | Was es macht |
|---|---|---|
| [Deposits Plus](plugins/deposits_plus) | 0.4.10 | Neue Vorkommenstypen, natürliche Verteilung, sandige Wiese, Arbeitsfahrzeuge |
| [Localization](plugins/localization) | 0.4.0 | Übersetzungsdienst: Textpakete werden zu erweiterten Sprachdateien |
| [Rail Physics Fix](plugins/rail_physics_fix) | 1.3.5 | Zugphysik: Antrieb, Bremsen, Steigungen, Kurven- und Bahnhofslimits, Verbrauch |
| [Research Expansion](plugins/research_expansion) | 0.4.2 | Neue Forschungen und Änderungen am Forschungsbaum |
| [Resources Button Fix](plugins/resources_button_fix) | 0.4.0 | Kompakte Werkzeugraster im Geländeeditor |
| [Technical Service Storage](plugins/technical_service_storage) | 0.3.3 | Streugutlager, Materialprioritäten und Schneepflugtanks |
| [UI Layout Fixes](plugins/ui_layout_fixes) | 0.3.0 | Zeilenabstand im Zollhaus, Textumbruch in Info-Fenstern |
| [Vanilla Buildings](plugins/vanilla_buildings) | 0.4.4 | Vorübergehende Änderungen an Gebäudedateien, ohne die Originale anzufassen |
| [Vehicle Materials](plugins/vehicle_materials) | 0.4.0 | Zusätzliche Materialien für die Fahrzeugproduktion |
| [Weather Roads](plugins/weather_roads) | 0.3.3 | Straßenschnee, Schmelze und Schutz nach dem Räumen |

Diese vier reisen im Paket des Republic Mod Manager mit:

| Plugin | Version | Was es macht |
|---|---|---|
| [Workshop Bridge](plugins/workshop_bridge) | 0.2.0 | Lädt Workshop-Pakete ohne den Soviet Mod Loader |
| [Buildings Plus](plugins/buildings_plus) | 0.1.10 | Neue Gebäude aus einer Erklärung: Spendergebäude plus geänderte Zeilen, beim Spielstart erzeugt |
| [Resources Plus](plugins/resources_plus) | 0.1.2 | Eigene Ressourcen aus `plugins\resources_plus.ini`, Fork von resources: behebt den Absturz beim Beenden, eigene INI einmal aus resources.ini übernommen, pausiert neben Original und SML |
| [Needs Plus](plugins/needs_plus) | 0.1.1 | Eigene Bürgerbedürfnisse aus `plugins\needs_plus.ini`, Fork von needs: eigene INI automatisch übernommen, doppelte Namen abgewiesen, pausiert neben Original und SML |

Jeder Plugin-Ordner hat eine eigene Anleitung in zwei Sprachen (`README_DE.md`, `README_EN.md`)
und englische Build-Notizen (`BUILD_INFO.md`). Gemeinsame Dateien: `plugins/grit_spreader_api.h`
(Dienst zwischen Technical Service Storage und Weather Roads) und `plugins/tesmio_config.h`
(INI-Basis und persönliches Overlay). `src/` enthält die zwei SDK-Header `tesmio_api.h` und
`tesmio_plugin.h` aus dem TesmioLoader von MaxLegend (GPL v3), damit die Plugins mit ihren
relativen Include-Pfaden aus diesem Repository heraus gebaut werden können. `tesmio_plugin.h`
trägt eine lokale Änderung: `TsmOpenLog` schreibt die Detail-Protokolle der Plugins nach
`tesmioloader\build\logs\`. `src/tesmioloader.cpp` ist der Loader selbst in der korrigierten
Fassung b0.3.6-rmm.1, die das Paket des Republic Mod Manager mitbringt und über die Original-DLL
installiert (gleiche API, gleiche Logzeilen; die Änderungen stehen in `src/BUILD_INFO.md`).

## Werkzeuge

| Werkzeug | Version | Was es macht |
|---|---|---|
| [Republic Mod Manager](tools/republic_mod_manager) | 0.5.20 | Windows-Programm: Einstellungen, Ein- und Ausschalten und Laden der Plugins; sein Workshop-Objekt bringt den Installer, den Loader und die vier Plugins oben mit |

## Installation

Workshop-Paket abonnieren und das Plugin im Republic Mod Manager einschalten.
Die Plugin-Anleitungen beschreiben die Wege, ein Plugin zu laden: klassischer TesmioLoader,
Soviet Mod Loader, Workshop Bridge und Republic Mod Manager.

## Bauen

Visual Studio 2022 oder neuer mit den x64-C++-Werkzeugen, dann in einer Developer Command Prompt
im jeweiligen Plugin-Ordner:

```
cl /nologo /O2 /MT /W3 /EHsc /std:c++17 /LD /Fo"build\\" /Fd"build\\" /Fe"build\<name>.dll" <name>.cpp /link kernel32.lib
```

Rail Physics Fix bringt ein eigenes `build.bat` mit. Der Republic Mod Manager baut mit
`tools/republic_mod_manager/build.bat` (C#-Compiler des .NET Framework 4). Die Exporte
`TsmPluginApiVersion`, `TsmPluginInit` und `TsmPluginStart` sind der Plugin-Vertrag des Loaders
(API 4). Adressen und Signaturen gelten nur für Spielversion 1.1.1.9 (Steam-Build 23935965).

## Fehlerberichte

Bei Problemen bitte ein Issue mit Spielversion, Loader-Version, aktiven Plugins und den
Protokollen `tesmioloader.log` aus `tesmioloader\build` und `tesmioloader.<plugin>.log` aus `tesmioloader\build\logs` eröffnen.

## Credits und Lizenz

GNU GPL v3, siehe [LICENSE](LICENSE). Deposits Plus, Buildings Plus, Resources Plus und Needs Plus
führen Plugins aus dem TesmioLoader von MaxLegend (Tesmio) weiter, und `src/tesmioloader.cpp` ist
sein Loader mit Korrekturen. Rail Physics Fix ist eine überarbeitete Fassung von
[RailPhysics 1.3.0](https://github.com/TheRealMeowMeow00/WRSR_RailPhysics) von Meow Meow
(TheRealMeowMeow00). `plugins/deposits_plus/third_party` enthält Code von Microsoft unter der
University of Illinois Open Source License (siehe `LICENSE.TXT` dort).
