# Resources Button Fix – build notes

Target: WRSR 1.1.1.9, TesmioLoader API 4. Build: MSVC x64, `/std:c++17 /O2 /MT /W3 /EHsc /LD`,
kernel32.lib only. Workshop package: `My Plugins\resources_button_fix` (local_copy). History newest first.

## 0.4.0 (2026-09-09)

- 2026-09-11, version unchanged: the detail log `tesmioloader.resources_button_fix.log` is written to `<loader>\logs\`
  (shared `TsmOpenLog` in `src/tesmio_plugin.h`, the folder is created on first use; if that fails the
  file lands next to `tesmioloader.log` as before).
First published version.

- Compact paint/erase button grids in the terrain editor's Resources and Rocks/Gravel windows, so
  many plugin resources fit; grid geometry, growing window and the position of the round red
  clear-all button are configurable per window; changed values are picked up live when the window
  is reopened, only the plugin switch needs a full restart.
- Configuration: `plugins\resources_button_fix.ini` when present, otherwise the INI beside the DLL
  (Workshop package under Soviet Mod Loader or the Workshop Bridge); the chosen file is logged at
  start and the live reload follows it.
- Package: Republic Mod Manager texts (57 keys de/en plus the English schema fallbacks) in player
  style; `[launcher] notice` (restart only for the plugin switch, live reload when the window is
  reopened); card "Troubleshooting" with the detailed log; ON/OFF wording. READMEs on the common
  template.
