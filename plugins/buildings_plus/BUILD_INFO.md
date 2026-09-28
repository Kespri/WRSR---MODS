# Buildings Plus – build notes

Target: WRSR 1.1.1.9, TesmioLoader API 4. Build: the standard line (`cl /O2 /MT /W3 /EHsc /std:c++17 /LD
... /link kernel32.lib`); exports TsmPluginApiVersion/TsmPluginInit/TsmPluginStart. Self-test: the same file with
`/DBUILDINGS_PLUS_TEST` as an executable, `buildings_plus_test.exe <fresh scratch folder>` builds a fake
game folder and checks generation, replacement rules, material rewrite, stamp, prune, refusal of foreign
folders, the object rename and the id catalog. `buildings_plus_test.exe --run <ini> <game folder> <out
folder>` generates the sections of a real INI from a real game folder into an out folder of your choice,
so a declaration can be checked before it goes into the game (the game folder is only read; the catalog
goes to `<out>\plugins`). History newest first.

## 0.1.10 (2026-09-20)

- Audit round, no new feature. `TsmPluginStart` and `Generate` catch C++ exceptions (`std::exception`
  and `...`) so a `bad_alloc`/`length_error` ends the plugin's own work instead of unwinding into the
  loader; `Generate` is a thin wrapper around `GenerateCore` for that.
- A folder is taken over only when its stamp is ours (`STAMP_MARK`) or Soviet Mod Loader's; any other
  stamp is refused with an error (`OwnStamp`/`SmlStamp`). Before 0.1.10 the mere existence of a stamp
  allowed the takeover and, for a Workshop donor, the deletion of the folder.
- A section that fails `ValidateDecl` (or was already invalid) stays in `keep`, so `prune` never
  deletes the folder of a building over a typo. Only switched-off and removed sections give their
  folder up.
- Rebuild from a Workshop donor: `DeleteTree` is checked, and a stamp with hash 0 is written back
  before any file goes in - a failed write leaves a folder that is rebuilt next start, not one that is
  refused as somebody else's for good. `WriteStamp` returns bool and both callers fail the building on
  a failed stamp; `RepairOwner` warns when the config cannot be written.
- `WriteTextFileAtomic` records the Windows error in `g_ioError` before the temporary file is removed;
  the material, building.ini and catalogue errors print it.
- `RescueReferences` runs its reporting pass even when the first pass fetched nothing; a reference
  the donor item does not have is therefore always reported (`*missing`, the INCOMPLETE hint).
- Bounds for Workshop donors: `SubDirs` skips reparse points (junction loops), `CollectTree` stops at
  `MAX_COPY_DEPTH` 16 and `MAX_COPY_FILES` 4096 with a warning, `CopyAsset` refuses files over
  `MAX_ASSET_FILE` 256 MB.
- Parser: `id` is normalised (`NormalizeId`, leading zeros dropped, also in the catalogue) so two
  spellings cannot claim one folder; more than `MAX_LINES` 512 `line` or `MAX_STRIPS` 64 `strip` keys
  warn at parse time and refuse the section; `LoadCatalog` warns on malformed lines instead of
  dropping them silently; `ResolveGameDir` rejects a truncated executable path.
- `ValidateDecl`: control characters in a line are refused on their own (`HasControlChars`); the old
  condition let a line through when it also carried a quote.
- `FormatV` uses `_vsnprintf_s`. Self-tests: 67 checks (7 new: foreign stamp refused and untouched,
  leading zero, quotes accepted, control character refused, refused section survives prune, gone
  sections still pruned, lone missing reference reported).

## 0.1.9 (2026-09-18)

- **SML's building numbers are adopted.** `SmlCatalogBuildingId` reads `<sml state>\catalog.ini`
  `[building_ids]` (`<mod id>::<section> = <number>`; the state folder follows
  `[loader] state_dir` of `plugins\soviet_mod_loader.ini`, default `soviet_mod_loader` beside the
  loader). In `AssignIds` a section without an explicit id takes SML's number when exactly one
  catalogue entry names the section and no other section declares that id; our own catalogue is
  updated, an older number of ours keeps its folder (prune removes it). `ValidateDecl` accepts an
  explicit id inside 9100000000..9199999999 only when SML's catalogue gives exactly that number to
  the section. A folder that SML wrote for the section carries SML's stamp - the hash never matches
  ours, so it is written again from our declaration with our stamp; SML does the same with its own
  stamp when it runs next (one regeneration per switch, no loss). Why this is safe on SML's side:
  its start check reads our stamp's `section=` up to ` donor=`, which is how we write it.
- Why: switching between Soviet Mod Loader and the Workshop Bridge used to leave two folder sets
  (91xx from SML, 93xx from us) for the same package buildings - every building twice in the build
  menu, and a save knowing only one set.
- Self-tests: 60 checks (six new: adopted number, taken-over folder with our stamp, owner id filled,
  catalogue records SML's numbers, a section SML lists twice keeps a number of ours, explicit id in
  SML's range accepted only with a matching catalogue entry).

## 0.1.8 (2026-09-15)

- The owner repair now covers generated folders this plugin did not write. Soviet Mod Loader's
  built-in buildings component is the original plugin and hard-codes `$OWNER_ID 0`, so every
  building it generates makes the game ask about missing Workshop items each time a saved game
  using it is loaded. `repair_owner_ids` is on by default, because there is nothing the player
  could do about that dialog otherwise.
- `ReplaceOwnerZero` swaps the single zero and leaves every other byte alone, line endings
  included - the file belongs to another generator and nothing is known about the rest of it.
  The match has to begin and end a line, so an `$OWNER_ID 07…` is never taken for a zero.
- Four conditions, all of them: the folder name is a generated id (9000000000..9999999999), the
  folder carries a `tesmioloader.stamp` (a generator wrote it, not the game's own editor), the
  file names no owner at all, and our own id resolved. A folder that already names somebody is
  never touched. The pass runs after prune, so a folder about to go is not written to first.
- The stamp itself is never touched. Soviet Mod Loader validates every folder of its own
  9100000000..9199999999 range against its plan and closes the game when one carries no stamp,
  belongs to another section, or is not in its catalog. It reads the folder name, the stamp and
  its catalog, never `workshopconfig.ini` - which is why writing the owner there is safe. Timing
  fits as well: SML generates during its init, this plugin runs in `TsmPluginStart` afterwards,
  so a folder it has just rewritten is put right in the same launch.
- Five self-tests (repair off, foreign folder repaired byte for byte, stamp untouched, no stamp
  means no repair, an existing owner is kept) and a run against copies of three real SML folders:
  the result is byte-identical to the same fix made by hand.

## 0.1.7 (2026-09-15)

- `workshopconfig.ini` carries the SteamID64 of the player who generated the building instead of
  `$OWNER_ID 0`. The game checks the owner of every Workshop item a saved game uses, and a zero makes
  it report "the Workshop items used in this saved game were not found" on every load - the save still
  loads and the building keeps working, but the dialog comes back every time. Proven in the game:
  same folder, same save, only the number changed by hand, and the warning was gone. The game's own
  editor writes the real id, which is why buildings made there never showed the message.
- Two sources, in this order: `HKCU\Software\Valve\Steam\ActiveProcess`, value `ActiveUser` (the 32 bit
  account id, SteamID64 = 76561197960265728 + it) and, when that is zero because the client is signed
  out, the `"MostRecent" "1"` section of `<SteamPath>\config\loginusers.vdf`. Neither answers: the zero
  stays, because a wrong owner is worse than none. `advapi32` is loaded by hand, so the build line
  keeps its single `kernel32.lib`.
- A folder written by an older version is put right on the next start: when the stamp still matches,
  `RepairOwner` rewrites that one file if it says `$OWNER_ID 0` and the id is known. The owner is
  deliberately NOT part of the stamp hash - a start with Steam signed out would otherwise rewrite
  every folder with a zero. `GENERATOR_VERSION` is unchanged for the same reason.
- Two self-tests: the generated config carries the id, and an up to date folder with a zero has it
  written into it. The test main sets a fixed id, so the checks say the same on every machine.

## 0.1.6 (2026-09-14)

- A `line` without a `$TOKEN` is now a data line of the one above it, the grammar the game's own
  building.ini uses. Before, validation refused every such line ("line N has no $TOKEN") and skipped
  the whole section, which meant no token carrying data could be declared at all: no
  `$CONNECTION_*`, no `$RESOURCE_VISUALIZATION`, no `$VEHICLE_STATION`. `strip` could remove such a
  block since 0.1.1 but nothing could create one.
- Only the first line of a section still has to carry a token: it is emitted after the `$NAME` line,
  so a data line there would attach itself to the name, and a typo would quietly become data. An
  empty line is refused as well. The replacement logic already ignored tokenless lines
  (`if (!mine.empty() && ...)`), and the writer already emitted them verbatim in order, so the block
  lands in the generated file exactly as declared; a blank line separates it from the donor's part.

## 0.1.5 (2026-09-14)

- An explicit `id` between 9100000000 and 9199999999 is rejected with a reason. Soviet Mod Loader
  reserves that range for the buildings it generates itself and checks every folder of it in
  `media_soviet\workshop_wip` against its own plan before the game starts; one it does not know
  terminates the launch. The automatic assignment has always stayed out of the range (9300000000..),
  so this only catches a hand-written number - the way the salt buildings used to be numbered.
- The self-test fixtures moved from 91xx to 94xx for the same reason, and one more section checks
  that a reserved id is refused.

## 0.1.4 (2026-09-14)

- `donor =` may name one building of a Workshop item, written `<item>\<object>` - the spelling
  Vanilla Buildings uses for its targets. Searched in `<library>\steamapps\workshop\content\784150`
  (derived from the game folder) and in `media_soviet\workshop_wip`, so an unpublished building of
  one's own serves as a donor too. Nothing of such a donor ships with the plugin: the copy is made
  on the player's machine from the item he is subscribed to.
- What travels with the clone: the chosen object folder, every loose file of the item, and every
  subfolder that holds no `building.ini` of its own. Asset folders carry whatever name their author
  picked (`mtl`, `Textures`, `materials`, `parking acc`), so the rule asks what is IN a folder and
  never what it is called. The other buildings of a multi-building item stay behind; the item's own
  `workshopconfig.ini` is replaced by one that lists the single declared object.
- The donor's `renderconfig.ini` is taken over verbatim, because it names the donor's own mesh and
  material (`Vokzal_Medium.nmf`, `Material.mtl` inside the object folder are both real cases); only
  an item without one gets a generated one. `building.ini` is the only file that is rewritten.
- Afterwards the clone is measured against the two references that are written as paths -
  renderconfig `MODEL`/`MATERIAL`/`MATERIALEMISSIVE` and `$TEXTURE_MTL` of every copied material -
  and a file that is still missing is fetched from the donor item under the same relative path
  (two passes, so a fetched material can pull its own textures). What the item does not have either
  is named in one warning per file rather than silently left out.
- The stamp hash covers the relative path, size and write time of every file that will be copied, so
  an updated Workshop item regenerates the clone. Reading several MB of meshes at every game start
  would cost far more than it could catch. Regenerating an item donor deletes the stamped folder
  first, because a different donor brings different file names along.
- `GENERATOR_VERSION` 5: every folder is written once more.

## 0.1.3 (2026-09-13)

- `name =` may hold a localisation key instead of a caption. A value with at least one dot and
  nothing but letters, digits, dot, underscore and hyphen is handed to the Localization service; a
  resolved id is written as `$NAME <id>`, everything else stays `$NAME_STR "..."`. Exactly one of
  the two lines is ever written: the game's parser reads both into the same field and which one
  would win is not established. An unresolved key falls back to the part after the last dot and
  warns once, so a missing text never leaves a building without a name.
- Generation moved from `TsmPluginInit` to a new `TsmPluginStart`. A service only exists once every
  plugin's init has run, and buildings_plus loads before localization. Nothing here hooks the game,
  and the folders under `media_soviet\workshop_wip` are only scanned by the game much later.
- GENERATOR_VERSION 4, so every folder is rewritten once. The hash covers the name line that really
  goes into the file, so a key that resolves to a different id regenerates the folder.
- Three self-tests with a stand-in localisation service: a plain name stays literal, a key becomes
  `$NAME` with the id and never both lines, an unknown key falls back and warns once.

## 0.1.2 (2026-09-12)

- First published version. Fork of the buildings plugin from TesmioLoader (MaxLegend, GPL v3): a
  section per building names a donor under `media_soviet\buildings_types` and the building.ini lines
  that differ; at startup the plugin writes the complete Workshop item into
  `media_soviet\workshop_wip\<id>\` (mesh, material with `$TEXTURE_MTL` rewritten to `$TEXTURE
  buildings/...`, emissive material when the donor has one, collision box, fire points, icons,
  workshopconfig.ini, renderconfig.ini, building.ini). The same section format is what Soviet Mod
  Loader reads from a mod's `tesmio\buildings.ini`.
- `id =` is optional. A section without one gets the highest number seen in 9300000000..9399999999
  (catalog, declared ids, every folder under workshop_wip, foreign ones included) plus one; the range
  sits beside Soviet Mod Loader's 9100000000..9199999999. Assignments live in
  `plugins\buildings_plus.ids.ini` (`[ids] <section> = <id>`, lower-case section names) and are never
  removed or reused: a deleted section keeps its line, a renamed section is a new building, an explicit
  `id =` wins and is written into the catalog too; a section whose cataloged number is taken by another
  section's explicit id is renumbered with a warning. The catalog is written before generation, so a
  failed generation does not lose the number. Log line per assignment: `[x] assigned id N`.
- Changes against the original: configuration from `plugins\buildings_plus.ini`, otherwise the INI
  beside the DLL; detail log `logs\tesmioloader.buildings_plus.log` (verbose lines there, results in
  tesmioloader.log); `std::string`/`std::vector` instead of fixed buffers (256 sections, 512 lines,
  4096 characters per line); every value validated (id numeric in 9000000000..9999999999 and unique,
  donor/object plain names, name/desc without quotes, life 1..1000000, every line with a `$TOKEN`,
  strip a single token); files written to a temporary name and moved into place; a renamed object
  removes its stale subfolder; `prune = 1` removes stamped folders of this plugin whose section is gone
  or switched off; the hash also covers the donor's mesh and materials; wide-character file APIs so a
  game folder with non-ASCII characters works; `[buildings]` accepted as the switch section for INIs
  written for the original.
- A dropped donor line takes its data lines with it: the points of a `$CONNECTION_*` and the placement
  block of a `$RESOURCE_VISUALIZATION` (positon, rotation, scale, numstepx, numstept) have no token of
  their own and would otherwise stay behind as orphan lines when their token is replaced or stripped.
  A blank line, a separator line of dashes or a `;` line ends the block. Generator version 3.
- Init returns 0 with an empty registry (the plugin stays listed), 1 only with `enabled = 0` or when
  media_soviet cannot be found.

## Technical notes

- The folder is generated in `TsmPluginInit`: plugins are initialised from DllMain before the game's
  main thread runs, so a folder written then is found when the game lists `workshop_wip`. The loader's
  VFS cannot serve it, because the game enumerates that directory and the VFS redirects opens only.
- `tesmioloader.stamp` marks generated folders; a folder without it is never touched (id collision
  with a real subscription). The stamp carries a hash over the declaration, the generator version and
  the donor's ini/mtl/nmf stamps, so a folder is rewritten only when something changed.
- Replacement rules for donor lines: equal token; `$NAME`/`$NAME_STR`; every `$TYPE_*`; every
  `$STORAGE*` together with `$RESOURCE_VISUALIZATION`; `$PRODUCTION`/`$CONSUMPTION`/
  `$CONSUMPTION_PER_SECOND` as one recipe. The first `$TOKEN` anywhere in a line is the token, as the
  game's parser has no comment syntax. Piles can be declared again after new storages: `line =
  $RESOURCE_VISUALIZATION 0` followed by its five data lines as further `line =` entries (lines are
  emitted verbatim, in order).
- Republic Mod Manager profiles capture `plugins\*.ini`, so the id catalog travels with a profile; the
  uninstaller of the RMM package never removes it.
