# 🚛 Technical Service Storage 0.3.3

**TesmioLoader plugin for grit storages, material priorities and snowplow tanks**

Extends the Technical Services window of *Workers & Resources: Soviet Republic* 1.1.1.9 with additional single-resource storages, clickable material priorities and warnings. Snowplows get a plugin-owned grit tank whose consumption, refill and return to the home depot are managed by the plugin. Together with Weather Roads the loaded material decides how much new snow a plowed road keeps off.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Materials and depot priorities](#-materials-and-depot-priorities)
- [Grit tank, consumption and return](#-grit-tank-consumption-and-return)
- [Building storages and migration](#-building-storages-and-migration)
- [Saving and loading](#-saving-and-loading)
- [Localization](#-localization)
- [Value ranges](#-value-ranges)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Technical Services with `$TYPE_GARBAGE_OFFICE` (internal building type 49) and snowplows with skill 35
- For grit operation: registered grit resources and matching single-resource storages in the depot (see [Building storages and migration](#-building-storages-and-migration))
- **Recommended:** the Localization plugin with the `technical_service_storage` text pack for translated labels; without it built-in fallback labels are used
- **Optional:** Weather Roads for material-dependent road protection

### In three steps
1. **Choose one installation method** (see below) and enable the plugin.
2. **Check the materials:** adapt the supplied list (`sand`, `gravel`) in Republic Mod Manager or in `technical_service_storage.ini` to the resources and depot storages you have. **Back up a savegame.**
3. **Restart the game completely.** The depot window shows the grit storages with priorities; the log `logs\tesmioloader.technical_service_storage.log` names the configuration path, the material list and the service status.

---

## ✨ Features

### 🎯 Core function
- ✅ Additional storage rows in the Technical Services window, reusing the native rendering
- ✅ Clickable per-depot material priorities: OFF → N → … → 2 → 1 → OFF; the lowest enabled number with stock is used
- ✅ Red warnings for configured materials that are empty or OFF in the building
- ✅ Ordered material list with a protection strength per material (up to 32 materials)
- ✅ Plugin-owned grit tank per snowplow, sized from empty weight and engine power; consumption proportional to the real fuel use
- ✅ Automatic return to the home depot at a reserve threshold; refills debit only what was actually loaded
- ✅ Missing grit storages are added empty to finished depots on load (add only, never rebuild)
- ✅ Priorities and tanks are saved in extra files beside the savegame
- ✅ Grit spreader service `tss.grit_spreader` for Weather Roads (material strength, dry plowing)
- ✅ The real vehicle fuel is read, never changed; original building definitions stay untouched

### 🆕 New in 0.3.3
- ✅ **Reserve threshold in percent:** `return_threshold_percent = 20` instead of basis points; the old key is still read while the new one is absent.
- ✅ **Tracked vehicles:** the table now holds 1024 vehicles (256 before); the default of `max_vehicles` is 512 and can be set between 1 and 1024.
- ✅ The shipped material list is only `sand` and `gravel`; custom resources such as road salt are yours to add.
- ✅ All Republic Mod Manager texts reworked in player style.

### 🆕 New in 0.3.1
- ✅ **INI beside the DLL:** when `plugins\technical_service_storage.ini` is missing, the DLL reads the INI from its own folder, i.e. from the Workshop package under Soviet Mod Loader or the Workshop Bridge. The chosen path is logged as `Configuration file:`.
- ✅ Editor schema for Republic Mod Manager in the package: the material list as a list with a plus button, every switch in cards on four tabs, German and English.
- Parser, validation, consumption, return, tank sizes, priorities and file formats are unchanged from 0.3.0.

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice.

---

### Method 1️⃣: Classic TesmioLoader

```
1. Copy technical_service_storage.dll and technical_service_storage.ini from hooks\
   → tesmioloader\build\plugins\

2. For translated labels: install the Localization plugin; the text pack ships
   with the Localization package under plugins\localization\technical_service_storage\

3. Enable technical_service_storage in the TesmioLauncher
4. Restart the game completely
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL through soviet.mod.ini from the package, the INI sits beside it
3. Remove or disable a local technical_service_storage.dll in plugins\ first
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

The package contains an editor schema in the `config` folder. Republic Mod Manager (0.34.0 or newer) shows Technical Service Storage in four tabs, German and English:

- **General:** notes, "Files local only", a button for this guide, the card "Saving in the save game" (storage migration, saving priorities and tanks, forgetting demolished depots) and below it "Troubleshooting" (detailed log, message limit)
- **Grit materials:** the material list on the left (up to 32), the selected material with its protection strength on the right; the plus button adds a material (internal resource name as text, suggestions grouped into custom resources from the Resources plugin and vanilla resources). Original lines can be hidden with the trash button and shown again
- **Depot window:** storage rows and Office Priority labels, priority controls, divider line and warnings
- **Grit operation:** consumption and detection, return to the home depot, tank capacity, vehicle display and timing

Personal materials live in `user_config\technical_service_storage.editor.ini`, the effective file is `plugins\technical_service_storage.ini`; the INI in the package stays unchanged. Decimals are normalised when saving (`0.80` becomes `0.8`), which the plugin reads the same way.

---

## ⚙️ Configuration

### Main file: `technical_service_storage.ini`

The DLL reads in this order:
- **First:** `tesmioloader\build\plugins\technical_service_storage.ini` when it exists (classic installation, "Files local only" or the effective INI written by Republic Mod Manager)
- **Otherwise:** the INI beside the DLL, in the package `hooks\technical_service_storage.ini` (Soviet Mod Loader, Workshop Bridge)

**UTF-8 without BOM** is recommended; UTF-8 with BOM and UTF-16 LE are read as well. At most 1 MiB. `;` only on its own line, no comments after values. Material strengths with a decimal point, switches as `0` or `1`. Changes apply after a complete restart.

⚠️ **Deleting the INI does not disable the plugin.** Without the file the defaults apply, including `enabled = 1` and `sand = 0.50`. To switch off, set `[general] enabled = 0` and restart.

### Sections

| Section | Purpose |
|---|---|
| `[general]` | `enabled`, `debug`, `debug_limit` (UI debug messages only) |
| `[storage_migration]` | add missing storages on load |
| `[ui]` | row spacing, Office Priority wrapping, priority controls, grit divider line, material warnings |
| `[priority_persistence]` | save depot priorities per savegame |
| `[tank_persistence]` | save grit tanks per savegame |
| `[grit_materials]` | ordered material list, `name = strength` |
| `[sand_diagnostic]` | grit operation: consumption, return, tank calculation, vehicle display, timing (legacy section name, controls the real operation) |
| `[tank_probe]` | depot lifecycle tracking (`building_lifecycle`, keep on) |

The INI explains every value right at the setting; Republic Mod Manager shows the same explanations.

### Validation and fallback behaviour

| Case | Behaviour |
|---|---|
| Unknown or obsolete section/key | warning, the entry is ignored |
| Duplicate section or key | the first one wins |
| Invalid switch or malformed integer | warning and compiled default |
| Integer outside its range | clamped to the boundary with a warning |
| Duplicate material | the first valid entry stays |
| Single invalid material | warning, the other materials stay |
| Missing INI or missing material section | defaults, in particular `sand = 0.50` |
| Explicitly empty or entirely invalid material list, unreadable file | no silent sand substitute: grit operation, migration and priority/tank persistence are suspended for this session |

The legacy section name `[Streumaterialien]` is still read as an alias; with both names the first section in the file wins.

---

## 🧂 Materials and depot priorities

The supplied list:

```ini
[grit_materials]
sand = 0.30
gravel = 0.45
```

These are **internal resource names**, not translated captions. An additional resource such as `road_salt` has to be registered by the game or a resource plugin; `sand` comes from the Resources plugin, `gravel` from the base game. Custom resources such as road salt are yours to supply (resource, images, storages) before you add them here. Unknown names are reported when a world loads and are disabled for that world. `fuel` is vehicle fuel and never grit.

The strength from `0.00` to `1.00` is the share of new snow a compatible Weather Roads prevents. It is neither a stock amount nor a consumption nor a duration. `0.00` is valid but grants no protection; use the depot's OFF control to exclude a material.

### Selection in the depot
- File order gives the initial priorities: first entry `1`, then `2` and so on.
- The lowest enabled number with available stock is selected; `1` is the highest priority.
- Click order: `OFF → N → … → 2 → 1 → OFF`. Choosing an occupied number swaps the two materials.
- When every permitted material is empty or OFF, plowing continues without grit.
- Priorities are per depot and saved; a new INI order does not replace saved decisions.

### Effect with Weather Roads
With material strength `S` the share `1 - S` of new snow remains during the strong phase, and `1 - S × (1 - M)` during the salt phase with the Weather Roads factor `M`. Example `S = 0.50`: 50 % in the strong phase, 75 % afterwards with `M = 0.50`. Duration and phases belong to the Weather Roads INI. Confirmed dry plowing reports strength `0.00`; whether an older treatment survives is decided there by `dry_plowing_preserves_treatment`. A weaker material does not replace a stronger active protection.

---

## ⛽ Grit tank, consumption and return

### Tank capacity

```text
base capacity in kg =
  empty weight in kg × tank_weight_percent / 100
  + engine power in kW × tank_power_kg_per_kw

then × tank_capacity_multiplier_percent / 100,
rounded to tank_capacity_step_kg,
clamped to tank_minimum_capacity_kg .. tank_maximum_capacity_kg
```

Supplied are 10 % weight share, 2 kg/kW, factor 100 %, rounding to 50 kg and the bounds 250 to 5000 kg. That gives about 500 kg for a GZ-53 and 850 kg for an SKD 706; changed vehicle definitions give other results.

### Consumption and refilling

```text
grit use in kg =
  tank capacity × eligible fuel used / fuel capacity
  × grit_fuel_consumption_factor_percent / 100
```

Only confirmed clearing counts; `110` equals factor 1.10. The remainder never drops below zero. A confirmed refill debits only the amount actually loaded from the selected depot storage; partial fills are possible. A restored tank does not debit the depot a second time.

### Return to the home depot
- `return_threshold_percent = 20` means 20 % of the individual capacity (0 to 100; the old key `return_threshold_basis_points` only counts without the new one). The threshold requests the return; it neither fills nor empties the tank.
- Routing and movement stay with the game (native home/refuelling destination). While grit remains, clearing on the way consumes the real remainder; at zero the plow works dry.
- Without a permitted material dry plowing stays possible; availability is checked again at later clearing events and by the regular depot observation.
- Refill confirmation uses the physical arrival or a qualifying fuel increase while the refuelling destination is active. No depot window needs to be open.

Two supported keys are not in the supplied INI and normally stay unchanged: `return_retry_interval_ms` (250, 250 to 10000 ms) and `return_arrival_settle_ms` (10, 10 to 2000 ms). The old key `consumption_enabled` only triggers a compatibility warning.

---

## 🏗️ Building storages and migration

The material list creates neither resources nor building storages. A material needs a matching single-resource storage in the depot, schematically:

```text
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_COVERED <capacity> <resource>
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL  <capacity> <resource>
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN    <capacity> <resource>
$STORAGE_FUEL           RESOURCE_TRANSPORT_OIL    <capacity>
```

These lines belong into the building definition or into a plugin that changes it, for example **Vanilla Buildings** (a rule set with `add` lines for `buildings_types\technical_services.ini`). The transport class has to match the resource; general multi-resource storages are not part of the contract.

With `[storage_migration] enabled = 1` missing configured grit imports of the current definition are taken over into finished depots on load:
- New storages are appended empty, in definition order.
- Existing order, capacity, stock and priorities are kept; nothing is shrunk, grown, converted or removed.
- Deviating or ambiguous assignments are reported, not guessed.
- Construction sites and buildings being demolished or collapsed are no migration targets.

⚠️ The next normal save can put the additions into the **native savegame**. Switching off does not undo them; to reset, use a savegame from before the migration.

---

## 💾 Saving and loading

Beside the native savegame these files appear in the savegame folder:

| File | Content |
|---|---|
| `tesmioloader.technical_service_storage.priorities.bin` | material priorities per depot |
| `tesmioloader.technical_service_storage.tanks.bin` | assigned material and remaining grit per vehicle |

The files are matched through the save/load hooks and replaced through temporary files; formats, sizes and records are verified. Vehicle matching uses home depot position, vehicle slot, engine power and empty weight; mismatching tank data is discarded and the first runtime reconstruction applies instead. Missing priority data falls back to the material-list order.

- Demolition and collapse end a depot's identity; a rebuilt depot starts with default priorities. Keep `[tank_probe] building_lifecycle = 1` on for that.
- Switching a persistence off leaves existing extra files in place.
- When transferring or backing up a savegame take the whole folder. Steam Cloud and ZIP autosaves are not confirmed for the extra files.
- The protection file of Weather Roads is a separate extra file of that other plugin.

---

## 🌐 Localization

Translated plugin texts come from the Localization plugin with the `technical_service_storage` text pack (`namespace = technical_service_storage`, `fallback = sovietEnglish`); it ships with the Localization package. Keys in use:

- `vehicle_tank.grit`, `vehicle_tank.none`, `vehicle_tank.dry_plowing`, `vehicle_tank.depot`
- `material_priority.off`
- `building_warning.resource`, `building_warning.empty`, `building_warning.off`

The resolved IDs have to lie between 2,000,000 and 2,999,999. Missing keys are reported per key; built-in fallback texts stay available. Resource names and the priority heading use the native game texts.

---

## 📏 Value ranges

| Quantity | Limit |
|---|---|
| Materials | at most 32; names up to 63 bytes, line up to 511 bytes |
| Protection strength | 0.00 to 1.00 |
| `debug_limit` | 0 to 10000 |
| `[ui]` distances and sizes | `resource_gap` 0–50, label width 120–400, line height 12–28, offset X 100–800, offset Y -50–100, width 60–300, height 12–60, gap to the divider line 48–100 |
| Consumption factor | 1 to 1000 percent |
| Reserve threshold | 0 to 100 % |
| Tank | weight share 0–50 %, 0–20 kg/kW, factor 10–500 %, step 1–1000 kg, minimum 1–10000 kg, maximum 1–20000 kg |
| Timing | duplicate window 0–60000 ms, summaries 100–60000 ms, clearing log 0–60000 ms, depot scan 250–10000 ms |
| Tracked vehicles | 1 to 1024 (default 512) |
| Display offset | -1000 to 1000 |
| INI | at most 1 MiB |

---

## 💾 Compatibility

### Savegames
Added storages can become part of the native savegame. Priorities and tanks live in extra files; without them the savegame loads normally, but the exact earlier priorities and tank remainders are not guaranteed.

### Other plugins
- **Weather Roads** uses the grit spreader service of this plugin; without it, it counts with strength 1.00.
- **Localization** provides the translated labels (recommended, not mandatory).
- **Vanilla Buildings** or the Buildings plugin add the required single-resource storages to the depots.
- Resource plugins (Resources, Deposits Plus) register additional materials such as `road_salt`.

### Version compatibility
- **0.3.3:** reserve threshold in percent (old key still read), vehicle table 1024 (default 512), default list without `road_salt`, texts; file formats unchanged
- **0.3.1:** INI fallback beside the DLL, editor schema in the package; operation and file formats unchanged
- **0.3.0:** one version number for plugin and grit component

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| `ini-unknown-key` / `ini-unknown-section` | spelling or old setting | remove or correct the entry |
| `ini-value` / `ini-range` | value invalid or out of range | default or boundary was used; check the value |
| `grit-material-list` | name, strength, duplicate, limit or empty list | check the material list |
| `validation=not-found` | resource not registered in this world | check the resource plugin and the exact name |
| `requires-single-resource-import` | building storage does not meet the single-resource contract | adjust the building definition |
| `already-present-with-different-layout-or-capacity` | existing storage differs | it is not rebuilt; compare definition and stock |
| `vehicle-tank-key` | text key missing or ID out of range | check the text pack |
| `no-sidecar` | no extra file yet | normal on the first load |
| `invalid-sidecar-defaults-used` | extra data rejected | use the matching savegame |
| No refill | service, material, stock, OFF priority or arrival | check the depot window and the log |

### Logging

All messages go to `tesmioloader.log` and the detail log `logs\tesmioloader.technical_service_storage.log`, with area, rule id and context. `debug = 0` suppresses routine details only; warnings, errors and start/save messages remain. `debug = 1` adds details and the read-only Ctrl+F8 lifecycle snapshot.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Search for:
- `Configuration file` → which INI was chosen
- `grit-material-list` → the accepted material list
- `completed with restrictions` → check the partial functions; installed hooks stay, not every function is active

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
technical_service_storage\
├── hooks\
│   ├── technical_service_storage.dll   (plugin)
│   └── technical_service_storage.ini   (original INI with explanations)
├── config\                             (editor schema for Republic Mod Manager)
│   ├── technical_service_storage.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                      (manifest for SML, Bridge and Republic Mod Manager)
├── workshopconfig.ini                  (Steam Workshop entry)
├── previewimage.png
├── README_DE.md
└── README_EN.md
```

**Loader folder** (method 1 by hand or "Files local only")
```
tesmioloader\build\
├── plugins\
│   ├── technical_service_storage.dll
│   ├── technical_service_storage.ini   (effective INI)
│   ├── localization.dll                (recommended, separate plugin)
│   └── localization\technical_service_storage\   (text pack, from the Localization package)
├── user_config\
│   └── technical_service_storage.editor.ini (personal materials from Republic Mod Manager)
├── logs\
│   └── tesmioloader.technical_service_storage.log
└── tesmioloader.log
```

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. The plugin contains no third-party code; the loader SDK header comes from the TesmioLoader by MaxLegend (GPL v3). The complete source lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/technical_service_storage. The service header `grit_spreader_api.h` connects it with Weather Roads.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Does the plugin change my building files?**
A: No. It changes the running depot and vehicle state; added storages can however be saved in the native savegame.

**Q: Why is there no grit storage in the depot?**
A: Usually the single-resource storage is missing in the building definition or the resource is not registered. The log names `requires-single-resource-import` or `validation=not-found`.

**Q: The plow does not return to refill.**
A: The return is requested only at the reserve threshold and only when a permitted material with stock and an enabled priority exists. Routing and movement are decided by the game.

**Q: Do I need Weather Roads?**
A: No. Without it the plugin manages storages, priorities and tanks; the effect on road snow comes from Weather Roads.

**Q: Can I edit the INI by hand?**
A: Yes, following the rules in [Configuration](#-configuration). Republic Mod Manager offers the same values with descriptions and range checks.

---

**Last update:** Technical Service Storage 0.3.3  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
