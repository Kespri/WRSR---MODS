# 📦 Resources Plus 0.1.2

**Extension of the TesmioLoader plugin resources**

Your own resources for *Workers & Resources: Soviet Republic* 1.1.1.9: new goods with an icon, cargo models, a price and trade at the customs house, all from one INI.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Relation to the original plugin](#-relation-to-the-original-plugin)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)
- [Licence & credits](#-licence--credits)
- [FAQ](#-faq)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4

### In three steps
1. **Install:** through the Republic Mod Manager package (`Install-RMM.bat`). The installer switches Resources Plus on and an existing original resources plugin off, and says both in its output.
2. **Start the game once:** Resources Plus creates `plugins\resources_plus.ini` and takes over your existing `plugins\resources.ini` unchanged. Your resources are there right away.
3. **Keep editing:** in Republic Mod Manager under "Resources Plus", or directly in `plugins\resources_plus.ini`.

---

## ✨ Features

### 🎯 Original features (kept)
- ✅ New resources as a copy of a template (`= steel, Cable`) or fully your own (`= custom` plus `[custom:<name>]`)
- ✅ Icon, cargo models, transport class, price kind and market values per resource
- ✅ Prices from the production chain, `[base_price]` and `[price]` to override them
- ✅ Customs houses pick up new resources as trade slots, even ones built earlier
- ✅ Any number of entries; the plugin grows the game's resource table itself

### 🆕 Improvements (new in Resources Plus)

#### 1️⃣ **No more crash on exit**
The original crashed the game on exit as soon as more than six own resources were declared (Windows reported `ucrtbase.dll`, code `c0000409`). Saves were never affected, but every exit left a crash report behind. Resources Plus allocates the enlarged table the way the game releases it on exit.

#### 2️⃣ **Its own file, taken over automatically**
Resources Plus reads `plugins\resources_plus.ini`. If it is missing, it is created at the first start from what you already have: from the original plugin's `plugins\resources.ini`, in the same order, with a header line saying where it came from. The original is only read, never changed. If that `resources.ini` was written by Soviet Mod Loader (which rewrites it at every game start with all packages merged in), Resources Plus takes SML's baseline instead, so no package resource appears twice.

#### 3️⃣ **Duplicate names are refused**
If a name is listed twice in `[list]`, the first one counts and the second is skipped with a log line. The original would have registered it twice.

#### 4️⃣ **Steps aside by itself**
While the original is enabled or Soviet Mod Loader is running, Resources Plus stays idle and writes one line to the log. There are never two plugins registering the same resource.

---

## 💾 Installation

Resources Plus and the original plugin resources never run at the same time. The installer of the Republic Mod Manager package handles that for you; by hand it works the same way.

### Method 1️⃣: Republic Mod Manager (package, recommended)

```
1. Subscribe to the Republic Mod Manager package and run Install-RMM.bat
2. resources_plus.dll and the template resources_plus.template.ini land in
   tesmioloader\build\plugins\ (an existing resources_plus.ini is left untouched)
3. The installer sets in tesmioloader.ini: resources_plus=1 and, if
   plugins\resources.dll exists, resources=0 - and tells you so
4. Start the game: the list from plugins\resources.ini is taken over
```

**Back to the original:** switch Resources Plus off and resources on in Republic Mod Manager, or set `resources=1` and `resources_plus=0` in `tesmioloader.ini`. `Uninstall-RMM.bat` does the same. The original's DLL is never deleted or overwritten.

### Method 2️⃣: By hand

```
1. Copy resources_plus.dll → tesmioloader\build\plugins\
2. Enable resources_plus in TesmioLauncher (or resources_plus=1 in tesmioloader.ini)
3. Disable the original plugin resources (resources=0)
4. Start the game: plugins\resources_plus.ini is created from your resources.ini;
   without a resources.ini the plugin uses the template resources_plus.template.ini beside the DLL
```

### Method 3️⃣: Soviet Mod Loader (SML)

⚠️ **While SML runs, Resources Plus stays idle.** SML carries its own copy of the original plugin and ends the game if a second plugin registers resources. If you want Resources Plus, switch `soviet_mod_loader` off in `tesmioloader.ini` (the "Plugin active" switch in Republic Mod Manager) and use method 1 or 2. At the next start Resources Plus takes your list over from SML's baseline; Republic Mod Manager provides the packages afterwards.

---

## 🧰 Republic Mod Manager

- The entry **Resources Plus** edits `plugins\resources_plus.ini`: list, template, display name, transport class, price kind, market values. If the file does not exist yet, RMM creates it when the page opens, by the same rule the plugin uses, and says so in a blue box. The old entry "Resources" stays in the list as a switched-off plugin, the way "Deposits" sits next to Deposits Plus.
- **Content packages** from the Workshop (`[content] resources`) go into `plugins\resources_plus.ini` once it exists, and can be taken back.
- The **"Plugin active"** switch for Resources Plus writes the entry in `tesmioloader.ini`.
- While SML runs, Resources Plus shows **amber**: paused, not broken. The page is locked until you choose "Edit anyway".
- The log window shows `logs\tesmioloader.resources_plus.log`.

---

## ⚙️ Configuration

### Main file: `plugins\resources_plus.ini`

| Section | Content |
|---|---|
| `[list]` | One line per resource: `<name> = <template or custom>[, <display name>]` |
| `[custom:<name>]` | Fine-tuning: `transport`, `kind`, `cargo`, `price`, `market_rub`, `market_usd` and more |
| `[base_price]` / `[price]` | Override prices before or after the game's price pass |
| `[resources]` | How the plugin reaches the game: `hook`, `resource_capacity`, `price_hook`, `price_report`, `custom_report` |
| `[customs]` | Customs house hook: `hook`, `probe` |

The format is the original plugin's; every key is explained in the file itself. The template decides transport class and cargo shape, so: `open` goods via steel, silo goods via alumina, bulk goods via rawgravel or gravel, food via food.

### Good to know
- **Saves depend on `[list]`.** A save with two own resources does not load without them, and a save without own resources does not load with them. Change the list only with a matching save or a new game. The takeover from `resources.ini` keeps the order, so your saves keep loading.
- Icon: `media_soviet\resources\<name>.png` (48×48, RGBA), best placed under `tesmioloader\vfs\`.
- Cargo models: `resources\<name>.nmf` for open goods, `<name>1..4.nmf` plus `<name>_vehicle.nmf` for bulk goods.

---

## 🔗 Relation to the original plugin

### Coexistence with `resources.dll`

Resources Plus and the original plugin `resources` share:
- **the same service name:** `resources`
- **the same file format**, in separate files: `plugins\resources_plus.ini` and `plugins\resources.ini`
- **the same text ids** for the display names (from 1000000)
- **compatibility:** saves stay valid as long as `[list]` has the same entries in the same order

### If both are installed

**Resources Plus only loads when the original is switched off.**

If `plugins\resources.dll` exists and is enabled in tesmioloader.ini:
- Resources Plus stays idle before it patches anything
- In the log: `resources_plus  idle - plugins\resources.dll is present and enabled`
- The original loads normally

The same applies under Soviet Mod Loader: `resources_plus  idle - soviet_mod_loader.dll brings its own resources`.

**Solution:** switch the original or SML off in TesmioLauncher or Republic Mod Manager. Deleting is not necessary.

---

## 💾 Compatibility

### Save format
Unchanged from the original: the number of resources is part of the save, the display names get the same text ids. A save made with the original or under SML loads with Resources Plus as long as `[list]` has the same entries in the same order. That is exactly what the automatic takeover guarantees.

### Version compatibility
- 0.1.2: safety round, no new feature: the customs house sync runs under the same lock as the move of the resource table and checks the game's push function for its bytes before the first call; transport classes beyond the record's 18 are no longer read; registering resources and the price bracket switch themselves off after a fault instead of repeating it; `packed = auto` does what the comment says again; a UTF-8 BOM in resources_plus.ini no longer hides `[list]`; `resource_capacity`, `resource_vector_rva`, `packed` and a numeric `family` are checked and refused with a WARN line; write errors name the Windows error code
- 0.1.1: the guard against Soviet Mod Loader also works when Resources Plus loads before SML (alphabetical order in `plugins`): `pluginssoviet_mod_loader.dll` present and switched on is enough. Without this fix SML blocked the game start with "resources registration hook was not installed".
- 0.1.0: first version as a fork of resources 1.7. Same features, plus the exit fix, its own file with automatic takeover, the duplicate-name check and the guard against the original and SML.

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| No own resources in the game | Original or SML enabled | Read the log: `resources_plus  idle …`, then switch the other one off |
| "Save needs mods" when loading | `[list]` does not match the save | Declare the same entries in the same order |
| A resource is missing, the log says "listed twice" | Name twice in `[list]` | Remove the second line |
| Storage shows 0.00 of 0.00 t | Template's transport class does not match the building's `$STORAGE` | Pick another template or set `transport =` in `[custom:<name>]` |
| Resource not tradeable at the customs house | Template `custom` without market values | Use a template with market values (e.g. food) or set `market_rub`/`market_usd` |
| No icon | PNG missing | `media_soviet\resources\<name>.png`, 48×48 RGBA |

### Logging
- Summary in `tesmioloader.log`: `plugin resources_plus 0.1.2`, at the first start `created plugins\resources_plus.ini from …`, then `published as index …` per resource
- Details in `logs\tesmioloader.resources_plus.log` (records, prices)

---

## 📦 File structure

```
tesmioloader\build\
├── plugins\
│   ├── resources_plus.dll
│   ├── resources_plus.ini              (your list; taken over from resources.ini when the RMM page first opens or at the first game start)
│   ├── resources_plus.template.ini     (template from the package, used only without a resources.ini)
│   └── resources.ini                   (original plugin, left untouched)
└── logs\
    └── tesmioloader.resources_plus.log
```

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. Resources Plus is a fork of the `resources` plugin from the TesmioLoader by MaxLegend (Tesmio), GPL v3, https://github.com/MaxLegend/TesmioLoader; service name, text ids and file format deliberately stay identical to the original. The complete source of Resources Plus lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/resources_plus.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Can I use both plugins at the same time?**
A: No. Either the original OR Resources Plus. While the original is enabled, Resources Plus stays idle.

**Q: Do I have to carry my resources.ini over by hand?**
A: No. At the first start Resources Plus copies it to `resources_plus.ini`, in the same order. From then on `resources_plus.ini` is the file that counts.

**Q: What about Soviet Mod Loader?**
A: SML ships the original built in. Under SML, Resources Plus pauses and the exit fix does not apply. If you want it, switch SML off and load your packages through the Workshop Bridge.

**Q: Why did the installer switch my resources off?**
A: So that the fixed version runs and not both at once. It says so in its output, deletes nothing, and one switch brings the original back.
