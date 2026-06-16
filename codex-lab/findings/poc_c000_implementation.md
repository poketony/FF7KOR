# Menu jafont Extension Runtime Implementation

Status: supersedes the old `C0 00 -> FA 00` and loose `korean_c0_page.tim` proof-of-concepts.

Current implementation target:

- Use `menu_ja.lgp` resources named `jafont_7.tex` through `jafont_19.tex`.
- Keep `FFVII.exe` unmodified on disk.
- Patch only live runtime-decrypted code.
- Interpret `C0..CC` as Korean two-byte lead bytes.
- Preserve the original `FA..FE` Japanese path.

## Asset Policy

Generated assets are under:

```text
codex-lab/generated/menu_ja_jafont_ext_tbl_exact
```

The mapping is exact `ff7K(PC).tbl` page/slot positioning:

```text
C0 xx -> jafont_7.tex slot xx
C1 xx -> jafont_8.tex slot xx
...
CC xx -> jafont_19.tex slot xx
```

Example:

```text
C0 1B -> jafont_7.tex, slot 0x1B, row 2, column 12
```

No entry is compacted, shifted, sorted by Unicode, or moved away from its original table byte.

## Runtime Loader Strategy

The original six-slot allocation cannot be extended in place. The six original `0x14`-stride slots end at `+0x78`; fixed data follows, and page 19 would run past the original `0x138` allocation.

The patcher therefore:

1. Installs a loader epilogue hook at `FUN_14156df20`, RVA `0x156E100`.
2. Reuses the native resource loader command at `FUN_14004AB00` with command id `0x6710AC`.
3. Temporarily writes logical names `jafont_7.tim` through `jafont_19.tim` into the existing native filename scratch slot at font struct `+0xB8`.
4. Lets the game resolver load the corresponding resources from `menu_ja.lgp`.
5. Stores the returned handles in patcher-owned remote state, not in the original six-slot structure.

This does not copy files beside `FFVII.exe` and does not use a loose external font file.

## Runtime Patch Sites

The current patcher installs these sites after validating original runtime bytes:

- `menu_jafont_7_19_loader_hook`, `FUN_14156df20`, RVA `0x156E100`
- `common_render_scanner_prefix`, `FUN_1415724a0`, RVA `0x1572577`
- `common_width_scanner_prefix`, `FUN_1415712b0`, RVA `0x1571336`
- `field_render_scanner_prefix`, `FUN_14156e430`, RVA `0x156F996`
- `field_layout_scanner_prefix`, `FUN_1415714b0`, RVA `0x15716D4`
- `glyph_renderer_c0_page_select`, `FUN_141571ec0`, RVA `0x15720B7`

Details are in:

```text
codex-lab/findings/poc_patch_sites.csv
```

## Scanner Semantics

For render and width scanners:

```text
if current byte is C0..CC:
    pending_prefix = current_byte << 8
    continue to the existing second-byte loop
else:
    execute original behavior
```

The next source byte is consumed by the original pending-prefix path, so `C0 xx` advances exactly two bytes. The trail byte is not processed again.

## Renderer Semantics

At `FUN_141571ec0`, page selectors `C0..CC` are redirected before the original `FA..FE` switch:

```text
lead_index = selector - C0
native_handle = remote_state.extra_jafont_handles[lead_index]
if native_handle != 0:
    use that handle and join the original glyph UV/render path
else:
    return through the original default/no-render path
```

`FA..FE` still execute the original Japanese page path.

## Test Text

The current field test target is:

```text
C:\Program Files (x86)\Steam\steamapps\common\FINAL FANTASY VII Steam Edition\ff7\workingdir\data\field\jfleve.lgp
```

Makou Reactor target:

```text
Field: md1stin
Text: 30
```

For the phrase `가다신참` newline `날따라와`, the table-exact bytes are:

```text
C0 1A C2 60 C7 02 C9 A9 0A C1 91 C3 26 C3 80 C8 10 FF
```

If Makou Reactor emits a field-specific line-break control sequence instead of raw `0A`, preserve Makou's actual line-break bytes and only replace the visible character byte pairs.

## Build and Artifact

Local build requires MSVC x64 tools:

```bat
codex-lab\poc_runtime_patch\build_msvc_x64.bat
```

GitHub Actions workflow:

```text
.github/workflows/build-c0-poc-patcher.yml
```

Artifact name:

```text
ff7-2026-menu-ja-jafont-extension-windows-x64
```

The artifact contains the patcher, `jafont_7.tex` through `jafont_19.tex`, mapping files, README, and build-info. It must not contain `FFVII.exe`, runtime dumps, Ghidra projects, or original game archives.
