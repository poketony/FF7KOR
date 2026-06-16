# Native Korean Font Loader Design

Status: implemented as a first visible-glyph POC in `codex-lab/poc_runtime_patch/c0_poc_patcher.cpp`.

## Native 2026 Font Facts Used

- Existing initializer: `FUN_141571d90`.
- Existing loader/uploader: `FUN_14156df20`.
- Existing Japanese filename format: `jafont_%d.tim`.
- Existing font struct handle global: `0x14207CDEC`.
- Existing font struct pointer global: `0x14207CE08`.
- Existing font struct size: `0x138`.
- Existing Japanese filename slots occupy offsets `0x00..0x77`.
- Static copied data occupies through `0xB7`.
- The zeroed tail `0xB8..0x137` is used by the patcher as a temporary filename scratch slot for `korean_c0_page.tim`.

## Loader Hook

Hook site:

- Function: `FUN_14156df20`
- RVA: `0x156E100`
- Runtime VA at image base `0x140000000`: `0x14156E100`
- Original bytes: `48 8B 6C 24 58 48 8B 74 24 60 48 8B 7C 24 40`

This site is immediately after the sixth Japanese font handle is stored at `+0x74` and before the epilogue restores callee-saved registers.

At this point the original loader context is still live:

- `EDI`: native resource argument 1
- `ESI`: native resource argument 2
- `EBP`: native resource argument 3
- `EBX`: native resource argument 5

The hook calls the same native loader command as the Japanese pages:

```text
FUN_14004AB00(0x6710AC, 5, EDI, ESI, EBP, DAT_14207CDEC + 0xB8, EBX)
```

The returned native texture/resource handle is stored in a patcher-owned remote state block. The common glyph renderer detour reads that handle for Korean page C0.

## Resource File

Artifact path:

```text
resources/korean_font/korean_c0_page.tim
```

The patcher validates the local file header before installing runtime hooks, then stages the file beside the target `FFVII.exe` as `korean_c0_page.tim` if that file is not already present. It also stages the sibling `korean_c0_page.tex` alias when present, because extracted native resources are TEX containers even though the original logical names use `*.tim`:

- TEX magic probe `u32[0] == 1`
- width `u32[15] == 1024`
- height `u32[16] == 1024`
- non-zero file size

The native loader receives the short filename `korean_c0_page.tim`. The current POC therefore depends on the native resource resolver being able to locate a loose file in the game directory by that name when the loader command runs. If the loader cannot resolve loose external files, the patcher times out without installing scanner/renderer detours.

## Renderer Hook

Hook site:

- Function: `FUN_141571ec0`
- RVA: `0x15720B7`
- Runtime VA: `0x1415720B7`
- Original bytes: `41 0F B7 C0 4C 89 BC 24 E0 00 00 00 3D FE 00 00 00`

Behavior:

- If page selector is `C0`, load the Korean C0 handle from remote state into `EDI`.
- Jump to the existing common handle-test/render path at `RVA 0x1572131` (`test edi,edi`).
- Otherwise execute the original `<= FE` jump-table path, preserving original `FA-FE` behavior.

The existing UV math already uses the low byte of the encoded glyph:

- column: `(glyph & 0x0F) * 64`
- row: `((glyph >> 4) & 0x0F) * 64`

Therefore `C0 21` selects cell `0x21`, where the generated page contains `가`.

## Scanner Hooks

The patcher treats `C0..CC` as Korean lead bytes in:

- common render scanner `FUN_1415724a0`, RVA `0x1572577`
- common width scanner `FUN_1415712b0`, RVA `0x1571336`
- field render scanner `FUN_14156e430`, RVA `0x156F996`
- field layout scanner `FUN_1415714b0`, RVA `0x15716D4`

For C0 page 0:

```text
lead = C0
pending_prefix = C000
trail = source[1]
encoded_glyph = C000 | trail
cursor advances exactly two bytes
```

The lower width helper `FUN_141571220` already returns default width `0x40` for non-special multibyte pages, so `C0 21` measures as width `64`, matching renderer advance for the first POC.

## Rollback

The patcher keeps original overwrite bytes for every hook site and restores them with:

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log first_korean_glyph_patch.log
```

Remote allocations are left inert until the process exits.

## Remaining Risk

- The native resolver for `0x6710AC` may not accept a loose external file named `korean_c0_page.tim` even when it is staged beside `FFVII.exe`.
- Staging may fail if the Steam game folder is not writable; in that case `korean_c0_page.tim` and `korean_c0_page.tex` can be copied manually beside `FFVII.exe`.
- If `FUN_14156df20` does not run after the patcher installs its loader hook, the Korean handle remains zero and install fails safely.
- Full C1-CC page loading is not implemented in this vertical slice.
