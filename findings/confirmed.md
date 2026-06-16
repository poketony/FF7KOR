# FFVII.exe jafont decoder - confirmed findings

Scope: Phase 1 read-only analysis of the 2026 native 64-bit `FFVII.exe`.

Method:
- Ghidra MCP was connected to the open CodeBrowser at `http://127.0.0.1:8080`.
- The user's minidump at `C:\Users\JO\AppData\Local\Temp\FFVII.DMP` was parsed read-only.
- A separate analysis overlay was created at `codex-lab/work/runtime_dump/FFVII_runtime_overlay.exe` by copying the original PE and replacing section bytes with runtime memory from the dump. The original `FFVII.exe` was not modified.
- Ghidra headless was run against the separate overlay project under `codex-lab/work/ghidra_projects/FFVII_runtime_quick`; the user's open Ghidra database and the original binary were not edited.

## PE and dump baseline

Ghidra loaded the original program at image base `0x140000000`.

Relevant original segments:

| Segment | Start | End |
|---|---:|---:|
| `.text` | `0x140001000` | `0x141646dff` |
| `.rdata` | `0x141647000` | `0x1416d15ff` |
| `.data` | `0x1416d2000` | `0x142093ae7` |
| `.pdata` | `0x142094000` | `0x1420d11ff` |

The dump contains the loaded `FFVII.exe` module at runtime base `0x7ff620130000`, size `0x212a000`, path `C:\Program Files (x86)\Steam\steamapps\common\FINAL FANTASY VII Steam Edition\FFVII.exe`.

The runtime overlay keeps preferred image base `0x140000000`, so the addresses below remain `0x141...`.

The overlay proves that on-disk `.text` is protected or encrypted:

| RVA | Original bytes | Runtime bytes |
|---:|---|---|
| `0x1571DC0` | random-looking protected bytes | starts `48 89 5c 24 30 ...` |
| `0x115AF00` | random-looking protected bytes | decompiles into a large draw/decoder routine |

## `jafont_%d.tim`

The exact string `jafont_%d.tim` exists once in `FFVII.exe`:

| Item | Value |
|---|---:|
| VA | `0x14164C300` |
| RVA | `0x164C300` |
| File offset | `0x164B500` |
| Occurrences in file | `1` |

The complete strings `jafont_1.tim`, `jafont_2.tim`, and `jafont_6.tim` do not occur as literal strings in the file.

In the runtime overlay, the direct string reference is confirmed in function `0x141571DC0`:

| Item | Value |
|---|---:|
| Function range from `.pdata` | `0x141571DC0` - `0x141571E84` |
| Direct RIP-relative reference | `0x141571E19` |
| Referenced string | `0x14164C300` / `jafont_%d.tim` |

Decompiler evidence for `0x141571DC0`:
- Calls `FUN_141574670(0x2000000, 8, 0x138)`.
- Stores the returned handle/global at `0x14207CDEC`.
- Stores the allocated struct pointer at `0x14207CE08`.
- Zeroes `0x138` bytes through `func_0x141624692`.
- Loops `i = 1..6`, calling `FUN_14003F0D0(slot, &UNK_14164C300, i)`.
- Slot stride is `0x14`, producing six formatted resource names.

Confirmed callees from the direct reference function:
- `FUN_141574670` - allocator/handle creation candidate.
- `func_0x14003F0A0` - address/VM helper used throughout this binary.
- `func_0x141624692` - memset-like zeroing helper.
- `FUN_14003F0D0` - formatted string writer candidate.

Confirmed referenced globals/data:
- `0x14207CDEC` - font resource handle/global.
- `0x14207CE08` - font resource struct pointer.
- `0x14164C300` - `jafont_%d.tim`.
- `0x14164C2C0` through `0x14164C2F8` - copied static data following the six filename slots.

Direct callers of `0x141571DC0` were not found by a simple `E8 rel32` scan of the overlay `.text`; this is still unresolved and may be dispatcher/VM mediated.

## Six font resources and objects

The six Japanese font resources are represented in the struct pointed to by `0x14207CE08`.

Confirmed layout evidence:

| Slot | Filename offset | Object/handle offset |
|---:|---:|---:|
| 1 | `+0x00` | `+0x10` |
| 2 | `+0x14` | `+0x24` |
| 3 | `+0x28` | `+0x38` |
| 4 | `+0x3C` | `+0x4C` |
| 5 | `+0x50` | `+0x60` |
| 6 | `+0x64` | `+0x74` |

Confirmed code references:
- `0x14156DF3A` checks each filename slot and stores object handles at `+0x10`, `+0x24`, `+0x38`, `+0x4C`, `+0x60`, `+0x74` after calls to `FUN_14004AB00(0x6710AC, 5, ...)`.
- `0x14156DE10` checks the same six handle offsets and calls `FUN_14004AB00(0x671082, 1, handle_slot)`.
- `0x1415720BB` selects one of the six handle offsets by page switch and submits/draws with `FUN_14004AB00(0x66E272, 2, 1, handle)`.

Workspace artifact confirmation:

| Path | Size |
|---|---:|
| `codex-lab/work/menu_ja_poc/unpacked/jafont_1.tex` | `4194540` |
| `codex-lab/work/menu_ja_poc/unpacked/jafont_2.tex` | `4194540` |
| `codex-lab/work/menu_ja_poc/unpacked/jafont_3.tex` | `1049748` |
| `codex-lab/work/menu_ja_poc/unpacked/jafont_4.tex` | `4194540` |
| `codex-lab/work/menu_ja_poc/unpacked/jafont_5.tex` | `4194540` |
| `codex-lab/work/menu_ja_poc/unpacked/jafont_6.tex` | `4194540` |

The executable load string uses `.tim`; the workspace extraction has `.tex`. The exact LGP/container path remains probable, not confirmed, because the original `menu_ja.lgp` is not present in the repo.

## FA-FE decoder evidence

Confirmed functions containing an `FA`-`FE` range check and second-byte consumption:

| Function | Evidence | Classification |
|---|---|---|
| `0x1415712E2` | Uses `(byte)(b + 6) < 5`, which is true for `FA`-`FE`; stores prefix as `b << 8`; next loop iteration combines `second_byte | prefix`; only `0xFF` terminates. | confirmed parser |
| `0x1415714B0` | Measurement/wrapping scanner; same `(byte)(b + 6) < 5`; stores prefix as high byte; next byte is passed as `second | prefix` to width helper. | confirmed measurement/wrapping |
| `0x14157251B` | Draw/text scanner; same `(byte)(b + 6) < 5`; consumes second byte and calls `FUN_141571EC0(..., second | prefix, ...)`. | confirmed text draw scan |
| `0x14115AF00` | Subtracts `0xFA`, checks result `< 5`, uses five-entry table at `0x6F563A`, increments the input index before using the next byte. | confirmed common draw path |

Confirmed terminator behavior in these loops:
- `0xFF` terminates FF7 text.
- `0x00` is not treated as a terminator in the observed parser loops.

## Page and glyph trace

Confirmed trace:

1. Text scanner reads a byte.
2. If the byte is `FA`-`FE`, it is retained as the high byte of a two-byte character.
3. The next byte is consumed and combined as `second_byte | (prefix << 8)`.
4. The selected code eventually reaches `0x1415720BB`.
5. `0x1415720BB` selects one of six font object handles from `0x14207CE08 + {0x10,0x24,0x38,0x4C,0x60,0x74}`.
6. The glyph byte is used for UV placement: high nibble selects row-like offset, low nibble selects column-like offset, both scaled by `0x40`.
7. The selected object is submitted through `FUN_14004AB00(0x66E272, 2, 1, handle)`.

Confirmed width-related helpers:
- `0x141571220` computes a width with cap `0x40`, using tables around `0x14164C3B0`, `0x14164C7B0`, and `0x14164C830`.
- `0x14115AF00` also reads a width/spacing table through pointer `0xDB958C`; the two-byte page cases set offsets `0xE7`, `0x1B9`, `0x2A0`, `0x372`, `0x444`.

## Caller buckets

Confirmed:
- Measurement/wrapping: `0x1415714B0`.
- Common draw/spacing path: `0x14115AF00`.
- Text draw scan: `0x14157251B`.
- Font object upload/store: `0x14156DF3A`.
- Font object use/release path: `0x14156DE10`.
- Page/object render selection: `0x1415720BB`.

Probable, still awaiting direct caller proof:
- Field text: `0x1410FB970`, `0x14111D6A0`, `0x1411F7DC0`.
- Menu text: `0x1410359F0`, `0x141070D60`.
- Battle text: `0x14107E9A0`, `0x14108F490`.

## Current Ghidra MCP continuation

The current open Ghidra program resolves the confirmed blocks, but several earlier addresses are instructions inside larger current Ghidra functions rather than function starts:

| Confirmed block/instruction | Current Ghidra function | Current range | Evidence |
|---:|---|---|---|
| `0x141571DC0` | `FUN_141571d90` | `0x141571D90` - `0x141571E88` | Contains the `jafont_%d.tim` formatting loop and writes `0x14207CE08`. |
| `0x1415720BB` | `FUN_141571ec0` | `0x141571EC0` - `0x14157237E` | Contains the six-way page/object switch and glyph UV write path. |
| `0x14157251B` | `FUN_1415724a0` | `0x1415724A0` - `0x141572744` | Contains the draw-string scanner and second-byte consumption. |
| `0x1415712E2` | `FUN_1415712b0` | `0x1415712B0` - `0x1415714A0` | Contains the width scanner and second-byte consumption. |
| `0x14156DF3A` | `FUN_14156df20` | `0x14156DF20` - `0x14156E114` | Contains the six-object upload/store block. |

New directly evidenced caller/callee results from the current program:

| Function | Classification | Direct callers | Relevant callees/evidence |
|---|---|---|---|
| `FUN_141571d90` / `0x141571D90` | confirmed | `FUN_1415742d0` at `0x1415742D4` | Generates six `jafont_%d.tim` names, allocates/zeros the font struct, writes `0x14207CE08`. |
| `FUN_141571ec0` / `0x141571EC0` | confirmed | `FUN_141570230`, `FUN_141570320`, `FUN_1415724a0`, `FUN_14156e430` | Common glyph renderer; all confirmed string/single-glyph JP render wrappers converge here. |
| `FUN_1415724a0` / `0x1415724A0` | confirmed | `FUN_141570410`, `FUN_141570510`, `FUN_141570610`, `FUN_141570730`, `FUN_141570c30`, `FUN_14156e120`, `FUN_14156e430` | Common JP string renderer; contains FA-FE prefix handling and recursive special-string paths. |
| `FUN_1415712b0` / `0x1415712B0` | confirmed | `FUN_141570050`, `FUN_14156ffe0`, `FUN_1415714b0`, `FUN_14156e120`, recursive self-calls | Common JP width/measurement scanner; contains FA-FE prefix handling and calls `0x141571220`. |
| `FUN_1415714b0` / `0x1415714B0` | confirmed | `FUN_14156fd10` only, plus data table reference | Field/textbox measurement-layout scanner; handles FA-FE, `0xE7`, and `0xE8`. |
| `FUN_14156fd10` / `0x14156FD10` | confirmed | `FUN_140ce3d10` | Calls `FUN_1415714b0` for JP text box measurement/layout and calls `FUN_14111d6a0` for draw continuation. |
| `FUN_14156e430` / `0x14156E430` | confirmed | `FUN_14111d6a0` | Independent JP render scanner; directly checks FA-FE, consumes the next byte, calls `FUN_141571ec0` and `FUN_1415724a0`. |
| `FUN_14156e120` / `0x14156E120` | confirmed | `FUN_14108f5f0` | Preprocesses/scans text, detects FA-FE, then calls `FUN_1415712b0` and `FUN_1415724a0`. |
| `FUN_141570050` / `0x141570050` | confirmed | Many UI/text callers, including `FUN_1411f7dc0` at `0x1412001F9` | JP mode calls `FUN_1415712b0`; non-JP paths call the legacy/common draw path. |
| `FUN_14156ffe0` / `0x14156FFE0` | confirmed | `FUN_1410cda50`, `FUN_1411bc360` | Alternate dispatch wrapper; JP mode calls `FUN_1415712b0`. |
| `FUN_141570410` / `0x141570410` | confirmed | Broad UI/text caller fan-in | JP mode calls `FUN_1415724a0`; non-JP paths fall through legacy/common wrappers. |

Confirmed sharing model:
- Field, menu, and battle candidates do not appear to own completely separate low-level glyph renderers.
- Confirmed JP text scanners and wrappers converge on one common glyph renderer: `FUN_141571ec0`.
- Confirmed JP measuring/wrapping consumers converge on one common width scanner/helper path: `FUN_1415712b0` and `FUN_141571220`.
- There are separate upper wrappers and scanners around those common low-level paths, especially `FUN_14156e430`, `FUN_1415724a0`, and the `FUN_141570xxx` wrapper family.

## Menu and battle wrapper-entry continuation

New confirmed menu render chain from the current Ghidra program and direct-call overlay scan:

| Chain | Direct call sites | Classification | Evidence |
|---|---|---|---|
| `FUN_141070d60` -> `FUN_141077ef0` -> `FUN_14106b4a0` -> `FUN_141570730` -> `FUN_1415724a0` -> `FUN_141571ec0` | `0x141071211`, `0x14107806A`, `0x14106BCEA`, then wrapper-internal calls | confirmed | `FUN_141070d60` is the mapped menu draw root; `FUN_14106b4a0` directly pushes text/coordinate/style arguments and calls the shared string wrapper. |
| `FUN_141070d60` -> `FUN_141077ef0` -> `FUN_14106b4a0` -> `FUN_141570050` -> `FUN_1415712b0` -> `FUN_141571220` | `0x141071211`, `0x14107806A`, `0x14106BD10`, then wrapper-internal calls | confirmed | `FUN_14106b4a0` directly calls the shared width wrapper with the same text pointer family used for drawing. |
| `FUN_1410734f0` -> `FUN_1410732b0` -> `FUN_141570730` -> `FUN_1415724a0` -> `FUN_141571ec0` | wrapper calls observed in `FUN_1410734f0` at sites previously xrefed to `FUN_141570730` | confirmed | `FUN_1410734f0` builds temporary text buffers through `FUN_1410732b0`, then renders those buffers through the shared string wrapper. |

New confirmed battle/helper render and width entries:

| Function | Direct wrapper entries | Classification | Evidence |
|---|---|---|---|
| `FUN_1410798e0` | `0x14107A8D4` -> `FUN_141570730`; `0x14107A962` -> `FUN_141570730`; `0x14107AAB3` -> `FUN_141570410`; `0x14107AED1` -> `FUN_141570050`; additional later calls include `FUN_141570730` and `FUN_141570410` | confirmed | Current decompile shows battle/menu state globals around `0x91B...` and `0xDC16B4`; direct calls reach the shared render and width wrappers. |
| `FUN_14107e9a0` | no direct call to `FUN_141570xxx` observed | confirmed | The mapped battle draw root calls `FUN_140DB5DF0`, `FUN_140061A50`, and repeated `FUN_140E2BC10`; direct transition from this root into the text wrapper layer remains unresolved. |

Confirmed wrapper argument roles:

| Wrapper | Text cursor/buffer | Coordinates | Color/style | Width/layout state | Font page/glyph state |
|---|---|---|---|---|---|
| `FUN_141570730` | stack `+0xC` is resolved through `FUN_14003f0a0` and passed as the `FUN_1415724a0` text pointer | stack `+0x4` -> x, `+0x8` -> y | stack `+0x10` is masked to one byte and passed to string renderer | none directly | page and glyph are decoded later by `FUN_1415724a0` and `FUN_141571ec0`; caller does not pass page directly |
| `FUN_141570410` | stack `+0xC` resolved through `FUN_14003f0a0` | stack `+0x4` -> x, `+0x8` -> y | stack `+0x10` style byte | none directly | same as `FUN_141570730`; wrapper forces the renderer flag argument to `1` |
| `FUN_141570050` | stack `+0x4` resolved through `FUN_14003f0a0` | none | none | result width is written to `DAT_1420395B8` | width scanner calculates glyph width through `FUN_1415712b0` and `FUN_141571220` |
| `FUN_1410732b0` | reads source bytes until `0x00`, writes adjusted bytes to an output buffer, then writes `0xFF` | none | none | none | does not inspect FA-FE; it is a string-buffer conversion helper before shared rendering |

New independent FA-FE findings:
- No new independent FA-FE range check was confirmed in `FUN_141070d60`, `FUN_14106b4a0`, `FUN_1410734f0`, `FUN_1410732b0`, `FUN_14107e9a0`, or `FUN_1410798e0`.
- The newly confirmed menu and battle/helper paths enter the previously confirmed shared scanners instead of decoding FA-FE themselves.

## Classification summary

Confirmed:
- Runtime dump contains decrypted code usable for this analysis.
- `jafont_%d.tim` direct reference function is `0x141571DC0`.
- Six formatted font resource names are generated with stride `0x14`.
- Font resource struct/global is rooted at `0x14207CE08`; related handle/global at `0x14207CDEC`.
- Six font object handles are stored at `+0x10`, `+0x24`, `+0x38`, `+0x4C`, `+0x60`, `+0x74`.
- Multiple runtime functions perform the `FA`-`FE` two-byte prefix check and consume a second byte.
- The render path selects one of six font objects and uses the glyph byte for UV coordinates.

Probable:
- The six resources are ultimately loaded from the Japanese menu LGP path as `jafont_1` through `jafont_6`.
- Existing obsolete field/menu/battle mappings are still useful for caller bucketing.

Speculative:
- Exact Korean patch points and safe expansion strategy. No patch should be proposed until the remaining direct caller links and data-size constraints are traced.
