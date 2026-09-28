# Weather Roads – build notes

TesmioLoader plugin (API 4) for WRSR 1.1.1.9, GPL v3. Build: the standard line of the root
`build.bat` (`cl /O2 /MT /W3 /EHsc /std:c++17 /LD`, kernel32.lib) compiles `weather_roads.cpp`;
it includes `src/tesmio_plugin.h`, `my_plugins/grit_spreader_api.h` (consumer side of the
`tss.grit_spreader` service of Technical Service Storage) and the two persistence headers
`weather_roads_persistence.h` / `weather_roads_persistence_format.h` (sidecar
`tesmioloader.weather_roads.protection.bin` beside the savegame). Hooks are installed only on the
verified SOVIET64.exe / C3DDLL64.dll build. User documentation: README_DE.md / README_EN.md.
History newest first.

## 0.3.3 (2026-09-09)

- 2026-09-11, version unchanged: the detail log `tesmioloader.weather_roads.log` is written to `<loader>\logs\`
  (shared `TsmOpenLog` in `src/tesmio_plugin.h`, the folder is created on first use; if that fails the
  file lands next to `tesmioloader.log` as before). The file was called `weather_roads.log` before; the new name follows the other plugins so the Republic Mod Manager log window lists it.
- Defaults `[snow] accumulation_multiplier` 0.30 -> 0.35 and `maximum_accumulation_per_burst`
  50 -> 95 (user decision after in-game tuning: too little snow stayed after short snowfalls).
  Changed in the compiled-in `g_cfg` initialiser, both INIs, the schema/DE/EN descriptions and
  the README tables. No code path changed. Users with their own values in `user_config` are not
  affected; the package defaults_hash changes, so RMM shows the update marker once.
- Fix: 0.3.1 inserted `releaseFollowsWeather` into `WeatherRoadsConfig` without adding an entry to
  the aggregate initialiser of `g_cfg`, so every compiled-in default after it was shifted by one
  (that was the source of the C4244/C4838 warnings at the initialiser, wrongly taken for
  pre-existing). No runtime effect as long as the INI carries every key, because the readers
  fall back to those values only for missing keys. Entry added, the build is warning-free again.

## 0.3.2 (2026-09-09)

- `+0xE28` of the world object is the winter weather roll, not a 0/1/2 precipitation state.
  Disassembly of the winter routine (exe+0x5D06B8..0x5D08FA): when the period timer at `+0xE2C`
  expires the game rolls `rand % 3` on climate 3 and `rand % 8` on every other climate, so the
  field holds 0..7. Only 1 is snowfall: the +30 road-snow ticks (`0x42AF60`, return RVA
  `0x5D08F5` in the limiter log) and the flags at `+0x5E4` run only for 1; 0 and 2..7 are all
  "no snow". Confirmed in the user's log (accumulation calls only during state 1, none during 2)
  and by a read-only memory probe of the paused game (roll 6 while the plugin reported
  "unavailable").
- `ReadWeatherSnapshot` accepts 0..7 (was 0..2). With the old range the snapshot was refused on
  5 of 8 rolls: 23 "weather tick unavailable" events in a 12-minute session, info window "no world
  data", and `release_follows_weather` could not act.
- `release_follows_weather` drops the queue on any roll other than 1 (was: only 0). Event line
  carries `precipitation_state=<n> (<name>)`.
- `WeatherRollName`: dry / snow / no snow / unknown for the weather EVENT lines and the info window;
  the window's old "rain" label for 2 was wrong.
- Package: INI comment, schema and DE/EN descriptions say "snowfall" instead of "precipitation"
  and "a few seconds" instead of "half a minute" (the burst maximum caps the queue at 80 units,
  4.4 s at step 2 / 110 ms).
- In-game test 2026-09-09 21:45 (user): 23 weather lines with named rolls 0..7 in two minutes, no
  "weather tick unavailable" line, one snowfall (roll 1, 11 ticks, 100 units queued) ended by
  "gradual snow release stopped with the weather: precipitation_state=4 (no snow), dropped_units=4"
  19 ms before the roll change was logged.

## 0.3.1 (2026-09-09)

- `[snow] release_follows_weather` (default 1, 0..1): `ServiceGradualRoadSnow` reads the weather
  snapshot before every release step; with `precipitation_state == 0` the remaining queued units
  are dropped (counted in `g_gradualSnowCancelledUnits`, one EVENT line), the next-step tick is
  cleared and the visual batch flushed. `ReadWeatherSnapshot` failing (no capture, stale > 30 s,
  implausible values) leaves the release untouched - fail open. Values seen in the user's log:
  precipitation_state 1 while it snows, 0 afterwards (plausible range 0..2).
- Reason: the queue (up to 255 units at 1 unit / 125 ms) kept whitening roads for up to ~30 s
  after the snowfall had visibly ended. The alternative of holding the weather object in the
  snowing state was rejected (feedback into the same queue, fighting the game's weather machine).
- Startup INFO line now ends with `release follows weather=on|off`.
- Package: schema field on the Snow tab (order 45), DE/EN texts, INI comment, READMEs.

## 0.3.0 (2026-09-08)

First published version.

- Scaled and optionally gradual road-snow build-up with a per-burst cap, independent natural
  melting scale, protection phase (game minutes) followed by a weaker salt phase (game hours) with
  a configurable factor, material strength and dry-plowing detection through the grit spreader
  service, visual snow correction on tracked plowed areas, F10 info window with built-in
  German/English texts, protection persistence beside the savegame (sidecar written after the
  final native file close), defensive loader checks and cleanup on exceptions.
- Configuration: `plugins\weather_roads.ini` when present, otherwise the INI next to the DLL
  (Workshop package under Soviet Mod Loader or the Workshop Bridge); own `ConfigString` reader
  identical to the loader's (GetPrivateProfileStringA plus trim). Chosen path logged as
  `configuration file:`.
- Package: five-tab RMM schema (33 fields); texts (schema, de/en) in player style: every boolean
  described with ON/OFF, line breaks in longer descriptions, the restart/Technical Service Storage
  note in `[launcher] notice` (needs Republic Mod Manager 0.4.3), the plow formula kept out of the
  UI, "protection phase after plowing" naming. READMEs on the common template.
