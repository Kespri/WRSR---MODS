# 📦 Needs Plus 0.1.1

**Extension of the TesmioLoader plugin needs**

Your own citizen needs for *Workers & Resources: Soviet Republic* 1.1.1.9: your citizens want one more good, the shops stock it, all from one INI.

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
- A resource list that holds the wanted good (Resources Plus or the original resources plugin)

### In three steps
1. **Install:** through the Republic Mod Manager package (`Install-RMM.bat`). The installer switches Needs Plus on and an existing original needs plugin off, and says both in its output.
2. **Start the game once:** Needs Plus creates `plugins\needs_plus.ini` and takes over your existing `plugins\needs.ini` unchanged. Your needs are there right away.
3. **Keep editing:** in Republic Mod Manager under "Needs Plus", or directly in `plugins\needs_plus.ini`.

---

## ✨ Features

### 🎯 Original features (kept)
- ✅ One new need per line: `<resource> = <donor>[, <factor>[, <category>[, <chance>[, <unhappiness>]]]]`
- ✅ The need is cloned from its donor (food, meat, clothes, eletronics) and lands in exactly the shops that stock the donor
- ✅ Shop category per need: like the donor, grocery only, department store only, none
- ✅ Chance and unhappiness per need
- ✅ Up to eight needs, the per-citizen ceiling is configurable

### 🆕 Improvements (new in Needs Plus)

#### 1️⃣ **Its own file, taken over automatically**
Needs Plus reads `plugins\needs_plus.ini`. If it is missing, it is created at the first start from what you already have: from the original plugin's `plugins\needs.ini`, in the same order, with a header line saying where it came from. The original is only read, never changed. If that `needs.ini` was written by Soviet Mod Loader (which rewrites it at every game start with all packages merged in), Needs Plus takes SML's baseline instead, so no package need appears twice.

#### 2️⃣ **Duplicate names are refused**
If a resource is listed twice in `[list]`, the first line counts and the second is skipped with a log line. The original would have given the citizens the need twice.

#### 3️⃣ **Steps aside by itself**
While the original is enabled or Soviet Mod Loader is running, Needs Plus stays idle and writes one line to the log. No citizen ever gets a need twice.

---

## 💾 Installation

Needs Plus and the original plugin needs never run at the same time. The installer of the Republic Mod Manager package handles that for you; by hand it works the same way.

### Method 1️⃣: Republic Mod Manager (package, recommended)

```
1. Subscribe to the Republic Mod Manager package and run Install-RMM.bat
2. needs_plus.dll and the template needs_plus.template.ini land in
   tesmioloader\build\plugins\ (an existing needs_plus.ini is left untouched)
3. The installer sets in tesmioloader.ini: needs_plus=1 and, if
   plugins\needs.dll exists, needs=0 - and tells you so
4. Start the game: the list from plugins\needs.ini is taken over
```

**Back to the original:** switch Needs Plus off and needs on in Republic Mod Manager, or set `needs=1` and `needs_plus=0` in `tesmioloader.ini`. `Uninstall-RMM.bat` does the same. The original's DLL is never deleted or overwritten.

### Method 2️⃣: By hand

```
1. Copy needs_plus.dll → tesmioloader\build\plugins\
2. Enable needs_plus in TesmioLauncher (or needs_plus=1 in tesmioloader.ini)
3. Disable the original plugin needs (needs=0)
4. Start the game: plugins\needs_plus.ini is created from your needs.ini;
   without a needs.ini the plugin uses the template needs_plus.template.ini beside the DLL
```

### Method 3️⃣: Soviet Mod Loader (SML)

⚠️ **While SML runs, Needs Plus stays idle.** SML carries its own copy of the original plugin. If you want Needs Plus, switch `soviet_mod_loader` off in `tesmioloader.ini` (the "Plugin active" switch in Republic Mod Manager) and use method 1 or 2. At the next start Needs Plus takes your list over from SML's baseline; Republic Mod Manager provides the packages afterwards.

---

## 🧰 Republic Mod Manager

- The entry **Needs Plus** edits `plugins\needs_plus.ini`: list, donor, factor, category, chance, unhappiness and the plugin switches. If the file does not exist yet, RMM creates it when the page opens, by the same rule the plugin uses, and says so in a blue box. The old entry "Needs" stays in the list as a switched-off plugin.
- The + dialog only offers resources that are in the resource list (Resources Plus or the original).
- **Content packages** from the Workshop (`[content] needs`) go into `plugins\needs_plus.ini` once it exists, and can be taken back.
- The **"Plugin active"** switch for Needs Plus writes the entry in `tesmioloader.ini`.
- While SML runs, Needs Plus shows **amber**: paused, not broken. The page is locked until you choose "Edit anyway".
- The log window shows `logs\tesmioloader.needs_plus.log`.

---

## ⚙️ Configuration

### Main file: `plugins\needs_plus.ini`

| Section | Content |
|---|---|
| `[list]` | One line per need: `<resource> = <donor>[, <factor>[, <category>[, <chance>[, <unhappiness>]]]]` |
| `[needs]` | How the plugin reaches the game: `enabled`, `demand`, `storage`, `max_demands`, `when_full`, `probe`, `log_seconds` |

| Field | Meaning |
|---|---|
| Donor | An existing citizen need: `food`, `meat`, `clothes` or `eletronics` (the game's spelling) |
| Factor | Amount relative to the donor, `1.0` = the same |
| Category | `auto` (like the donor), `grocery`, `advanced` (department store), `none` (only your own shops with `$STORAGE_SPECIAL`) |
| Chance | 0 to 1, how often a citizen picks the need up at all |
| Unhappiness | 0 to 1, what a citizen loses per failed shopping trip |

The format is the original plugin's; every key is explained in the file itself.

### Good to know
- **The resource has to exist.** It sits in the resource list (Resources Plus: `plugins\resources_plus.ini`), and its transport class has to fit the donor's shops. Easiest: clone the resource from the same donor there.
- **Saves depend on the list.** Every need adds a storage slot to the affected shops, and the slot list is part of the save. Test changes on a copy first.

---

## 🔗 Relation to the original plugin

### Coexistence with `needs.dll`

Needs Plus and the original plugin `needs` share:
- **the same hook sites** in the game (citizen daily plan, storage slots, shop tick)
- **the same file format**, in separate files: `plugins\needs_plus.ini` and `plugins\needs.ini`
- **compatibility:** saves stay valid as long as `[list]` has the same entries

### If both are installed

**Needs Plus only loads when the original is switched off.**

If `plugins\needs.dll` exists and is enabled in tesmioloader.ini:
- Needs Plus stays idle before it patches anything
- In the log: `needs_plus  idle - plugins\needs.dll is present and enabled`
- The original loads normally

The same applies under Soviet Mod Loader: `needs_plus  idle - soviet_mod_loader.dll brings its own needs`.

**Solution:** switch the original or SML off in TesmioLauncher or Republic Mod Manager. Deleting is not necessary.

---

## 💾 Compatibility

### Save format
Unchanged from the original: the storage slots of the shops are part of the save. A save made with the original or under SML loads with Needs Plus as long as `[list]` has the same entries. That is exactly what the automatic takeover guarantees.

### Version compatibility
- 0.1.1: safety round, no new feature: its own log `logs\tesmioloader.needs_plus.log` for `probe`, the summary stays in `tesmioloader.log`; factor, chance and unhappiness of a line as well as `max_demands`, `when_full` and `log_seconds` are checked and set to the allowed value with a WARN line; a UTF-8 BOM in needs_plus.ini no longer hides `[list]`; more than 16 needs are reported once instead of being cut off silently; the shop slot fix switches itself off after a fault instead of repeating it
- 0.1.0: first version as a fork of needs 1.0. Same features, plus its own file with automatic takeover, the duplicate-name check and the guard against the original and SML.

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| No own needs in the game | Original or SML enabled | Read the log: `needs_plus  idle …`, then switch the other one off |
| Log says "no resource named …" | Resource missing from the resource list | Create it in Resources Plus, then restart |
| Log says "cannot be stored in transport class …" | Transport class does not fit the donor's shops | Clone the resource from the same donor in the resource list |
| A need is missing, the log says "listed twice" | Name twice in `[list]` | Remove the second line |
| Citizens want the good, shops do not have it | `storage = 0` | Set `storage = 1` or stock the shops yourself |

### Logging
- Summary in `tesmioloader.log`: `plugin needs_plus 0.1.1`, at the first start `created plugins\needs_plus.ini from …`, then one line per need with donor, factor and category
- With `probe = 1` the plugin writes the citizens' demand lists and the shops' storage slots into its own log `logs\tesmioloader.needs_plus.log`; invalid INI values show up there as WARN lines

---

## 📦 File structure

```
tesmioloader\build\
├── logs\
│   └── tesmioloader.needs_plus.log   (detailed log, created at the first start)
└── plugins\
    ├── needs_plus.dll
    ├── needs_plus.ini              (your list; taken over from needs.ini when the RMM page first opens or at the first game start)
    ├── needs_plus.template.ini     (template from the package, used only without a needs.ini)
    └── needs.ini                   (original plugin, left untouched)
```

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. Needs Plus is a fork of the `needs` plugin from the TesmioLoader by MaxLegend (Tesmio), GPL v3, https://github.com/MaxLegend/TesmioLoader; hook sites and file format deliberately stay identical to the original. The complete source of Needs Plus lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/needs_plus.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Can I use both plugins at the same time?**
A: No. Either the original OR Needs Plus. While the original is enabled, Needs Plus stays idle.

**Q: Do I have to carry my needs.ini over by hand?**
A: No. At the first start Needs Plus copies it to `needs_plus.ini`. From then on `needs_plus.ini` is the file that counts.

**Q: What about Soviet Mod Loader?**
A: SML ships the original built in. Under SML, Needs Plus pauses. If you want it, switch SML off and load your packages through the Workshop Bridge.

**Q: Why did the installer switch my needs off?**
A: So that the two do not run at once. It says so in its output, deletes nothing, and one switch brings the original back.
