# Research Expansion – build notes

TesmioLoader plugin (API 4) for WRSR 1.1.1.9, GPL v3. Build: the standard line of the root
`build.bat` (`cl /O2 /MT /W3 /EHsc /std:c++17 /LD`, kernel32.lib) compiles `research_expansion.cpp`;
it includes `src/tesmio_plugin.h` and consumes the `localization` service of the Localization
plugin. Output: a generated `research.ini` plus one `<id>.png` per new research in the loader's VFS
(`<loader>\vfs\media_soviet\research`). User documentation: README_DE.md / README_EN.md.
History newest first.

## 0.4.2 (2026-09-17)

- Log wording only: the startup line builds its version marker from `PLUGIN_VERSION`, so it always
  names the running build instead of a fixed number. Version comments inside the source and the
  shipped INI are gone as well. No logic change.

## 0.4.1 (2026-09-11)

- 2026-09-11, version unchanged: the detail log `tesmioloader.research_expansion.log` is written to `<loader>\logs\`
  (shared `TsmOpenLog` in `src/tesmio_plugin.h`, the folder is created on first use; if that fails the
  file lands next to `tesmioloader.log` as before).
- Log wording only: the WARN for the unused keys `icon_folder` / `noimage_name` no longer refers to
  an earlier build. No other change.

## 0.4.0 (2026-09-10)

First published version.

- Configuration: `plugins\research_expansion.ini` when present, otherwise the INI beside the DLL
  (Workshop package under Soviet Mod Loader or the Workshop Bridge); the chosen path is logged.
- New research as free `$RESEARCH` blocks or as `[research:<id>]` sections (the INI form written by
  Republic Mod Manager): `ValidateGeneralConfigLayout` collects the sections (`ResearchSection`,
  keys enabled, type, cost, name, desc, requires, unlock, line), `ExpandResearchSections` turns the
  enabled ones into `NewBlock` line lists after the free blocks, each line carrying the INI line of
  its key, so `ValidateNewBlock` / `ValidateAllNewBlocks` check both forms with the same rules and
  messages. `requires = <dependency> | before/after/normal | <anchor>` becomes `+<dependency>` plus
  `@before_<anchor>` / `@after_<anchor>`. Duplicate ids between a free block and a section are
  rejected (`research-duplicate`); unknown keys, bad values and repeated single keys fail closed.
- Short text keys: `name` / `desc` in `[research:]` are optional and may be a bare word; a value
  without a dot is completed to `research_expansion.<value>.name` / `.desc` (the namespace of the
  plugin's own text pack), a missing key uses the research id, a value with dots is emitted
  unchanged. Republic Mod Manager shows the fields as `[<word>].[name]` with the id as placeholder.
- `[modify:<id>]` accepts `cost = <points>` (once, positive integer): `ApplyVanillaModifications`
  turns it into a `replace` of the block's single `$COST` line before the listed operations (rule
  `modify-cost` when the line is missing or ambiguous). Sections with only a `cost` key count as
  active.
- Icons: the VFS research folder is the only icon store. `PlanIcons`: an existing `<id>.png` there
  is kept (validated as 128 x 128 PNG, an unusable one fails closed with `icon-invalid`); a missing
  one is seeded from `<id>.png` in `research_expansion\icons` beside the DLL, else created from
  `noimage.png` (`icon-fallback` WARN). `noimage.png` candidates in order: `research_expansion\icons`
  beside the DLL, `plugins\research_expansion\noimage.png`, `plugins\research_expansion\icons\noimage.png`.
  The keys `icon_folder` and `noimage_name` are not used; they stay accepted by the strict INI
  layout check and log one `legacy-key` WARN when set to something else than the defaults. Log
  lines: `Icon store: <path>`, `Fallback icon: <path or reason>`, `Icon for <id> kept from the VFS
  research folder`, `Icon for <id> created from noimage.png|the package icon`.
- Package: Republic Mod Manager tabs New research, Localization and Vanilla edits (research and
  line pickers, original block view, cost as a number field, reference checks). Depends on
  `tesmio.localization >= 0.4.0`.
- In-game test 2026-09-10 (user): `[research:clay_study]` with `requires = faculty_geology | before
  | uranium_study`, `name = clay_study`, no desc; log "1 new research entr(y/ies) (1 of them
  [research:] sections)", "Icon for clay_study created from noimage.png"; name, description and the
  position before uranium_study confirmed in the game; a vanilla cost change confirmed the same day.
