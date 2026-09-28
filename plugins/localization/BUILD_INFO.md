# Localization – build notes

TesmioLoader plugin (API 4) for WRSR 1.1.1.9, GPL v3. Build: the standard line of the root
`build.bat` (`cl /O2 /MT /W3 /EHsc /std:c++17 /LD`, kernel32.lib) compiles `localization.cpp`;
it includes `src/tesmio_plugin.h` and provides the `localization` service (key -> text id in the
reserved range 2,000,000..2,999,999). Output: extended `soviet<Language>.btf` overlays in the
loader's VFS (`<loader>\vfs\media_soviet`), removed again on a start with `enabled = 0`.
User documentation: README_DE.md / README_EN.md. History newest first.

## 0.4.0 (2026-09-10)

- 2026-09-11, version unchanged: the detail log `tesmioloader.localization.log` is written to `<loader>\logs\`
  (shared `TsmOpenLog` in `src/tesmio_plugin.h`, the folder is created on first use; if that fails the
  file lands next to `tesmioloader.log` as before).
First published version.

- Configuration: `plugins\localization.ini` when present, otherwise the INI beside the DLL
  (Workshop package under Soviet Mod Loader or the Workshop Bridge). Text packs are loaded from
  the folder `localization` beside the DLL first, then from `plugins\localization`; both paths are
  logged.
- Pack folders of the same name are merged instead of shadowed: `MergePack` lays a local folder
  over the shipped one key by key (local `localization.ini` decides namespace, fallback and
  missingText; local language files add or replace keys; shipped-only languages stay). The
  fallback/text checks run in `FinishPack` after the merge, so a local folder that carries only a
  new language plus its `localization.ini` is valid. Log: `Pack '<folder>': local folder merged
  over the shipped one (<n> keys added, <m> replaced, <k> local language file(s))`; a namespace
  mismatch between the two is a WARN, the local one applies. Reason: Republic Mod Manager edits
  the research_expansion pack in `plugins\localization`; a package update by the author must still
  reach players with local edits.
- Shipped text packs: `research_expansion`, `technical_service_storage`, `tesmio_lang`.
- Package: Republic Mod Manager schema with the tabs General (notices, guides, troubleshooting) and
  Plugin ("Apply extension" with the switch-off order, text packs as a folder list with a short text
  per pack, needs Republic Mod Manager 0.4.56). The research_expansion package requires
  `tesmio.localization >= 0.4.0`.
- In-game test 2026-09-10 (user): log "Pack 'research_expansion': local folder merged over the
  shipped one (4 keys added, 4 replaced, 2 local language file(s))", 3 packs / 14 keys ready; the
  local clay_study texts showed in the game.
