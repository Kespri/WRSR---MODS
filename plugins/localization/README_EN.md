# 🌐 Localization 0.4.0

**TesmioLoader plugin for custom texts and translations**

Adds custom names, descriptions and labels to *Workers & Resources: Soviet Republic* 1.1.1.9 without touching the original language files. Text packs are plain INI files; at game start the plugin builds extended `soviet*.btf` language files from them in the TesmioLoader virtual file system (VFS) and provides a translation service to other plugins. The package contains the text packs of Research Expansion, Technical Service Storage and the shared pack `tesmio_lang`.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Text packs](#-text-packs)
- [Language files](#-language-files)
- [Use in other plugins](#-use-in-other-plugins)
- [Value ranges](#-value-ranges)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9
- TesmioLoader API 4 with VFS enabled
- At least one valid text pack (three are included)

### In three steps
1. **Choose one installation method** (see below) and enable the plugin.
2. **Check the text packs:** the included packs need no setting. Own packs go as folders to `plugins\localization\<packname>` with `localization.ini` and at least the fallback language file.
3. **Restart the game completely.** `logs\tesmioloader.localization.log` names the configuration file, the pack folders, the loaded packs and the generated language files under `tesmioloader\vfs\media_soviet`.

---

## ✨ Features

### 🎯 Core function
- ✅ Namespaced text packs: one folder per plugin with `localization.ini` and `soviet<Language>.ini` files
- ✅ Fallback language per pack and a visible placeholder `[MISSING TEXT: …]` for missing keys
- ✅ Text IDs in the reserved range 2,000,000 to 2,999,999, assigned deterministically at every start
- ✅ Service `localization` for other plugins: key to text ID
- ✅ Strict validation of encoding, sections, keys, escape sequences and BTF format; a faulty pack is rejected alone
- ✅ Generated files are removed before every start and when the plugin is switched off; the originals under `media_soviet` are never changed

### 🆕 New in 0.4.0
- ✅ **INI and text packs beside the DLL:** when `plugins\localization.ini` is missing, the DLL reads the INI from its own folder, i.e. from the Workshop package under Soviet Mod Loader or the Workshop Bridge. Text packs are loaded from `plugins\localization` **and** from the folder `localization` beside the DLL. Both paths are logged.
- ✅ **Local text packs are merged instead of replacing:** a pack folder under `plugins\localization` is laid over the folder of the same name beside the DLL key by key. Your own texts and languages stay, new keys and languages from a Workshop update still arrive; for the same key your local text wins. The local `localization.ini` decides namespace, fallback and missingText.
- ✅ The text packs `research_expansion`, `technical_service_storage` and `tesmio_lang` travel with the package.
- ✅ Republic Mod Manager: tabs General (notices, guide, troubleshooting) and Plugin ("Apply extension" with the switch-off order, text packs as a list with a short text and origin).

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice, and the service name `localization` may be registered only once.

---

### Method 1️⃣: Classic TesmioLoader

```
1. Copy localization.dll and localization.ini from hooks\
   → tesmioloader\build\plugins\

2. Copy the folder hooks\localization (the text packs)
   → tesmioloader\build\plugins\localization\

3. Enable localization in the TesmioLauncher
4. Restart the game completely
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL through soviet.mod.ini from the package; INI and text packs sit beside it
3. Remove or disable a local localization.dll in plugins\ first
```

---

### Method 3️⃣: Workshop Bridge (without SML)

```
1. Select the package in Republic Mod Manager
2. Switch "Plugin enabled" on
3. workshop_bridge loads the DLL straight from the package; it finds INI and text packs beside itself
4. Steam updates apply immediately
```

---

### Method 4️⃣: Republic Mod Manager with "Files local only"

```
1. Select the package in Republic Mod Manager, adjust the settings, Save
2. Tab "General", card "Notes": switch "Files local only" on
3. Confirmation with file list → DLL, INI and the folder localization with the
   text packs are copied to tesmioloader\build\plugins\ when saving
4. The Workshop Bridge skips the package automatically afterwards
```

**"Files local only" in detail**
- For everyone who wants to keep using the plugin without a Steam subscription
- With local files Steam updates apply only after saving again (yellow "Update" mark)
- Switching it off removes only the files Republic Mod Manager copied

---

## 🧰 Republic Mod Manager

The package contains a presentation schema in the `config` folder. Republic Mod Manager shows Localization on one tab, German and English: notes, "Files local only", a button for this guide, the two switches (plugin enabled, verbose log) and an overview of the included text packs. Text packs themselves are not edited in the manager; they are folders with INI files.

Republic Mod Manager writes the effective file `plugins\localization.ini` as UTF-8 without BOM; the INI in the package stays unchanged.

---

## ⚙️ Configuration

### Main file: `localization.ini`

The DLL reads in this order:
- **First:** `tesmioloader\build\plugins\localization.ini` when it exists (classic installation, "Files local only" or the effective INI written by Republic Mod Manager)
- **Otherwise:** the INI beside the DLL, in the package `hooks\localization.ini` (Soviet Mod Loader, Workshop Bridge)

The main file must be **UTF-8 without BOM**. Exactly one section `[localization]` with the keys `enabled` and `verbose` is allowed, both exactly `0` or `1`. Unknown or duplicate sections and keys are rejected; the plugin then stays inactive and the Vanilla language files apply.

```ini
[localization]
; 1 = load text packs, provide the service, generate language files; 0 = remove generated files, inactive
enabled = 1
; 1 = technical paths in the detail log
verbose = 0
```

⚠️ **Before disabling the DLL** set `enabled = 0` and start the game once: only then does the plugin remove the generated `soviet*.btf` files from the VFS. If the DLL is switched off directly in the launcher, the extended language files stay in place.

### Pack folders

Text packs are loaded from two folders:
- `tesmioloader\build\plugins\localization\<packname>` (classic installation, "Files local only", own packs)
- the folder `localization` beside the DLL, in the package `hooks\localization` (Soviet Mod Loader, Workshop Bridge)

A pack folder under `plugins\localization` is laid over the folder of the same name beside the DLL key by key: local texts win, everything else still comes from the package, including languages and keys a Workshop update adds. That way you can adapt an included text pack locally without falling behind. At most 256 pack folders in total.

---

## 📦 Text packs

Every text pack is a folder with a `localization.ini` and at least the fallback language file:

```text
research_expansion\
├── localization.ini
├── sovietEnglish.ini
└── sovietGerman.ini
```

```ini
[localization]
namespace = research_expansion
fallback = sovietEnglish
missingText = [MISSING TEXT: {key}]
```

| Setting | Meaning |
|---|---|
| `namespace` | unique name of the pack: lowercase letters, digits, `_`, `-`, up to 80 characters |
| `fallback` | language without `.ini` used when the game language or a text in it is missing; the file must exist and be valid |
| `missingText` | placeholder text, `{key}` is replaced by the full key; default `[MISSING TEXT: {key}]` |

Pack configuration and language files may be UTF-8 with or without BOM. Two packs with the same namespace are both rejected.

**Included packs**

| Pack | Namespace | Content |
|---|---|---|
| `research_expansion` | `research_expansion` | names and descriptions of new research of Research Expansion |
| `technical_service_storage` | `technical_service_storage` | labels of the grit tank and the depot warnings of Technical Service Storage |
| `tesmio_lang` | `tesmio_lang` | shared texts, for example `depletion.deposit_remaining` for the deposit caption of Depletion |

---

## 🔤 Language files

Language files are named like the game's language files: `sovietEnglish.ini`, `sovietGerman.ini`, `sovietFrench.ini` and so on. Each has exactly one section `[strings]`:

```ini
[strings]
quartz_smasher.name = Quartz Crusher
quartz_smasher.desc = Crushes quartz for glass production.\nNeeds additional workers.
```

- Keys: lowercase letters, digits, `_`, `-` and single dots, up to 160 characters, once per file; the namespace is prepended automatically (`research_expansion.quartz_smasher.name`)
- Escape sequences: only `\n` (line break) and `\\` (backslash); any other makes the whole language file invalid
- A text may contain `=`, the split is at the first equals sign; empty texts are not allowed; comments only on their own lines with `;` or `#`

**Fallback:** for every text the plugin looks in the active game language first, then in the fallback language, finally in the placeholder. A key that appears in no language file is not registered and yields ID 0.

---

## 🔌 Use in other plugins

The plugin provides the TesmioLoader service `localization` in version 1. Another plugin queries it in `TsmPluginStart()`:

```cpp
static const TsmLocalizationApi* L = nullptr;

extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    L = (const TsmLocalizationApi*)H->consume(TSM_SERVICE_LOCALIZATION, TSM_LOCALIZATION_VERSION);
    if (!L) return 1;
    int nameId = L->resolveFull("research_expansion.quartz_smasher.name");
    int descId = L->resolve("research_expansion", "quartz_smasher.desc");
    return (nameId && descId) ? 0 : 1;
}
```

The IDs lie between 2,000,000 and 2,999,999 and fit `$NAME` and `$DESC` in game files. An invalid namespace, an invalid key or a key without a language file yields 0.

---

## 📏 Value ranges

| Quantity | Limit |
|---|---|
| `enabled`, `verbose` | exactly 0 or 1 |
| size per INI file | 16 MiB |
| size per read or generated BTF file | 256 MiB |
| pack folders (both paths together) | 256 |
| registered text keys | 100,000 |
| text length | 65,535 UTF-16 code units |
| namespace / key | 80 / 160 characters |
| reserved IDs | 2,000,000 to 2,999,999 |

---

## 💾 Compatibility

### Savegames
Language files are not part of the savegame. Texts of a research or resource a savegame knows need the same key after an update so the ID stays stable: IDs are assigned in a fixed order of packs and keys.

### Other plugins
- **Research Expansion** requires this service.
- **Technical Service Storage** uses it for translated labels but runs without it (built-in fallback texts).
- **Depletion** 1.1.2 or newer uses `tesmio_lang.depletion.deposit_remaining` when the service is present.
- Other plugins that replace `soviet*.btf` in the VFS are not merged.

### Version compatibility
- **0.4.0:** first published version; Research Expansion requires `tesmio.localization >= 0.4.0`

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| `main-config`, `unknown-section`, `unknown-key`, `invalid-value` | main file faulty | only `[localization]` with `enabled`/`verbose` as 0 or 1 |
| `utf8-bom` | main file saved with BOM | save as UTF-8 without BOM |
| `pack-root` | neither `plugins\localization` nor a folder `localization` beside the DLL | provide text packs |
| `Pack was rejected because its localization.ini is invalid` | required setting missing or value invalid | check the pack configuration |
| `Fallback ... has no valid language file` | fallback file missing or faulty | create or correct the file |
| `Invalid escape sequence` | sequence other than `\n` or `\\` | correct the text |
| `namespace-collision` | two packs with the same namespace | rename one; between `plugins\localization` and the folder beside the DLL only the folder name counts, not the namespace |
| `missing-key` | a plugin asks for a key that is in no language file | add the key to the fallback file |
| `id-collision` | assigned text ID collides with an existing one | check the log; all generated files are removed |
| `cleanup`, `cleanup-scan` | a generated VFS file could not be removed | close the game and programs accessing it |
| texts missing in the game | plugin inactive or pack rejected | read the log: `Ready: N pack(s)` and the errors before it |

### Logging

All messages go to `tesmioloader.log` and the detail log `logs\tesmioloader.localization.log` with file, line, rule and reason.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Search for:
- `Configuration file` and `Pack folders` → which INI and which pack folders were chosen
- `Ready:` → number of loaded packs and keys
- `is skipped; plugins\localization has a folder of that name` → a local pack overrode the package copy

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
localization\
├── hooks\
│   ├── localization.dll            (plugin)
│   ├── localization.ini            (original INI)
│   └── localization\               (text packs)
│       ├── research_expansion\
│       ├── technical_service_storage\
│       └── tesmio_lang\
├── config\                         (schema for Republic Mod Manager)
│   ├── localization.launcher.ini
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
│   │   ├── localization.dll
│   │   ├── localization.ini        (effective INI)
│   │   └── localization\           (text packs, own and included)
│   ├── logs\
│   │   └── tesmioloader.localization.log
│   └── tesmioloader.log
└── vfs\media_soviet\               (generated soviet*.btf)
```

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. The plugin contains no third-party code; the loader SDK header comes from the TesmioLoader by MaxLegend (GPL v3). The complete source lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/localization.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Are the game's language files changed?**
A: No. The extended files are created in the loader's VFS; the originals under `media_soviet` are only read.

**Q: How do I add my own texts?**
A: Create a folder under `plugins\localization\<name>` with `localization.ini` (namespace, fallback) and at least the fallback language file, keys under `[strings]`, restart the game.

**Q: Can I adapt an included text pack?**
A: Yes: copy the folder to `plugins\localization` and change it there. Your keys win over the package copy, the rest still comes from the package, and Workshop updates leave your files alone.

**Q: How do I get rid of the extended language files again?**
A: Switch the plugin off, start the game once, then disable the DLL.

**Q: Do I have to edit the INI by hand?**
A: No. Republic Mod Manager offers the two switches; text packs are folders with INI files.

---

**Last update:** Localization 0.4.0  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
