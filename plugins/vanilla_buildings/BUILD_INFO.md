# Vanilla Buildings – build notes

Target: WRSR 1.1.1.9, TesmioLoader API 4. Build: the standard line (`cl /O2 /MT /W3 /EHsc /LD
... /link kernel32.lib`); exports TsmPluginApiVersion/TsmPluginInit/TsmPluginStart. History
newest first.

## 0.4.4 (2026-09-13)

- New per-section key `name =`. It replaces whatever `$NAME` or `$NAME_STR` line every target of
  the section carries, so the old line never has to be looked up; a target without exactly one
  name line is rejected and named. A value that looks like a localisation key (at least one dot,
  only letters, digits, dot, underscore, hyphen) is resolved through the Localization service and
  written as `$NAME <id>`, everything else becomes `$NAME_STR "..."`. An unresolved key falls back
  to the part after the last dot and warns once.
- The key is expanded into an ordinary `replace` at overlay-build time, once the target's own
  lines are known. Every existing guarantee therefore applies unchanged: the unique-match rule,
  overlap detection against the section's own rules, and the per-target rejection.
- The Localization service is consumed in the init phase. The Workshop Bridge loads Localization
  before this plugin, so it is on the noticeboard in time. In a local-copy install that loads this
  plugin first, a key falls back with the warning above instead of failing.
- Four self-tests: a literal name, a resolved key, the fallback without the service, and a source
  without a name line, which must be rejected.

## 0.4.3 (2026-09-12)

- One-point connections. The game writes dead ends such as `$CONNECTION_ROAD_DEAD` either inline
  (`$TOKEN x y z`) or as a token line plus one point line; neither the single-line commands (refused
  for every `$CONNECTION` token) nor the two-point connection commands could touch them. `FindConnections`
  now records both forms as blocks with `points = 1` (`pointText` keeps the point as written);
  `IsConnectionTokenName` accepts any `$CONNECTION_*` name for them. Commands: `remove_connection =
  token | point`, `add_connection = token | point` (emitted inline, only an exact duplicate is refused -
  dead ends share their point with a two-point connection by design), `replace_connection = old token |
  point | new token [| new point]` (result written inline; a four-field command is one-point when field
  3 starts with `$`). `Operation.points` carries the form to `ApplyOperations`. Self-tests: inline dead
  end removed, two-line dead end replaced and moved, dead end added before the two-point add, unknown
  point rejected, two-point remove of a dead end rejected.

## 0.4.2 (2026-09-12)

- `replace_connection` accepts six fields: `old token | point 1 | point 2 | new token | new point 1 |
  new point 2`. With the two extra fields the block's point lines are replaced as well (the user
  needed another connection height without remove + add). The token may stay the same then (a plain
  move); with four fields the old rule stands (token must change). Validation: both new points parse,
  differ, and neither touches a point of another existing block or of a planned `add_connection`
  (the moved connection's own old points are ignored); the new points are added to the planned list.
  `AddOperation` takes 4 or 6 fields for this key. Self-tests: moved connection output, same-token
  move accepted, occupied new point rejected.

## 0.4.1 (2026-09-09)

- 2026-09-11, version unchanged: the detail log `tesmioloader.vanilla_buildings.log` is written to `<loader>\logs\`
  (shared `TsmOpenLog` in `src/tesmio_plugin.h`, the folder is created on first use; if that fails the
  file lands next to `tesmioloader.log` as before).
- `insert` accepts as anchor a line that an earlier `add`, `insert` or `replace` of the same
  section produces (`Operation.anchorOp`). Validation: when the anchor has no match in the
  original, the earlier operations are searched (add field 0, insert field 2, replace field 1);
  none found is still "0 matches"; the same line twice at one produced anchor is rejected.
  Produced anchors skip the neighbour and collision checks (they are not original lines).
- `ApplyOperations` now builds a vector of (producer, line) pairs and splices produced-anchor
  inserts afterwards in INI order, so chains work (add sand -> insert 1 after sand: gravel ->
  insert 0 before gravel: road_salt gives sand, road_salt, gravel). The duplicate-position check
  ignores operations without a slot.
- Self-test extended with exactly that chain. Reason: the user's rule set anchored a storage
  line on another storage line his own `add` produced; every target was rejected with
  "anchor has 0 matches" and the plugin stayed inactive.
- In-game test 2026-09-09: confirmed by the user (storages present, insert before and after the anchor work).

## 0.4.0 (2026-09-09)

First published version.

- Temporary changes to building INIs of the game, the DLCs and the Workshop without touching the
  original files: the native building reader is reached through its actual `fopen` call
  (`fopen_s`, `_wfopen`, `_wfopen_s` covered as well, the engine buffer reader stays supported);
  modified copies are written to a temporary folder at every start and only the game's read
  accesses are redirected. Write and update accesses are not redirected.
- Targets below `buildings_types`, `dlcN/buildings` and Workshop ids (looked up in the game's Steam
  library under `steamapps/workshop/content/784150`); several `target = ...` lines per section
  share all patch rules; every target file is checked independently against its unchanged source
  file; at most 256 target files in total. The optional keys `target1..3` additionally check the
  target kind Vanilla, DLC or Workshop.
- Commands: `add`, `replace`, `remove`, `insert = <position> | <anchor> | <line>` (0 before, 1 after
  the anchor; `$COST_RESOURCE_AUTO` only with a `$COST_WORK` anchor, position 1 with the anchor `end`
  rejected), `add_connection`, `replace_connection`, `remove_connection`. Every rejected active
  target gets a precise warning, even with `debug = 0`: section, target, cause, and for rule errors
  the command and the INI line number. A failed target receives no partial change. The first
  successful access to a replacement file is logged as `[overlay-opened]`.
- Configuration: `plugins\vanilla_buildings.ini` when present, otherwise the INI beside the DLL
  (Workshop package under Soviet Mod Loader or the Workshop Bridge); the chosen file is logged
  (`Configuration file:`).
- Self-test: `cl /DVANILLA_BUILDINGS_TEST` builds `vb_selftest.exe <media_soviet> <ini>`; the
  transformed-output check covers the exact `$COST_RESOURCE_AUTO` sequences.
- Package: editor schema for Republic Mod Manager (keyed_sections with `lines` fields, building
  picker, launcher notice, [activity], Troubleshooting card, group notice, multi-line field texts);
  the shipped INI carries the plastics factory example only, switched off.

Verification of the file layer: 98/98 validation and file tests succeeded, including six really
installed Vanilla/DLC/Workshop buildings and unchanged source files; four DLL lifecycle tests
(disabled, active with all four CRT readers, partially failed hook installation, missing mandatory
`fopen` import); precise warnings for an already existing line and a missing file.

Scope: existing saved buildings are not extended by missing storages; that migration is
implemented by Technical Service Storage for depots.
