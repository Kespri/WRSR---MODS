# 🚚 Vehicle Materials 0.4.0

**TesmioLoader plugin for additional vehicle materials**

Freely configurable extra materials for vehicle production in *Workers & Resources: Soviet Republic* 1.1.1.9: glass, cables, copper or any other resource the Resources plugin registers, set separately for road, rail, ship and aircraft.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Preparing production buildings](#-preparing-production-buildings)
- [Configuration](#-configuration)
- [Value ranges](#-value-ranges)
- [Mapping vehicle types](#-mapping-vehicle-types)
- [Relation to Resources](#-relation-to-resources)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Mandatory: the Resources plugin (resources.dll/resources.ini) with every resource you want to use as a vehicle material. Without Resources, Vehicle Materials does not start; the reason is in the log.

### In three steps
1. **Register the resources:** every material must exist in `plugins\resources.ini`, for example `glass` or `cable`. In Republic Mod Manager use the Resources entry.
2. **Choose one installation method** (see below), enable the plugin and enter the materials with their coefficients, most comfortably through Republic Mod Manager.
3. **Adjust the production buildings:** every vehicle factory needs a storage for the new material (see [Preparing production buildings](#-preparing-production-buildings)). Then start the game.

---

## ✨ Features

### 🎯 Core function
- ✅ Additional materials for building vehicles, without changing any game file
- ✅ The game's original material requirements stay in place
- ✅ Up to 32 materials, each one registered by Resources
- ✅ The configuration is checked completely at start-up; on any error the plugin stays off and the game runs as usual

### 🆕 Settings

#### 1️⃣ **Separate coefficients per vehicle class** (`[road]`, `[rail]`, `[ship]`, `[airplane]`)
- One value per material and class; `0` or a missing entry disables the material for that class
- The game multiplies the coefficient with the vehicle's internal production value: bigger vehicles need more

#### 2️⃣ **Personal overlay** (`user_overlay = 1`)
- The DLL reads its normal INI first, then `build\user_config\vehicle_materials.ini` key by key on top
- Republic Mod Manager writes only the overlay; the original INI stays untouched whether the DLL is loaded from `plugins`, from SML or through the Workshop Bridge

#### 3️⃣ **Vehicle type mapping** (`[mapping]`)
- Automatic: type 1 road, type 6 ship, type 7 aircraft, everything else rail
- Overridable by hand per internal type 0 to 15

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice. Resources has to be installed and enabled in every case.

---

### Method 1️⃣: classic TesmioLoader

```
1. Copy vehicle_materials.dll and vehicle_materials.ini from hooks\
   → tesmioloader\build\plugins\

2. Enable vehicle_materials in TesmioLauncher
3. Make sure resources is enabled and the materials exist in resources.ini
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL from the package through soviet.mod.ini, the INI lies beside it
3. The configured resources must exist in SML's resource catalogue as well
4. Remove or disable any local vehicle_materials.dll in plugins\ first
```

---

### Method 3️⃣: Workshop Bridge (without SML)

```
1. Select the package in Republic Mod Manager
2. Switch "Plugin active" on
3. workshop_bridge loads the DLL straight from the package
4. Steam updates apply immediately
```

---

### Method 4️⃣: Republic Mod Manager with "Files local only"

```
1. Select the package in Republic Mod Manager, add materials with +, save
2. Tab "General", card "Notices": switch "Files local only" on
3. Confirmation with file list → DLL and INI are copied into
   tesmioloader\build\plugins\ when saving
4. The Workshop Bridge then skips the package automatically
```

**"Files local only" in detail**
- For everyone who wants to keep using the plugin without the Steam subscription
- With local files, Steam updates apply only after saving again (yellow "Update" badge)
- Switching it off removes only the files Republic Mod Manager copied

---

## 🧰 Republic Mod Manager

The package ships a launcher schema in the `config` folder. Republic Mod Manager shows Vehicle Materials with four tabs, in German and English:

- **General:** notices, "Files local only" and the diagnostics settings
- **Resources:** the material list; the plus button offers only resources Resources has registered and creates the coefficient for every vehicle class; the trash button removes material and coefficients again
- **Vehicle classes:** the coefficients as a table of material by class
- **Advanced mapping:** vehicle types 0 to 15

The "Plugin active" switch in the header also sets `enabled = 1` when switched on. Personal values live in `user_config\vehicle_materials.ini`; the shipped INI stays unchanged. If you prefer editing the INI by hand, everything you need is below.

---

## 🏗️ Preparing production buildings

⚠️ **Every new material needs its own storage line in every vehicle factory that is to build with it.** Otherwise it is neither delivered nor accepted. The plugin adds the vehicle's requirement only; it creates no storage. The factories know the transport classes **COVERED** and **OPEN** from the start; the transport class of the new line must match the resource (see the Resources editor, column transport class). Three ways are open for the storage line:

1. **Copy a building** and enter the storage lines by hand, for example with the TesmioLoader plugin Buildings.
2. **Vanilla Buildings** from the Workshop: adds the storage lines to the game's own buildings.
3. **Manual entry** in the building's `building.ini`.

In all three cases it is the same `$STORAGE_IMPORT_SPECIAL` line, one per material:

```ini
$STORAGE_IMPORT_CARPLANT RESOURCE_TRANSPORT_COVERED 250
$STORAGE_IMPORT_CARPLANT RESOURCE_TRANSPORT_OPEN 300
$STORAGE_EXPORT RESOURCE_TRANSPORT_VEHICLES 15
; new:
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN 100 glass
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN 50 cable
```

- `100` and `50` are the storage capacity
- `glass` and `cable` are the exact resource IDs from resources.ini
- the transport type must match the resource's transport class; in the example glass and cable are registered as OPEN

This applies to the factories for road vehicles, rail vehicles, ships and aircraft. Buildings can be adjusted through a building mod of your own or a TesmioLoader plugin. Check in the game that the new storage slots appear on the building and get supplied.

---

## ⚙️ Configuration

### Main file: `vehicle_materials.ini`

The DLL reads in this order:
- **Base:** `tesmioloader\build\plugins\vehicle_materials.ini` if present, otherwise the INI beside the DLL (in the package `hooks\vehicle_materials.ini`)
- **Overlay:** `tesmioloader\build\user_config\vehicle_materials.ini`, key by key on top; this is where Republic Mod Manager writes

⚠️ **Comments only on their own lines, starting with `;`.** A comment behind a value is read as part of the value and rejects the whole configuration. The file must be UTF-8 without BOM; all seven sections must exist, even empty ones; unknown or duplicate sections and keys, empty values and invalid numbers reject the whole configuration. No hot reload: edit the INI, restart the game.

### Section `[general]`

```ini
[general]
; 1 enables the plugin, 0 disables it; exactly 0 or 1
enabled = 0
; 1 writes materials and vehicle builds to the detail log tesmioloader.vehicle_materials.log
debug = 0
; upper limit for repeated warnings and diagnostic messages per session, 0 suppresses them
debug_limit = 80
```

### Section `[resources]` and the four vehicle classes

Example with two materials, the way Republic Mod Manager creates them:

```ini
[resources]
; number of entries resource0, resource1, ...
count = 2
; exact resource ID from resources.ini, each one only once
resource0 = glass
resource1 = cable

[road]
; coefficient per material; 0 or missing = no requirement in this class
glass = 0.020
cable = 0.010

[rail]
glass = 0.030
cable = 0.015

[ship]
glass = 0.010
cable = 0.005

[airplane]
glass = 0.015
cable = 0.020

[mapping]
; -1 = automatic, 0 = road, 1 = rail, 2 = ship, 3 = aircraft
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

Rules:
- numbering starts at `resource0`, `count` must match the number of entries, entries at or above `count` are rejected
- names are unique regardless of case and must not contain spaces, path or special characters
- a key in a vehicle class must be listed under `[resources]`
- a material without a positive value in at least one class is ignored; with `enabled = 1` Republic Mod Manager requires at least one such material
- a material a vehicle already requires is not added a second time

Start with small values and check the quantities in the game.

---

## 📏 Value ranges

The DLL checks these limits at startup. A value outside them rejects the whole configuration; the plugin then installs no hook and the game runs with its original materials.

| Key | Range | Default |
|---|---|---|
| `enabled`, `debug` | exactly 0 or 1 | 0 / 0 |
| `debug_limit` | 0 to 10000 | 80 |
| `count` | 0 to 32 | 0 |
| `resource0` … `resource31` | ID from resources.ini, at most 63 characters | none |
| coefficients in `[road]`, `[rail]`, `[ship]`, `[airplane]` | finite number 0 to 1000000 | none |
| `type0` … `type15` | -1 to 3 | -1 |
| file | at most 1 MiB, keys and values at most 63 characters | |

---

## 🔀 Mapping vehicle types

The game keeps vehicles under internal production types. The plugin maps them like this:

| Internal type | Class |
|---|---|
| 1 | road |
| 6 | ship |
| 7 | aircraft |
| all others | rail |

`[mapping]` overrides this per type 0 to 15:

| Value | Class |
|---|---|
| -1 | automatic |
| 0 | road |
| 1 | rail |
| 2 | ship |
| 3 | aircraft |

Normally the section stays unchanged. With `debug = 1` the detail log states which type was mapped to which class.

---

## 🔗 Relation to Resources

Vehicle Materials creates no resource. At startup it asks the registry service of the Resources plugin and uses only names published there:

- if Resources is missing or disabled, Vehicle Materials does not start; the reason is in the log
- a material missing from resources.ini rejects the configuration
- the name must be spelled exactly the same in both INIs

When a material is removed from resources.ini, Republic Mod Manager also cleans up the Vehicle Materials entries after asking.

---

## 💾 Compatibility

### Saved games

The plugin changes no game file and stores nothing in the saved game. Materials that vehicles require are part of the save's resource list: a resource already used in a saved game must not be removed from resources.ini any more.

### Version compatibility

- **0.4.0:** first published version; the configuration is read as base plus personal overlay

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| Plugin does not start | Resources missing or disabled | install resources.dll and enable it in the launcher |
| Configuration rejected | material not in resources.ini, comment behind a value, value out of range | read the log, correct names and values |
| Vehicle does not demand the material | `enabled = 0` or coefficient 0 in this class | switch "Plugin active" on, set the coefficient |
| Factory does not accept the material | no `$STORAGE_IMPORT_SPECIAL` in the building | adjust the building (see above) |
| Wrong vehicle class | internal type differs from the expectation | `debug = 1`, set the mapping in `[mapping]` |

### Logging

Every message is in `tesmioloader.log`, with `debug = 1` additionally in `logs\tesmioloader.vehicle_materials.log`.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Expected on a successful start:
```
vehicle_materials  Hook active at SOVIET64.exe+0x...
vehicle_materials  Automatic mapping: type1=road, type6=ship, type7=airplane, other=rail
vehicle_materials  v0.4.0 ready
```

Search for:
- `vehicle_materials` → every message of the plugin
- `FATAL` → rejections with cause and recommended action
- `Personal overlay applied` → the overlay from user_config was read

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
vehicle_materials\
├── hooks\
│   ├── vehicle_materials.dll       (plugin)
│   └── vehicle_materials.ini       (original INI, no materials, enabled = 0)
├── config\                         (launcher schema for Republic Mod Manager)
│   ├── vehicle_materials.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                  (manifest for SML, Bridge and Republic Mod Manager)
├── workshopconfig.ini              (Steam Workshop entry)
├── previewimage.png
├── README_DE.md
└── README_EN.md
```

**Loader folder** (method 1 by hand or "Files local only")
```
tesmioloader\build\
├── plugins\
│   ├── resources.dll               (mandatory, separate plugin)
│   ├── resources.ini
│   ├── vehicle_materials.dll
│   └── vehicle_materials.ini       (original INI)
└── user_config\
    └── vehicle_materials.ini       (personal values from Republic Mod Manager)
```

Through the Bridge or SML only the original INI lies in `plugins`; the DLL stays in the package. The personal values live in `user_config` in every case.

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. The plugin contains no third-party code; the loader SDK header comes from the TesmioLoader by MaxLegend (GPL v3). The complete source lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/vehicle_materials.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Can I use materials that are not in resources.ini?**
A: No. Vehicle Materials uses only resources the Resources plugin has registered.

**Q: Why does nobody deliver the material to the factory?**
A: The factory needs its own storage for it, a `$STORAGE_IMPORT_SPECIAL` line in its building.ini. The plugin does not create that storage.

**Q: Are the original materials replaced?**
A: No. Steel, mechanical components and everything else stay; the new materials are added.

**Q: What happens on an error in the INI?**
A: The whole plugin is rejected and installs no hook. The game builds vehicles as without the plugin; the reason is in the log.

**Q: Do I have to edit the INI by hand?**
A: No. Republic Mod Manager offers the materials from resources.ini with +, creates the coefficients per class and checks the value ranges.

---

**Last update:** Vehicle Materials 0.4.0  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
