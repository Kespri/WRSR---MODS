# 🚆 Rail Physics Fix 1.3.5

**TesmioLoader plugin for physical train dynamics**

Extends the train physics of *Workers & Resources: Soviet Republic* 1.1.1.9: traction, resistance, braking, grade, curve limits, station and customs approaches and diesel and electricity consumption. It is the Windows/TesmioLoader port of **RailPhysics 1.3.0** (GPL v3). Everything works through in-memory hooks; no game file is changed on disk, no VFS replacement files are created and there is no separate save format.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Feature switches](#-feature-switches)
- [Traction, resistance and braking](#-traction-resistance-and-braking)
- [Curves, stations and customs](#-curves-stations-and-customs)
- [Consumption and electricity grid](#-consumption-and-electricity-grid)
- [Value ranges](#-value-ranges)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9, `SOVIET64.exe` build 23935965 (PE identity, timestamp, size, signatures and instructions are verified; other builds get no hooks)
- TesmioLoader API 4
- The original `railphysics.dll` must **not** be loaded at the same time
- No Localization plugin, language pack or VFS content needed

### In three steps
1. **Choose one installation method** (see below) and enable the plugin; switch an old `railphysics.dll` off first.
2. **Check the values:** the supplied INI is a tuned configuration (among others power 1.5, service brake 1.6 m/s², station limit 60 km/h, grid multiplier 2.0). Leave it unchanged for the first test and use **a copy of your savegame**.
3. **Restart the game completely.** `logs\tesmioloader.rail_physics_fix.log` names version, configuration file, preflight and `9 subsystem(s) patched` with all supplied features.

---

## ✨ Features

### 🎯 Core function
- ✅ Physical acceleration from engine power, adhesion, Davis resistance and grade
- ✅ Own service and emergency brake rates with adjustment of planned braking
- ✅ Curve limits from lateral acceleration and radius, route lookahead up to 8000 m; native limits are never raised
- ✅ Station zones, smooth station stops and customs approaches from zones, route end and track corridors
- ✅ Load-based diesel and electricity consumption through the game's own bookkeeping
- ✅ Optional grid-wide transfer-ceiling multiplier
- ✅ Strict executable preflight, signature check of every hook site, verified bridges and allocations
- ✅ Three independent diagnostic switches; warnings always stay visible

### 🆕 New in 1.3.5
- ✅ Every Republic Mod Manager text in player style: notice and info box on General, "Troubleshooting" card, brake margin with the brakes, "curve and route look-ahead" everywhere.
- ✅ In-game test confirmed in the log: 9 subsystems patched, curve limits, station stop, station zone and consumption model working.
- ✅ **Fixed against the original RailPhysics 1.3.0:** three argument-passing bugs of the bridges, a use-after-free in the corridor build, unchecked memory reads and the locale-dependent parsing of decimals. Physics, values and hooks are the original's; on torn data, inactive wagons and memory failures the plugin degrades like the original (partial look-ahead instead of a stop, unreadable chain skipped, last zone tables kept).
- ✅ **INI beside the DLL:** when `plugins\rail_physics_fix.ini` is missing, the DLL reads the INI from its own folder, i.e. from the Workshop package under Soviet Mod Loader or the Workshop Bridge. The classic path through the loader is unchanged; the chosen path is logged as `configuration file:`.
- ✅ Schema for Republic Mod Manager in the package: four tabs with all 30 settings, German and English.
- ✅ Offline test suite with 17 processes, including the `beside_dll` scenario.

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice, and the original `railphysics.dll` has to be off (otherwise the plugin refuses with `RP404`).

---

### Method 1️⃣: Classic TesmioLoader

```
1. Copy rail_physics_fix.dll and rail_physics_fix.ini from hooks\
   → tesmioloader\build\plugins\

2. Enable rail_physics_fix in the TesmioLauncher (switch railphysics off)
3. Restart the game completely
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL through soviet.mod.ini from the package, the INI sits beside it
3. Remove or disable a local rail_physics_fix.dll in plugins\ first
```

---

### Method 3️⃣: Workshop Bridge (without SML)

```
1. Select the package in Republic Mod Manager
2. Switch "Plugin enabled" on
3. workshop_bridge loads the DLL straight from the package; it finds the INI beside itself
4. Steam updates apply immediately
```

---

### Method 4️⃣: Republic Mod Manager with "Files local only"

```
1. Select the package in Republic Mod Manager, adjust the settings, Save
2. Tab "General", card "Notes": switch "Files local only" on
3. Confirmation with file list → DLL and INI are copied to
   tesmioloader\build\plugins\ when saving
4. The Workshop Bridge skips the package automatically afterwards
```

**"Files local only" in detail**
- For everyone who wants to keep using the plugin without a Steam subscription
- With local files Steam updates apply only after saving again (yellow "Update" mark)
- Switching it off removes only the files Republic Mod Manager copied

---

## 🧰 Republic Mod Manager

The package contains a presentation schema in the `config` folder. Republic Mod Manager shows Rail Physics Fix in four tabs, German and English:

- **General:** notes (warning and info box), "Files local only", a button for this guide; feature switches and log settings
- **Traction and braking:** adhesion, power, Davis coefficients, gradient factor, brake rates, brake margin
- **Curve and route look-ahead:** station limit, lateral acceleration, look-ahead range, customs entry
- **Fuel and electricity:** idle load, load factor, electric demand, grid multiplier

The schema's ranges are editor limits; the plugin itself validates only `power_scale` (0.1 to 5.0) and `grid_boost` (at most 10). Republic Mod Manager writes the effective file `plugins\rail_physics_fix.ini`; the INI in the package stays unchanged.

---

## ⚙️ Configuration

### Main file: `rail_physics_fix.ini`

The DLL reads in this order:
- **First:** `tesmioloader\build\plugins\rail_physics_fix.ini` when it exists (classic installation, "Files local only" or the effective INI written by Republic Mod Manager), through the loader's readers as before
- **Otherwise:** the INI beside the DLL, in the package `hooks\rail_physics_fix.ini` (Soviet Mod Loader, Workshop Bridge)

The section is still called **`[railphysics]`** for compatibility; do not rename it. **UTF-8 without BOM**, decimal point (`0.30`), `;` only on its own line, every key once, switches as `0` or `1`. Changes apply after a complete restart.

⚠️ **Deleting the INI does not disable the plugin.** Missing keys use the code defaults, including `enabled = 1`. Seven supplied values differ from the code defaults: `station_limit_kmh`, `curve_lateral_ms2`, `power_scale`, `grade_scale`, `service_brake_ms2`, `emergency_brake_ms2` and `grid_boost`. To switch off, set `enabled = 0` and restart; a DLL with active hooks must not be unloaded while the game runs.

### Validation and fallback behaviour
- Non-empty, invalid or non-finite floating-point values are reported with `RP201` and replaced by the code default.
- `power_scale` outside 0.1 to 5.0 is reset to 1.0 with `RP202`.
- `grid_boost` up to 1 installs no grid hook; above 1 up to 10 enables it; above 10 is logged and not applied.
- Unknown keys, duplicates and relations between values are **not** validated comprehensively. Keep numbers short (64-byte read buffer).

---

## 🔀 Feature switches

| Key | Supplied | Code default | Meaning |
|---|---:|---:|---|
| `enabled` | 1 | 1 | whole plugin |
| `accel` | 1 | 1 | traction, resistance and grade in the acceleration |
| `brake` | 1 | 1 | own brake rates and adjustment of planned braking |
| `slope` | 1 | 1 | separate slope hooks, existing 1.1.1.9 branch behaviour |
| `fuel` | 1 | 1 | load-based diesel/electricity consumption |
| `curves` | 1 | 1 | curve limit and shared lookahead |
| `stations` | 1 | 1 | station zones, needs `curves` |
| `smoothstop` | 1 | 1 | smooth station stops, needs `curves` |
| `customstop` | 1 | 1 | customs approaches, needs `curves` |

The switches select hook areas, not independent models: `slope = 0` does not remove grade from `accel` and `fuel`; `grid_boost` is a separate hook and not tied to `fuel`. `curves = 0` also switches `stations`, `smoothstop` and `customstop` off.

---

## 🚂 Traction, resistance and braking

| Key | Supplied | Code default | Meaning |
|---|---:|---:|---|
| `adhesion_mu` | 0.30 | 0.30 | adhesion coefficient, caps low-speed tractive effort |
| `power_scale` | 1.5 | 1.0 | multiplier of the detected engine power |
| `davis_a` | 1.5 | 1.5 | constant resistance, N per tonne |
| `davis_b` | 0.006 | 0.006 | speed-proportional resistance, N per tonne and km/h |
| `davis_c` | 0.40 | 0.40 | quadratic resistance of the whole train, N per (km/h)² |
| `grade_scale` | 0.06 | 0.12 | scale of the average internal grade |
| `service_brake_ms2` | 1.6 | 0.8 | service brake and braking budget of the lookahead |
| `emergency_brake_ms2` | 2.6 | 1.3 | emergency brake |
| `brake_min_vanilla_ratio` | 1.0 | 1.0 | share of the native brake rate as floor outside planned limit braking |

Resistance: `R = mass_t × (A + B × v_kmh) + C × v_kmh²`. `grade_scale = 0.06` halves the grade contribution against 0.12 without changing terrain. Planned braking is modulated by the situation; `brake_min_vanilla_ratio = 1.0` does not mean every planned braking is at least as strong as the native one.

---

## 🛤️ Curves, stations and customs

| Key | Supplied | Code default | Meaning |
|---|---:|---:|---|
| `station_limit_kmh` | 60 | 30 | limit of detected station zones |
| `curve_lateral_ms2` | 1.4 | 0.9 | lateral acceleration: curve speed = √(a × radius) |
| `curve_brake_margin` | 1.25 | 1.25 | divisor of the service brake in the lookahead budget (1.25 = 80 %) |
| `curve_lookahead_m` | 1200 | 1200 | range of the curve and route look-ahead, grows with the stopping distance + 150 m, at most 8000 m |
| `customs_entry_kmh` | 50 | 50 | target speed at the customs entry |

Station zones cover detected cargo, passenger and waiting-station track; the limit follows the train head. Smooth stops match the route end against detected station nodes and hand the last 25 m to native logic. Customs approaches combine zones, route end and precomputed track corridors. Station and customs tables are rebuilt every 30 seconds, so new facilities take effect with a delay.

---

## ⚡ Consumption and electricity grid

| Key | Supplied | Code default | Meaning |
|---|---:|---:|---|
| `idle_load` | 0.05 | 0.05 | idle share in the load model |
| `fuel_load_max` | 1.0 | 1.0 | cap of the load factor |
| `electric_load_scale` | 1.0 | 1.0 | reduces the electric demand for values below 1 |
| `grid_boost` | 2.0 | 1.0 | multiplier of the grid transfer ceiling, **grid-wide** |

The load factor comes from the mechanical power demand and goes to the game's consumption functions; tank, refuelling and bookkeeping stay with the game. `grid_boost = 2.0` affects the whole electricity grid but creates no generation capacity; `1.0` leaves the hook out.

---

## 📏 Value ranges

| Quantity | Limit |
|---|---|
| `power_scale` | 0.1 to 5.0 (checked by the plugin, otherwise 1.0) |
| `grid_boost` | up to 1 off, above 1 up to 10 active, above 10 refused |
| route lookahead | at most 8000 m effective |
| numbers | 64-byte read buffer, keep them short |
| editor limits in the schema | brakes 0.05–10 m/s², adhesion 0.01–1, Davis A 0–20, B 0–1, C 0–10, grade scale 0–1, limits 1–200 km/h, lateral 0.1–5 m/s², margin 1–5, lookahead 100–8000 m, idle 0–1, load factor 0.05–10, electric 0–1 |

Apart from `power_scale` and `grid_boost` the plugin itself enforces none of these limits.

---

## 💾 Compatibility

### Savegames
No separate save format. Positions, speeds and consumption already affected stay in the normal savegame; a restart without the plugin does not reset them retroactively. Run comparisons on a copy.

### Other plugins
- Never load the original `railphysics.dll` in parallel (`RP404`).
- Native vehicle and track limits are not raised; the developer mentions `railspeed` as an optional companion, without confirmed compatibility of every variant.
- Other changes at the same hook sites are detected by the signature check and raise `RP401` to `RP406`.

### Version compatibility
- **1.3.5:** first published version of the port; physics and settings as in RailPhysics 1.3.0, the INI section `[railphysics]` stays compatible

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| no effect, no current detail log | plugin not active or `enabled = 0` | check activation, DLL path and INI, read the main log |
| INI change has no effect | no complete restart or wrong INI | restart; `configuration file` in the log shows the file read |
| `RP201` / `RP202` | invalid number or `power_scale` out of range | correct the value; the code default was active |
| `RP401` to `RP406` | executable, signature, instruction or helper check failed | check game version and competing modifications |
| `RP404` | `railphysics.dll` loaded as well | enable one version only |
| `RP303` / `RP304` | hooks installed only partially | do not unload; save the logs, quit the game, check the cause |
| `RP100` to `RP105` | invalid vehicle/wagon data, zone or memory build-up | report recurring cases with the log |
| `RP900` / `RP901` | detail log unavailable | check write permissions and free space |
| electricity grid behaves differently | `grid_boost` is grid-wide | use 1.0 for the comparison |

### Logging

Messages go to `tesmioloader.log` and the detail log `logs\tesmioloader.rail_physics_fix.log` (timestamp, severity, area, rule id). The detail log is reopened and overwritten when the plugin is enabled. Repeated runtime warnings are limited to one output per rule every 30 seconds.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Search for:
- `configuration file` → which INI was chosen
- `subsystem(s) patched` → number of patch sites; `9` with all supplied features and `grid_boost` above 1
- `RP` → every rule id of the plugin

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
rail_physics_fix\
├── hooks\
│   ├── rail_physics_fix.dll        (plugin)
│   └── rail_physics_fix.ini        (original INI with explanations)
├── config\                         (schema for Republic Mod Manager)
│   ├── rail_physics_fix.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                  (manifest for SML, Bridge and Republic Mod Manager)
├── workshopconfig.ini              (Steam Workshop entry)
├── previewimage.png
├── LICENSE                         (GPL v3)
├── README_DE.md
└── README_EN.md
```

**Loader folder** (method 1 by hand or "Files local only")
```
tesmioloader\build\
├── plugins\
│   ├── rail_physics_fix.dll
│   └── rail_physics_fix.ini        (effective INI)
├── logs\
│   └── tesmioloader.rail_physics_fix.log
└── tesmioloader.log
```

---

## 📜 Licence & credits

**GPL v3**, see `LICENSE`. Rail Physics Fix is a reworked version of **RailPhysics 1.3.0** by **Meow Meow** (TheRealMeowMeow00): original on the Workshop at https://steamcommunity.com/sharedfiles/filedetails/?id=3776784867, source of the original at https://github.com/TheRealMeowMeow00/WRSR_RailPhysics. Physics and settings are taken over unchanged; the changes of this version (argument passing to the hook sites, memory access, configuration parsing, degrading behaviour on torn data, diagnostics, package support) are described in the "New in …" sections and in `BUILD_INFO.md`. The complete source of Rail Physics Fix (source, bridges, offline tests with 17 processes, build evidence) lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/rail_physics_fix. The loader SDK header comes from the TesmioLoader by MaxLegend (GPL v3).

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Does the plugin change game files?**
A: No. It installs hooks in memory and verifies the executable first; without a matching version nothing happens.

**Q: Why does the log report fewer than nine patches?**
A: Disabled features or `grid_boost = 1.0` request fewer patch sites. `only N/9 requested patches installed`, however, is a warning.

**Q: Can I use only the brakes or only the consumption?**
A: Yes, through the feature switches. Grade stays part of the acceleration and consumption model even with `slope = 0`.

**Q: Why does my train not reach its top speed?**
A: Native vehicle and track limits, power, mass, resistance, grade and power supply still apply; the plugin raises no limits.

**Q: Can I edit the INI by hand?**
A: Yes, following the rules in [Configuration](#-configuration). Republic Mod Manager offers the same values with descriptions and range checks.

---

**Last update:** Rail Physics Fix 1.3.5  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
