# ❄️ Weather Roads 0.3.3

**TesmioLoader plugin for road snow, melting and protection after plowing**

Controls in *Workers & Resources: Soviet Republic* 1.1.1.9 how fast snow builds up on roads, how fast it melts naturally, and gives plowed roads a temporary protection: a protection phase in game minutes, then a weaker "salt phase" in game hours. With Technical Service Storage the loaded grit decides how strong that protection is. Snow and melting work without Technical Services and without a snowplow.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Features](#-features)
- [Installation](#-installation)
- [Republic Mod Manager](#-republic-mod-manager)
- [Configuration](#-configuration)
- [Snow, melting and appearance](#-snow-melting-and-appearance)
- [Protection after plowing](#-protection-after-plowing)
- [Overlay](#-overlay)
- [Saving and loading](#-saving-and-loading)
- [Value ranges](#-value-ranges)
- [Compatibility](#-compatibility)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Windows x64
- WRSR 1.1.1.9 (the identities of `SOVIET64.exe` and `C3DDLL64.dll` and the hook signatures are verified; other builds are refused)
- TesmioLoader API 4
- **Optional:** Technical Service Storage with its grit spreader service for material-dependent protection and dry-plowing detection; without it every plowing counts with strength 1.00
- **No** Localization plugin needed; the overlay has built-in German and English texts

### In three steps
1. **Choose one installation method** (see below) and enable the plugin.
2. **Check the values:** the supplied settings are tuned (snow 30 %, melting 45 %, protection phase 240 game minutes, salt phase 24 game hours). Overlay and detailed log stay off for normal play.
3. **Restart the game completely.** `logs\tesmioloader.weather_roads.log` names version, configuration path, signature check, active components and the status of the grit spreader service.

---

## ✨ Features

### 🎯 Core function
- ✅ Scaled and optionally gradual build-up of the internal road snow with a cap per snow burst
- ✅ Independently adjustable natural snow reduction
- ✅ Strong protection phase after plowing, then a weaker salt phase with a configurable factor
- ✅ Material-dependent effect through the grit spreader service of Technical Service Storage; treatment kept on dry plowing when wanted
- ✅ Stronger grit overrides weaker grit, never the other way round
- ✅ Visual snow correction on tracked road areas
- ✅ Protection and road look are saved with the savegame and restored on load, without restarting the protection period
- ✅ Optional diagnostic overlay (F10) and detailed event logs
- ✅ No VFS overrides, no change to game or save files; unknown game builds are refused before any hook is installed

### 🆕 New in 0.3.3
- ✅ **New defaults for the snow build-up:** `accumulation_multiplier = 0.35` (was 0.30) and `maximum_accumulation_per_burst = 95` (was 50). With the old values too little snow stayed on the roads after short snowfalls. Your own values in `user_config` are untouched.

### 🆕 New in 0.3.2
- ✅ **Weather field read correctly:** In winter the game rolls a value from 0 to 7 at the end of every weather period (0 to 2 on climate type 3). Only 1 means snowfall, every other value means "no snow". Until now the plugin accepted 0 to 2 only and refused the weather snapshot on 5 out of 8 rolls (log line "weather tick unavailable" in the middle of play, overlay "no world data", "Stop with the weather" without effect). Now 1 = snow and everything else = no snow; overlay and log lines name the roll.
- ✅ **"Stop with the weather"** now triggers on any roll other than 1, not only on 0.

### 🆕 New in 0.3.1
- ✅ **Snow stops with the weather:** `release_follows_weather = 1` (default) drops the road snow still waiting in the queue as soon as the game reports no more precipitation. Before, the gradual build-up queue could run on for about half a minute after the snowfall ended and roads turned white under a clear sky. The weather itself is untouched; when the weather snapshot cannot be read, the plugin behaves as before.

### 🆕 New in 0.3.0
- ✅ Every text in Republic Mod Manager and in this guide rewritten: shorter, in players' language, switches with ON and OFF instead of 1 and 0, line breaks in longer explanations. The note on the interplay with Technical Service Storage now sits in the "Notes" card.
- ✅ **INI beside the DLL:** when `plugins\weather_roads.ini` is missing, the DLL reads the INI from its own folder, i.e. from the Workshop package under Soviet Mod Loader or the Workshop Bridge. The chosen path is logged as `configuration file:`.
- ✅ Schema for Republic Mod Manager in the package: five tabs with every setting, German and English.

---

## 💾 Installation

Choose **one** of the four methods. The same DLL must never be loaded twice.

---

### Method 1️⃣: Classic TesmioLoader

```
1. Copy weather_roads.dll and weather_roads.ini from hooks\
   → tesmioloader\build\plugins\

2. Enable weather_roads in the TesmioLauncher
3. Restart the game completely
```

---

### Method 2️⃣: Soviet Mod Loader (SML)

```
1. Subscribe to the Workshop item – SML reads subscribed packages by itself
2. SML loads the DLL through soviet.mod.ini from the package, the INI sits beside it
3. Remove or disable a local weather_roads.dll in plugins\ first
```

---

### Method 3️⃣: Workshop Bridge (without SML)

```
1. Select the package in Republic Mod Manager
2. Switch "Plugin enabled" on
3. workshop_bridge loads the DLL straight from the package; it finds the INI beside itself
4. Steam updates apply immediately
```

---

### Method 4️⃣: Republic Mod Manager with "Files local only"

```
1. Select the package in Republic Mod Manager, adjust the settings, Save
2. Tab "General", card "Notes": switch "Files local only" on
3. Confirmation with file list → DLL and INI are copied to
   tesmioloader\build\plugins\ when saving
4. The Workshop Bridge skips the package automatically afterwards
```

**"Files local only" in detail**
- For everyone who wants to keep using the plugin without a Steam subscription
- With local files Steam updates apply only after saving again (yellow "Update" mark)
- Switching it off removes only the files Republic Mod Manager copied

---

## 🧰 Republic Mod Manager

The package contains a presentation schema in the `config` folder. Republic Mod Manager shows Weather Roads in five tabs, German and English:

- **General:** notes, "Files local only", a button for this guide; plugin, save road protection, detailed events
- **Snow and melting:** snow build-up, natural melting, appearance of plowed areas
- **Snowplowing:** protection phase, salt phase, factor, dry plowing
- **Overlay:** window, language, key, position and appearance
- **Advanced:** timing and batching of the gradual build-up

Republic Mod Manager writes the effective file `plugins\weather_roads.ini` with every key; the INI in the package stays unchanged.

---

## ⚙️ Configuration

### Main file: `weather_roads.ini`

The DLL reads in this order:
- **First:** `tesmioloader\build\plugins\weather_roads.ini` when it exists (classic installation, "Files local only" or the effective INI written by Republic Mod Manager)
- **Otherwise:** the INI beside the DLL, in the package `hooks\weather_roads.ini` (Soviet Mod Loader, Workshop Bridge)

**UTF-8 without BOM.** `;` only on its own line, no comments after values. Decimals with a dot (`0.30`), switches as `0` or `1`. Changes apply after a complete restart.

Known numeric values are checked for valid numbers and ranges; invalid values produce an `invalid [section]` message and use the compiled fallback. Unknown or duplicate keys are **not** validated comprehensively: copy names exactly and never enter a setting twice.

⚠️ **Keep the INI installed.** Missing keys use the compiled fallbacks of the DLL, and some of them differ from the supplied file: without `[overlay] enabled` the fallback is `1` and the overlay appears. To switch the plugin off, set `[general] enabled = 0` and restart.

### Sections

| Section | Purpose | Supplied values |
|---|---|---|
| `[general]` | whole plugin | `enabled = 1` |
| `[snow]` | snow build-up | `enabled = 1`, multiplier `0.35`, cap `95`, gradual `1` |
| `[melting]` | natural snow reduction | `enabled = 1`, multiplier `0.45` |
| `[visual_snow]` | appearance of tracked areas | levels `0`, shader range `0.85`, curve `1.00` |
| `[snowplow]` | protection after plowing | `enabled = 1`, `240.00` game minutes, then `24.00` game hours, factor `0.50`, dry plowing keeps `1` |
| `[persistence]` | save and load extra data | `enabled = 1` |
| `[overlay]` | information window | `enabled = 0`, `auto`, `top_right`, 20/80, width 350, opacity 220, 250 ms, key 121 (F10), foreground `0` |
| `[logging]` | detailed events and mirroring | both `0` |
| `[advanced]` | timing and batching | `1000` ms, `1` unit, `125` ms, `10` units |

Further keys the DLL knows but the INI does not contain are listed under [Value ranges](#-value-ranges).

---

## 🌨️ Snow, melting and appearance

- `accumulation_multiplier` scales positive internal snow increments; `maximum_accumulation_per_burst` caps the sum of one grouped burst (`0` removes only this cap). `95` does not mean every snowfall brings 95 units.
- `release_follows_weather = 1` cuts that spread short as soon as the game's weather roll is no longer 1 (snowfall); the rest of the queue is dropped (event "gradual snow release stopped with the weather" in the detail log).
- `gradual_accumulation = 1` spreads verified weather increments over small steps. The intervals in `[advanced]` are real milliseconds; the release also needs the game time to advance and pauses in-game.
- `[melting]` scales how fast snow melts away on its own. `0.00` stops only the melting, not clearing by vehicles and not full resets of the snow cover.
- `[visual_snow]` only decides how plowed roads look, not the snow of the whole map and not the snow amount itself. "Protection after plowing" has to be switched on for it.

---

## 🚜 Protection after plowing

The material strength `S` comes from the grit spreader service of Technical Service Storage (list `[grit_materials]`). It decides how much snow is prevented, **not the duration**:

| Phase | Remaining share of new snow | Example `S = 0.50` |
|---|---|---|
| protection phase (`protection_minutes`, game minutes) | `1 - S` | 50 % |
| salt phase (`salt_effect_hours`, game hours) with factor `M` | `1 - S × (1 - M)` | with `M = 0.50`: 75 % |

The factors apply on top of the snow scaling; rounding and the burst cap influence the individual steps.

- A weaker material neither reduces nor refreshes a stronger active treatment; an equal or stronger one refreshes it.
- `dry_plowing_preserves_treatment = 1` keeps an existing treatment and its age on dry plowing **detected by the service**; an untreated road gains no protection from it. With `0` dry plowing removes the treatment.
- Without a compatible service, or for a clearing that cannot be matched, strength `1.00` applies. That is no confirmation that grit was loaded; check the `grit-spreader` status in the log.
- "Salt" only names the effect phase. Weather Roads neither creates nor consumes any resource.

---

## 🪟 Overlay

The overlay ships **switched off**. To show it, set `[overlay] enabled = 1` and restart.

- `language = auto`: German on a German Windows interface, otherwise English; **not** the game language. `de` or `en` fix the language.
- `toggle_key = 121`: F10 shows or hides the window; `0` disables the key. The footer still names F10.
- `foreground_only = 1` shows the window only while the game is in front.
- The window takes no mouse clicks. Without a found game window a monitor fallback position is used.

The overlay shows samples and diagnostic counters, not statistics of every road. Its failure does not switch the weather functions off.

---

## 💾 Saving and loading

With persistence enabled, `tesmioloader.weather_roads.protection.bin` appears in the savegame folder with material strength, original treatment time, rounding remainders and known visual road-snow data. The file is written through a temporary file after the final native file close succeeds; native save files are never touched.

On load the plugin checks whether the sidecar file belongs to exactly this savegame; if it does not, it is discarded. Roads are matched by their position; unclear cases are skipped.

- Loading does not restart the protection period; higher INI durations do not extend saved treatments afterwards.
- Without an extra file the savegame loads normally (`status=no-sidecar`); earlier treatments cannot be restored then.
- Damaged or mismatching extra data is discarded. Very old files from 0.2.7 can be rejected because of the earlier save timing; treat again and save.
- `[persistence] enabled = 0` stops reading and writing; existing files stay.
- When transferring a savegame take the whole folder. Steam Cloud and ZIP autosaves are not confirmed.

---

## 📏 Value ranges

| Quantity | Limit |
|---|---|
| `accumulation_multiplier`, `reduction_multiplier` | 0.00 to 10.00 |
| `maximum_accumulation_per_burst` | 0 to 255 |
| `snow_levels` / `shader_range` / `visual_curve` | 0–8 / 0.00–1.00 / 0.10–5.00 |
| `protection_minutes` / `salt_effect_hours` | 0.00–1440.00 game minutes / 0.00–168.00 game hours |
| `salt_accumulation_multiplier` | 0.00 to 1.00 |
| overlay `offset_x`, `offset_y` / `window_width` / `opacity` | 0–4000 / 280–600 / 80–255 |
| `update_interval_ms` / `toggle_key` | 100–2000 ms / 0–255 |
| `burst_reset_after_ms` / `gradual_step_units` / `gradual_step_interval_ms` / `visual_update_batch_units` | 100–10000 ms / 1–32 / 16–5000 ms / 1–255 |

Further supported keys with compiled fallbacks (not in the supplied INI, change only with a reason):

| Section | Key | Fallback | Range |
|---|---|---|---|
| `[logging]` | `weather_sample_interval_ms` | 500 | 100–60000 ms |
| `[logging]` | `weather_heartbeat_seconds` / `road_heartbeat_seconds` / `road_state_heartbeat_seconds` / `mask_sample_heartbeat_seconds` | 60 / 30 / 30 / 30 | 0–3600 s (`0` suppresses the message) |
| `[logging]` | `road_state_sample_interval_ms` / `mask_batch_interval_ms` / `mask_sample_interval_ms` | 500 / 5000 / 2000 | 50–60000 / 500–60000 / 250–60000 ms |
| `[safety]` | `road_state_tracking_seconds` | 900 | 10–86400 s |
| `[safety]` | `maximum_tracked_roads` / `maximum_road_bytes` | 16 / 16384 | 1–32 / 16–1048576 |
| `[safety]` | `maximum_tracked_plow_points` / `maximum_tracked_mask_points` | 8192 / 8 | 128–8192 / 1–16 |

---

## 💾 Compatibility

### Savegames
Without the plugin the savegame loads normally; the extra file is then not applied. Extra files already written are not deleted when the plugin is switched off.

### Other plugins
- **Technical Service Storage** provides material strength and dry plowing; without it strength 1.00 applies.
- The weather call chain accounts for the hook of `daynight`; other combinations are not verified.

### Version compatibility
- **0.3.3:** defaults `accumulation_multiplier` 0.35 and `maximum_accumulation_per_burst` 95; otherwise unchanged
- **0.3.2:** weather field read as a roll 0 to 7, only 1 = snowfall; names in the info window and the log; otherwise unchanged
- **0.3.1:** `release_follows_weather` (default 1), otherwise unchanged
- **0.3.0:** first published version

---

## ⚙️ Troubleshooting

### Common problems

| Problem | Cause | Solution |
|---|---|---|
| `unsupported SOVIET64.exe` / `unsupported C3DDLL64.dll` | unsupported game build | use the matching plugin version |
| `invalid [section] …` | value invalid or out of range | check the value; the fallback was used |
| `grit-spreader service unavailable` | Technical Service Storage missing | strength 1.00 active; check plugin and service |
| `protection-persistence=unavailable` | save hook missing or refused | check earlier warnings |
| `rejected-invalid-or-stale-sidecar` | extra file does not match the savegame | use the matching backup or rebuild the protection |
| `atomic sidecar write failed` | write access, disk space | check permissions and free space |
| `detail-open` / `detail-write` / `wait-failed` | log or worker disturbed | check the main log, restart the game |
| Overlay missing | `enabled = 0`, key, foreground rule | check the setting and the window-creation message |
| Details missing | `detailed_events = 0` | set `1`; mirroring alone is not enough |

### Logging

Messages go to `tesmioloader.log` (mirrored status, warning and error messages) and `logs\tesmioloader.weather_roads.log` (plugin log, with `detailed_events = 1` also EVENT messages). Switch the details off again after troubleshooting.
- In **Republic Mod Manager** the document icon at the bottom of the plugin bar opens the log view with filter and sender.

Search for:
- `configuration file` → which INI was chosen
- `protection persistence save`, `load`, `restore-road`, `restore-visual` → saving and loading; `stage=after-final-native-close` is the save point, `native_save_fingerprints=matched` confirms matching native files
- `grit-spreader` → connection to Technical Service Storage

---

## 📦 File structure

**Workshop package** (Steam subscription, SML, Workshop Bridge)
```
weather_roads\
├── hooks\
│   ├── weather_roads.dll           (plugin)
│   └── weather_roads.ini           (original INI with explanations)
├── config\                         (schema for Republic Mod Manager)
│   ├── weather_roads.launcher.ini
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
│   ├── weather_roads.dll
│   └── weather_roads.ini           (effective INI)
├── logs\
│   └── tesmioloader.weather_roads.log
└── tesmioloader.log
```

---

## 📜 Licence & credits

**GNU GPL v3**, see `LICENSE` in the package. The plugin contains no third-party code; the loader SDK header comes from the TesmioLoader by MaxLegend (GPL v3). The complete source lives at https://github.com/Kespri/WRSR-MODS/tree/main/plugins/weather_roads. The service header `grit_spreader_api.h` connects it with Technical Service Storage.

**Attention, comrade:** this plugin was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

---

## ❓ FAQ

**Q: Do I need Technical Service Storage?**
A: No. Snow, melting and the protection after plowing work without it; only the material strength and dry plowing come from there.

**Q: Why does the overlay show up although I set nothing?**
A: The INI is missing or has no `[overlay] enabled` key; the compiled fallback is 1. The supplied INI explicitly sets 0.

**Q: Is the protection renewed on load?**
A: No. Age and remaining strength are taken from the extra file.

**Q: How do I see that the plugin runs?**
A: In `logs\tesmioloader.weather_roads.log`: version, `configuration file`, signature check and the list of active components.

**Q: Can I edit the INI by hand?**
A: Yes, following the rules in [Configuration](#-configuration). Republic Mod Manager offers the same values with descriptions and range checks.

---

**Last update:** Weather Roads 0.3.3  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
