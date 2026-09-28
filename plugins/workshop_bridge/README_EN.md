# 🌉 Workshop Bridge 0.2.0

**TesmioLoader plugin: loads Workshop packages without Soviet Mod Loader**

Loads the hook DLLs of your subscribed Workshop packages straight into TesmioLoader for *Workers & Resources: Soviet Republic* 1.1.1.9. No Soviet Mod Loader, no copying by hand, Steam updates apply at once.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Value ranges](#-value-ranges)
- [Flow & log](#-flow--log)
- [What the bridge never does](#-what-the-bridge-never-does)
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
- TesmioLoader b0.3.6 (API 4)
- Republic Mod Manager (recommended, keeps the package list for you)

### In three steps
1. **Bridge into the loader folder:** it ships with the Republic Mod Manager package into `tesmioloader\build\plugins\` (installer or the "Manual Installation" folder, see Installation).
2. **Switch the bridge on:** `tesmioloader.ini` needs `workshop_bridge = 1` under `[plugins]`. Republic Mod Manager sets that on its first save; in TesmioLauncher it is the check mark.
3. **Release packages:** in Republic Mod Manager switch "Plugin active" on for each Workshop package. The bridge loads its hooks at the next game start.

---

## ✨ Features

- **Loads straight from the package:** the DLL stays in the Workshop folder, nothing is copied. A Steam update is in at the next start.
- **Same view as under SML:** every loaded hook gets the very host table the bridge received itself, so the same loader folder, the same `user_config`, the same services.
- **Two phases like the loader:** every hook is initialised inside the bridge's Init, so its services are on the board before any Start; the Starts run inside the bridge's Start.
- **Package list by switch:** which packages load is decided per package by "Plugin active" in Republic Mod Manager. Without the manager the `policy` rule applies.
- **Stays out of the way:** idle under Soviet Mod Loader, a local copy in `plugins\` always wins, a DLL is never loaded twice.
- **No game code:** the bridge hooks nothing and patches nothing. A game update cannot break it.

---

## 💾 Installation

The bridge is distributed together with Republic Mod Manager. Pick **one** method.

### Method 1️⃣: Installer of the Republic Mod Manager package

```
1. Subscribe to or download the Republic Mod Manager package
2. Run Install-RMM.ps1 (or the .bat)
3. The installer creates rmm.exe, the folders and workshop_bridge.dll + .ini
   in tesmioloader\build\plugins\ and enters the bridge in tesmioloader.ini
```

### Method 2️⃣: "Manual Installation" folder

```
1. Open the "Manual Installation" folder in the package
2. It holds the complete structure from tesmioloader\ downwards
3. Drag the tesmioloader folder over the one in the game folder (merge)
4. Tick workshop_bridge in TesmioLauncher
   or save once in Republic Mod Manager
```

### Method 3️⃣: Built yourself (developers)

```
1. Build workshop_bridge.cpp with the build line from the repository front page
2. Copy workshop_bridge.dll and workshop_bridge.ini into <game>\tesmioloader\build\plugins\
3. Set workshop_bridge=1 under [plugins] in tesmioloader.ini
```

**Note:** with Soviet Mod Loader you do not need the bridge. While SML is loaded or switched on in `tesmioloader.ini`, the bridge stays idle by itself (log line `bridge   idle`).

---

## 🧰 Republic Mod Manager

Republic Mod Manager lists the bridge as its own entry "Workshop Bridge" with one card for its four switches. The package list is deliberately not a field there: it comes from the "Plugin active" switch of each Workshop package and lands in `user_config\workshop_bridge.ini`. Each package's status line shows "Load path: Workshop Bridge" when it runs through the bridge, and "Last loaded in game" reads the bridge line from `tesmioloader.log`.

Personal values of the bridge itself live in `user_config\workshop_bridge.ini` as well; the base `plugins\workshop_bridge.ini` stays untouched.

---

## ⚙️ Configuration

The base is `plugins\workshop_bridge.ini`; `user_config\workshop_bridge.ini` lies over it key by key. Republic Mod Manager writes only the overlay file.

```ini
[bridge]
; 0 = bridge idle, no package is loaded
enabled = 1
; list = only packages with 1 under [packages]; all = every package with hooks unless 0
policy = list
; auto = the game's Steam library, otherwise an absolute path with one subfolder per package
workshop_root = auto
; 1 = every skipped package gets a log line
log_verbose = 0

[packages]
; Workshop item number or folder name = 1 loads its hooks, 0 skips them
3794994476 = 1
```

A package with `enabled = 0` in its own `soviet.mod.ini` is always skipped, whatever stands here.

---

## 📏 Value ranges

| Key | Range | Default |
|---|---|---|
| `enabled` | 0 or 1 | 1 |
| `policy` | `list` or `all` | `list` |
| `workshop_root` | `auto` or absolute path | `auto` |
| `log_verbose` | 0 or 1 | 0 |
| `[packages] <name>` | 0 or 1 | – |

---

## 🔄 Flow & log

1. In Init the bridge reads INI and overlay, checks for Soviet Mod Loader and walks the package folders under `workshop_root`.
2. Every released package with `[hooks] dll` is loaded and its Init runs at once. Services a hook registers with `provide` are on the board before any Start.
3. The bridge's Start runs the Starts of all hooks.

A normal start in `tesmioloader.log`:

```
workshop_bridge  config base=...\plugins\workshop_bridge.ini (loader folder) overlay=...\user_config\workshop_bridge.ini
bridge   Workshop C:\...\steamapps\workshop\content\784150, policy list
plugin   service "tss.grit_spreader" v1 from workshop_bridge.dll
bridge   hook technical_service_storage 0.3.3    from <item number>\hooks\technical_service_storage.dll
bridge   10 hook(s) loaded, 0 skipped, 10 package(s) with hooks
plugin   workshop_bridge  0.2.0 from workshop_bridge.dll
```

The loader credits a hook's services to `workshop_bridge.dll` in its log; the bridge names the real name and version right beside it.

---

## 🚫 What the bridge never does

- **Soviet Mod Loader** is loaded, or installed and switched on: SML loads the hooks itself, the bridge stays idle.
- **`plugins\<name>.dll` exists:** that copy belongs to the loader, on or off as the launcher says. The package is skipped and a log line says why. If you want the package, delete the local copy or switch "Files local only" off in Republic Mod Manager.
- **Load twice:** a DLL already in the process under the same file name is not loaded again.
- **Escape the package:** hook paths with `..` or an absolute path are refused.
- **Touch game code:** no hook, no patch, no address.

---

## 🔗 Compatibility

- WRSR 1.1.1.9, TesmioLoader b0.3.6 with API 4 (3 at least). The bridge knows no game addresses, so it survives game updates; whether the loaded hooks do is up to each hook.
- Soviet Mod Loader: compatible, the bridge steps aside.
- Republic Mod Manager 0.4.12 or newer (status line with load path).

---

## 🛠️ Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `bridge   0 hook(s) loaded` | No package released, or `policy = list` with an empty list | Switch "Plugin active" on for the package in Republic Mod Manager |
| `bridge   idle` | Soviet Mod Loader is loaded or switched on | Intended. Either SML or the bridge |
| Package skipped, log line names `plugins\<name>.dll` | The local copy wins | Delete the copy or switch "Files local only" off |
| Package skipped, log line names `enabled = 0` | The package is switched off in its `soviet.mod.ini` | Check the package manifest |
| Status line "Not loaded at the last game start" | The game has not run since the switch | Start the game once |
| No `bridge` lines in the log | `workshop_bridge = 0` in `tesmioloader.ini` or the DLL is missing | Check the TesmioLauncher tick or the installation |

With `log_verbose = 1` every package looked at and not loaded gets its own line.

---

## 📁 File structure

```
tesmioloader\build\
├── plugins\
│   ├── workshop_bridge.dll
│   └── workshop_bridge.ini           (base)
├── user_config\
│   └── workshop_bridge.ini           (overlay: package list and personal values)
└── tesmioloader.ini                  ([plugins] workshop_bridge = 1)
```

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE`. The bridge uses the TesmioLoader API by MaxLegend (Tesmio), https://github.com/MaxLegend/TesmioLoader. The complete source lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/workshop_bridge.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Do I need the bridge with Soviet Mod Loader?**
A: No. Under SML it stays idle. It is for everyone who runs TesmioLoader only.

**Q: Anything to do after a Steam update?**
A: No. The DLL is loaded straight from the updated package at the next start.

**Q: Why does the log credit a package's services to workshop_bridge.dll?**
A: The loader only knows the bridge as a plugin. The `bridge   hook …` line right beside it names the real one.

**Q: Can I keep a package locally, without the subscription?**
A: Yes, via "Files local only" in Republic Mod Manager. The bridge then skips the package automatically.

**Q: I do not use Republic Mod Manager. Does the bridge still work?**
A: Yes. Set `policy = all` or list the packages under `[packages]` by hand.

---

**Last update:** Workshop Bridge 0.2.0  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
