# Vehicle Materials – build notes

TesmioLoader plugin (API 4) for WRSR 1.1.1.9, GPL v3. Build: the standard line of the root
`build.bat` (`cl /O2 /MT /W3 /EHsc /LD`, kernel32.lib) compiles `vehicle_materials.cpp`; it
includes `src/tesmio_plugin.h` and `my_plugins/tesmio_config.h` (base INI beside the DLL or in
`plugins\`, personal overlay `user_config\vehicle_materials.ini`). User documentation: README_DE.md / README_EN.md. History newest first.

## 0.4.0 (2026-09-08)

- 2026-09-11, version unchanged: the detail log `tesmioloader.vehicle_materials.log` is written to `<loader>\logs\`
  (shared `TsmOpenLog` in `src/tesmio_plugin.h`, the folder is created on first use; if that fails the
  file lands next to `tesmioloader.log` as before).
First published version.

- Additional production materials per vehicle class (road, rail, ship, airplane) registered
  through the Resources plugin; coefficients multiplied with the vehicle's internal production
  value; automatic vehicle-type mapping (type 1 road, 6 ship, 7 airplane, others rail) with
  `[mapping]` overrides; strict configuration validation that rejects the whole plugin on any
  error and leaves the game untouched.
- Configuration read as base plus personal overlay through `tesmio_config.h`; the base is
  `plugins\vehicle_materials.ini` when present, otherwise the INI beside the DLL; the overlay is
  merged key by key and validated with the same strict reader. The Workshop package declares
  `user_overlay = 1` and `local_copy = 1`.
- Package texts (RMM schema, de/en) in player style; the storage-line notice names
  `$STORAGE_IMPORT_SPECIAL`; schema fallbacks are English. READMEs on the common template.
