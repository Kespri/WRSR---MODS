# UI Layout Fixes – build notes

- Plugin version 0.3.0. Target: SOVIET64.exe 1.1.1.9, TesmioLoader API 4.
- Source folder: `my_plugins\ui_layout_fixes\`. Build: `build.bat` (Microsoft Visual C++ x64,
  `/O2 /MT /W3 /EHsc /LD`, kernel32.lib). Output: `build\plugins\ui_layout_fixes.dll` and
  `build\plugins\ui_layout_fixes.ini`.

## 0.3.0 (2026-09-09)

- 2026-09-11, version unchanged: the detail log `tesmioloader.ui_layout_fixes.log` is written to `<loader>\logs\`
  (shared `TsmOpenLog` in `src/tesmio_plugin.h`, the folder is created on first use; if that fails the
  file lands next to `tesmioloader.log` as before).
First published version, with the modules CUSTOMHOUSE and TEXT_WRAP described below. Package:
keyed_list editor texts in player style, `add_id_help` for the add dialog, game-text picker
(`id_picker = game_texts`, RMM 0.4.21+), tabs General / Customs house / Text wrap / Text ids;
`user_overlay` is not declared because the list editor writes the effective INI. In-game test
2026-09-09: confirmed by the user. Log: print hooks 6/6, id 1970 -> 3 lines (widest 50 at width 58),
font size 17.0 -> line step 19.5 (label rows use 20); route hint drawn as three lines, other windows
unchanged.

## TEXT_WRAP module

- Purpose: long captions such as the route hint in the vehicle window are drawn as one line by the
  game and run off the window. The module wraps any caption listed by text id; a new id only needs
  an INI line, the module does not depend on the executable build.
- INI: `[text_wrap]` enabled (1), max_chars (58, 20..200), max_lines (4, 0..12), line_spacing
  (1.15, 0.50..3.00, multiple of the font size), keep_breaks (0), log_long_texts (0, 0..400);
  `[text_wrap_ids]` with `<id> = <chars>` lines (0 = default width, else 20..200), up to 64
  entries, read from the user_config overlay as a whole when it has the section, otherwise from
  the base INI (`GetPrivateProfileSectionA`, step measured before the `=` cut). A missing
  section keeps 1970 as the one entry.
- Hook 1: `C3D_LANGUAGE::GetString` via the IAT; a table of entries with one source and one
  wrapped buffer each (`g_wrapped[64][1024]`, contiguous). The game's own line breaks become
  spaces unless `keep_breaks = 1`, greedy wrap at the width, then per paragraph the narrowest
  width that keeps the same line count (balanced lines), width widened in steps of 4 while the
  line count exceeds `max_lines`. With `log_long_texts > 0` every id seen once whose longest line
  exceeds the value is logged with its first 80 characters (bitset of 65536 ids).
- Hook 2: the six print imports SOVIET64.exe has from C3DDLL64.dll -
  `C3D_FONTMANAGER::PrintLeftUnicode/PrintCenterUnicode/PrintRightUnicode/PrintLeftUnicodeNoArg`
  and `C3D_FONT::PrintLeftUnicode/PrintRightUnicode` - are redirected through generated stubs
  in one RWX page (59 bytes each, `mov rax,[rsp+0x30|0x28]` = the text pointer, two `mov r10,
  imm64 / cmp rax,r10` range checks against `g_wrapped`, `jmp` to the handler or to the
  original read from the slot before the patch). Encoding checked against ml64 (`cmp` uses
  the 4C 39 D0 form, ml64 the 49 3B C2 form; same operation). The stubs never touch the
  stack, so the variadic originals get every argument as the caller built it. Handlers take
  the fixed arguments (floats from xmm1..3 as the non-variadic prototype expects; the call
  sites duplicate them in r8/r9 too), split at '\n' and print each line through the original
  with `L"%ls"` (NoArg: the line itself), y advanced by `C3D_FONT::GetSize(font) *
  line_spacing` (export resolved with GetProcAddress; fallback 16 when missing or absurd).
  The first wrapped print logs font size and step for calibration.
- Limit: texts the game copies into its own buffers before printing stay single-line.
- Failures: stub page or all print imports failing is an ERROR and the module stays inactive;
  a single import missing is logged and skipped; caption hook failure after the print hooks is
  an ERROR (nothing wrapped). Start still returns 0 in every case.
- Offline check of the wrap with the real texts of id 1970 (`sovietGerman.btf` /
  `sovietEnglish.btf`, big-endian tables, UTF-16BE payload): DE 74/76 chars -> 3 lines with
  merged paragraphs at `max_chars = 58`, EN 50/66 -> 2.

## Configuration (`my_plugins\tesmio_config.h`)

- base: `<loader>\plugins\ui_layout_fixes.ini`, otherwise the INI beside the DLL
- overlay: `<loader>\user_config\ui_layout_fixes.ini`, key by key, if present
- Both paths are logged at start. The DLL never writes either file.

## Verified executable identity

- PE TimeDateStamp `0x6A3EB6AD`, SizeOfImage `0x00A9D000`
- Reference SHA-256 `296644A9F207D609031FC2AE73FED2DCB34619A1D55A35D1C7B51965CE6841B8`

## CUSTOMHOUSE module

| Site | Address |
|---|---|
| Panel | `exe+0x71B410` |
| Resource-list measurement | `exe+0x71C475` |
| Resource-list drawing | `exe+0x71C4B5` |
| Native resource-column helper | `exe+0x7C6720` |
| Native row-pitch instruction | `exe+0x7C678C` |

The two CUSTOMHOUSE calls are redirected through one near bridge. The shared native row-pitch
operand is redirected to plugin-owned nearby storage whose default is the original 25.0f.
Only either verified CUSTOMHOUSE call changes that storage temporarily. Native measurement,
downstream vertical positioning and scroll-range propagation remain in control.
