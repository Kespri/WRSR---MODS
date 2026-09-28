# 🏭 Deposits Plus 0.4.10

**Extension of the TesmioLoader plugin deposits**

Fully configurable resource deposits for *Workers & Resources: Soviet Republic* 1.1.1.9 with natural generation, a visual ground texture and vehicle integration.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Value ranges](#-value-ranges)
- [Frequency & size](#-frequency--size)
- [Natural generation](#-natural-generation)
- [Sandy meadow](#-sandy-meadow-visual-texture)
- [Working vehicles](#-working-vehicles)
- [Relation to the original plugin](#-relation-to-the-original-plugin)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Recommended: the Resources plugin (resources.dll/resources.ini) – only needed so that a deposit's minimap button can show the icon of a resource registered there (`icon`). The deposits themselves do not need it.

### In three steps
1. **Disable the original:** if `plugins\deposits.dll` exists, switch "Plugin active" off for deposits in Republic Mod Manager or clear its checkbox in TesmioLauncher. Deleting it is not necessary.
2. **Choose one installation method** (see below) and enable the plugin.
3. **Start the game** and load a saved game. The settings live in `deposits_plus.ini`, most comfortably through Republic Mod Manager.

---

## ✨ Features

### 🎯 Original functions (kept)
- ✅ Custom deposit types (not present in the base game)
- ✅ Fully INI-driven: types, minimap, editor brushes
- ✅ Code patch for new mineral types

### 🆕 Extensions (new in Deposits Plus)

#### 1️⃣ **Natural generation** (`generation`)
- Elongated, branched deposit fields instead of grid points
- Fading richness (rich → weak → empty)
- Automatic adaptation to the country border and to water
- No overlap with other deposits
- Random or fixed seed per world

#### 2️⃣ **Sandy meadow** (`sand_surface`)
- Visual ground texture on sand deposits
- Strength scales with richness (rich fields show more clearly)
- Tile table per ground texture: sets Vanilla, Siberia (summer and snow-dusted autumn), Asia - Jungle and Ultimate Vanilla +; your own DDS files in your own folders welcome
- Purely visual, no gameplay effect
- Desert maps: `desert_fill` turns the whole land into the sand deposit, organically with 60 to 100 % in the lowlands and thinning out with height; the ores still find room

#### 3️⃣ **Working vehicles** (`working_vehicle_skill`)
- A deposit can borrow the vehicle skill of gravel mining
- Example: sand with `gravelmining`
- Works with the vehicle skills the game already has

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice, and Deposits Plus and the original deposits plugin never run at the same time.

### ⚠️ Important: disable the original plugin

If `plugins\deposits.dll` exists:
- **Republic Mod Manager:** switch "Plugin active" off for deposits
- **TesmioLauncher:** clear the checkbox
- **Deleting is not necessary** – while the original is enabled, Deposits Plus stays idle by itself (log line `deposits_plus  idle`)

---

### Method 1️⃣: classic TesmioLoader

```
1. Copy deposits_plus.dll and deposits_plus.ini from hooks\
   → tesmioloader\build\plugins\

2. Copy the folder hooks\deposits_plus (the textures)
   → tesmioloader\build\plugins\deposits_plus\

3. Enable deposits_plus in TesmioLauncher
4. Disable the original plugin deposits
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

⚠️ **While SML is running, Deposits Plus stays idle.** SML brings its own deposits; the plugin loads, steps aside and writes one line into the log. If you want Deposits Plus, switch `soviet_mod_loader` off in `tesmioloader.ini` (the "Plugin active" switch in Republic Mod Manager) and use method 1 or 3. The steps below only make sense if you want to keep the package ready for later.

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL from the package through soviet.mod.ini
3. The DLL finds its textures beside itself in the package
4. Remove or disable any local deposits_plus.dll in plugins\ first
```

---

### Method 3️⃣: Workshop Bridge (without SML)

```
1. Select the package in Republic Mod Manager
2. Switch "Plugin active" on
3. workshop_bridge loads the DLL straight from the package
4. The DLL finds its textures beside itself in the package
5. Steam updates apply immediately
```

---

### Method 4️⃣: Republic Mod Manager with "Files local only"

```
1. Select the package in Republic Mod Manager, adjust the settings, save
2. Tab "General", card "Notices": switch "Files local only" on
3. Confirmation with file list → DLL, INI and the texture folder are copied
   into tesmioloader\build\plugins\ when saving
4. The Workshop Bridge then skips the package automatically
```

**"Files local only" in detail**
- For everyone who wants to keep using the plugin without the Steam subscription
- With local files, Steam updates apply only after saving again (yellow "Update" badge)
- Switching it off removes only the files Republic Mod Manager copied

---

## 🧰 Republic Mod Manager

The package ships an editor schema in the `config` folder. Republic Mod Manager (version 0.4.0 and later) shows Deposits Plus with three tabs, in German and English:

- **General:** notices, "Files local only", buttons for this guide, and the plugin switches code patch, minimap layers, editor brushes
- **Sand structure:** sandy meadow and natural generation
- **Sand tiles:** the tile table, one entry per ground texture with colour and normal-map file
- **Deposits:** the list on the left, every setting of the selected deposit on the right, a plus button for new deposits with resources from resources.ini and an automatically proposed type number

Personal changes live in `user_config\deposits_plus.editor.ini`; the effective file is `plugins\deposits_plus.ini`. The INI in the package stays untouched; a Steam update is the new baseline at once. If you prefer editing the INI by hand, everything you need is below.

---

## ⚙️ Configuration

### Main file: `deposits_plus.ini`

The original INI lives:
- **Classic loader:** `tesmioloader\build\plugins\deposits_plus.ini`
- **Workshop/SML/Bridge:** in the package folder under `hooks\deposits_plus.ini`; Republic Mod Manager writes the effective version to `plugins\deposits_plus.ini`

⚠️ **Comments only on their own lines, starting with `;`.** The parser takes everything after `=` as the value. A comment behind a value breaks tokens and switches generation off ("generation disabled for safety" in the log). No hot reload: edit the INI, restart the game, load the saved game.

### Section `[deposits_plus]` (global switches)

```ini
[deposits_plus]
; Code and UI patches
; 1 splices the new deposit types into the game executable (required for everything else)
code_patch = 1
; 1 adds a minimap button and overlay layer per deposit (hover previews, click keeps)
minimap = 1
; 1 adds a paint/erase pair per deposit to the terrain editor
editor = 1

; Natural generation
; 1 checks every deposit type after each world load and places the ones still empty
generation = 1
; 1 also fills channels recorded as empty earlier; 0 protects old empty channels
generate_existing_empty = 1
; 0 = random seed per new world; any other number = always the same layout
generation_seed = 0

; Distances
; minimum distance between a new field and any other deposit (metres)
generation_gap_m = 40
; minimum distance between a new field and water (metres)
generation_shore_m = 40
; height the terrain inside a cell must lie above the water level (metres)
generation_water_clearance_m = 2
; 1 = the terrain's gravel/rock zone counts as occupied too (mountain maps such as Siberia then leave almost no room)
generation_block_gravel = 0

; Sandy meadow (optional)
; 1 = on, 0 = off
sand_surface = 1
; strength 0 to 1; 1.0 = full look, smaller values fade it
sand_surface_strength = 1.0
; token of the deposit the surface follows
sand_surface_token = $TYPE_MINE_SAND
```

### Per deposit: its own section

The section name is the deposit's name; `deposits_plus` is the only forbidden name. Example sand, as shipped, with every possible key:

```ini
[sand]
; required: the token in the mine's building.ini
token = $TYPE_MINE_SAND
; required: type number 10 to 127, no duplicates (0 to 9 belong to the game)
type = 11
; auto, resourcemap, resourcemap2 ... resourcemap10 or terrain
map = terrain
; colour channel 0 to 3 of the map; required unless map = auto
component = 1
; 1 allocates a separate resource channel and copies existing sand there
independent_map = 1
; 1 = on desert maps ($TYPE_DESERT in script.ini) the whole land is this deposit when its map is first created; never blocks the other deposits
desert_fill = 1
; 1 = richness falls with height (full band in the lowlands, 0 on the highest land), 0 = the band everywhere
desert_fill_relief = 1
; richness band in percent in the lowlands, varied by noise
desert_fill_min = 60
desert_fill_max = 100
; 7 = mine (default), 92 = water well
building_type = 7
; search radius of the mine: oil, ore, bauxite, gravel, wood, water, watersurface, or a number in metres
radius = gravel
; resource whose icon the minimap button shows (empty = no icon)
icon = sand
; 1 = minimap button and layer (hover previews it, click keeps it), 0 = none
minimap = 1
; name of the editor brush, at most 31 characters (empty = no brush)
editor = sand
; only for the depletion plugin: tonnes per saturated texel, 0 = infinite, empty = global figure
; deplete = 500

; Natural generation
; 1 places this deposit once, 0 only prevents the first placement
generation = 1
; frequency 1 to 6 (see table), default 3
generation_frequency = 5
; size class 1 to 3 (see table), default 2
generation_size = 3
; richness 0.001 to 1, independent of the presets
; generation_richness_min = 0.45
; generation_richness_max = 1.00

; Working vehicles: gravelmining or none; needs building_type = 7
working_vehicle_skill = gravelmining
```

Further deposits in the shipped INI: copper (type 10), clay (type 12) and gas (type 13). A deposit is rejected and reported in the log when its token is missing, its type number lies outside 10 to 127, its map is unknown, its type number, token or channel is a duplicate, or its radius is unusable.

---

## 📏 Value ranges

The DLL checks these limits. A value outside them switches generation off for the deposit (section) or for all (section `[deposits_plus]`) and writes "generation WARN … generation disabled for safety" to the log.

| Key | Range | Default |
|---|---|---|
| `generation`, `generate_existing_empty`, `independent_map` | 0 or 1 | 1 / 1 / 0 |
| `generation_seed` | 0 to 4294967295, integer | 0 |
| `generation_gap_m`, `generation_shore_m` | 0 to 500 m | 40 / 40 |
| `generation_water_clearance_m` | 0 to 50 m | 2 |
| `generation_block_gravel` | 0 or 1 | 0 |
| `generation_frequency` | 1 to 6 | 3 |
| `generation_size` | 1 to 3 | 2 |
| `generation_richness_min`, `_max` | 0.001 to 1 | 0.45 / 1.00 |
| `sand_surface_strength` | 0 to 1, otherwise the surface is off | 1.0 |
| `type` | 10 to 127 | required |
| `component` | 0 to 3 | required, except with auto |
| `editor` | at most 31 characters | empty |

---

## 📊 Frequency & size

### Frequency levels (`generation_frequency`)

| Value| Frequency          | Fields |
|------|--------------------|--------|
|  1   | extremely rare     |   1    |
|  2   | rare               |   2    |
|  3   | normal (default)   |   3    |
|  4   | frequent           |   4    |
|  5   | very frequent      |   5    |
|  6   | extremely frequent |   6    |

### Field sizes (`generation_size`)

|Value| Size             | Base radius   |
|-----|------------------|---------------|
| 1   | small            |   150–350 m   |
| 2   | medium (default) |   350–550 m   |
| 3   | large            |   550–750 m   |

**Note:** the size scales the whole field, not single points. Fields are elongated and branched, not circles; separate pieces count as one field. Along its main axis a field spans about 3.4 to 4.6 times the base radius, so large fields need room.

---

## 🌍 Natural generation

### How does it work?

1. **On load:** the plugin compares every deposit in the INI, by token, with the history of the saved game
2. **New deposits:** are placed once, at the first terrain draw after loading
3. **Existing ones:** stay as they are, even after complete depletion; nothing is refilled
4. **Shape:** elongated, branched fields with fading richness

### Rules

✅ **Allowed:**
- Stretched fields with irregular edges
- Occasional branches
- Real gaps and gradually falling richness
- Several separated pieces per field

❌ **Not allowed:**
- Placement in water
- Less distance to the shore than `generation_shore_m`
- Overlap with other deposits, including the game's oil, iron, coal, uranium and bauxite; the gravel/rock zone only with `generation_block_gravel = 1`
- Leaving the country border (without a border polygon the rectangular building limits apply)

A field counts only if at least 60 % of its area remains after clipping. If there is not enough room, fewer or no fields are created; the log states target, result and the reasons for rejection.

### First load of an old saved game

⚠️ **Important: adopt old saved games safely**

```
1. Back up the complete saved-game folder
2. Do NOT remove or reorder the existing deposits in the INI
   (old saves without a mapping list depend on the order)
3. Load the save → save under a new name
   → tesmio_deposits.bin is created
4. From the second load on only the token matters, the order no longer does
```

### Adding a new deposit

```
1. Save the game
2. Add a new section to deposits_plus.ini with its own token and type
   (in Republic Mod Manager: Deposits tab, plus button)
3. Restart the game completely (loading from the main menu is not enough)
4. Load the save → only the new deposit is placed
5. Save
```

---

## 🏞️ Sandy meadow (visual texture)

### What does it do?

Blends a sandy, patchy ground texture over sand deposits, more clearly on rich deposits. **Purely visual – no gameplay effect.** Saved game and maps are not changed.

- **Meadow:** sand patches on green meadow in summer, on brown meadow in autumn
- **Siberia:** own entries for summer and the snow-dusted autumn (`grass2snow.dds`)
- **Jungle:** summer with sand; autumn runs through the normal autumn meadow
- **Desert and winter:** always native - desert is sand already, snow covers everything
- **With mining:** the texture disappears with the resource

Requirement: the deposit named in `sand_surface_token` has its own channel from resourcemap3 upwards, for sand through `independent_map = 1`. Otherwise the log reports "no independent map; disabled".

### Setting

```ini
[deposits_plus]
; 1 = on, 0 = off
sand_surface = 1
; 0 to 1, 1.0 = full look
sand_surface_strength = 1.0
sand_surface_token = $TYPE_MINE_SAND
```

### Tile table (since 0.4.1)

One section `[sand_tile:<name>]` per ground texture: `base` is the texture the map's material.mtl names on slot 5, folder included; `color` and `normal` are your DDS files under `deposits_plus\assets`, which the DLL looks for beside itself first (package: `hooks\deposits_plus\assets`) and then under `plugins\deposits_plus\assets`. Since 0.4.2 the files may sit in set folders (`Siberia/sand_meadow_siberia_color.dds`); `..` and absolute paths are refused. In Republic Mod Manager the table lives on the Sand tiles tab: the file fields list every DDS file grouped by set folder, and before saving RMM checks that the file exists and has the right format.

```ini
[sand_tile:meadow]
base   = tiles_normal/grass2.dds
color  = Vanilla/sand_meadow_color.dds
normal = Vanilla/sand_meadow_normal.dds

[sand_tile:siberia_autumn]
base   = dlc2/tiles_siberia/grass2snow.dds
color  = Siberia/sand_meadow_autumn_siberia_color.dds
normal = Siberia/sand_meadow_autumn_siberia_normal.dds
```

Shipped sets: `Vanilla` (meadow summer/autumn), `Siberia` (summer and snow-dusted autumn), `Asia - Jungle` (summer) and `Ultimate Vanilla +` (a meadow pair matching that texture pack, not the default: point the meadow entries at its files when you use the pack). A ground texture without an entry stays native, and so does an entry whose files are missing (log line `sand surface WARN tile`). Known ground textures: `tiles_normal/grass2.dds`, `tiles_normal/grass2_fall.dds`, `dlc2/tiles_siberia/grass2.dds`, `dlc2/tiles_siberia/grass2snow.dds`, `dlc2/tiles_asia/jungle_swamp_dm.dds`.

### Files

```
Vanilla\sand_meadow_color.dds                          (meadow summer colour)
Vanilla\sand_meadow_normal.dds                         (meadow summer normal map)
Vanilla\sand_meadow_autumn_color.dds                   (meadow autumn colour)
Vanilla\sand_meadow_autumn_normal.dds                  (meadow autumn normal map)
Siberia\sand_meadow_siberia_color.dds                  (Siberia summer)
Siberia\sand_meadow_siberia_normal.dds
Siberia\sand_meadow_autumn_siberia_color.dds           (Siberia snow-dusted autumn)
Siberia\sand_meadow_autumn_siberia_normal.dds
Asia - Jungle\sand_meadow_jungle_color.dds             (jungle summer)
Asia - Jungle\sand_meadow_jungle_normal.dds
Ultimate Vanilla +\sand_meadow_color_ultimatevanilla.dds          (meadow summer for Ultimate Vanilla+)
Ultimate Vanilla +\sand_meadow_normal_ultimatevanilla.dds
Ultimate Vanilla +\sand_meadow_autumn_color_ultimatevanilla.dds   (meadow autumn for Ultimate Vanilla+)
Ultimate Vanilla +\sand_meadow_autumn_normal_ultimatevanilla.dds
```

**Format:** square, power of two from 256 to 4096 (1024 or 2048 recommended), complete mipmap chain
- **Colour:** BC1/DXT1
- **Normal:** BC3/DXT5

### In-game test

```
1. Restart the game completely
2. Look at a sand deposit on green meadow → patches visible?
3. Autumn → brown meadow with sand patches?
4. Snow → snow cover overlays everything?
5. Reload from the main menu → same look?
```

Expected in `tesmioloader.log`:
```
sand surface assets from <folder>
sand surface shader preparation: 13/13 verified native programs augmented
```

---

## 🚜 Working vehicles

### What does it do?

A deposit can borrow the vehicle skill of gravel mining. Example: sand with the **excavators that also mine gravel**.

### Setting

```ini
[sand]
token                 = $TYPE_MINE_SAND
working_vehicle_skill = gravelmining
building_type         = 7
```

**Supported values:**
- `none` (default, no vehicle assignment)
- `gravelmining` (the skill of gravel mining)

Any other value produces a warning in the log and acts like `none`. Case does not matter.

**Requirement**
- `building_type = 7` (mine). Otherwise the vehicle assignment is disabled with a warning; the deposit itself is kept.

### How does it work?

- The mine's `building.ini` keeps its token, for example `$TYPE_MINE_SAND`
- Vehicles use their existing skill `$SKILL_GRAVELMINING`
- **No new vehicle type** – existing excavators are simply assigned
- Needs `$WORKING_VEHICLES_NEEDED` and parking slots in the `building.ini` as always
- Resources, production, fuel: everything as usual

### Gameplay test

```
1. Open a sand mine with working-vehicle slots
2. Buy and assign an excavator
3. Without workers → does the excavator mine sand?
4. Fuel empty → does it stop correctly?
5. Old gravel and bauxite mines → no change
```

---

## 🔗 Relation to the original plugin

### Coexistence with `deposits.dll`

Deposits Plus and the original plugin `deposits` share:
- **the same registry service:** `deposits`
- **the same save file:** `tesmio_deposits.bin`
- **compatibility:** existing saved games and type numbers stay valid as long as type numbers and channels are not changed

### If both are installed

**Deposits Plus loads only while the original is switched off.**

If `plugins\deposits.dll` exists and is enabled in tesmioloader.ini:
- Deposits Plus stays idle before it patches anything
- In the log: `deposits_plus  idle - plugins\deposits.dll is present and enabled`
- The original loads as usual

**Solution:** switch the original off in TesmioLauncher or in Republic Mod Manager. Deleting is not necessary.

---

## 💾 Compatibility

### Save format

- **The DDS maps of the saved game** hold the mining state as always; the maps resourcemap3 and higher created by the plugin are saved with the terrain
- **`tesmio_deposits.bin`** additionally stores:
  - deposit identities (tokens)
  - channel assignments
  - the generation seed (same seed, same layout)
  - the list of removed deposits

### Backing up saves

When copying or backing up, **always take the complete saved-game folder**:
```
MyGame\
├── dds\                    (terrain and maps)
├── buildings.bin
├── ...
└── tesmio_deposits.bin     (← do not forget)
```

### Version compatibility

- **Within the 0.4 series:** updates carry save games and `tesmio_deposits.bin` over automatically; type numbers, map slots and tokens of running save games stay off limits (see above)
- **With Deposit Depletion:** keeps working as before; the key `deplete` is passed on to the depletion plugin
- **With Soviet Mod Loader:** SML brings deposits of its own. While it runs, this plugin pauses and says so in the log; if you want Deposits Plus instead, switch `soviet_mod_loader` off in `tesmioloader.ini` (in Republic Mod Manager the "Plugin active" switch)
- **0.4.10:** safety round, no new feature: the extra maps are hooked only once the type patch is in place; the terrain editor overlay and the brush restore their state after a fault and switch themselves off; the extra map table is read under the lock; a blank DDS file that could not be written completely is reported with the error code and removed; `building_type` must be 7 or 92 and `minimap` 0 or 1, otherwise WARN and default; the game EXE headers are checked before they are read. In RMM: at most 32 deposits and 16 sand tiles (the plugin's limits), the card "Natural generation" now sits on the General tab
- **0.4.9:** the guard against Soviet Mod Loader also works when Deposits Plus loads before SML (local copy in `plugins`, alphabetical order): it is enough that `plugins\soviet_mod_loader.dll` exists and is switched on in `tesmioloader.ini`.
- **0.4.8:** the plugin steps aside on its own under Soviet Mod Loader
- **0.4.7:** the brush name may now be up to 31 characters. Existing names and the two PNG files stay as they are

---

## ⚙️ Troubleshooting

### Common problems

| Problem                    | Cause                                                      | Solution                                                                        |
|----------------------------|------------------------------------------------------------|---------------------------------------------------------------------------------|
| Deposits Plus does nothing | original deposits enabled                                  | check the log for `deposits_plus  idle`, switch the original off                |
| Deposits not generated     | `generation = 0` or an invalid value                       | check the log for `generation WARN`, bring the value into range                 |
| Textures not visible       | assets folder missing or no independent channel            | `deposits_plus\assets` beside the DLL or under `plugins`; `independent_map = 1` |
| Excavators do not work     | `working_vehicle_skill` wrong or `building_type` not 7     | check the log for `vehicles WARN`                                               |
| Old deposits gone          | INI heavily rebuilt                                        | restore the saved game, adopt it again                                          |

### Logging

Every message is in `tesmioloader.log`.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Search for:
- `generation` → generation
- `vehicles` → working vehicles
- `sand surface` → ground texture
- `deposits_plus` → general messages, rejected deposits

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
deposits_plus\
├── hooks\
│   ├── deposits_plus.dll           (plugin)
│   ├── deposits_plus.ini           (original INI)
│   └── deposits_plus\assets\       (sandy meadow textures)
│       ├── Vanilla\                (sand_meadow_*.dds)
│       ├── Siberia\                (sand_meadow_siberia_*.dds, sand_meadow_autumn_siberia_*.dds)
│       ├── Asia - Jungle\          (sand_meadow_jungle_*.dds)
│       └── Ultimate Vanilla +\     (sand_meadow_*_ultimatevanilla.dds)
├── config\                         (editor schema for Republic Mod Manager)
│   ├── deposits_plus.launcher.ini
│   └── languages\
│       ├── de.ini
│       └── en.ini
├── soviet.mod.ini                  (manifest for SML, Bridge and Republic Mod Manager)
├── workshopconfig.ini              (Steam Workshop entry)
├── previewimage.png
├── README_DE.md
├── README_EN.md
└── THIRD-PARTY-LICENSE.txt         (licence of the included shader-compiler code)
```

**Loader folder** (method 1 by hand or "Files local only")
```
tesmioloader\build\
├── plugins\
│   ├── deposits_plus.dll
│   ├── deposits_plus.ini           (effective INI)
│   └── deposits_plus\assets\
│       ├── Vanilla\
│       ├── Siberia\
│       ├── Asia - Jungle\
│       └── Ultimate Vanilla +\
└── user_config\
    └── deposits_plus.editor.ini    (personal values from Republic Mod Manager)
```

Through the Bridge or SML only the effective INI lies in `plugins`; DLL and textures stay in the package.

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. Deposits Plus is a fork of the `deposits` plugin from the TesmioLoader by MaxLegend (Tesmio), GPL v3, https://github.com/MaxLegend/TesmioLoader; service name and savegame file deliberately stay identical to the original. **Code from the DirectX Shader Compiler** (DXIL signature of the adjusted shaders) – see `THIRD-PARTY-LICENSE.txt`. The complete source of Deposits Plus lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/deposits_plus.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Can I use both plugins at the same time?**
A: No. Either the original OR Deposits Plus. While the original is enabled, Deposits Plus stays idle.

**Q: Do I lose my saved games?**
A: No. Old saves keep working. `tesmio_deposits.bin` is created on the first load.

**Q: Can I change the generation later?**
A: Yes, but only deposits not yet placed are generated. Existing fields stay.

**Q: Do I need all functions?**
A: No. The code patch for the deposit types is the minimum. Generation, sand texture and working vehicles are optional.

**Q: Does Deposit Depletion still work?**
A: Yes, fully compatible. Mining works as always.

**Q: Do I have to edit the INI by hand?**
A: No. Republic Mod Manager shows every setting with a description, checks the value ranges and proposes the next free type number for new deposits.

---

**Last update:** Deposits Plus 0.4.10  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
