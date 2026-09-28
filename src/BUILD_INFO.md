# tesmioloader.dll – build notes

The loader itself: MaxLegend's TesmioLoader b0.3.6 (GPL v3, https://github.com/MaxLegend/TesmioLoader)
with the corrections below. Target: WRSR 1.1.1.9 (SOVIET64.exe PE stamp 0x6A3EB6AD), plugin
API 4, accepts plugins built for API 3..4. Build: the loader line of the root `build.bat`
(`cl /O2 /MT /W3 /EHsc /LD`, kernel32.lib; user32 is pulled in by a pragma for the one message
box). Offline self-test: compile the same file with `/DTESMIO_SELFTEST` as an EXE and run it
with a fresh scratch folder - 35 checks over the ini reader, the VFS path rule, the plugin
order, the service noticeboard, the save manifest and the repeat counter.

Compatibility promise: the host table, the API version, the export names, the two-phase
Init/Start rule, the `[plugins]` switches and every log line other tools read (`plugin <name>
<version> from <dll>`, `<name>  idle - `, `plugin N loaded`, `--- shutdown`, the service lines)
are unchanged. A plugin built for b0.3.6 runs here; a plugin built here runs on b0.3.6.
Soviet Mod Loader is a plugin of this loader and sees no difference. Republic Mod Manager
ships this DLL in its Workshop package and installs it over the upstream one.

## b0.3.6-rmm.1 (2026-09-20)

- **Save manifest from the live services.** `tesmioloader.save.ini` used to list resources
  only when a plugin named `resources` was loaded, deposits only for `deposits`, buildings
  only from `plugins\buildings.ini` - with Resources Plus, Deposits Plus, Buildings Plus or
  Soviet Mod Loader it was empty, and the load check said "resources.dll is off". Now the
  writer asks the `resources` v1 and `deposits` v2 services (the original plugins, the forks
  and SML all publish them) and lists generated buildings as the stamped, numbered folders
  under every `out` dir (`plugins\buildings.ini` and `plugins\buildings_plus.ini` are read for
  it); the checker asks the same services (`indexOf`, `get`) and falls back to the provider's
  own ini (`resources_plus.ini` before `resources.ini`, `deposits_plus.ini` before
  `deposits.ini`) only when no service is published. Relative save paths resolve the game
  folder to the working directory instead of skipping the buildings check. The manifest
  also records `[tesmioloader] loader`.
- **Withdrawn services.** A plugin that published a service in Init and then declined (returned
  non-zero) was unloaded with its interfaces still on the noticeboard, so `consume()` handed
  out pointers into a freed DLL. `DropServices` removes them before `FreeLibrary`.
- **Non-ASCII install paths.** `g_vfsRoot` came from `GetModuleFileNameA` and was re-decoded as
  UTF-8 in `VfsResolveW`, so every wide file open (textures) missed the VFS when the game
  folder held an umlaut or a Cyrillic letter. The loader now keeps the wide paths
  (`g_baseDirW`, `g_vfsRootW`) and derives the ANSI copies for the plugin table with CP_ACP.
- **Deterministic plugin order.** File names are collected, sorted case-insensitively and then
  loaded; `FindFirstFile` order is only alphabetical on NTFS.
- **BOM-tolerant ini.** `tesmioloader.ini` is read by the loader's own reader (`LoadIniText`,
  `IniReadValue`, `IniInt`): a UTF-8 BOM no longer hides the `[plugins]` section. The profile
  API stays behind `configInt`/`configString` for plugins, unchanged.
- **VFS root is a root.** A relative path with a `..` step is never redirected.
- **Read-only after writing.** Inline-hook trampolines are switched to PAGE_EXECUTE_READ and the
  menu version string to PAGE_READONLY once written; `allocNear` stays RWX because plugins
  write their own code into it. `installInlineHook` also checks that the target is readable
  before comparing the prologue.
- **Log.** Header with loader version, build time, API range, date, the game executable, its
  image size and PE stamp (with a note when it is not the 1.1.1.9 build the addresses were taken
  from), the base folder; a `summary` line before `--- hooks installed ---` with hooks placed
  and refused, plugins and services; the crash report lists the loaded modules on the first
  report; the previous run survives as `logs\tesmioloader.previous.log`.
- **A log worth reading.** One ordinary start wrote 952 lines, 104 KB, and most of it was the
  same thing over and over: the VFS naming a file at every open (the engine opens one `.nmf`
  eight times, 116 lines for 26 files) and the game complaining about every folder it walked
  (47 `Failed to open ...` in a row, all about other people's Workshop items). Three changes:
  - the VFS names a redirected file **once**, and `ReportRepeats` writes the totals at shutdown;
  - a repeating message of the game is written `game_repeat_limit` times (default 3) and then
    counted. `Shape()` decides what "the same message" means: a word holding a path separator
    becomes `<path>`, a word of digits becomes `#`, the wording is what is left. The tally is in
    the log at shutdown (`repeats  47 x Failed to open <path>`), so nothing disappears silently.
    A message that leaves **no** wording behind is never counted together: the game prints bare
    numbers (`1`, `2`, `3`) as separate warnings, and grouping those would hide nine distinct
    messages behind a shape that reads `#`. Measured in the first run with a real game;
  - `log_game` is a level, not a switch: 0 off, 1 errors, **2 errors and warnings (the new
    default)**, 3 everything. A `log_game = 1` in an existing ini now means errors only - that is
    the point, and the header line says which level is running.
  The game's own `log.html` is untouched: every call is still forwarded to the engine.
- Limits raised: 64 plugins, 64 services, 128 files considered in `plugins\`.
- `CheckSaveManifest` returns the number of missing items (for the self-test); the message box
  can be silenced for tests only (`g_noMessageBox`). The box now names Republic Mod Manager.
