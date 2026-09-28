# Workshop Bridge – build notes

TesmioLoader API 4 (min 3), no game addresses. Sources: `workshop_bridge.cpp`, includes
`..\..\src\tesmio_plugin.h` and `..\tesmio_config.h`. Build: the standard line in the root `build.bat`
(`cl /O2 /MT /W3 /EHsc /LD ... /link kernel32.lib`). Exports: TsmPluginApiVersion, TsmPluginInit,
TsmPluginStart. Runtime files: `workshop_bridge.ini` beside the DLL (base), `user_config\workshop_bridge.ini`
(overlay; Republic Mod Manager writes the `[packages]` list there). Log prefix `bridge   ` in
tesmioloader.log, no detail log. Users get the bridge inside the Republic Mod Manager package. User documentation:
README_DE.md / README_EN.md. History newest first.

## 0.2.0 (2026-09-10)

First published version.

- Walks the subscribed packages under `workshop_root`, loads the hook DLLs of the released packages
  with the bridge's own host table, Init inside Init and Start inside Start, idle under Soviet Mod
  Loader, local `plugins\<name>.dll` wins, no double load, no path escape.
- `[bridge] enabled / policy / workshop_root / log_verbose`, `[packages] <folder> = 0|1`; the
  package list is kept by Republic Mod Manager through each package's "Plugin active" switch.
- Installer: `-Replace` overwrites the DLL only; an existing `workshop_bridge.ini` is the player's
  base configuration and stays unless `-ReplaceIni` is given; refuses to run while the game, the
  launcher or rmm.exe is open.
- Republic Mod Manager: local schema texts (settings_schemas, de/en) in player style, a card notice
  about the package list, a "Clean up now" action for stale `[packages]` lines and a red box when the
  bridge's workshop folder differs from the manager's.
- Offline test suite: seven scenarios, 51 checks. In-game test 2026-09-10 (user): 10 hooks loaded
  from the test folder.
