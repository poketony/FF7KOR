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

## Loader Paths

The first artifact proved that installing only the `FUN_14156df20` epilogue hook is too timing-sensitive: when the patcher is run after the six Japanese pages have already loaded, the hook may never execute again and the Korean handle remains zero.

The patcher now supports starting before `FFVII.exe`: if no target process exists, it waits up to `--wait-ms` for `FFVII.exe`, then waits for the runtime-decrypted signatures.

The patcher tries two loader paths:

1. Direct loader attempt from an external remote stub.
   - Reads the same VM-stack-derived arguments used by `FUN_14156df20` from `DAT_1420395C8 + 4`, `+8`, `+0xC`, and `+0x14`.
   - Calls `FUN_14004AB00(0x6710AC, 5, arg0, arg1, arg2, DAT_14207CDEC + 0xB8, arg3)`.
   - Stores the returned handle in patcher-owned remote state.
   - Restores `DAT_1420395C8` after the call because `FUN_14004AB00` mutates the VM stack pointer.
   - This path is attempted only when all six Japanese font handles are already nonzero.
2. Fallback hook at the existing native Japanese font loader, described below.

The scanner and renderer hooks are still installed only after one of these paths produces a nonzero Korean C0 handle.

## Loader Hook Fallback

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

The hook reads `DAT_14207CE08` and `DAT_14207CDEC` at execution time, copies the patcher-owned filename string into `DAT_14207CE08 + 0xB8`, uses `DAT_14207CDEC + 0xB8` as the VM filename handle, and saves/restores `DAT_1420395C8` around the extra Korean loader call to avoid leaving an extra VM command result on the interpreter stack. This allows the hook to be installed before the font allocation has been initialized.

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

The final Korean-resource direction is to treat `C0..CC` as Korean lead bytes after resources are converted. The current crash-safe first-glyph POC is narrower: the common menu/battle render and width scanners only accept exact `C0 21`, because unconverted original resources still contain legitimate single-byte `C0` text.

Hooked scanner sites:

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

The common scanner detours currently check both the lead byte and next byte:

```text
if lead == C0 and trail == 21:
    pending_prefix = C000
else:
    execute original single-byte / FA-FE path
```

The field render/layout detours still implement the broader C0-page path for the `jfleve.lgp` test. They should be narrowed to `txt.cpp`-validated byte pairs before enabling broad testing on unconverted field resources.

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
- The direct loader attempt runs from an external remote thread. It restores the VM stack pointer, but arbitrary current VM-stack arguments after font loading are not enough to recreate the original font-loader context; observed direct-load test returned handle `0`.
- If direct loading returns zero and `FUN_14156df20` does not run after the patcher installs its loader hook, the Korean handle remains zero and install fails safely. The current recommended test flow is to run the patcher first and then launch `FFVII.exe`.
- A runtime test with successful Korean handle creation still exited back to the launcher after broad scanner hooks were installed. The common scanner hooks were narrowed to exact `C0 21` because unconverted original menu/startup strings can contain `C0` as an ordinary single-byte character.
- The field hooks still need equivalent `txt.cpp`-validated gating if a field scene exits or corrupts text after the narrowed common-scanner build.
- Full C1-CC page loading is not implemented in this vertical slice.
