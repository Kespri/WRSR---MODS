# 🔬 Research Expansion 0.4.2

**TesmioLoader plugin for new research and changes to the research tree**

Adds your own research entries to *Workers & Resources: Soviet Republic* 1.1.1.9, hangs them onto existing research, positions their unlocks and edits Vanilla research where needed, without touching the original `research.ini`. The extended file is generated at every game start in the TesmioLoader virtual file system (VFS).

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [New research](#-new-research)
- [Editing Vanilla research](#-editing-vanilla-research)
- [Icons and texts](#-icons-and-texts)
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
- **Mandatory:** the Localization plugin (localization.dll), enabled. Without its service the extension is not applied.
- For new research: a Localization text pack with the names and descriptions (namespace `research_expansion`); it ships with the Localization package.

### In three steps
1. **Choose one installation method** (see below) and enable the plugin; Localization must be running.
2. **Enter research:** write new research blocks in `research_expansion.ini` (examples are inside, commented out), put icons as 128 x 128 PNG named `<research_id>.png` into the folder `tesmioloader\vfs\media_soviet\research` (Republic Mod Manager shows and opens it on the Research icons card). Edits of Vanilla research are most convenient in Republic Mod Manager.
3. **Restart the game completely.** The generated `research.ini` then sits under `tesmioloader\vfs\media_soviet\research`.

---

## ✨ Features

### 🎯 Core function
- ✅ New research blocks with type, cost, unlocks, name and description in one INI
- ✅ Dependencies on Vanilla or new research through `+` lines; the reverse `$UNLOCK_RESEARCH` lines are created automatically and can be positioned with `@before_` and `@after_`
- ✅ Scoped edits of existing research: replace, remove, add, insert or move lines
- ✅ Own icons per research with a fallback icon
- ✅ Every error rejects the whole extension and leaves Vanilla research active; the log names file, rule and line
- ✅ Original files stay untouched; the generated file lives in the loader's VFS

### 🆕 New in 0.4.2
- Log wording only: the startup line now names the version of the running plugin instead of a fixed number.

### 🆕 New in 0.4.1
- Log wording only: the message about the unused keys `icon_folder` and `noimage_name` is clearer.

### 🆕 New in 0.4.0
- ✅ Ready for Republic Mod Manager: tabs New research, Localization and Vanilla edits with "Choose research…", "Choose line…", the "Original in the game" box and the check for unknown research before saving.
- ✅ **New research as an INI section:** `[research:<id>]` with the keys `type`, `cost`, `name`, `desc`, `requires`, `unlock` and `line` is the same research as a `$RESEARCH` block, only as a section so Republic Mod Manager can edit it. Both forms may be mixed and pass the same checks; sections are placed after the free blocks.
- ✅ **Short text keys:** in `[research:<id>]` `name = quartz_smasher` or nothing at all is enough; the plugin turns it into `research_expansion.quartz_smasher.name` and `.desc`. A key with dots is used as it is.
- ✅ **Change the cost without a replace command:** in `[modify:<id>]` `cost = 1800` is enough; the plugin replaces the Vanilla block's `$COST` line itself. In Republic Mod Manager you pick the research via "Choose research…", see the original block and build every command line via "Choose line…" from the real lines.
- ✅ **One icon folder:** icons live in `tesmioloader\vfs\media_soviet\research`, the folder the game reads them from. An icon that is there stays untouched; a missing one is created at start from `noimage.png` (from `research_expansion\icons` beside the DLL, else from `plugins\research_expansion\noimage.png`).
- ✅ **INI beside the DLL:** when `plugins\research_expansion.ini` does not exist, the DLL reads the INI from its own folder, i.e. from the Workshop package under Soviet Mod Loader or the Workshop Bridge. The chosen path is logged.

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice. Localization must be installed and enabled in every case.

---

### Method 1️⃣: Classic TesmioLoader

```
1. Copy research_expansion.dll and research_expansion.ini from hooks\
   → tesmioloader\build\plugins\

2. Copy the folder hooks\research_expansion (the icons)
   → tesmioloader\build\plugins\research_expansion\

3. Enable research_expansion and localization in TesmioLauncher
4. Restart the game completely
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL through soviet.mod.ini from the package, INI and icons sit beside it
3. Remove or disable a local research_expansion.dll in plugins\ first
```

---

### Method 3️⃣: Workshop Bridge (without SML)

```
1. Select the package in Republic Mod Manager
2. Switch "Plugin active" on
3. workshop_bridge loads the DLL straight from the package; it finds INI and icons beside itself
4. Steam updates apply immediately
```

---

### Method 4️⃣: Republic Mod Manager with "Files local only"

```
1. Select the package in Republic Mod Manager, adjust the settings, Save
2. "General" tab, "Notes" card: switch "Files local only" on
3. Confirmation with file list → DLL, INI and the icon folder are copied to
   tesmioloader\build\plugins\ on save
4. The Workshop Bridge skips the package from then on
```

**"Files local only" in detail**
- For everyone who wants to keep using the plugin without the Steam subscription
- With local files, Steam updates apply only after saving again (yellow "Update" badge)
- Switching it off removes only the files Republic Mod Manager copied

---

## 🧰 Republic Mod Manager

The package ships an editor schema in the `config` folder. Republic Mod Manager shows Research Expansion in two tabs, German and English:

- **General:** notes, "Files local only", the button for this guide and the plugin settings (apply extension, research icons, diagnostic log)
- **Vanilla edits:** the edited research on the left, the selected one on the right with its switch and one multi-line field each for replace, remove, add, insert and move; the plus button adds an edit for a Vanilla research id

New research blocks (`$RESEARCH … $RESEARCH_ADD`) are free text blocks and are still written in the INI; Republic Mod Manager leaves them untouched. Personal changes live in `user_config\research_expansion.editor.ini`, the effective file is `plugins\research_expansion.ini`; the INI in the package stays untouched.

---

## ⚙️ Configuration

### Main file: `research_expansion.ini`

The DLL reads in this order:
- **First:** `tesmioloader\build\plugins\research_expansion.ini` if it exists (classic installation, "Files local only", or the effective INI written by Republic Mod Manager)
- **Otherwise:** the INI beside the DLL, in the package `hooks\research_expansion.ini` (Soviet Mod Loader, Workshop Bridge)

⚠️ **Comments only on their own lines with `;`.** UTF-8 without BOM, at most 8 MiB. Directive names are case-sensitive. `[general]` allows only `enabled` and `debug` (`icon_folder` and `noimage_name` are ignored). Changes take effect after a complete restart.

### Section `[general]`

```ini
[general]
; 1 generates the extension, 0 removes the generated research.ini from the VFS on the next start
enabled = 1
; 1 logs staged operations, placements and local paths
debug = 0
```

⚠️ **Before disabling the DLL** set `enabled = 0` and start the game once: only then does the plugin remove the generated `research.ini` from the VFS. If the DLL is disabled straight in the launcher, the file stays and the game keeps reading the extended research.

---

## 🧪 New research

A block starts with `$RESEARCH <id>` and ends with `$RESEARCH_ADD`; it stands freely in the INI, not inside a `[section]`:

```ini
$RESEARCH quartz_smasher
+faculty_geology
@before_uranium_study
---------------------------------------
$TYPE_TECHNICAL
$COST 1800
$UNLOCK_BUILDING_PRODUCTION raw_quartz
$NAME research_expansion.quartz_smasher.name
$DESC research_expansion.quartz_smasher.desc
$RESEARCH_ADD
```

| Line | Meaning |
|---|---|
| `$RESEARCH <id>` | Unique id of letters, digits and `_`; also names the icon `<id>.png` |
| `+<research>` | Prerequisite; several `+` lines form an AND condition. The parent automatically gets `$UNLOCK_RESEARCH <id>` |
| `@before_<x>` / `@after_<x>` | Position of that unlock in the parent, relative to its line `$UNLOCK_RESEARCH <x>`; applies to the last `+` line, never copied into the game |
| `$TYPE_TECHNICAL`, `$TYPE_SOVIET`, `$TYPE_MEDICAL` | exactly one |
| `$COST <number>` | 1 to 2,147,483,647 |
| `$NAME`, `$DESC` | language keys `namespace.key` from the Localization text pack; the resolved ids must lie between 2,000,000 and 2,999,999 |
| `$UNLOCK_…`, `$YEAR`, `$LOCK_AFTER_DAYS`, `$AVAILABLE`, `$IGNORE_…` | optional game directives as in the original research.ini |

Rules: unknown directives, missing required fields, duplicate directives, missing parents, self-references and cycles are rejected. `$AVAILABLE` cannot be combined with `+` lines. A line of dashes only is an allowed separator.

### As an INI section

The same research as a section, the way Republic Mod Manager writes it:

```ini
[research:quartz_smasher]
requires = faculty_geology | before | uranium_study
type = technical
cost = 1800
unlock = $UNLOCK_BUILDING_PRODUCTION raw_quartz
```

| Key | Meaning |
|---|---|
| `enabled` | `0` keeps the section but applies nothing; a missing key means `1` |
| `type` | `technical`, `soviet` or `medical` |
| `cost` | positive whole number |
| `name`, `desc` | optional: a word without a dot becomes `research_expansion.<word>.name` or `.desc`, a missing key means the research id; a key with dots is used literally |
| `requires` | one line per parent: `<research>`, optionally `\| before` or `\| after` and `\| <anchor>`; equals a `+` line plus `@before_`/`@after_` |
| `unlock` | one `$UNLOCK_…` line per entry |
| `line` | any other directive line, copied as it is |

Free blocks and sections may be mixed; an id may occur once. Sections are placed after the free blocks.

---

## ✏️ Editing Vanilla research

One section `[modify:<id>]` per existing research, in Republic Mod Manager one entry on the Vanilla edits tab:

```ini
[modify:faculty_geology]
enabled = 1
replace = $COST 1500 | $COST 1800
move_before = $UNLOCK_RESEARCH uranium_study | $UNLOCK_RESEARCH bauxite_study
```

| Command | Meaning |
|---|---|
| `remove = LINE` | remove exactly one matching complete line |
| `replace = OLD \| NEW` | replace exactly one line |
| `add = LINE` | insert immediately before `$RESEARCH_ADD` |
| `insert_before = ANCHOR \| LINE` / `insert_after = …` | insert before or after a unique anchor line |
| `move_before = LINE \| ANCHOR` / `move_after = …` | move an existing line unchanged |
| `cost = POINTS` | set the cost: replaces the block's `$COST` line |

The commands run in file order, after the automatic unlocks of new blocks; later ones see the changes of earlier ones. `$RESEARCH` and `$RESEARCH_ADD` themselves must not be touched. A `+` line added by a command is literal, without an automatic reverse unlock. `enabled = 0` skips the section, its syntax must still be valid. Matching needs the complete line including case; `|` is reserved as the separator.

---

## 🖼️ Icons and texts

**Icons:** PNG, exactly 128 x 128 pixels, at most 4 MiB, file name `<research_id>.png` in the folder `tesmioloader\vfs\media_soviet\research`. The plugin creates the folder on its first start. An icon that is there stays; a missing one is created from `noimage.png` and a warning is logged (source: `research_expansion\icons` beside the DLL, else `plugins\research_expansion\noimage.png`). An unusable icon in the folder or a missing `noimage.png` rejects the extension.

**Texts:** `$NAME` and `$DESC` refer to a Localization text pack, for example `plugins\localization\research_expansion\sovietEnglish.ini`:

```ini
[strings]
quartz_smasher.name = Quartz Crusher
quartz_smasher.desc = Crushes quartz for glass production.\nRequires additional workers.
```

The text pack belongs to the Localization plugin and ships with its package. Pure Vanilla edits with existing numeric text ids below 2,000,000 need neither a text pack nor icons; the Localization service stays mandatory.

---

## 📏 Value ranges

| Quantity | Limit |
|---|---|
| `enabled`, `debug` | exactly 0 or 1 |
| `$COST` | 1 to 2,147,483,647 |
| resolved text ids | 2,000,000 to 2,999,999 |
| INI | at most 8 MiB |
| active research blocks | at most 256 |
| edit sections / commands | at most 256 / 4096 |
| line inside a command | at most 4096 bytes |
| `+` and `$UNLOCK_RESEARCH` lines of new blocks | at most 4096 together |
| icon | PNG, 128 x 128, at most 4 MiB |

---

## 💾 Compatibility

### Savegames
Research is part of the savegame: a new research a savegame already knows should keep its id. The Vanilla file stays unchanged; without the plugin the game reads its own `research.ini` again once the VFS copy is removed (see `enabled = 0`).

### Other plugins
Localization is mandatory. Other plugins that replace `research.ini` are not merged.

### Version compatibility
- **0.4.2:** log wording only; logic and INI unchanged
- **0.4.1:** log wording only; logic and INI unchanged
- **0.4.0:** first published version

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| Extension is not applied | Localization missing or off | install and enable localization.dll |
| `Localization key ... could not be resolved` | key missing in the text pack | check the text pack, write the key exactly |
| `No icon for ... and no usable noimage.png` | icon and fallback icon missing | put a 128 x 128 PNG named `<id>.png` into `vfs\media_soviet\research` or provide `noimage.png` |
| `Icon for ... is unusable` | PNG in the VFS folder broken or wrong size | replace or delete the file, it is then recreated from `noimage.png` |
| `Unknown directive` | typo or unsupported directive | compare the line with the original research.ini |
| `Research ... already exists` | duplicate id | choose a unique id |
| `Dependency cycle detected` | cycle in the `+` lines | untangle the dependencies |
| Research still visible after disabling | DLL disabled without `enabled = 0` first | enable the plugin, set `enabled = 0`, start the game once |

### Logging

All messages go to `tesmioloader.log` and the detail log `logs\tesmioloader.research_expansion.log`, with rule names such as `placement-target`, `modify-match`, `research-cycle`.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Look for:
- `Configuration file`, `Icon store` and `Fallback icon` → which INI, icon folder and fallback icon were chosen
- `research_expansion` → every message of the plugin, rejections with rule and line

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
research_expansion\
├── hooks\
│   ├── research_expansion.dll      (plugin)
│   ├── research_expansion.ini      (original INI, examples commented out)
│   └── research_expansion\icons\
│       └── noimage.png             (fallback icon)
├── config\                         (editor schema for Republic Mod Manager)
│   ├── research_expansion.launcher.ini
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
tesmioloader\
├── build\
│   ├── plugins\
│   │   ├── localization.dll        (mandatory, separate plugin)
│   │   ├── localization\research_expansion\   (text pack, from the Localization package)
│   │   ├── research_expansion.dll
│   │   ├── research_expansion.ini  (effective INI)
│   │   └── research_expansion\noimage.png (only without the Workshop, copied by hand)
│   └── user_config\
│       └── research_expansion.editor.ini (personal edits from Republic Mod Manager)
└── vfs\media_soviet\research\      (generated research.ini and the icons <id>.png)
```

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. The plugin contains no third-party code; the loader SDK header comes from the TesmioLoader by MaxLegend (GPL v3). The complete source lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/research_expansion.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Is my research.ini modified?**
A: No. The extended file is generated in the loader's VFS; the original stays untouched.

**Q: Why don't I see my new research?**
A: Usually Localization or a language key is missing, or an error rejected the whole extension. The detail log names the cause.

**Q: Can I edit Vanilla research only, without adding new ones?**
A: Yes. Edit sections work without new blocks; only the Localization service must be running.

**Q: How do I get rid of the extension again?**
A: Switch the plugin off, start the game once, then disable the DLL. That way the plugin cleans up the VFS.

**Q: Do I have to edit the INI by hand?**
A: For new research blocks, yes. Edits of Vanilla research and the plugin settings are offered by Republic Mod Manager as a list with descriptions.

---

**Last update:** Research Expansion 0.4.2  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
