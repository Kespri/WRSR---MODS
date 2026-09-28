# 🛠️ Republic Mod Manager

**English** | [Deutsch](README_DE.md)

Settings, switching and loading of your TesmioLoader plugins in one window. This Workshop package brings the program `rmm.exe`, the settings pages for the plugins that ship with the loader, and the Workshop Bridge that loads subscribed plugin packages straight from the Steam Workshop.

---

## 📋 Contents

1. [Quick start](#-quick-start)
2. [What is inside](#-what-is-inside)
3. [Installation](#-installation)
4. [Update and removal](#-update-and-removal)
5. [File structure](#-file-structure)
6. [Troubleshooting](#-troubleshooting)
7. [Licence & credits](#-licence--credits)
8. [FAQ](#-faq)

---

## 🚀 Quick start

### Requirements

- *Workers & Resources: Soviet Republic* 1.1.1.9
- **TesmioLoader** by MaxLegend, installed in the game folder (subscribe in the Steam Workshop and set it up as its guide says). This package brings a corrected build of its `tesmioloader.dll` (b0.3.6-rmm.1, same API, source in the GitHub repo) and replaces the one you have; the launcher and `tesmioloader.ini` stay MaxLegend's
- Windows 10 or 11 (.NET Framework 4 is part of it)

### In three steps

1. Subscribe to this Workshop item in Steam and wait for the download to finish.
2. Open the item's folder (`Steam\steamapps\workshop\content\784150\<item number>`) and double-click **Install-RMM.bat**. The window ends with "Republic Mod Manager is installed".
3. Start `rmm.exe` in `<game>\tesmioloader\build`, or the desktop shortcut if you created one.

---

## ✨ What is inside

### 🎯 Republic Mod Manager (`rmm.exe`)

- One list with every subscribed plugin package and every plugin that lives in `tesmioloader\build\plugins`.
- Settings pages generated from the package schema: tabs, cards, switches, number fields with plus and minus, lists, pickers for buildings, research entries and game texts, all with descriptions and range checks.
- "Plugin active" switch per package. Your personal values stay separate from the shipped INI, a package update never overwrites them.
- Profiles and restore points for the whole plugin configuration.
- Log window with `tesmioloader.log` and every plugin log, problems highlighted.
- "Save + Start" launches the game through `tesmiolauncher.exe`.
- German and English, following the Windows language.

### 🌉 Workshop Bridge (`plugins\workshop_bridge.dll`)

Loads the plugin packages you switched on in Republic Mod Manager straight from the Steam Workshop folder. No Soviet Mod Loader needed. The manager keeps the package list, you never edit it by hand.

### 🏭 Buildings Plus (`plugins\buildings_plus.dll`)

New buildings from a declaration: you name a donor building of the game and the lines that should differ, and at game start a complete Workshop item appears under `media_soviet\workshop_wip`. Needed for new resources that want a mine or a factory. Installed switched off; enable it in the manager under "Buildings Plus" and declare buildings.

### 📦 Resources Plus (`plugins\resources_plus.dll`)

Your own resources, as a replacement for the original resources plugin: the same entries, the same saves, without the crash on exit the original had from seven own resources on. **The installer switches Resources Plus on and an existing resources off** and says so in its output; the original's DLL stays where it is. At the next game start Resources Plus takes your list over from `plugins\resources.ini` into `plugins\resources_plus.ini`, in the same order. Back to the original: switch Resources Plus off and resources on in the manager, or run `Uninstall-RMM.bat`. Under Soviet Mod Loader, Resources Plus stays idle by itself.

### 📦 Needs Plus (`plugins\needs_plus.dll`)

Your own citizen needs, as a replacement for the original needs plugin: the same entries, the same saves, in a file of its own. **The installer switches Needs Plus on and an existing needs off** and says so in its output; the original's DLL stays where it is. At the next game start Needs Plus takes your list over from `plugins\needs.ini` into `plugins\needs_plus.ini`. Back to the original: switch Needs Plus off and needs on in the manager, or run `Uninstall-RMM.bat`. Under Soviet Mod Loader, Needs Plus stays idle by itself.

### 📄 Settings pages (`settings_schemas\`)

German and English settings pages for the plugins that come with the TesmioLoader: Accumulator, Cities, Day and Night, Depletion, Deposits, Easy Start, Needs, Resources, Walking Distance and the Workshop Bridge.

---

## 💾 Installation

### Way 1️⃣: Installer (recommended)

1. Close the game, the TesmioLauncher and any running Republic Mod Manager.
2. Double-click **Install-RMM.bat** in the folder of the Workshop item.

The installer, in order:

- finds the game folder (it is in the same Steam library as the Workshop folder; otherwise it asks you),
- checks that `tesmioloader\build\tesmiolauncher.exe` and `tesmioloader.ini` exist and stops with a hint if not; the existing `tesmioloader.dll` is replaced by the build from the package (the old one goes into the backup),
- copies `rmm.exe`, `rmm.ini`, the guides, `settings_schemas\` and the Workshop Bridge into `tesmioloader\build`; replaced files go to `tesmioloader\rmm_install_backup\<date>`,
- leaves an existing `rmm.ini` and `plugins\workshop_bridge.ini` alone (your base settings),
- writes `workshop_bridge=1` under `[plugins]` in `tesmioloader.ini`,
- asks whether to create a desktop shortcut.

If something goes wrong the window stays open and shows in red what failed and why. If everything runs through it closes itself after five seconds.

If the game folder is not found you can pass it:

```
Install-RMM.bat -GamePath "D:\Games\Steam\steamapps\common\SovietRepublic"
```

### Way 2️⃣: By hand

The folder **Manual Installation** contains the complete `tesmioloader` folder exactly as it has to lie in the game folder.

1. Close the game, the TesmioLauncher and Republic Mod Manager.
2. Drag `Manual Installation\tesmioloader` onto the game folder and confirm the merge.
3. In `tesmioloader\build\tesmioloader.ini` add the line `workshop_bridge=1` under `[plugins]` (save as UTF-8 without BOM).
4. Start `rmm.exe`.

The file `Manual Installation\WHERE.txt` repeats these steps.

---

## 🔄 Update and removal

**Update:** Steam only updates the folder of the Workshop item, not the copy in the game folder. After an update just run **Install-RMM.bat** again. Your `rmm.ini`, `workshop_bridge.ini` and everything under `user_config` are kept.

**Removal:** double-click **Uninstall-RMM.bat**. It deletes the installed files, sets `workshop_bridge=0` in `tesmioloader.ini` and removes the desktop shortcut. The `user_config` folder with your settings stays; the TesmioLoader is not touched.

---

## 📦 File structure

**Workshop package** (what you subscribe to)
```
<item number>\
├── Install-RMM.bat                 (installer, double-click)
├── Uninstall-RMM.bat               (removal, double-click)
├── Install-RMM.ps1                 (does the actual work)
├── Manual Installation\
│   ├── WOHIN.txt / WHERE.txt       (the three steps by hand)
│   └── tesmioloader\build\         (target structure, to drag over)
│       ├── rmm.exe
│       ├── rmm.ini
│       ├── rmm.README_EN.md, rmm.README_DE.md   (guide)
│       ├── rmm.SCHEMA_EN.md, rmm.SCHEMA_DE.md   (reference for plugin authors)
│       ├── settings_schemas\
│       └── plugins\
│           ├── workshop_bridge.dll
│           ├── workshop_bridge.ini
│           ├── buildings_plus.dll
│           ├── buildings_plus.ini
│           ├── resources_plus.dll
│           ├── resources_plus.template.ini
│           ├── needs_plus.dll
│           └── needs_plus.template.ini
├── workshopconfig.ini              (Steam Workshop entry)
├── previewimage.png
├── LICENSE
├── README_DE.md
└── README_EN.md
```

**Game folder** after the installation
```
SovietRepublic\tesmioloader\
├── build\
│   ├── tesmioloader.dll                                 (TesmioLoader b0.3.6-rmm.1, from this package)
│   ├── tesmiolauncher.exe, tesmioloader.ini              (TesmioLoader by MaxLegend, not from this package)
│   ├── rmm.exe, rmm.ini, rmm.README_EN/DE.md, rmm.SCHEMA_EN/DE.md
│   ├── settings_schemas\
│   ├── plugins\
│   │   ├── workshop_bridge.dll
│   │   ├── workshop_bridge.ini
│   │   ├── buildings_plus.dll
│   │   ├── buildings_plus.ini
│   │   ├── resources_plus.dll
│   │   ├── resources_plus.ini          (taken over from resources.ini at the first start)
│   │   ├── needs_plus.dll
│   │   └── needs_plus.ini              (taken over from needs.ini at the first start)
│   ├── user_config\                (your personal values, written by the manager)
│   └── logs\                       (detail logs of the plugins)
└── rmm_install_backup\<date>\      (only when the installer replaced files)
```

---

## ⚙️ Troubleshooting

| Installer message | Cause | Fix |
|---|---|---|
| The TesmioLoader is not installed | `tesmioloader\build\tesmiolauncher.exe` or `tesmioloader.ini` missing | Subscribe to the TesmioLoader by MaxLegend, install it as its guide says, run again |
| The game folder was not found | Game is in another Steam library or the package was copied elsewhere | `Install-RMM.bat -GamePath "<game folder>"` |
| The game or Republic Mod Manager is still running | Files are in use | Close them, run again |
| File could not be copied | Write permissions or antivirus | Run as administrator or whitelist the folder in the antivirus |
| The package is incomplete | Steam download incomplete | Unsubscribe and resubscribe the item |

Windows SmartScreen may report `rmm.exe` as an unknown program on the first start because it is not signed. "More info" and "Run anyway" once is enough.

The manager has its own log window (icon in the sidebar) with `tesmioloader.log` and the plugin logs from `build\logs\`. The manager's guide lies as `rmm.README_EN.md` (German: `rmm.README_DE.md`) beside `rmm.exe`.

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. Republic Mod Manager and the Workshop Bridge are own developments; the bundled `tesmioloader.dll` is a corrected build of MaxLegend's TesmioLoader b0.3.6 (GPL v3), the launcher and the ini stay his work. The complete source of the plugins, the manager and the loader lives at https://github.com/Kespri/WRSR-MODS.

**Attention, comrade:** this program was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Do I need the Soviet Mod Loader?**
A: No. The bundled Workshop Bridge loads the packages. If the Soviet Mod Loader is installed anyway, the bridge stays idle and the manager only writes the settings.

**Q: Why does subscribing alone install nothing?**
A: Steam only puts Workshop files into its own folder. A program in the game folder has to be copied there by someone, and that is what the installer does.

**Q: Do I lose my settings on an update?**
A: No. `rmm.ini` and `workshop_bridge.ini` are only created when missing, and the installer never touches `user_config`.

**Q: Where are the plugins?**
A: Every plugin is its own Workshop item. Subscribe, switch it on in the manager, done.

---

**Last update:** Republic Mod Manager 0.4.79 with Workshop Bridge 0.2.0  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
