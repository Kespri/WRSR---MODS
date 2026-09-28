# Deposits Plus – build notes

Fork of the `deposits` plugin from the TesmioLoader by MaxLegend (Tesmio), GPL v3. Target:
WRSR 1.1.1.9 (SOVIET64.exe SHA-256 `296644A9F207D609031FC2AE73FED2DCB34619A1D55A35D1C7B51965CE6841B8`),
TesmioLoader API 4. Build: the standard line of the root `build.bat` (`cl /O2 /MT /W3 /EHsc /LD`,
kernel32.lib) compiles `deposits_plus.cpp`; the headers `deposit_generation.h`, `deposit_country.h`,
`deposit_visual_shader.h` and the `.inl` files are included, `third_party` holds Microsoft's DXIL
hash helper (see below). Service name `deposits` and the savegame file `tesmio_deposits.bin`
deliberately stay identical to the original so consumers such as Depletion keep working.
User documentation: README_DE.md / README_EN.md. History newest first, technical notes below.

## 0.4.10 (2026-09-20)

- Audit round, no new feature; addresses, record layouts and the save format are unchanged.
- `PrepareExtraMaps()` runs before `PatchDepositType`; `InstallExtraMaps()` only when the type patch
  is in place (`depositTypesReady`), otherwise `g_extraCount = 0` - a refused type patch no longer
  leaves the texture hook armed for map slots the game never learned about.
- `h_ED_TerrainDraw`: `__try/__finally` restores the overlay state, an outer `__except(FaultFilter)`
  drops the editor patch; `h_ED_PaintTexels` restores the slot in `__finally`; `PaintDeposit`
  restores `g_brushDep` and the bauxite byte in `__finally`.
- Reads and writes of `g_extra` run under `g_lock`.
- The blank DDS writer checks the written byte count, logs GetLastError and deletes the partial file;
  `GenerationAtomicWrite` logs GetLastError before DeleteFileA.
- `ValidateDeposits`: `building_type` must be 7 or 92, `minimap` 0 or 1 (WARN and default otherwise).
- `deposit_visual_runtime.inl`: DOS and NT headers are checked with ReadablePtr, `e_magic` and
  `Signature` before any field is read.
- RMM schema (package): `maximum_items = 32` (MAX_DEPOSITS) and `[list:tiles] maximum_items = 16`
  (MAX_SAND_TILES) instead of 118 and unlimited; the natural generation card moved to the General tab.

## 0.4.9 (2026-09-18)

- The Soviet Mod Loader guard no longer relies on the module handle alone. TesmioLoader inits the
  DLLs in `plugins\` in name order, and `deposits_plus` sorts before `soviet_mod_loader`: a local
  copy of this plugin would init while SML is not in the process yet, hook, and make SML's
  embedded deposits refuse its own sites. The guard now also fires when
  `plugins\soviet_mod_loader.dll` exists and its key in `tesmioloader.ini` is not 0. Found on
  Resources Plus 0.1.0, which lives in `plugins\` and blocked SML's start exactly this way; fixed
  here before it can happen with "Files local only".

## 0.4.8 (2026-09-14)

- Steps aside under Soviet Mod Loader. The existing guard looks for `plugins\deposits.dll`, but SML
  carries resources, deposits, needs and buildings compiled into itself, so that file does not exist
  and the guard never fired. A second check in `TsmPluginInit` now asks whether
  `soviet_mod_loader.dll` (or `000_soviet_mod_loader.dll`) is loaded in this process and whether its
  `[loader] embedded_plugins` is on; if so the plugin logs one line and returns 1 before anything is
  hooked. The module handle also answers the loader switch: with `soviet_mod_loader = 0` the DLL is
  not in the process and this plugin runs normally. `embedded_plugins` is read as well so a future
  per-capability switch in SML lets the plugin work again without another change here.
- Why it matters even though the patches defend themselves: the deposit type splice, the world save
  call, the minimap hooks and the editor hooks all compare their expected original bytes and refuse
  once SML has patched the same sites. The texture import in `InstallExtraMaps` does not - an IAT
  entry is a pointer with nothing to verify - so it would install and chain in front of SML's own
  texture hook, for maps this plugin believes it owns and can never save (its own log says "extra
  maps load but are never written back"). The guard has to sit before that, and it does.

## 0.4.7 (2026-09-12)

- The editor brush name is no longer capped at seven characters (four with `map = terrain`). The
  name in the tool descriptor is only an internal registry key: the button's picture is bound from
  a path this plugin passes itself, the hover text comes from `TOOL_CAPTION`, and the paint hook
  matches the active tool by pointer, never by name. So the descriptor now gets a short generated
  key `<verb>_t<type>` - ten characters at most, which fits `paint_rock` as well as
  `paint_bauxite` - while the icon path keeps the configured name
  (`editor/tool_<verb>_<editor>.png`). The configured name may be up to 31 characters of letters,
  digits, `_` and `-`; the two PNGs a deposit ships are named exactly as before.
- Found because a deposit called `rocksalt` silently lost its brush: the old code refused the
  14-character `paint_rocksalt`, logged `editor FAILED tool name: ...` and dropped the pair, which
  left the deposit working and the brush missing. The minimap button was never affected - it takes
  its picture from the resource record named by `icon`, where no length is involved.

## 0.4.6 (2026-09-10)

- Desert map test by the user: sand at 100% everywhere left no room for copper, clay and gas, because the
  filled sand record counted as an occupied deposit. A `desert_fill` deposit's record is now skipped in
  the occupancy mask on desert maps; the ores respect only the vanilla maps and each other.
- Organic fill: `GenerationHeights` samples the terrain height at every cell centre; the richness is a
  two-octave value-noise band `desert_fill_min..desert_fill_max` (percent, default 60..100) and, with
  `desert_fill_relief = 1` (default), falls linearly with height from the 2nd percentile of the land
  heights to 0 at the 98th, with a little noise on the slope. Seeded from the world seed and the
  deposit name. `desert_fill_relief = 0` keeps the band over the whole land. Log line names the band
  and the two heights.
- Package schema: three new fields under "Desert maps: everywhere"; READMEs updated.

## 0.4.5 (2026-09-10)

- Minimap: hovering a mod deposit button previews its overlay layer like the vanilla five do; the
  hovered layer wins over the selected one while the mouse rests on the button (`h_MM_DrawOverlay`
  picks state 1 before state 2; the row hook already tracked state 1). A vanilla layer that is on
  while a mod button is hovered stays underneath because the vanilla overlay has already been drawn.

## 0.4.4 (2026-09-10)

- Root cause of the Siberia/Asia failure, confirmed by the 0.4.3 line on terrain_siberia2: the live terrain
  mask's B channel (the gravel/rock texture layer) reserved 305236 cells, the resource maps 168000; the
  union of 417754 grew to 724009 with the 40 m gap because the layer is speckled over the mountains.
  Copper took the last three fitting spots, sand/clay/gas got nothing.
- New `[deposits_plus] generation_block_gravel` (0/1, default 0): only when set does the mask layer count
  as an occupied deposit. Vanilla places iron and coal in the mountains itself, so overlapping the gravel
  zone is harmless; the resource maps (coal, iron, oil, bauxite, uranium) stay respected either way.
  Logged in the `generation enabled=` line (`block_gravel=`) and in the occupancy line.
- Package schema: switch "Gravel zone blocks" on the Natural generation card; READMEs updated.

## 0.4.3 (2026-09-10)

- Diagnostic only: `generation occupancy:` log line before the gap dilation with the reserved cells per
  source (resourcemap R/G/B, resourcemap2 R/G, mask B, tombstones), the live mask texture's size and
  non-zero count per channel, and the union. Reason: on the Siberia and Asia DLC maps the placement
  reserved 724009 / 624610 cells although the resource maps on disk cover about 157000 / 115000 cells
  (offline replay incl. a 3-cell gap: 198504 / 147392); sand, clay and gas then found no room while
  copper still fit. The runtime mask texture is the suspect; the line will tell.

## 0.4.2 (2026-09-10)

- Tile files may live in set folders under `deposits_plus\assets`: `color` / `normal` are paths relative to
  the assets folder (`Siberia/sand_meadow_siberia_color.dds`); `VsAssetPathOk` refuses `..` elements, drive
  letters and absolute paths, everything else goes straight into the existing `VsReadFile` join. The
  `SandTile` fields for both names grew to 128 characters.
- Shipped assets are sorted into sets: `Vanilla` (meadow summer/autumn), `Siberia` (summer, snow-dusted
  autumn), `Asia - Jungle` (summer) and `Ultimate Vanilla +` (meadow pair matching that texture pack,
  not referenced by default). All colour files DXT1, all normal maps DXT5 with height in alpha,
  1024x1024, 11 mips. The shipped `[sand_tile:]` table points at the set files; Siberia and jungle no
  longer reuse the meadow pair.
- Package schema: the two file fields are grouped file lists with a pre-save check (Republic Mod
  Manager 0.4.48: `picker = files`, `reference = files`, `reference_format = dds_dxt1|dds_dxt5`).

## 0.4.1 (2026-09-10)

- Sand surface tile table: `[sand_tile:<id>]` sections (`base` = terrain base texture on material slot 5
  with its folder, `color` / `normal` = DDS files in `deposits_plus\assets`) replace the two hard-coded
  meadow names. `VsTile` matches the engine's texture path from the end as a whole path element, so
  `dlc2/tiles_siberia/grass2.dds` and `tiles_normal/grass2.dds` are different entries; a base without an
  entry, or an entry whose files fail to load, stays native. `VsDds` accepts any square power-of-two side
  from 256 to 4096 with a complete mip chain (file limit 48 MB). An INI without `[sand_tile:]` sections
  falls back to the classic meadow summer/autumn pair. The shipped INI maps meadow, Siberia (summer and
  the snow-dusted autumn `grass2snow.dds`) and jungle; Siberia/jungle reuse the meadow files until the
  user supplies their own. Winter and desert bases have no entry.
- `desert_fill = 1` per deposit: when the world's `script.ini` (shipped with saves too) says
  `$TYPE_DESERT`, the first distribution of that deposit fills every non-water cell of its 1024x1024
  map at full richness (`GenerationRun`, before the random placement branch; record status 1). Only a
  deposit that never held data is filled; saved data is never touched. Infinite unless Depletion mines
  it down. Shipped on for `[sand]`.
- Parser: `[sand_tile:]` sections are not deposits; unknown tile keys are logged and ignored.

## 0.4.0 (2026-09-07)

First published version.

- Generation presets: `generation_frequency` (1..6 regions) and `generation_size` (1..3 radius
  classes) are the only way to size the generation; an absent preset keeps the defaults (3 regions,
  class 2, richness 0.45..1.00).
- Assets are looked up beside the DLL first (`<DLL folder>\deposits_plus\assets`), then under
  `plugins\deposits_plus\assets`; the folder found is logged (`sand surface assets from ...`).
- `TsmPluginInit` returns 1 ("deposits_plus idle") while the upstream `plugins\deposits.dll`
  exists and is enabled in tesmioloader.ini, so both never run together.
- Workshop package `My Plugins\deposits_plus` with `local_copy = 1`, `[assets] dir = hooks\deposits_plus`
  and an RMM keyed_sections schema (tabs General / Sand structure / Deposits); texts (schema, de/en)
  reworked by the user, headings "Natural generation" and "Working vehicles" in the detail panel.
- No change to hooks, savegame format or `tesmio_deposits.bin` against the original plugin.

## Technical notes: sandy meadow surface

Optional ground visuals over the independent sand deposits: the meadow texture with irregular
sand traces is blended in at sand deposits; the resource density drives the transition.
Settings `sand_surface`, `sand_surface_strength` (0.0–1.0), `sand_surface_token` in
`[deposits_plus]`; the token must be a deposit with `independent_map = 1`. Summer and autumn use
separate colour/normal textures, also during the seasonal blend; the native order is
summer → autumn → snow; `grass2_snow.dds` and the native snow calculation are not replaced.

Technique: the extra texture is injected at load time into 13 identified native terrain pixel
shaders inside the actual virtual `CreateShaders` call; their existing sequence for terrain
blending, light, fog and snow is kept, the game loader's normal shader class linkage is passed
through unchanged. Sampling uses the native UVs, samplers and derivatives; only RGB is blended,
the original alpha stays. No shader file is changed. The selection mask is the current GPU
resource map; its identity is re-checked on a world change; no GPU readback or full resource copy
per frame. Unknown engine/shader builds, missing assets, unsuitable resource formats and binding
slots taken by other mods disable only this visual with a concrete warning; other Deposits Plus
features stay independent. Assets: DDS 1024 × 1024 with eleven mip levels, colour BC1/DXT1, normal
BC3/DXT5; `build.bat` copies them into `plugins\deposits_plus\assets`.

Offline verification: all 13 original shaders with D3D11/WARP, rendered mask/light/snow
fixtures, DDS loading, season flags, world-map change, temporary shader hooks and restoration
of the graphics bindings; both seasonal colour/normal pairs with 69,120 GPU pixel comparisons;
material accesses of the installed engine. In game confirmed: summer look, shrinking with
Depletion, save/load. Expected log lines:
`sand surface shader preparation: 13/13 verified native programs augmented`,
`sand surface active: token=$TYPE_MINE_SAND ...`,
`sand surface terrain selection: season=2 transition=0 surface=autumn/native ...`.
`prepared` alone means the hooks are in place, not that anything was rendered yet.

Third-party code: `third_party/d3d12TokenizedProgramFormat.hpp` and `third_party/DxilHash.cpp`
are unchanged from Microsoft's DirectXShaderCompiler repository (University of Illinois Open
Source License, `third_party/LICENSE.TXT`, shipped as `THIRD-PARTY-LICENSE.txt`). The hash
helper computes the DXBC format checksum only; it is not a security signature check.

## Technical notes: natural generation inside the country border

- Every loaded world: all resources of the current INI are matched by token against the
  savegame's history. Unknown empty resources and empty resources that were only skipped so far
  are generated once. Existing or already initialised deposits stay, also after full depletion.
  No built-in resource lists, savegame names or world ids.
- Natural, elongated, winding fields with varying width, occasional branches, irregular edges,
  real gaps and fading richness; a field may consist of several separate patches but counts
  once. Isolated grid points are removed.
- No placement in water: terrain heights inside every resource cell are checked against water
  level, wave amplitude and a safety margin, plus the shore distance (`generation_shore_m`,
  `generation_water_clearance_m`).
- Only inside the country border: a verified snapshot of the loaded native `BORDER_POLYGON`;
  without a polygon the native rectangular build limits apply. Border cells are excluded
  conservatively. The border structure is verified against the installed engine; an unknown or
  invalid structure suppresses new placements instead of placing outside the country.
- No overlap with other newly generated fields, existing plugin deposits or Vanilla
  oil/iron/coal/uranium/bauxite (`generation_gap_m`); the gravel/rock terrain zone only with
  `generation_block_gravel = 1`. Existing overlaps are not changed.
- Round robin over all new resources: at most one field and 32 attempts per turn, then the next
  resource; the order rotates with the saved seed. At most 256 attempts per requested field.
- Water, border and occupied pieces are cut away; a field counts only when at least 60 % of its
  original area remains after clipping and removal of tiny remnants (size 3 is not a remnant).
  Otherwise another place is tried.
- The log names target/result, clipped fields, attempts and the separate rejection reasons:
  country border, water/shore, deposits/gap, map edge, too little remaining area.
- First generation runs once at the first terrain draw after loading; no continuous generator.
- Levels: `generation_frequency` 1–6 (desired fields 1–6) and `generation_size` 1–3 (base
  measure 150–350 / 350–550 / 550–750 m). The base measure is chosen per field and scales its
  whole course; the elongated course spans about 3.4 to 4.6 times the base measure along its
  main axis. Invalid levels disable only that resource's generation with a warning.
- Sand: the shipped INI keeps `map = terrain` / `component = 1` as the origin note and adds
  `independent_map = 1`; the plugin assigns sand its own separate channel after the existing
  auto channels. On the first adoption of an old savegame without metadata the terrain richness
  is adopted (area-averaged from terrain to resource resolution) unless a sand deposit already
  exists; if that is empty too, the general first distribution applies. The visible terrain
  mask is neither recoloured nor reduced by sand mining; the sand brush lives in the editor's
  resource tab. Switching back to a terrain mask is not a supported migration.

Metadata `tesmio_deposits.bin`: versioned, checksummed identities, channel mapping, seed and
history of removed resources; written after the native map save from its DDS files and
replaced atomically through a temporary file; both are needed on load. History states:
adopted, generated, not yet initialised, generation attempted but no/too little room. Once data
is adopted or saved in a channel it is marked initialised, and later mining does not reset that.
Limit with old data: without metadata the plugin cannot prove that an empty channel was never
mined; `generate_existing_empty = 0` then protects old or incompletely initialised empty
entries. That option is never a regeneration command. Worlds from `media_soviet/save/...` and
`saved_last` count as old savegames without metadata, terrain templates as new worlds.
Unreadable/invalid metadata, missing original map files, contradicting types, unknown DDS
formats or missing engine functions produce warnings and suppress new generation and metadata
overwrites; a broken file is never interpreted as a new empty world.

Offline verification: all 18 level combinations, fair round-robin distribution, INI-order
independence, clipping/minimum area, concave country borders and border cells, layout
signature of the installed engine, water/occupancy masks, no overlap, bounded search, metadata
checksums, re-adding, re-sorting, depletion, remove/re-adopt, sand adoption and unchanged
native vehicle gates; plus a test with the real border, height and Vanilla occupancy data of
the coastal map template with several seeds; offline tools `rmhist` (DDS channel histogram) and
`occ` (occupancy replay). Log lines start with `generation`.

## Technical notes: working vehicles for own deposits

Per deposit section: `working_vehicle_skill = gravelmining` (or `none`, case-insensitive;
unknown values warn and disable only the vehicle extension of that section). The building
stays `$TYPE_MINE_SAND` (building_type 7) and still needs `$WORKING_VEHICLES_NEEDED`,
`$VEHICLE_PARKING` places and the normal operating conditions; vehicles use their existing
`$SKILL_GRAVELMINING`, there is no `$SKILL_SANDMINING`. The deposit map and the `$PRODUCTION`
resource are not changed. Fuel, state, capacity and production rules of the game stay.

Technique: four native type checks are extended by the explicitly configured mine types:
vehicle admission for purchase/assignment, two work-performance calculation paths and the
arrival/work state of pit vehicles. No building or vehicle types are rewritten in memory; the
original branches for gravel and bauxite stay first; an additional check limits new assignments
to building type 7. Installed at plugin start only; all four sites including surrounding
identification bytes are verified against 1.1.1.9 first. Deviating bytes or a preparation
failure leave all four checks untouched; without an active assignment no vehicle patch is
installed; `code_patch = 0` disables this extension too; other plugins rewriting the same sites
cause a warning and rejection. No new save format, no periodic vehicle scans, no change to the
executable on disk. Log prefix `vehicles`; successful start: `4 native gates installed` plus
one assignment line per released mine.

Offline verification against the installed executable: DLL built with the central build.bat
switches; 24,960 real x64 test calls of the original/extension branches (registers and defined
arithmetic flags, Vanilla behaviour, unconfigured types, building-type separation, limits and
up to 32 deposits); INI parser, defaults, invalid values, existing extra keys; four sites and
preconditions checked directly against executable bytes; deviating bytes and memory/protection/
cache failures before activation tested, never partially installed vehicle branches; a
successful installation changes only the four intended areas of a private in-memory copy.
