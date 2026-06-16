# FFVII.exe jafont caller chains

Scope: continuation analysis against the currently open Ghidra program for the 2026 native 64-bit `FFVII.exe` runtime overlay.

Rules used here:
- Ghidra function names are recorded as observed. Proposed descriptive names are not applied to the database.
- `confirmed` means the current Ghidra program directly shows the call, reference, range check, or global use.
- `probable` means the lower edge is directly observed but the higher-level subsystem name still depends on obsolete/FFNx mapping or incomplete caller context.
- `speculative` is used only where the branch is plausible but still lacks a direct native edge.

## Core shared path

| Address | RVA | Current name | Proposed name | Callers | Callees | Strings | Globals | Classification | Evidence | Unresolved |
|---:|---:|---|---|---|---|---|---|---|---|---|
| `0x141571D90` | `0x1571D90` | `FUN_141571d90` | `jafont_init_or_language_font_resource_init` | `FUN_1415742d0` at `0x1415742D4`; data refs `0x1420C8650`, `0x1416BCB60`, `0x1416BCB70` | `func_0x14157ae60`, `_time64`, `srand`, `FUN_141574670`, `FUN_14003f0a0`, memset-like helper, `FUN_14003f0d0` | `jafont_%d.tim` at `0x14164C300` | `DAT_14207AE5C`, `0x14207CDEC`, `0x14207CE08`, static data `0x14164C2C0..0x14164C2F8` | confirmed | Contains the old `0x141571DC0` loader block, allocates/zeros `0x138`, formats six names with stride `0x14`. | Exact archive resolver after name creation is still unresolved. |
| `0x14156DF20` | `0x156DF20` | `FUN_14156df20` | `jafont_load_upload_store_six_objects` | `FUN_141038bf0` at `0x141039B4A`; data refs `0x1420C8404`, `0x1416BCBD8`, `0x1416BCBE8` | `FUN_14004ab00(0x6710AC, 5, ...)`, `FUN_14003f0a0` | none direct | `DAT_14207AE5C`, `0x14207CDEC`, `0x14207CE08` filename slots and object slots | confirmed | Stores six object handles at `+0x10,+0x24,+0x38,+0x4C,+0x60,+0x74`. | Exact graphics object type is still inferred from call behavior. |
| `0x14156DE10` | `0x156DE10` | `FUN_14156de10` | `jafont_release_or_submit_six_objects` | `FUN_141038510` at `0x141038616`, `FUN_141038bf0` at `0x141038D46`; data ref `0x1420C83F8` | `FUN_14004ab00(0x671082, 1, ...)`, `FUN_14003f0a0` | none direct | `DAT_14207AE5C`, `0x14207CDEC`, `0x14207CE08` object slots | confirmed | Checks all six font object slots and submits/releases them through a common graphics call. | The `0x671082` operation name is not confirmed. |
| `0x141571220` | `0x1571220` | `FUN_141571220` | `jafont_glyph_width_helper` | `FUN_1415712b0`, `FUN_1415714b0`, `FUN_141571ec0` from decompiler/xrefs | table reads only | none direct | width tables near `0x14164C3B0`, `0x14164C7B0`, `0x14164C830` | confirmed | Returns capped glyph width; used by measurement and renderer advance logic. | Table dimensions and all page bounds still need full enumeration. |
| `0x1415712B0` | `0x15712B0` | `FUN_1415712b0` | `jafont_measure_string_width_scan` | `FUN_141570050`, `FUN_14156ffe0`, `FUN_1415714b0`, `FUN_14156e120`, recursive self-calls; data refs `0x1420C85E4`, `0x1416BCF04`, `0x1416BCF14` | `FUN_141571220`, recursive self-calls | static special strings around `0x14164C310`, `0x14164C8B0` | width tables and JP mode-dependent static text | confirmed | Reads until `0xFF`; `0x00` is printable; `(byte)(b+6)<5` detects `FA-FE`; stores prefix and consumes second byte. | Need exact mapping of every special `FD/FE` expansion string. |
| `0x141571EC0` | `0x1571EC0` | `FUN_141571ec0` | `jafont_render_glyph_common` | `FUN_141570230`, `FUN_141570320`, `FUN_1415724a0`, `FUN_14156e430`; data refs `0x1420C8680`, `0x1416BCF58`, `0x1416BCF98` | `FUN_14004ab00(0x66E272,2,1,handle)`, `FUN_141571220`, `FUN_14003f0a0` | none direct | `0x14207CE08` object slots, `0x14164B030`, `0x14164B034`, `0x14164D010` | confirmed | Switches page `0,FA,FB,FC,FD,FE` to six object slots and writes glyph UV vertices from low-byte nibbles. | Whether more page slots can be addressed safely is not known. |
| `0x1415724A0` | `0x15724A0` | `FUN_1415724a0` | `jafont_render_string_scan` | `FUN_141570410`, `FUN_141570510`, `FUN_141570610`, `FUN_141570730`, `FUN_141570c30`, `FUN_14156e120`, `FUN_14156e430`, recursive self-calls | `FUN_141571ec0`, recursive self-calls | static special strings around `0x14164C...` | render state globals including `0x91AA8C` path | confirmed | String renderer consumes `FA-FE` second bytes and sends combined code to the common glyph renderer. | Need classify every wrapper caller into field/menu/battle. |
| `0x14115AF00` | `0x115AF00` | `FUN_14115af00` | `legacy_common_submit_draw_char_or_spacing` | `FUN_141570050`, `FUN_14156ffe0`; data ref `0x1420BD7A8` | width/spacing table users | none direct | text buffer/index globals, width table pointer `0xDB958C` | confirmed | Independently subtracts `0xFA`, checks a five-entry table, increments input index, and consumes the second byte. | This legacy/common path may need separate prefix support even if JP renderer is patched. |

## Field Chain

| Address | RVA | Current name | Proposed name | Callers | Callees | Strings | Globals | Classification | Evidence | Unresolved |
|---:|---:|---|---|---|---|---|---|---|---|---|
| `0x1411F7DC0` | `0x11F7DC0` | `FUN_1411f7dc0` | `field_text_opcode_handler_candidate` | data ref `0x1420BE534` | `FUN_141570050`, `FUN_141570410`, `FUN_141570730`, `FUN_141570c30` by xrefs-to those wrappers | not collected | not collected | probable | Old mapping names this as `field_text_opcode`; current xrefs show calls to shared text wrappers including `0x1412001F9 -> FUN_141570050`. | Exact opcode cases and string sources are not narrowed. |
| `0x1410FB970` | `0x10FB970` | `FUN_1410fb970` | `field_submit_draw_text_render_state_setup` | `FUN_141218130`, `FUN_1411a51f0`, `FUN_14121d790`, `FUN_14123d440`; data refs `0x1420BD208`, `0x1416B5B44`, `0x1416B5B7C` | `FUN_141078840`, render object helpers | none direct | `0xDC1020`, render object offsets `+0x70`, `+0x78`, `+0x7C` | probable | Old mapping names this `field_submit_draw_text_640x480`; current decompile sets render object state but does not directly call JP decoder. | Needs direct link to `FUN_14156e430` or `FUN_141570xxx` wrappers. |
| `0x140CE3D10` | `0x0CE3D10` | `FUN_140ce3d10` | `field_scene_or_ui_frame_textbox_driver_candidate` | not fully enumerated | `FUN_14156fd10` at `0x140CE542B` | none collected | many field/state globals including `0xCFF...`, `0xCC...`, `0xDC0BFA` | probable | Directly calls `FUN_14156fd10`, which performs JP textbox measurement/layout. | Function is very large; exact subsystem name is unconfirmed. |
| `0x14156FD10` | `0x156FD10` | `FUN_14156fd10` | `field_textbox_layout_jp_wrapper` | `FUN_140ce3d10`; data refs `0x1420C8464`, `0x1416BCD7C`, `0x1416BCD8C` | `FUN_1415714b0`, `FUN_14111d6a0` | static text refs `DAT_14164CEF8`, `DAT_14164CF18`, `DAT_14164CF58` | `DAT_14207AE5C`, textbox state globals | confirmed | In JP mode calls `FUN_1415714b0` for width/height and then `FUN_14111d6a0`; clamps box dimensions. | Exact field text-box struct layout remains unresolved. |
| `0x14111D6A0` | `0x111D6A0` | `FUN_14111d6a0` | `field_textbox_draw_coordinator_candidate` | `FUN_14156fd10`, `FUN_141349c20`, `FUN_14134ab70`, `FUN_14134b520`; data refs `0x1420BD418`, `0x1416B5DA0`, `0x1416B5DB0` | `FUN_14156e430` at `0x141120903` | not collected | not collected | probable | Direct lower edge to confirmed JP render scanner; old mapping associates this area with field text boxes. | MCP decompile timed out; render-state setup evidence is incomplete. |
| `0x14156E430` | `0x156E430` | `FUN_14156e430` | `jp_field_text_render_scan_candidate` | `FUN_14111d6a0`; data refs `0x1420C8434`, `0x1416BCC2C`, `0x1416BCC5C` | `FUN_141571ec0`, `FUN_1415724a0`, `FUN_1410fc900` in non-JP branch | static refs near `0x14164D020`, `0x14164D028`, and special string paths | `0xDC3CCC`, `0xDC3CB4`, `0xDC3CB8`, `0xDC3CB0`, `0xDC3CC0`, `0xDC3CC4`, `0xDC3CEC`, `0x91F028`, `0xDC3CC8` | confirmed | Independently checks FA-FE, consumes second byte, and calls common glyph renderer; field role remains probable. | Need confirm whether it is field-only or shared with another UI branch. |

Field chain status:
- Confirmed lower edges: `FUN_14156fd10 -> FUN_1415714b0 -> FUN_1415712b0`; `FUN_14111d6a0 -> FUN_14156e430 -> FUN_141571ec0`; `FUN_1411f7dc0 -> shared text wrappers`.
- Probable field roots: `FUN_1411f7dc0`, `FUN_1410fb970`, `FUN_14111d6a0`.

## Menu Chain

| Address | RVA | Current name | Proposed name | Callers | Callees | Strings | Globals | Classification | Evidence | Unresolved |
|---:|---:|---|---|---|---|---|---|---|---|---|
| `0x141070D60` | `0x1070D60` | `FUN_141070d60` | `menu_draw_everything_candidate` | data ref `0x1420BC35C` | `FUN_141077540`, `FUN_1410788c0`, `FUN_141077ef0`, `FUN_141071b50`, `FUN_141074d10`, other menu helpers | none collected | `0xDC12CC`, `0xDC12D8`, `0xDC1110`, `0xDC12F4`, `0xDC12F8`, `0xCBF9DC`, `0xCC0D84` | confirmed | Old mapping names this `menu_draw_everything`; direct-call scan confirms the route `FUN_141070d60 -> FUN_141077ef0 -> FUN_14106b4a0 -> shared wrappers`. | Exact user-facing subpanel names are still unresolved. |
| `0x141035040` | `0x1035040` | `FUN_141035040` | `menu_or_common_ui_text_caller_candidate` | not enumerated | `FUN_141570050`, `FUN_141570730` | not collected | not collected | probable | Current xrefs show direct calls to shared text wrappers; address region overlaps menu mapping set. | Needs strings/globals before menu classification can be confirmed. |
| `0x141033DA0` | `0x1033DA0` | `FUN_141033da0` | `menu_or_common_ui_text_caller_candidate_2` | not enumerated | `FUN_141570050`, `FUN_141570730` | not collected | not collected | probable | Directly calls shared text wrappers multiple times. | Needs direct menu-specific evidence. |
| `0x141038BF0` | `0x1038BF0` | `FUN_141038bf0` | `menu_font_graphics_resource_lifecycle_candidate` | `FUN_141037150`, `FUN_1410758f0`, `FUN_141075f90`; data ref `0x1420BBD68` | `FUN_14156df20`, `FUN_14156de10` | none direct | font resource globals through callees | probable | Directly calls the confirmed six-object upload/store and release/submit functions; menu lifecycle role remains probable. | It proves resource lifecycle, not menu text rendering. |
| `0x141077EF0` | `0x1077EF0` | `FUN_141077ef0` | `menu_draw_helper_dispatch_to_text_state` | `FUN_141070d60` at `0x141071211` and `0x141071482`; other callers include `FUN_14108f5f0` by direct scan | `FUN_14106b4a0`, `FUN_1410623d0`, `FUN_1411a0d80` | not collected | menu state globals through callees | confirmed | Shortest confirmed menu route to text wrappers goes through `FUN_141077ef0 -> FUN_14106b4a0`. | Exact helper purpose needs naming. |
| `0x14106B4A0` | `0x106B4A0` | `FUN_14106b4a0` | `menu_text_state_and_wrapper_entry` | `FUN_141077ef0` at `0x14107806A` | `FUN_141570730`, `FUN_141570050`, `FUN_141048b30`, `FUN_141049c80` | text pointer `0x91AA28` observed before wrapper calls | menu globals `0xDC1294`, `0xDC1298`, `0xDC1120`, `0xDC1138`, `0xDC12E8`, `0xDC12EC`, `0xDC130C` | confirmed | Direct calls `FUN_141570730` at `0x14106BCEA` and `FUN_141570050` at `0x14106BD10`; this confirms menu render and width entry into shared wrappers. | Need enumerate all text pointers in this helper. |
| `0x1410732B0` | `0x10732B0` | `FUN_1410732b0` | `menu_c_string_to_ff7_text_buffer` | `FUN_1410734f0`, `FUN_1410798e0` by decompile/direct scan | byte copy/conversion helper `FUN_141567f80` | source is caller-provided, not a fixed global | caller-provided source/destination buffers | confirmed | Reads bytes until `0x00`, writes adjusted bytes, and appends `0xFF`; no FA-FE decoding observed. | Need identify every caller buffer. |
| `0x1410734F0` | `0x10734F0` | `FUN_1410734f0` | `menu_temp_string_render_helper` | direct helper route from menu draw path remains partially indirect; direct xrefs to wrappers are confirmed | `FUN_1410732b0`, `FUN_141570730`, `FUN_141180df0` | source globals include `0xDC1298`, `0xDC1294`, `0xDC1224`, `0x91AB04`, `0xDC112C` | menu globals `0xDC130C`, `0xDC08B8`, `0xDC1154`, `0xDC128C` | confirmed | Builds temporary strings with `FUN_1410732b0` and then calls `FUN_141570730` at multiple sites. | Exact on-screen menu labels need mapping. |

Menu chain status:
- `FUN_141070d60` now has a confirmed shortest render chain to the common glyph renderer: `FUN_141070d60 -> FUN_141077ef0 -> FUN_14106b4a0 -> FUN_141570730 -> FUN_1415724a0 -> FUN_141571ec0`.
- `FUN_141070d60` also has a confirmed shortest width chain: `FUN_141070d60 -> FUN_141077ef0 -> FUN_14106b4a0 -> FUN_141570050 -> FUN_1415712b0 -> FUN_141571220`.
- Resource lifecycle is directly tied through `FUN_141038bf0`, but that is not a text rendering caller by itself.

## Battle Chain

| Address | RVA | Current name | Proposed name | Callers | Callees | Strings | Globals | Classification | Evidence | Unresolved |
|---:|---:|---|---|---|---|---|---|---|---|---|
| `0x14107E9A0` | `0x107E9A0` | `FUN_14107e9a0` | `battle_draw_menu_everything_candidate` | `FUN_1400e2610`; data refs `0x1420BC5A8`, `0x1416B51F0`, `0x1416B5200` | `FUN_140061a50`, `FUN_140db5df0`, `FUN_140e2bc10` | none collected | `0xDC38C4`, `0xDC1F38`, `0xDC1998`, `0xDC16DC`, `0xDC16E4`, `0xDC1788` | probable | Old mapping names this `battle_draw_menu_everything`; current decompile shows battle/menu state handling but no direct JP decoder call. | Direct root-to-wrapper transition remains unresolved and may be callback/submission based. |
| `0x1410798E0` | `0x10798E0` | `FUN_1410798e0` | `battle_menu_text_wrapper_entry` | `FUN_14103c5c0`; data refs `0x1420BC4AC`, `0x1416B5180`, `0x1416B51CC`, `0x1416B51DC` | `FUN_141570050`, `FUN_141570410`, `FUN_141570730`, `FUN_1410732b0`, `FUN_141180df0` | text pointers observed: `0x91B002`, `0x91B034`, `0x91ACB0`, `0x91ACE2`, `0x91AD14` | battle/menu state globals `0x91B...`, `0xDC1350`, `0xDC1354`, `0xDC16B4`, `0xDC16B8` | confirmed | Direct calls to shared wrappers: `0x14107A8D4` and `0x14107A962` -> `FUN_141570730`; `0x14107AAB3` -> `FUN_141570410`; `0x14107AED1` -> `FUN_141570050`. | Exact battle screen labels need mapping. |
| `0x14108F5F0` | `0x108F5F0` | `FUN_14108f5f0` | `name_entry_or_battle_top_text_root_candidate` | `FUN_1410aaef0`; data refs `0x1420BC758`, `0x1416B5350`, `0x1416B5360` | `FUN_14156e120` at `0x14108FEBA` | not collected | not collected | probable | Direct caller of a confirmed FA-FE scanning preprocess/measure/render helper. | Exact UI role remains unresolved. |

Battle chain status:
- Confirmed battle/helper render chains: `FUN_1410798e0 -> FUN_141570730 -> FUN_1415724a0 -> FUN_141571ec0` and `FUN_1410798e0 -> FUN_141570410 -> FUN_1415724a0 -> FUN_141571ec0`.
- Confirmed battle/helper width chain: `FUN_1410798e0 -> FUN_141570050 -> FUN_1415712b0 -> FUN_141571220`.
- `FUN_14107e9a0` remains a probable battle root from mapping, but no direct JP decoder edge was found in that root.

## Measurement Chain

| Address | RVA | Current name | Proposed name | Callers | Callees | Strings | Globals | Classification | Evidence | Unresolved |
|---:|---:|---|---|---|---|---|---|---|---|---|
| `0x141570050` | `0x1570050` | `FUN_141570050` | `text_width_or_draw_dispatch_wrapper` | `FUN_1411f7dc0`, `FUN_141035040`, `FUN_141033da0`, `FUN_1410798e0`, `FUN_141048b30`, `FUN_14106b4a0`, many others | `FUN_1415712b0`, `FUN_14115af00`, `FUN_14004ab00` path | none direct | `DAT_14207AE5C`, `0x14207CE08`, `0x1420395B8` | confirmed | JP mode calls `FUN_1415712b0`; non-JP/legacy path calls `FUN_14115af00`. | Caller classification remains broad. |
| `0x14156FFE0` | `0x156FFE0` | `FUN_14156ffe0` | `text_width_or_draw_dispatch_wrapper_alt` | `FUN_1410cda50`, `FUN_1411bc360`; data ref `0x1420C8488` | `FUN_1415712b0`, `FUN_141570050`, `FUN_14115af00` | none direct | `DAT_14207AE5C` | confirmed | Alternate entry to the same JP measurement scanner. | Need identify why two dispatch wrappers exist. |
| `0x1415714B0` | `0x15714B0` | `FUN_1415714b0` | `field_textbox_measure_and_layout_jp` | `FUN_14156fd10`; data ref `0x1420C8608` | `FUN_1415712b0`, `FUN_141571220` | static special strings via nested calls | text-box layout outputs through pointer args; source text base `0xCBF578`; savemap/global accessor `0xDBFD38` | confirmed | Handles FA-FE, line breaks, width/height output, and layout controls. | Need exact caller-specific box struct fields. |
| `0x14156E120` | `0x156E120` | `FUN_14156e120` | `jp_centered_string_preprocess_measure_render` | `FUN_14108f5f0`; data ref `0x1420C8428` | `FUN_1415712b0`, `FUN_1415724a0` | static bytes around `0x14164D030` | output/preprocess buffer pointers | confirmed | Scans text and FA-FE before measuring and rendering centered output; input/name role remains probable. | Determine whether this is name entry, battle top display, or another input-related UI. |

Measurement status:
- High priority non-rendering consumers are `FUN_1415712b0`, `FUN_1415714b0`, `FUN_141570050`, `FUN_14156ffe0`, and the pre-scan part of `FUN_14156e120`.

## Wrapping/Layout Chain

| Address | RVA | Current name | Proposed name | Callers | Callees | Strings | Globals | Classification | Evidence | Unresolved |
|---:|---:|---|---|---|---|---|---|---|---|---|
| `0x14156FD10` | `0x156FD10` | `FUN_14156fd10` | `field_textbox_layout_jp_wrapper` | `FUN_140ce3d10` | `FUN_1415714b0`, `FUN_14111d6a0` | `DAT_14164CEF8`, `DAT_14164CF18`, `DAT_14164CF58` | text-box state globals, `DAT_14207AE5C` | confirmed | JP branch measures active text boxes and clamps dimensions before drawing continuation. | Exact relation to field root needs more upstream evidence. |
| `0x1415714B0` | `0x15714B0` | `FUN_1415714b0` | `field_textbox_measure_and_layout_jp` | `FUN_14156fd10` | `FUN_1415712b0`, `FUN_141571220` | static special strings through recursive width calls | source text base `0xCBF578`; state accessor `0xDBFD38` | confirmed | Handles `0xE7/0xE8`, FA-FE, and output width/height. | Need confirm all line-wrap exit cases. |

Wrapping/layout status:
- This is the strongest direct field-style measurement chain found so far.

## Shared Render Wrapper Branches

| Address | RVA | Current name | Proposed name | Callers | Callees | Strings | Globals | Classification | Evidence | Unresolved |
|---:|---:|---|---|---|---|---|---|---|---|---|
| `0x141570230` | `0x1570230` | `FUN_141570230` | `render_single_glyph_wrapper` | `FUN_1410c78d0`; data ref `0x1420C84A0` | `FUN_141571ec0`, legacy `FUN_14115b800` | none direct | `DAT_14207AE5C`, `0x1420395B8` | confirmed | JP mode calls common glyph renderer for a single code. | System bucket unresolved. |
| `0x141570320` | `0x1570320` | `FUN_141570320` | `render_single_glyph_alt_wrapper` | `FUN_14115d910`, `FUN_141218130`, `FUN_1410cda50`, `FUN_1411b2ed0`; data ref `0x1420C84AC` | `FUN_141571ec0`, legacy `FUN_14115b800` | none direct | `DAT_14207AE5C`, `0x1420395B8` | confirmed | JP mode calls common glyph renderer with alternate flag. | System bucket unresolved. |
| `0x141570410` | `0x1570410` | `FUN_141570410` | `render_string_wrapper_alt_scale` | Broad caller fan-in; includes `FUN_1411f7dc0`, `FUN_1410798e0`, many UI functions | `FUN_1415724a0`, `FUN_141570730`, `FUN_14115d910` | none direct | `DAT_14207AE5C` | confirmed | JP mode calls common string renderer. | Caller fan-in is too broad for subsystem classification. |
| `0x141570510` | `0x1570510` | `FUN_141570510` | `render_string_wrapper_fixed_z` | `FUN_141059560`; data ref `0x1420C84C4` | `FUN_1415724a0`, `FUN_141570730`, `FUN_14115d910` | none direct | `DAT_14207AE5C`, `0x14164D01C` | confirmed | JP mode calls common string renderer with fixed Z/depth. | Exact UI role unresolved. |
| `0x141570610` | `0x1570610` | `FUN_141570610` | `render_string_wrapper_palette_or_selector` | `FUN_141218130`; data refs `0x1420C84D0`, `0x1416BCD50`, `0x1416BCD60` | `FUN_1415724a0`, `FUN_141570730` | none direct | `DAT_14207AE5C` | confirmed | JP mode maps selector values and calls common string renderer. | Exact selector semantics unresolved. |
| `0x141570730` | `0x1570730` | `FUN_141570730` | `render_string_dispatch_or_object_lookup` | Very broad caller fan-in including `FUN_1411f7dc0`, `FUN_1410798e0`, `FUN_141035040`, `FUN_141033da0` | `FUN_1415724a0`, object lookup path, `FUN_14004ab00(0x6F5B03,5,...)` | none direct | object lookup globals and `DAT_14207AE5C` | confirmed | JP path calls common string renderer; non-JP path uses object lookup/render call. | Caller fan-in requires per-subsystem narrowing. |
| `0x141570C30` | `0x1570C30` | `FUN_141570c30` | `render_fixed_jp_string_wrapper` | `FUN_1410d5ee0`, `FUN_1411ce6e0`, `FUN_1411f7dc0`; data ref `0x1420C8548` | `FUN_1415724a0`, legacy `FUN_14116ca40` | `0x14164CF6C` fixed string | `DAT_14207AE5C`, `0x14164B024` | confirmed | JP path renders a fixed/static string through the common string renderer. | Exact fixed-string meaning is unresolved. |

## Unresolved Branches

| Branch | Classification | Current evidence | Next evidence needed |
|---|---|---|---|
| Extra menu helper branches outside the shortest `FUN_141077ef0 -> FUN_14106b4a0` route | probable | The shortest render and width routes from `FUN_141070d60` are confirmed, but sibling helpers such as `FUN_141071b50` and `FUN_141074d10` still need classification. | Follow sibling helpers only if full menu subpanel coverage is required. |
| Exact battle text chain from `FUN_14107e9a0` to shared wrappers | probable | Battle root mapping and state globals are present; nearby/helper `FUN_1410798e0` calls shared wrappers. | Follow `FUN_140e2bc10` data/object submissions or locate referenced battle strings. |
| `FUN_14108f5f0 -> FUN_14156e120` UI role | probable | Direct call to FA-FE preprocessor/measure/render helper. | Collect strings/globals and caller context from `FUN_1410aaef0`. |
| `FUN_14115af00` legacy/common path integration | confirmed | It independently consumes FA-FE and is called by dispatch wrappers; caller semantics remain unresolved. | Determine whether field/menu/battle still enter this path in JP mode fallbacks or only non-JP paths. |
| Complete list of functions that advance input without rendering | probable | Confirmed current set: `FUN_1415712b0`, `FUN_1415714b0`, `FUN_14156e120`, `FUN_14115af00`. | Search for additional `FA-FE` range checks and cursor increments beyond the confirmed set. |

## Shared Decoder/Renderer Answer

Current evidence supports:
- One common JP glyph renderer: `FUN_141571ec0`.
- One common JP string renderer/scanner: `FUN_1415724a0`, plus an independent JP scanner `FUN_14156e430` that still calls the same glyph renderer.
- One common JP width scanner/helper path: `FUN_1415712b0` and `FUN_141571220`.
- Separate field/menu/battle upper wrappers around common lower-level renderer/measurement code.

Current evidence does not support:
- Completely separate field/menu/battle glyph renderers.
- Expanding the six-slot resource structure.
- Production expansion of the six-slot font resource structure or final Korean page allocation.
