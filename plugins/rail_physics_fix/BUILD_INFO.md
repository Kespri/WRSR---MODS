# Rail Physics Fix – build notes

Windows/TesmioLoader port of RailPhysics 1.3.0 by Meow Meow (TheRealMeowMeow00,
https://github.com/TheRealMeowMeow00/WRSR_RailPhysics, Workshop item 3776784867), GPL v3.
Target: SOVIET64.exe 1.1.1.9 (build 23935965), TesmioLoader API 4. The INI section stays
`[railphysics]` for compatibility with the original. History newest first, port notes below.

## 1.3.5 (2026-09-08)

- 2026-09-11, version unchanged: the detail log `tesmioloader.rail_physics_fix.log` is written to `<loader>\logs\`
  (shared `TsmOpenLog` in `src/tesmio_plugin.h`, the folder is created on first use; if that fails the
  file lands next to `tesmioloader.log` as before).
First published version of the port.

- Physics, decision logic, signatures, offsets, keys and defaults are those of RailPhysics 1.3.0
  (verified by a full source review against the original). The port fixes three ABI bugs of the
  original stubs (fifth argument of the brake helper, saved registers in the callee's shadow space,
  xmm0 in the curve stub), a use-after-free in the corridor build, unchecked reads and the
  locale-dependent `atof`.
- Where the original degrades, the port degrades the same way instead of stalling a train or
  dropping every zone: `ComputePhysics` preflight through `ReadableFast` (region cache) instead of
  one VirtualQuery per wagon and frame, inactive wagons skipped before their type is read; a
  non-finite native curve limit passes through; `CollectStationNodes` skips an unreadable chain or
  table; `BuildCorridors` publishes the corridors found so far on an allocation/capacity stop
  (warning RP103); `RescanStations` keeps the previous complete tables when the rebuild fails
  (`RestoreZones`); the route look-ahead ends with the points collected so far on an unreadable
  segment, invalid length, unreadable end node, invalid polyline or non-finite point (warning
  RP_ROUTE), only allocation failures remain `CurveScanFailure`; `SpanCount` truncates a remainder
  like the original's pointer division.
- Configuration: `ResolveConfigFile` checks `plugins\rail_physics_fix.ini` below the loader base
  directory; when the file exists, every read goes through `H->configInt`/`H->configString`. When
  it is missing, `rail_physics_fix.ini` beside the DLL (GetModuleHandleEx on the DLL's own address)
  is read with GetPrivateProfileIntA/GetPrivateProfileStringA and the same trim the loader uses
  (`CfgInt`/`CfgString`). Log line `rail_physics_fix  configuration file: <path>`.
- Package `My Plugins\rail_physics_fix`: soviet.mod.ini with local_copy, presentation schema with
  four tabs and all 30 settings, DE/EN in player style (`[launcher] notice` for restart and INI
  deletion, `[launcher] info` on what the port fixed, card "Troubleshooting", "curve and route
  look-ahead" everywhere), READMEs per template, LICENSE GPL v3.
- In-game test 2026-09-08 (log 21:07-21:10): 9 subsystems patched, curve limits, station stop,
  station zone and consumption model observed on a 1313 t train; customs approach and emergency
  brake not exercised. Open cosmetic item: `gapcust`/`gapstat` print FLT_MAX when no customs node
  is in range.

## Port notes

Toolchain: Microsoft C++ / linker 14.52.36615, x64, static runtime (`/MT`); release
`/std:c++17 /utf-8 /O2 /MT /W4 /wd4505 /EHsc /LD` (`4505` concerns unused static SDK helper
functions); the normal root build flags `/O2 /MT /W3 /EHsc /LD` build it as well, without an
additional assembler link step. Clang 22.1.3 only to generate/verify the bridges and for the
test harness. DLL dependency: `KERNEL32.dll` only. Exports: `TsmPluginApiVersion`,
`TsmPluginInit`, `TsmPluginStart`; API 4.

Hooks: twelve source signatures (eleven unique hits; two references of the twelfth agree on the
same global address), five replaced instruction blocks, three function calls and two branch
targets. Other changes at the same hook sites are detected by the signature check and raise
RP401 to RP406. The tests use synthetic vehicles/tracks and a data copy of the executable; no
game function is executed offline.

| File | SHA-256 |
|---|---|
| Original `railphysics.dll` | `EE54F6430BF0773CCD79FD7452B108B364F8778326EB6D3D3342863A36B9FCC7` |
| Original `railphysics.cpp` | `7F40A955B2CA54E448650C669BFDC9AFAD78F94BA4D15B05C34A1B08D3D19340` |
| Original `railphysics.ini` | `F22BE07D5AF977B4F3E7D71A7DADE0AECE11369680FADE1DEEE82B06E1FB7981` |
| Verified `SOVIET64.exe` | `296644A9F207D609031FC2AE73FED2DCB34619A1D55A35D1C7B51965CE6841B8` |

The verified executable has PE timestamp `0x6A3EB6AD` and ImageSize `0xA9D000`. The headers
match the TesmioLoader SDK available at build time byte for byte. The executable, the
original DLL and the original source are not distributed as part of this plugin folder.

Assessment: the added safeguards act on failed allocations, invalid memory ranges/numbers or
Windows API errors. Valid physics and route data are processed with the original formulas.
Time base, simulation-tick detection, object lifetime/caches, INI policy, scan budgets and
hook grouping were not fundamentally rebuilt. The direct numeric comparison and the offline
tests do not replace a test in the running game.
