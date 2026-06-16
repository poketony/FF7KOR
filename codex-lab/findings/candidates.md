# FFVII.exe jafont decoder - candidates and working map

Classification rules:
- Confirmed: directly observed in the runtime overlay decompiler output, byte scan, PE metadata, or workspace artifact scan.
- Probable: supported by runtime-adjacent evidence or obsolete/FFNx mapping, but missing a direct caller/callee proof in the 2026 native overlay.
- Speculative: plausible, but not yet supported by direct native evidence.

## Current state

The original on-disk Ghidra import is still not useful for decoder proof because `.text` is protected. The useful dataset is the runtime overlay created from `C:\Users\JO\AppData\Local\Temp\FFVII.DMP`.

No binary patch is proposed in this phase.

## Confirmed `jafont_%d.tim` loader path

| Item | Classification | VA | Evidence |
|---|---|---:|---|
| `jafont_%d.tim` string | confirmed | `0x14164C300` | Exact file string occurs once at file offset `0x164B500` |
| direct loader initializer | confirmed | `0x141571DC0` | Runtime decompile references `&UNK_14164C300` and formats six slots |
| direct string reference instruction | confirmed | `0x141571E19` | `lea rdx, [rip+0x0DA4E0]` targets `0x14164C300` |
| font resource handle/global | confirmed | `0x14207CDEC` | Written by `0x141571DC0`; used by `0x14156DE10` |
| font resource struct pointer | confirmed | `0x14207CE08` | Written by `0x141571DC0`; used by loader/render functions |

Direct loader details:
- Allocates or registers `0x138` bytes through `FUN_141574670(0x2000000, 8, 0x138)`.
- Zeroes the struct.
- Calls `FUN_14003F0D0(slot, "jafont_%d.tim", i)` for `i = 1..6`.
- Uses filename slot stride `0x14`.
- Direct `E8 rel32` callers to `0x141571DC0` were not found in the overlay scan.

Candidate caller status:
- `0x141571D90` is now downgraded to an adjacent pre-loader/thunk candidate, not the direct `jafont_%d.tim` loader.
- `0x1415742D0` remains a possible wrapper/dispatcher candidate.
- `0x141038510` calls `0x14156DE10` and remains a probable broader menu graphics init path, but it is not the direct `jafont_%d.tim` formatter.

## Confirmed font object path

| Function | Classification | Role | Evidence |
|---|---|---|---|
| `0x14156DF3A` | confirmed | load/upload/store six font objects | Checks filename slots and stores handles at `+0x10,+0x24,+0x38,+0x4C,+0x60,+0x74` after `FUN_14004AB00(0x6710AC, 5, ...)` |
| `0x14156DE10` | confirmed | use/release/drawprep six font objects | Checks the same six handle offsets and calls `FUN_14004AB00(0x671082, 1, handle_slot)` |
| `0x1415720BB` | confirmed | select page object and submit glyph | Switches across six handle offsets and calls `FUN_14004AB00(0x66E272, 2, 1, handle)` |

Confirmed struct layout:

| Page | Prefix/source | Filename slot | Object slot |
|---:|---|---:|---:|
| 0 | single-byte/base | `+0x00` | `+0x10` |
| 1 | `FA` | `+0x14` | `+0x24` |
| 2 | `FB` | `+0x28` | `+0x38` |
| 3 | `FC` | `+0x3C` | `+0x4C` |
| 4 | `FD` | `+0x50` | `+0x60` |
| 5 | `FE` | `+0x64` | `+0x74` |

The page mapping above is inferred from the established six-page behavior plus the confirmed six-slot order. The slot order itself is confirmed.

## Confirmed decoder and renderer candidates

| Area | Function | Classification | Evidence |
|---|---|---|---|
| Text parser | `0x1415712E2` | confirmed | `(byte)(b + 6) < 5` detects `FA`-`FE`; stores prefix high byte; consumes next byte |
| Measurement/wrapping | `0x1415714B0` | confirmed | Same `FA`-`FE` check; computes width through `0x141571220`; handles `0xE7/0xE8` line controls |
| Text draw scan | `0x14157251B` | confirmed | Same `FA`-`FE` check; passes combined code to `FUN_141571EC0` |
| Page/glyph render | `0x1415720BB` | confirmed | Selects six font object slots; uses glyph nibble layout for UVs |
| Common draw/spacing | `0x14115AF00` | confirmed | Subtracts `0xFA`, checks `<5`, uses five-entry table at `0x6F563A`, increments input index for second byte |
| Width helper | `0x141571220` | confirmed | Returns capped width using tables around `0x14164C3B0`, `0x14164C7B0`, `0x14164C830` |

Important confirmed behavior:
- `0xFF` terminates text.
- `0x00` is not a string terminator in these observed loops.
- `FA` through `FE` are two-byte page prefixes.
- The second byte is the glyph/code index consumed after the prefix.

## Remaining caller buckets

Confirmed buckets:
- Measurement and wrapping: `0x1415714B0`.
- Common draw/spacing: `0x14115AF00`.
- Text scan/draw: `0x14157251B`.
- Page/object rendering: `0x1415720BB`.

Probable field bucket:

| Candidate | VA | Status |
|---|---:|---|
| `field_submit_draw_text_640x480_6E706D` equivalent | `0x1410FB970` | probable |
| `field_draw_text_boxes_and_text_graphics_object_6ECA68` equivalent | `0x14111D6A0` | probable |
| `field_text_opcode` equivalent | `0x1411F7DC0` | probable |
| `field_text_box_window_opening_6317A9` equivalent | `0x140CB7980` | probable |

Probable menu bucket:

| Candidate | VA | Status |
|---|---:|---|
| `main_menu_draw_everything_maybe_6C0B91` equivalent | `0x1410359F0` | probable |
| `menu_draw_everything_6CC9D3` equivalent | `0x141070D60` | confirmed shortest chain to shared wrappers |
| `engine_load_menu_graphics_objects_6C1468_jp` equivalent | `0x141038510` | probable |

Probable battle bucket:

| Candidate | VA | Status |
|---|---:|---|
| `battle_draw_menu_everything_6CEE84` equivalent | `0x14107E9A0` | probable |
| `draw_text_top_display_6D1CC0` equivalent | `0x14108F490` | probable |

## Resource storage

Confirmed in code:
- The executable generates six resource names from `jafont_%d.tim`.
- The generated names live in a runtime struct rooted at `0x14207CE08`.
- The six loaded object handles are stored in the same struct.

Confirmed in workspace:
- `codex-lab/work/menu_ja_poc/unpacked/jafont_1.tex` through `jafont_6.tex` exist.

Probable:
- The live game loads the six resources from the Japanese menu LGP path.
- `.tim` in the executable and `.tex` in the extraction are two representations/naming stages of the same font pages.

Unresolved:
- The exact archive/container call that resolves `jafont_%d.tim`.
- Full caller chain from field/menu/battle frontend functions into the confirmed decoder/render functions.
- Whether Korean prefixes `C0`-`CC` can be inserted safely without expanding every related table and range check.

## Patch readiness

Ready to start a minimal decoder-path `C0 00` proof-of-concept design.

Still not ready for a production patch because a safe final patch needs:
- Full page table and width table size constraints.
- Confirmation of all places that assume only five extended prefixes.
- Confirmation of asset loading limits for more than six pages.
- A final strategy for whether the six-slot font resource structure can remain fixed or must be extended.

## Current caller-chain narrowing

This section uses the currently open Ghidra program. Function names below are the current Ghidra names; proposed descriptive names are not applied to the database.

### Field candidates

| Candidate chain | Classification | Evidence | Unresolved |
|---|---|---|---|
| `FUN_1411f7dc0` -> `FUN_141570050` -> `FUN_1415712b0` | probable | `FUN_1411f7dc0` is mapped from old `field_text_opcode`; current xref shows a confirmed call to `FUN_141570050` at `0x1412001F9`; JP mode in `FUN_141570050` calls `FUN_1415712b0`. | Exact opcode case and text source still need narrowing. |
| `FUN_1411f7dc0` -> `FUN_141570410` or `FUN_141570730` -> `FUN_1415724a0` -> `FUN_141571ec0` | probable | `FUN_1411f7dc0` has confirmed xrefs into `FUN_141570410` and `FUN_141570730`; those wrappers call the confirmed JP string renderer path. | Which field opcode cases are dialogue, labels, or fixed strings remains unresolved. |
| `FUN_140ce3d10` -> `FUN_14156fd10` -> `FUN_1415714b0` -> `FUN_1415712b0` | probable | `FUN_140ce3d10` directly calls `FUN_14156fd10`; `FUN_14156fd10` directly calls `FUN_1415714b0`; `FUN_1415714b0` calls the width scanner and handles JP line controls. | `FUN_140ce3d10` is very large; exact subsystem name is not confirmed. |
| `FUN_14156fd10` -> `FUN_14111d6a0` -> `FUN_14156e430` -> `FUN_141571ec0` | probable | `FUN_14156fd10` calls `FUN_14111d6a0`; `FUN_14111d6a0` directly calls `FUN_14156e430`; `FUN_14156e430` directly checks FA-FE and calls the glyph renderer. | `FUN_14111d6a0` decompile timed out through MCP, so exact state setup is still incomplete. |
| `FUN_1410fb970` | probable field render-state setup | Old mapping identifies this as `field_submit_draw_text_640x480`; current decompile sets sprite/render object data around `0xDC1020`, but does not directly call the JP decoder. | Needs caller-specific link to `FUN_14156e430` or `FUN_141570xxx` wrappers. |

### Menu candidates

| Candidate chain | Classification | Evidence | Unresolved |
|---|---|---|---|
| `FUN_141070d60` -> `FUN_141077ef0` -> `FUN_14106b4a0` -> shared wrappers | confirmed menu root path | Old mapping identifies `FUN_141070d60` as `menu_draw_everything`; direct-call scan confirms the helper route to `FUN_141570730` and `FUN_141570050`. | Sibling menu helpers still need classification for full menu subpanel coverage. |
| `FUN_141035040` / `FUN_141033da0` -> `FUN_141570050` and `FUN_141570730` | probable menu/common UI text | These functions directly call shared text wrappers; they sit in the same low address region as known menu mappings. | Region alone is insufficient; referenced strings or mode globals need collection. |
| `FUN_141038bf0` -> `FUN_14156df20` and `FUN_14156de10` | probable | Direct xrefs show confirmed upload/store and use/release calls for the six font objects. | This is font resource lifecycle, not a user-facing menu text renderer. |

### Battle candidates

| Candidate chain | Classification | Evidence | Unresolved |
|---|---|---|---|
| `FUN_14107e9a0` | probable battle root | Old mapping identifies this as `battle_draw_menu_everything`; current xrefs include dispatcher data and a direct call from `FUN_1400e2610`. | Decompile does not directly call the confirmed JP decoder; likely delegates through helpers. |
| `FUN_1410798e0` -> `FUN_141570050`, `FUN_141570410`, `FUN_141570730` | confirmed battle/helper text wrapper entry | Current decompile and direct-call scan show direct calls into shared text wrappers and battle/menu state globals/text pointers around `0x91B...` and `0xDC16B4`. | Exact screen labels still need mapping. |
| `FUN_14108f5f0` -> `FUN_14156e120` -> `FUN_1415712b0` and `FUN_1415724a0` | probable name-entry/input or battle-top branch | Direct xref shows `FUN_14108f5f0` calls `FUN_14156e120`; `FUN_14156e120` scans FA-FE and then measures/renders. | Exact UI role is unresolved; old mapping nearby suggests battle/top-display but direct strings are needed. |

### Non-rendering consumers

| Consumer | Classification | Evidence | Why it matters |
|---|---|---|---|
| `FUN_1415712b0` | confirmed | Direct FA-FE scanner; calls width helper `0x141571220`; does not render glyph vertices. | Any new page prefix range must be reflected here or widths will desync. |
| `FUN_1415714b0` | confirmed | Field/textbox measurement-layout scanner; handles `0xE7/0xE8` and calls `FUN_1415712b0`. | Wrapping and box sizing can fail even if rendering works. |
| `FUN_141570050` | confirmed | Shared dispatch wrapper; JP mode calls measurement scanner. | Many upper UI systems enter measurement through this wrapper. |
| `FUN_14156ffe0` | confirmed | Alternate wrapper; JP mode calls measurement scanner. | It has a separate caller set and may need parallel patching. |
| `FUN_14156e120` | confirmed | Scans FA-FE before calling measure/render; has preprocessing behavior; exact input/name role remains probable. | It may advance cursors or copy buffers independently of the main render scanner. |

### Independent FA-FE inspectors

Confirmed independent inspectors beyond the already-known decoder path:
- `FUN_14156e430` independently checks FA-FE, consumes the second byte, and calls `FUN_141571ec0`.
- `FUN_14156e120` independently scans FA-FE before calling measurement and render helpers.
- `FUN_14115af00` independently subtracts `0xFA`, checks a five-entry table, and advances the text index for the second byte.

Current conclusion:
- One common low-level glyph renderer is shared: `FUN_141571ec0`.
- One common JP width scanner/helper path is shared: `FUN_1415712b0` and `FUN_141571220`.
- Field/menu/battle appear to use separate upper wrappers around common low-level JP renderer/measurement code.
- Completely separate decoding paths are not supported by current evidence, but `FUN_14115af00` remains a legacy/common path with its own FA-FE handling and must not be ignored.

## Menu/battle helper trace update

Confirmed menu entry into wrapper layer:

| Candidate chain | Classification | Evidence | Unresolved |
|---|---|---|---|
| `FUN_141070d60` -> `FUN_141077ef0` -> `FUN_14106b4a0` -> `FUN_141570730` -> `FUN_1415724a0` -> `FUN_141571ec0` | confirmed | Direct call sites: `0x141071211`, `0x14107806A`, `0x14106BCEA`; `FUN_141570730` JP branch calls the shared string scanner. | Exact user-facing menu subpanel for this helper is not named. |
| `FUN_141070d60` -> `FUN_141077ef0` -> `FUN_14106b4a0` -> `FUN_141570050` -> `FUN_1415712b0` -> `FUN_141571220` | confirmed | Direct call sites: `0x141071211`, `0x14107806A`, `0x14106BD10`; `FUN_141570050` JP branch calls the shared width scanner. | Need enumerate every text pointer used by this helper. |
| `FUN_1410734f0` -> `FUN_1410732b0` -> `FUN_141570730` | confirmed | `FUN_1410734f0` builds temporary `0xFF`-terminated buffers with `FUN_1410732b0`, then calls the shared render wrapper multiple times. | Exact source strings and menu labels need mapping. |

Confirmed battle/helper entry into wrapper layer:

| Candidate chain | Classification | Evidence | Unresolved |
|---|---|---|---|
| `FUN_1410798e0` -> `FUN_141570730` -> `FUN_1415724a0` -> `FUN_141571ec0` | confirmed | Direct call sites include `0x14107A8D4`, `0x14107A962`, and later `FUN_141570730` calls; decompile shows text pointers such as `0x91B002`, `0x91B034`, `0x91ACB0`, `0x91ACE2`, `0x91AD14`. | Exact battle/menu screen names still need labeling. |
| `FUN_1410798e0` -> `FUN_141570410` -> `FUN_1415724a0` -> `FUN_141571ec0` | confirmed | Direct call sites include `0x14107AAB3` and later `FUN_141570410` calls; wrapper forces the string-render flag argument to `1`. | Exact flag semantics remain unresolved. |
| `FUN_1410798e0` -> `FUN_141570050` -> `FUN_1415712b0` -> `FUN_141571220` | confirmed | Direct call site `0x14107AED1`; result width is used through `DAT_1420395B8`. | Need determine whether every battle text layout path also uses this wrapper. |
| `FUN_14107e9a0` -> text wrapper layer | probable | Direct calls in `FUN_14107e9a0` go to `FUN_140DB5DF0`, `FUN_140061A50`, and repeated `FUN_140E2BC10`, not directly to `FUN_141570xxx`. | This root likely delegates through submitted helper/callback work; the indirect transition remains unresolved. |

Argument-role summary:
- `FUN_141570730` and `FUN_141570410` use stack `+0x4`/`+0x8` for x/y, stack `+0xC` for the text pointer handle, stack `+0x10` for style/color selector, and stack `+0x14` for z/depth-like state.
- `FUN_141570050` uses stack `+0x4` as the text pointer handle and returns width through `DAT_1420395B8`.
- Font page and glyph state are not carried by these upper helper arguments; they are calculated by `FUN_1415724a0` and `FUN_141571ec0` from the text bytes.
- `FUN_1410732b0` is a string-buffer conversion helper: it reads until `0x00`, writes converted bytes, and appends `0xFF`. It does not implement FA-FE page decoding.

New non-rendering finding:
- No additional independent FA-FE check was found in the newly traced menu/battle helper layer.
- The non-rendering paths still known to require C0-CC support are `FUN_1415712b0`, `FUN_1415714b0`, `FUN_141570050`, `FUN_14156ffe0`, `FUN_14156e120`, and `FUN_14115af00`.

Patch-readiness note:
- Evidence is now sufficient to start designing a minimal decoder-path `C0 00` proof-of-concept.
- Evidence is not sufficient for production expansion of the six-slot font resource structure or for final Korean page allocation.
