# 🏗️ Vanilla Buildings 0.4.4

**TesmioLoader plugin for temporary changes to building files**

Adjusts Vanilla, DLC and Workshop buildings in *Workers & Resources: Soviet Republic* 1.1.1.9 without modifying a single original file: every game start generates temporary copies with the wanted lines, and only the game's read calls are redirected to them. Typical use: give new materials such as road salt, gravel or sand a storage in the technical services so they are delivered and accepted.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Commands](#-commands)
- [Value ranges](#-value-ranges)
- [Safety rules](#-safety-rules)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9 (reference version; after a game update original lines may change, rules that no longer match are then rejected safely)
- TesmioLoader API 4
- No dependency on other plugins. Resources a rule refers to (for example `road_salt`) must exist in the game, for example through the Resources plugin.

### In three steps
1. **Choose one installation method** (see below) and enable the plugin.
2. **Set up rule sets:** the shipped INI contains one disabled example, `[example_plastics_factory]` with every command. In Republic Mod Manager you add your own rule set with +, for example grit storages for the technical services as shown below; a typed name such as "Technical Service Storage" becomes the section name technical_service_storage.
3. **Restart the game completely.** The log shows `[overlay-opened]` per target as soon as the game reads the changed file.

---

## ✨ Features

### 🎯 Core function
- ✅ Game buildings (`buildings_types\*.ini`), DLC buildings (`dlcN\buildings\...\building.ini`) and Workshop buildings (`WorkshopID\...\building.ini`) in one rule set
- ✅ Several target files per rule set share the same commands; every target is checked on its own against its unchanged original file
- ✅ Commands: replace, remove, add a line, insert before an anchor; add, replace, remove connection blocks
- ✅ Original files in the game and Workshop folders are never touched; a restart without the plugin restores everything
- ✅ A rejected target does not block the others; every rejection is logged with section, target and cause

### 🆕 New in 0.4.0
- ✅ Every Republic Mod Manager text in player style: warning only in the Notes card, one header switch, Troubleshooting card, notice box and multi-line field texts on the Buildings tab.
- ✅ **`insert` with a position:** `insert = 0 | ANCHOR | LINE` inserts before the anchor, `insert = 1 | ANCHOR | LINE` after it. For `$COST_RESOURCE_AUTO`, 1 adds the material to the phase of the `$COST_WORK` anchor line, 0 to the phase before it.
- ✅ **INI beside the DLL:** when `plugins\vanilla_buildings.ini` does not exist, the DLL reads the INI from its own folder, i.e. from the Workshop package under Soviet Mod Loader or the Workshop Bridge. The plugin runs straight from the Steam subscription.
- ✅ The chosen configuration file is logged at start.
- ✅ Editor schema for Republic Mod Manager in the package: rule sets with targets and commands as a list, German and English.

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice.

---

### Method 1️⃣: Classic TesmioLoader

```
1. Copy vanilla_buildings.dll and vanilla_buildings.ini from hooks\
   → tesmioloader\build\plugins\

2. Enable vanilla_buildings in TesmioLauncher
3. Enable or add rule sets in the INI, restart the game completely
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL through soviet.mod.ini from the package, the INI sits beside it
3. Remove or disable a local vanilla_buildings.dll in plugins\ first
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
1. Select the package in Republic Mod Manager, set up rule sets, Save
2. "General" tab, "Notes" card: switch "Files local only" on
3. Confirmation with file list → DLL and INI are copied to
   tesmioloader\build\plugins\ on save
4. The Workshop Bridge skips the package from then on
```

**"Files local only" in detail**
- For everyone who wants to keep using the plugin without the Steam subscription
- With local files, Steam updates apply only after saving again (yellow "Update" badge)
- Switching it off removes only the files Republic Mod Manager copied

---

## 🧰 Republic Mod Manager

The package ships an editor schema in the `config` folder. Republic Mod Manager shows Vanilla Buildings in two tabs, German and English:

- **General:** notes, "Files local only", the button for this guide and the plugin switches (plugin active, detailed log)
- **Buildings:** one row per rule set with its switch, its name, the first target file and the buttons Change and Delete. "Add" and "Change…" open the very same window with two tabs: **Details** (the target files) and **Lines** (the line editor: pick the target file at the top, click its lines on the left, and turn them into Append/Replace/Before/After/Remove; two views, **Original** and **Result** – the result shows, read only, the file the game will read and marks every line a command adds or replaces; connections are handled as blocks, clicking a point line takes the whole connection; a command that does not fit this file is red with the reason, and while one is red the plugin writes nothing at all for that file; the box "Check against every target file" below the buttons says in how many targets a command fits). Every row in a field is one INI line; the count sits below the field, which can be dragged open at its grip. "Choose buildings…" opens the list of every building of the game, the DLCs and the subscribed Workshop items, grouped by kind with search and filters; checked buildings are entered as target lines

**Renaming is a deliberate act, not a side effect.** Republic Mod Manager offers no field for it: Vanilla Buildings changes existing buildings and the game's names should stay; a building of your own is what Buildings Plus is for. If you do want to rename one, do it on the Lines tab with a replace, for example `$NAME 6218` → `$NAME_STR "Salt store"`. That only works in a rule set with **exactly that one** target file: the old line differs in every file, in the other targets the command would not find its anchor, and one failing command discards the whole rule set there. The box "Check against every target file" shows it right away.

Original rule sets from the package can be hidden or overridden line by line. Personal changes live in `user_config\vanilla_buildings.editor.ini`, the effective file is `plugins\vanilla_buildings.ini`; the INI in the package stays untouched. If you prefer editing the INI by hand, everything else is below.

---

## ⚙️ Configuration

### Main file: `vanilla_buildings.ini`

The DLL reads in this order:
- **First:** `tesmioloader\build\plugins\vanilla_buildings.ini` if it exists (classic installation, "Files local only", or the effective INI written by Republic Mod Manager)
- **Otherwise:** the INI beside the DLL, in the package `hooks\vanilla_buildings.ini` (Soviet Mod Loader, Workshop Bridge)

⚠️ **Comments only on their own lines with `;` or `#`.** The file must be UTF-8 without BOM. Section and key names are case-insensitive, game directives and searched lines must match the original exactly. Every command sits on one physical line; `|` separates its fields. Changes take effect after a complete game restart.

### Section `[general]`

```ini
[general]
; 1 enables the plugin, 0 installs no hook; only exactly 0 or 1
enabled = 1
; 1 also logs disabled rule sets and every redirected file open
debug = 0
```

The old names `[vanilla_buildings]` and `verbose` stay supported with a warning in the log; use only one global section and only one debug key.

### Rule sets: one section per rule set

The section name is free and must be unique. Every `target` line adds a target file; all commands apply to every target, wherever they sit in the section. Example of a rule set of your own that gives the technical services storages for road salt, gravel and sand:

```ini
[technical_service_storage]
; 0 keeps the rule set in the file but applies nothing
enabled = 1
; target files, relative to media_soviet or to the game's Workshop folder
target = buildings_types\technical_services_small.ini
target = buildings_types\technical_services.ini
target = buildings_types\technical_services_big.ini
target = dlc3\buildings\technical_services_small\building.ini
target = 2496571917\utrzymanie1\building.ini
; new storage lines, one per material
add = $STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 road_salt
add = $STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 gravel
add = $STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_GRAVEL 120 sand
```

Paths pick their root by themselves: `buildings_types\...` and `dlcN\buildings\...` sit under `<game>\media_soviet`, a numeric Workshop id under `<Steam library>\steamapps\workshop\content\784150`. The DLC or Workshop item must be installed. Absolute paths, `.` and `..` are rejected. `target1`, `target2`, `target3` are optional aliases that additionally check the target kind Vanilla, DLC or Workshop.

The same target file may appear only once across enabled rule sets; all changes to one file belong in one rule set. Existing saved buildings are not retrofitted: new storages appear on newly built ones.

---

## 🛠️ Commands

### Single lines

| Command | Meaning |
|---|---|
| `replace = OLD \| NEW` | Replaces a complete line that occurs exactly once. |
| `remove = LINE` | Removes a complete line that occurs exactly once. |
| `add = LINE` | Inserts a new single line before the final `end`; rejected when it already exists. |
| `insert = 0 \| ANCHOR \| LINE` | Inserts a line before a unique anchor line; several with the same anchor keep their order. |
| `insert = 1 \| ANCHOR \| LINE` | The same after the anchor line. The anchor may also be a line an earlier `add`, `insert` or `replace` of the same rule set produces. `insert_before = ANCHOR \| LINE` is the old spelling of `insert = 0 \| …`. |

Whitespace at the start and end of a line is ignored, differences inside the line are not.

### Connection blocks

| Command | Meaning |
|---|---|
| `add_connection = TOKEN \| POINT1 \| POINT2` | Inserts a new three-line block after the last connection. |
| `replace_connection = TOKEN \| POINT1 \| POINT2 \| NEW_TOKEN` | Changes only the token of the block found uniquely by both points. |
| `replace_connection = TOKEN \| POINT1 \| POINT2 \| NEW_TOKEN \| NEW_POINT1 \| NEW_POINT2` | Same as above and also replaces both points, for example for another height. The token may stay the same. The new points must not touch another connection. |
| `remove_connection = TOKEN \| POINT1 \| POINT2` | Removes the whole three-line block. |
| `add_connection = TOKEN \| POINT` | One-point connection such as `$CONNECTION_ROAD_DEAD`, inserted as one line `$TOKEN x y z`. |
| `replace_connection = TOKEN \| POINT \| NEW_TOKEN` | Changes the token of a one-point connection; with `\| NEW_POINT` behind it also its point. |
| `remove_connection = TOKEN \| POINT` | Removes a one-point connection, whether the game writes it on one line or as token plus point line. |

A point is three numbers, for example `14.5 0 2`; the comparison is numeric, `0` and `0.0000` are equal. The two points must differ and follow the order of the original file. Occupied points and new `*_ALLOWPASS` connections are rejected.

Complete example with every command, shipped as `[example_plastics_factory]` with `enabled = 0`:

```ini
[example_plastics_factory]
enabled = 0
target = buildings_types\plastics_factory.ini
replace = $PRODUCTION plastics 0.11 | $PRODUCTION plastics 0.20
replace_connection = $CONNECTION_CONNECTION | 14.5 0.0 23.3 | 14.5 0.0 21.3 | $CONNECTION_WATERPIPE_INPUT
replace_connection = $CONNECTION_CONNECTION | -19.12 0.36 -27.02 | -19.12 0.36 -23.32 | $CONNECTION_WATERPIPE_INPUT | -19.12 -2.15 -27.02 | -19.12 -2.15 -23.32
remove = $CONSUMPTION_PER_SECOND eletric 0.26
remove_connection = $CONNECTION_CONNECTION | -23.4 0.0 15.9 | -21.4 0.0 15.9
add = $PRODUCTION glass 0.45
add_connection = $CONNECTION_WATERPIPE_OUTPUT | 30 0 0 | 32 0 0
insert = 0 | $COST_WORK SOVIET_CONSTRUCTION_STEEL_LAYING 1.0 | $COST_RESOURCE_AUTO steel 2.0
```

---

## 📏 Value ranges

| Quantity | Limit |
|---|---|
| `enabled`, `debug` | exactly 0 or 1 |
| Rule sets | at most 256 |
| Target files in total | at most 256 |
| Commands per rule set | at most 512 |
| Section name | at most 128 bytes |
| Target path | at most 220 bytes |
| Active configuration line | at most 4096 bytes |
| Plugin INI and every original file | at most 4 MiB |
| Generated file | at most 8 MiB |

Errors in `[general]`, ambiguous section boundaries, a wrong encoding or exceeded file limits reject the whole INI; no hook is installed then.

---

## 🔒 Safety rules

- Every rule is checked against the **unchanged original file** before a copy is created.
- `replace`, `remove`, `insert` and the connection commands need exactly one match; otherwise the target is rejected as a whole, without a partial change.
- Overlapping changes and anchors that another rule modifies are not allowed.
- `add` accepts no `$COST_` lines; `insert` accepts as a new `$COST_` line only `$COST_RESOURCE_AUTO` with a unique `$COST_WORK` line as anchor (1 = material of that phase, 0 = of the phase before).
- Only read calls (`fopen`, `fopen_s`, `_wfopen`, `_wfopen_s`, engine buffer reader) on files under the real game or Workshop folder are redirected; write access never.
- The checks protect the patch structure, not the game-specific values of the directives.

---

## 💾 Compatibility

### Savegames
The plugin changes building definitions, not saved buildings. New storages apply to new constructions; existing buildings need a separate retrofit. Savegames and files in the game folder stay untouched.

### Other plugins
Other plugins that replace the same building file are not merged with these changes. For materials from Vehicle Materials this plugin provides the matching `$STORAGE_IMPORT_SPECIAL` line in the vehicle factories.

### Version compatibility
- **0.4.4:** new per-section key `name`: replaces the existing `$NAME` or `$NAME_STR` line of every target without you knowing the old one; with dots in it a translation key through the Localization plugin
- **0.4.3:** one-point connections such as `$CONNECTION_ROAD_DEAD` can be edited with `add_connection`, `replace_connection` and `remove_connection` (token | point)
- **0.4.2:** `replace_connection` optionally takes two new points after the new token and moves the connection
- **0.4.1:** `insert` also accepts lines produced by earlier commands of the same rule set as anchors
- **0.4.0:** first published version

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| Nothing happens | every rule set `enabled = 0` (as shipped) or plugin off | enable a rule set, restart the game |
| `expected buildings_types...` | target path has no supported form | write the path as above |
| `source line has 0 matches` | searched line missing in the original | copy the complete line from the original file |
| `... matches instead of exactly one` | line or connection not unique | check the line, for connections the coordinates |
| `duplicate target` | target in two enabled rule sets | merge the changes into one rule set |
| `could not read building source file` | DLC or Workshop item not installed | check path and installation |
| `[utf8-bom]`, `[duplicate-key]`, `[boolean]` | INI format | UTF-8 without BOM, keys once, switches 0 or 1 |

### Logging

All messages go to `tesmioloader.log` and the detail log `logs\tesmioloader.vanilla_buildings.log`, with level (INFO, DEBUG, WARN, ERROR, FATAL) and rule name. Every rejected target gets a `WARN [target-rejected]` message even without debug, with section, target, cause and, for command errors, the INI line.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Look for:
- `Configuration file` → which INI the DLL chose
- `target-rejected` → rejected targets with cause
- `overlay-opened` → the game actually read a changed file
- `configuration` → the whole INI was rejected

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
vanilla_buildings\
├── hooks\
│   ├── vanilla_buildings.dll       (plugin)
│   └── vanilla_buildings.ini       (original INI, examples disabled)
├── config\                         (editor schema for Republic Mod Manager)
│   ├── vanilla_buildings.launcher.ini
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
│   ├── vanilla_buildings.dll
│   └── vanilla_buildings.ini       (effective INI)
└── user_config\
    └── vanilla_buildings.editor.ini (personal rule sets from Republic Mod Manager)
```

The temporary copies live under `%TEMP%\TesmioLoader\vanilla_buildings\<process id>-<start id>-<attempt>\`; every start creates a new folder, old ones can be deleted safely.

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. The plugin contains no third-party code; the loader SDK header comes from the TesmioLoader by MaxLegend (GPL v3). The complete source lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/vanilla_buildings.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Are my game files modified?**
A: No. Only temporary copies are created; without the plugin everything is as before.

**Q: Do existing buildings get the new storages?**
A: No, only new constructions. Existing buildings need a separate retrofit.

**Q: Can I change a Workshop building?**
A: Yes, with the Workshop id as the start of the path, for example `2496571917\utrzymanie1\building.ini`. The item must be subscribed and installed.

**Q: What happens after a game update?**
A: Rules whose original lines changed are rejected for the affected target and logged; everything else keeps working.

**Q: Do I have to edit the INI by hand?**
A: No. Republic Mod Manager shows the rule sets as a list with targets and commands, one entry per row, and adds new rule sets with the plus button.

---

**Last update:** Vanilla Buildings 0.4.4  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
