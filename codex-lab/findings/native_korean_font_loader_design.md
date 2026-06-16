# Native Korean Font Loader Design

Status: revised after rejecting the loose-file Korean page loader.

## Design Decision

Use the game's existing `menu_ja.lgp` resource mechanism.

The Korean font pages are inserted into `menu_ja.lgp` as:

```text
jafont_7.tex
jafont_8.tex
...
jafont_19.tex
```

At runtime the patcher asks the native loader to resolve logical names:

```text
jafont_7.tim
jafont_8.tim
...
jafont_19.tim
```

This follows the original `jafont_%d.tim` path while letting the archive contain the extracted TEX resources, as with the existing Japanese font files.

## Why The Original Six-Slot Struct Is Not Expanded In Place

The confirmed font allocation is `0x138` bytes. Existing Japanese pages use six `0x14`-stride slots:

```text
+0x10
+0x24
+0x38
+0x4C
+0x60
+0x74
```

Fixed data follows after `+0x78`. Extending the same slot array to page 19 would overwrite fixed fields and run past the original allocation. Therefore the patcher must not blindly change the original load loop from `1..6` to `1..19` inside that same structure.

## Current Safe Architecture

The patcher:

1. Allocates patcher-owned remote state inside the live `FFVII.exe` process.
2. Installs a hook at `FUN_14156df20`, RVA `0x156E100`.
3. Reuses the native resource command `FUN_14004AB00(0x6710AC, 5, ...)`.
4. Temporarily writes `jafont_7.tim` through `jafont_19.tim` into the existing filename scratch slot at font struct `+0xB8`.
5. Stores returned native handles in the remote state array:

```text
remote_state.extra_jafont_handles[0]  -> C0 -> jafont_7
remote_state.extra_jafont_handles[1]  -> C1 -> jafont_8
...
remote_state.extra_jafont_handles[12] -> CC -> jafont_19
```

The original six Japanese handles remain untouched.

## Decoder And Renderer Link

Scanner hooks convert `C0..CC` into the same pending-prefix form used by Japanese multibyte characters:

```text
pending_prefix = lead << 8
```

The existing loop then consumes the trail byte on the next iteration. The renderer receives selectors such as `C0 1A`, `C2 60`, or `C9 A9`.

The glyph renderer hook intercepts only selectors whose high byte is `C0..CC`, selects the corresponding remote-state handle, and joins the original glyph UV/render path. The original `FA..FE` Japanese path remains unchanged.

## Required Resource Setup

Insert the generated pages from:

```text
C:\Users\JO\FF7KOR\codex-lab\generated\menu_ja_jafont_ext_tbl_exact
```

into the game's `menu_ja.lgp`.

Do not copy Korean font pages beside `FFVII.exe`. Do not use `korean_c0_page.tim`.

## Runtime Risk

The hook must be installed before the original font-loader lifecycle has fully passed. The practical procedure is:

1. Start `c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 ...` first.
2. When it waits for `FFVII.exe`, launch the game.
3. Continue only after the patcher reports that `jafont_7..19` handles were created and scanner/renderer hooks were installed.

If the patcher is started too late, handle creation can time out safely.
