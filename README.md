# Workers & Resources: Soviet Republic “ TesmioLoader plugins

**English** | [Deutsch](README_DE.md)

## Attention, comrade: an AI helped build this

These plugins were written with the help of an artificial intelligence. The five-year plans
behind them were still drawn up, tested and sworn at by a human every time the game crashed.
If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

## About

Native plugins for *Workers & Resources: Soviet Republic* 1.1.1.9, loaded by the
[TesmioLoader](https://github.com/MaxLegend/TesmioLoader) (API 4), the Soviet Mod Loader
or the Workshop Bridge. Every plugin ships as a Steam Workshop package with a settings
schema for the Republic Mod Manager. This repository holds the source code, as the
GNU GPL v3 requires; there are no ready-made DLLs here.

## Plugins

| Plugin | Version | What it does |
|---|---|---|
| [Deposits Plus](plugins/deposits_plus) | 0.4.8 | New deposit types, natural generation, sandy meadow, working vehicles |
| [Depletion](plugins/depletion) | 1.1.2 | Deposits that run out, shown in the mine window |
| [Localization](plugins/localization) | 0.4.0 | Translation service: text packs become extended language files |
| [Rail Physics Fix](plugins/rail_physics_fix) | 1.3.5 | Train physics: traction, braking, grades, curve and station limits, consumption |
| [Research Expansion](plugins/research_expansion) | 0.4.2 | New research and changes to the research tree |
| [Resources Button Fix](plugins/resources_button_fix) | 0.4.0 | Compact tool grids in the terrain editor |
| [Technical Service Storage](plugins/technical_service_storage) | 0.3.3 | Grit storages, material priorities and snowplow tanks |
| [UI Layout Fixes](plugins/ui_layout_fixes) | 0.3.0 | Row spacing in the customs house, text wrapping in info windows |
| [Buildings Plus](plugins/buildings_plus) | 0.1.8 | New buildings from a declaration: donor building plus changed lines, generated at game start |
| [Vanilla Buildings](plugins/vanilla_buildings) | 0.4.4 | Temporary changes to building files without touching the originals |
| [Vehicle Materials](plugins/vehicle_materials) | 0.4.0 | Extra materials for vehicle production |
| [Weather Roads](plugins/weather_roads) | 0.3.3 | Road snow, melting and protection after plowing |
| [Workshop Bridge](plugins/workshop_bridge) | 0.2.0 | Loads Workshop packages without the Soviet Mod Loader |

Each plugin folder has its own guide in two languages (`README_DE.md`, `README_EN.md`)
and English build notes (`BUILD_INFO.md`). Shared files: `plugins/grit_spreader_api.h`
(service between Technical Service Storage and Weather Roads), `plugins/tesmio_config.h`
(INI base and personal overlay), `plugins/tests` (offline tests). `src/` holds the two SDK
headers `tesmio_api.h` and `tesmio_plugin.h` from the TesmioLoader by MaxLegend (GPL v3), so
the plugins build from this repository with their relative include paths. `tesmio_plugin.h`
carries one local change: `TsmOpenLog` writes the plugin detail logs into `tesmioloader\build\logs\`.

## Tools

| Tool | Version | What it does |
|---|---|---|
| [Republic Mod Manager](tools/republic_mod_manager) | 0.5.9 | Windows program: settings, switching and loading of the plugins; its Workshop item brings the installer and the Workshop Bridge |

## Installing

Subscribe to the Workshop package and switch the plugin on in the Republic Mod Manager.
The plugin guides describe the four ways to load a plugin: classic TesmioLoader,
Soviet Mod Loader, Workshop Bridge and Republic Mod Manager.

## Building

Visual Studio 2022 or newer with the x64 C++ tools, then in a Developer Command Prompt
inside the plugin folder:

```
cl /nologo /O2 /MT /W3 /EHsc /std:c++17 /LD /Fo"build\\" /Fd"build\\" /Fe"build\<name>.dll" <name>.cpp /link kernel32.lib
```

Rail Physics Fix and Deposits Plus bring their own `build.bat`. The exports
`TsmPluginApiVersion`, `TsmPluginInit` and `TsmPluginStart` are the loader's plugin contract
(API 4). Addresses and signatures are valid for game build 1.1.1.9 (Steam build 23935965) only.

## Bug reports

Please open an issue with the game version, the loader version, the active plugins and the
logs `tesmioloader.log` from `tesmioloader\build` and `tesmioloader.<plugin>.log` from `tesmioloader\build\logs`.

## Credits and licence

GNU GPL v3, see [LICENSE](LICENSE). Deposits Plus and Depletion continue plugins from the
TesmioLoader by MaxLegend (Tesmio). Rail Physics Fix is a reworked version of
[RailPhysics 1.3.0](https://github.com/TheRealMeowMeow00/WRSR_RailPhysics) by Meow Meow
(TheRealMeowMeow00). `plugins/deposits_plus/third_party` contains Microsoft code under the
University of Illinois Open Source License (see `LICENSE.TXT` there).
