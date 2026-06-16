# C0/Korean Runtime POC Implementation

## Current implementation: first Korean glyph

The previous `C0 00 -> FA 00` normalization POC is superseded for the Korean build. The final Korean-resource direction still reserves `C0..CC` as Korean two-byte lead bytes, but the current first-glyph POC must not treat every original `C0..CC` byte in unconverted resources as Korean.

Implemented in `codex-lab/poc_runtime_patch/c0_poc_patcher.cpp`:

- Loads one generated Korean C0 font page: `codex-lab/resources/korean_font/korean_c0_page.tim`.
- Stages that page beside the target `FFVII.exe` as `korean_c0_page.tim` before asking the native loader to resolve it.
- Uses `txt.cpp` mapping, not Unicode order. The first selected glyph is `가 = C0 21`.
- First attempts a direct native loader command from a remote stub using the current `DAT_1420395C8` VM stack arguments.
- Restores `DAT_1420395C8` after the direct loader command because `FUN_14004AB00` mutates the VM stack pointer.
- Falls back to a native loader hook at `FUN_14156df20`, RVA `0x156E100`, when direct loading returns no handle.
- If `FFVII.exe` is not running, waits for the process so the patcher can be launched before the game.
- The fallback hook stages the Korean filename at hook execution time by reading `DAT_14207CE08` and `DAT_14207CDEC`, so it can be installed before the native font struct has been initialized.
- During install, the patcher waits first only for the loader hook signature. It waits for scanner/renderer signatures later, after a nonzero Korean C0 handle exists, to avoid missing the native font-loader lifecycle during early startup.
- Stores the Korean C0 native resource handle in a patcher-owned remote state block.
- Patches `FUN_141571ec0`, RVA `0x15720B8`, so page selector `C0` uses the Korean C0 handle and then joins the original glyph UV/render path.
- Patches common render scanner `FUN_1415724a0`, common width scanner `FUN_1415712b0`, field render scanner `FUN_14156e430`, and field layout scanner `FUN_1415714b0`.
- The common menu/battle scanners are narrowed to the exact first test sequence `C0 21`; this prevents original full-width single-byte `C0` text from being consumed as Korean during startup/menu rendering.
- The field scanner hooks still use the broader C0-page path for the `jfleve.lgp` field test. They remain a known risk for unconverted field strings and should be narrowed to `txt.cpp`-validated byte pairs after the next runtime test.
- Runtime testing on `md1stin` Text30 showed that this field dialogue path can still bypass those scanner hooks and run the direct field text loop in `FUN_1411f7dc0`, reading `0xDC0230` text bytes and field glyph tables directly.
- A temporary field overlay smoke-test detour was added at `FUN_141570730` and `FUN_14106ccc0`; after running the original helper, it calls `FUN_141571ec0` with glyph code `0xC021`. This is intentionally a visibility proof for the Korean page/common glyph renderer, not the final field decoder implementation.
- Width for `C0 21` is `0x40`, matching the native default full-cell multibyte width and renderer advance.

Safety behavior:

- The patcher validates every expected runtime-decrypted byte sequence before patching.
- Scanner/render detours are not installed until either direct loading or the native loader hook returns a nonzero Korean C0 handle.
- `restore` writes back original overwrite bytes for all implemented hook sites.
- No original `FFVII.exe` bytes are modified on disk.

Known runtime dependency:

- The hook uses the existing native `0x6710AC` font/resource loader command and passes the filename `korean_c0_page.tim` through confirmed unused space at `DAT_14207CDEC + 0xB8`.
- The first artifact timed out when `FUN_14156df20` did not execute again after the hook was installed; the direct-load attempt was added to reduce that timing dependency.
- The second artifact showed direct loading after the lifecycle had passed returned handle `0`, confirming that current arbitrary `DAT_1420395C8` values are not enough to recreate the original loader context.
- The next artifact showed a nonzero Korean C0 page handle and successful hook installation, followed by a game exit back to the launcher. The most likely cause is the previous broad common `C0..CC` scanner consuming unconverted original text. The common scanners now require exact `C0 21`.
- If the native resolver cannot find the loose external resource by that name, or if the resource command is only valid on the main thread and the fallback hook is never reached, install times out safely.

Patch sites are recorded in `codex-lab/findings/poc_patch_sites.csv`.

---

# Historical C0 00 runtime POC implementation

Scope: first runnable POC for normalizing `C0 00` to the already-supported `FA 00` path in the runtime-decrypted FFVII 2026 native 64-bit executable.

No original executable bytes are modified on disk. No Ghidra database changes are required.

## Delivery method

Chosen method: minimal external runtime patcher.

Rejected for the first POC:

- Runtime DLL injection: useful later, but slower to build and test.
- Proxy DLL: more deployment surface and game-load-order assumptions.
- Debugger-only patch: fastest manually, but weak logging and harder rollback.

The external patcher is the smallest method that still satisfies the important safety properties:

- ASLR-safe: resolves `FFVII.exe` module base at runtime and uses RVA.
- Runtime-decryption-safe: waits until expected decrypted bytes are visible in the running process.
- Reversible: stores original overwrite bytes and has a `restore` action.
- Logged: records PID, module base, target addresses, original bytes, detour bytes, stub addresses, and success/failure.
- No permanent EXE modification.

Implementation folder:

- `poc_runtime_patch/c0_poc_patcher.cpp`
- `poc_runtime_patch/build_msvc_x64.bat`
- `poc_runtime_patch/README.md`

## Phase A verification

`reference/FF7_TEXT_ENCODING_NOTES.md` was requested but is not present in this workspace. Encoding facts were taken from the existing findings and the current Ghidra/runtime-overlay verification.

### Packed on-disk versus runtime-decrypted bytes

The workspace `FFVII.exe` still contains protected/packed bytes at the mandatory offsets. The runtime overlay contains the executable instructions used for patching.

| Site | RVA | Packed on-disk file offset | Packed bytes at offset | Runtime dump file offset | Runtime bytes |
|---|---:|---:|---|---:|---|
| render scanner | `0x1572577` | `0x1571977` | `C6 A0 E3 5D E9 71 AA ...` | `0x1571977` | `8D 41 06 44 0F B7 C9 ...` |
| width scanner | `0x1571336` | `0x1570736` | `14 54 46 65 42 17 16 ...` | `0x1570736` | `8D 48 06 80 F9 04 77 ...` |

The patcher validates the runtime-decrypted bytes in the live process and never patches the packed on-disk file.

### `FUN_1415724a0` render scanner

Instruction block:

```asm
141572560  movzx ecx, byte ptr [r13]
141572565  lea   r13, [r13+1]
141572569  test  bx, bx
14157256c  jnz   1415725c6
14157256e  cmp   cl, 0xff
141572571  jz    1415726fc
141572577  lea   eax, [rcx+6]
14157257a  movzx r9d, cx
14157257e  cmp   al, 4
141572580  ja    14157258e
141572582  movzx ebx, cx
141572585  shl   bx, 8
141572589  jmp   1415726d7
```

Register assumptions:

- Current source byte: `CL`.
- Source cursor: `R13`, already advanced by `lea r13,[r13+1]`.
- Pending prefix: `BX`.
- Next byte: read on the next loop iteration from `[R13]`.
- Encoded glyph value: `R9W` after `or r9w,bx` at `0x1415726B5`.

Existing `FA 00` path:

1. `FA` is read into `CL`; `R13` advances to `p+1`.
2. `(FA + 6) & 0xff <= 4` succeeds.
3. `BX = 0xFA00`.
4. Loop continues without rendering.
5. Next iteration reads `00`; `R13` advances to `p+2`.
6. Pending-prefix path executes `R9W = 0xFA00 | 0x00`.
7. `FUN_141571ec0` receives `0xFA00`.
8. `BX` is cleared at `0x1415726D5`.

POC `C0 00` redirect:

1. Stub checks `CL == 0xC0`.
2. If true, it sets `EBX = 0xFA00`.
3. It jumps to `0x1415726D7`, the same loop continuation used after a recognized prefix.
4. The existing second-byte path consumes the real following byte.

Overwrite selection:

- Overwrite length: 14 bytes.
- Overwritten bytes: `8D 41 06 44 0F B7 C9 3C 04 77 0C 0F B7 D9`.
- Detour type: 14-byte RIP-indirect absolute jump.
- Ghidra MCP returned no incoming xrefs for any byte in the overwritten range `0x141572577-0x141572584`.

### `FUN_1415712b0` width scanner

Instruction block:

```asm
141571320  movzx eax, byte ptr [r14]
141571324  lea   r14, [r14+1]
141571328  test  r8w, r8w
14157132c  jnz   141571380
14157132e  cmp   al, 0xff
141571330  jz    141571464
141571336  lea   ecx, [rax+6]
141571339  cmp   cl, 4
14157133c  ja    14157134c
14157133e  movzx r8d, ax
141571342  shl   r8w, 8
141571347  jmp   14157144f
```

Register assumptions:

- Current source byte: `AL`.
- Source cursor: `R14`, already advanced by `lea r14,[r14+1]`.
- Pending prefix: `R8W`.
- Next byte: read on the next loop iteration from `[R14]`.
- Encoded glyph value for width: `CX` after `or cx,r8w` at `0x141571428`.

Existing `FA 00` path:

1. `FA` is read into `AL`; `R14` advances to `p+1`.
2. `(FA + 6) & 0xff <= 4` succeeds.
3. `R8W = 0xFA00`.
4. Loop continues without measuring.
5. Next iteration reads `00`; `R14` advances to `p+2`.
6. Pending-prefix path executes `CX = 0xFA00 | 0x00`.
7. `FUN_141571220` receives `0xFA00`.
8. `R8D` is cleared at `0x14157144C`.

POC `C0 00` redirect:

1. Stub checks `AL == 0xC0`.
2. If true, it sets `R8D = 0xFA00`.
3. It jumps to `0x14157144F`, the same loop continuation used after a recognized prefix.
4. The existing second-byte path consumes the real following byte.

Overwrite selection:

- Overwrite length: 17 bytes.
- Overwritten bytes: `8D 48 06 80 F9 04 77 0E 44 0F B7 C0 66 41 C1 E0 08`.
- Detour type: 14-byte RIP-indirect absolute jump plus 3 NOPs.
- Ghidra MCP returned no incoming xrefs for any byte in the overwritten range `0x141571336-0x141571346`.

## Runtime patch behavior

The patcher installs two remote stubs:

- Render stub:
  - `CL == C0`: set `EBX = FA00`, jump to render loop continuation.
  - Otherwise preserve original `FA-FE` and non-prefix behavior.
- Width stub:
  - `AL == C0`: set `R8D = FA00`, jump to width loop continuation.
  - Otherwise preserve original `FA-FE` and non-prefix behavior.

The stubs use RIP-indirect absolute jumps, so they do not clobber general-purpose registers while returning to original code. This matters for the width non-prefix path because `AX` is still needed by `movzx ecx,ax`.

If any install step fails after writing one site, the patcher restores both mandatory overwrite regions before returning an error.

## Build result

Build attempted with:

```bat
cmd /c poc_runtime_patch\build_msvc_x64.bat
```

Result in this Codex environment:

```text
cl.exe was not found, and vswhere.exe is not installed.
```

So the patcher source is implemented, but an executable was not built here. It should build from a Developer Command Prompt or on a machine with Visual Studio Build Tools and the MSVC x64 toolchain installed.

## Restore behavior

`c0_poc_patcher.exe restore` writes back the original overwrite bytes for both mandatory sites and flushes the instruction cache. Remote stub allocations are left inert and disappear when `FFVII.exe` exits.
