# 🧰 Resources Button Fix 0.4.0

**TesmioLoader plugin for compact tool grids in the terrain editor**

Rearranges the tool buttons in two windows of the terrain editor of *Workers & Resources: Soviet Republic* 1.1.1.9: in the **Resources** window the paint buttons stand in blocks above their erase buttons, in the **Rocks/Gravel** window either side by side or in two rows. Extra tools from deposits.dll or Deposits Plus, such as sand, are included automatically.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Value ranges](#-value-ranges)
- [Live reload](#-live-reload)
- [Automatic scaling](#-automatic-scaling)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4
- Optional: deposits.dll or Deposits Plus – their additional resource and terrain tools are arranged in the right window as well.

### In three steps
1. **Choose one installation method** (see below) and enable the plugin.
2. **Start the game** and open the Resources or Rocks/Gravel window in the terrain editor.
3. **Adjust** if you like: columns, blocks, size and spacing live in `resources_button_fix.ini`, most conveniently through Republic Mod Manager. Most changes apply the next time the window is opened.

---

## ✨ Features

### 🎯 Core function
- ✅ Resources window: paint buttons in blocks above their matching erase buttons, up to 32 columns and 4 blocks
- ✅ Rocks/Gravel window: paint and erase side by side (P E P E) or as two rows (P P above E E)
- ✅ Automatic or fixed button size, freely adjustable spacing and offsets
- ✅ The window grows when needed; the red delete-all button and the Brush controls move down with it
- ✅ Tree tools and every other editor window stay untouched; no game file and no VFS file is written

### 🆕 New in 0.4.0
- ✅ Every Republic Mod Manager text in player style: notice box on General, "Troubleshooting" card with the detailed log, ON/OFF instead of 1/0.
- ✅ **INI beside the DLL:** when `plugins\resources_button_fix.ini` does not exist, the DLL reads the INI from its own folder, i.e. from the Workshop package under Soviet Mod Loader or the Workshop Bridge. The plugin runs straight from the Steam subscription without copying anything.
- ✅ The chosen configuration file is logged at start; live reload follows that file.

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice.

---

### Method 1️⃣: Classic TesmioLoader

```
1. Copy resources_button_fix.dll and resources_button_fix.ini from hooks\
   → tesmioloader\build\plugins\

2. Enable resources_button_fix in TesmioLauncher
3. Restart the game completely
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL through soviet.mod.ini from the package, the INI sits beside it
3. Remove or disable a local resources_button_fix.dll in plugins\ first
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
1. Select the package in Republic Mod Manager, adjust the settings, Save
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

The package ships a launcher schema in the `config` folder. Republic Mod Manager shows Resources Button Fix in three tabs, German and English:

- **General:** notes, "Files local only", a button for this guide and the log settings (detail log)
- **Resources window:** arrangement (columns, blocks, size, spacing) plus position, window and delete-all button
- **Rocks/Gravel window:** arrangement (P E P E or two rows, size, spacing) plus position and window

The "Plugin active" switch in the header also sets `enabled = 1` when switched on. Republic Mod Manager writes the effective file to `plugins\resources_button_fix.ini`; personal values are kept separately in `user_config\resources_button_fix.ini`, the INI in the package stays untouched. If you prefer editing the INI by hand, everything else is below.

---

## ⚙️ Configuration

### Main file: `resources_button_fix.ini`

The DLL reads in this order:
- **First:** `tesmioloader\build\plugins\resources_button_fix.ini` if it exists (classic installation, "Files local only", or the effective INI written by Republic Mod Manager)
- **Otherwise:** the INI beside the DLL, in the package `hooks\resources_button_fix.ini` (Soviet Mod Loader, Workshop Bridge)

⚠️ **The file is checked strictly.** UTF-8 without BOM; exactly one `[general]`, `[resources_window]` and `[rocks_window]` section; no unknown or repeated sections and keys; every key with a value; switches only `0` or `1`; numbers within the documented range; comments only on their own lines with `;` or `#`, never behind a value. A missing single key uses the compiled default; a missing required section or an otherwise invalid file prevents any change to the game at start. The file may be at most 16 MiB, a value at most 63 characters.

### Section `[general]`

```ini
[general]
; 1 enables the whole plugin, 0 disables it (restart required)
enabled = 1
; 1 writes layout diagnostics to the log, troubleshooting only
debug = 0
```

### Section `[resources_window]`

```ini
[resources_window]
; 1 rearranges the Resources window, 0 keeps the original layout
enabled = 1
; resource pairs beside each other per block (1 to 32)
columns_per_block = 6
; blocks below each other (1 to 4); capacity = columns x blocks
maximum_blocks = 2
; auto or a fixed share of the original size 0.25 to 1.00
button_scale = auto
; distance within the row and between paint and erase row, 1.00 = normal
horizontal_spacing = 1.00
vertical_spacing = 1.00
; extra distance between two blocks
block_gap = 0.35
; offset of the whole grid, positive = right or down
x_offset = 0.00
y_offset = 0.00
; 1 lets the window grow, 0 shrinks the buttons to fit
expand_window = 1
; below_grid or original
cancel_position = below_grid
; visible gaps around the red delete-all button
grid_to_cancel_gap = 10.00
cancel_to_brush_gap = 40.00
; fine adjustment of the red button
cancel_x_offset = 0.00
cancel_y_offset = 0.00
```

The two visible gaps are arranged like this:

```
[resource buttons]
       |  grid_to_cancel_gap
   [red X button]
       |  cancel_to_brush_gap
     Brush
```

### Section `[rocks_window]`

```ini
[rocks_window]
; 1 rearranges the Rocks/Gravel window, 0 keeps the original layout
enabled = 1
; 1 = P E P E in one row, 2 = paint row above erase row
blocks = 2
; auto or a fixed share of the original size 0.25 to 1.00
button_scale = auto
horizontal_spacing = 1.00
vertical_spacing = 1.00
; offset of the whole grid
x_offset = 0.00
y_offset = 0.00
; 1 lets the window grow and moves the Brush controls down
expand_window = 1
; visible gap between grid and Brush with a growing window
controls_gap = 20.00
```

The `blocks` setting produces these arrangements (`P` = paint, `E` = erase):

```
blocks = 1          blocks = 2
P E  P E            P P
                    E E
```

---

## 📏 Value ranges

The DLL checks these limits on every read. A value outside rejects the whole file: at start the game stays unchanged, on live reload the previous values stay active.

| Key | Range | Default |
|---|---|---|
| `enabled`, `debug`, `expand_window` | exactly 0 or 1 | 1 / 0 / 1 |
| `columns_per_block` | 1 to 32 | 6 |
| `maximum_blocks` | 1 to 4 | 2 |
| `blocks` | 1 or 2 | 2 |
| `button_scale` | `auto` or 0.25 to 1.00 | auto |
| `horizontal_spacing`, `vertical_spacing` | 0.50 to 2.00 | 1.00 |
| `block_gap` | 0.00 to 2.00 | 0.35 |
| `x_offset`, `y_offset`, `cancel_x_offset`, `cancel_y_offset` | -100.00 to 100.00 | 0.00 |
| `cancel_position` | `below_grid` or `original` | below_grid |
| `grid_to_cancel_gap`, `cancel_to_brush_gap`, `controls_gap` | 0.00 to 100.00 | 10.00 / 40.00 / 20.00 |
| File | at most 16 MiB, values at most 63 characters | |

---

## 🔄 Live reload

Almost every setting applies without a restart:

```
1. Save the INI (or Save in Republic Mod Manager)
2. Open the Resources or Rocks/Gravel window again
3. The changed file is read at the first recognised tool button
```

The file is checked as a complete new configuration set. If only one line is invalid, **no** new values are applied; all last valid settings stay active and the reason is logged (`reload-rejected`). Only `[general] enabled` needs a complete restart; a change while the game runs is logged but not applied.

---

## 📐 Automatic scaling

`button_scale = auto` computes a suitable size from the detected number of tools, the columns and the spacing. The safe lower limit is `0.25`; if the plugin has to use it, a warning is logged. The tool count comes from the previous complete window pass; newly added tools are taken into account stably from the following pass. Surplus tools beyond the capacity `columns_per_block x maximum_blocks` keep their original position and produce a single warning.

---

## 💾 Compatibility

### Game and loader
- Fixed code positions verified for 1.1.1.9; another game version is refused before any hook is installed
- Before every change the PE signature, 64-bit machine type, time stamp, image size, the bytes of the shared button hook and the reach of every jump are checked
- Calls from trees and other windows are passed through unchanged; a runtime fault safely disables the layout change for the session
- Without near memory for the optional transitions the arrangement stays active, only the following controls then keep their position

### Savegames
The plugin changes neither game files nor savegames; it only arranges buttons in the memory of the running game.

### Version compatibility
- **0.4.0:** first published version

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| Plugin does nothing | `enabled = 0`, game version not 1.1.1.9 or invalid INI | check the log for `unsupported-build` or `configuration` |
| Change not visible | window not reopened or live reload rejected | close and open the window; check the log for `reload-rejected` |
| Buttons too small | automatic lower limit 0.25 reached | fewer columns, more blocks or `expand_window = 1` |
| Tools missing from the grid | capacity exceeded | raise `columns_per_block` or `maximum_blocks` |
| Red button in the wrong place | `cancel_position` or gaps | `below_grid` with `expand_window = 1`, adjust the gaps |

### Logging

All messages go to `tesmioloader.log` and the detail log `logs\tesmioloader.resources_button_fix.log`. Every message names level, source and rule; initialisation and start end with a summary.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Look for:
- `Configuration file` → which INI the DLL chose
- `configuration`, `unknown-key`, `config-range`, `duplicate-key` → rejected INI
- `reload-rejected` → live change discarded, previous values active
- `hook-install`, `near-memory`, `layout-fault` → hook or layout logic

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
resources_button_fix\
├── hooks\
│   ├── resources_button_fix.dll    (plugin)
│   └── resources_button_fix.ini    (original INI)
├── config\                         (launcher schema for Republic Mod Manager)
│   ├── resources_button_fix.launcher.ini
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
│   ├── resources_button_fix.dll
│   └── resources_button_fix.ini    (effective INI)
└── user_config\
    └── resources_button_fix.ini    (personal values from Republic Mod Manager)
```

Through the Bridge or SML only the effective INI sits in `plugins`, the DLL stays in the package. Without Republic Mod Manager the DLL reads the INI straight from the package.

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. The plugin contains no third-party code; the loader SDK header comes from the TesmioLoader by MaxLegend (GPL v3). The complete source lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/resources_button_fix.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Are the tree tools rearranged as well?**
A: No. Only the Resources and Rocks/Gravel windows; every other editor window stays unchanged.

**Q: Do I have to restart the game after every change?**
A: No. Only the plugin switch needs a restart; everything else applies the next time the window is opened.

**Q: What happens on an error in the INI?**
A: At start the game stays unchanged, on live reload the previous values stay active. The reason is in the log.

**Q: Does the plugin work with Deposits Plus?**
A: Yes. Extra tools from deposits.dll or Deposits Plus are arranged in the right window as well.

**Q: Do I have to edit the INI by hand?**
A: No. Republic Mod Manager shows every setting with a description and checks the value ranges.

---

**Last update:** Resources Button Fix 0.4.0  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
