# C0 00 decoder-path POC design

Scope: minimal reversible proof of concept for the native 64-bit FFVII 2026 runtime where the byte pair `C0 00` is accepted as one two-byte character and temporarily rendered/measured exactly like `FA 00`.

This is a design only. Do not patch the executable, expand the six-slot font structure, or load new textures in this phase.

## Confirmed baseline

The confirmed shared render and width paths are:

- Menu render: `FUN_141070d60 -> FUN_141077ef0 -> FUN_14106b4a0 -> FUN_141570730 -> FUN_1415724a0 -> FUN_141571ec0`
- Menu width: `FUN_141070d60 -> FUN_141077ef0 -> FUN_14106b4a0 -> FUN_141570050 -> FUN_1415712b0 -> FUN_141571220`
- Battle/helper render: `FUN_1410798e0 -> FUN_141570730/FUN_141570410 -> FUN_1415724a0 -> FUN_141571ec0`
- Battle/helper width: `FUN_1410798e0 -> FUN_141570050 -> FUN_1415712b0 -> FUN_141571220`
- Field render: `FUN_14156e430 -> FUN_141571ec0`

The smallest POC should normalize `C0` to an existing `FA` prefix inside scanner code. It should not change `FUN_141571ec0` page selection or the font-resource slots.

## Current FA-FE mapping

### Scanner behavior

The common pattern is:

1. Read current byte and advance the input pointer/cursor by one.
2. If no pending prefix exists:
   - `0xFF` terminates FF7 text.
   - `(byte)(b + 6) <= 4` recognizes `FA-FE`.
   - Recognized `FA-FE` is stored as `prefix = b << 8`.
   - No glyph is rendered/measured yet.
3. On the next loop iteration:
   - The second byte is read and the input pointer/cursor advances again.
   - The code value is `second_byte | prefix`.
   - The pending prefix is cleared.

This means existing `FA 00` already consumes exactly two bytes and reaches render/width code as `0xFA00`.

### Page and object slot mapping

In `FUN_141571ec0`, the high byte selects a font object:

| High byte | Object slot from `*0x14207CE08` | Jump target | Role |
|---:|---:|---:|---|
| `00` | `+0x10` | `0x1415720EB` | base page |
| `FA` | `+0x24` | `0x1415720F7` | Japanese extended page 1 |
| `FB` | `+0x38` | `0x141572103` | Japanese extended page 2 |
| `FC` | `+0x4C` | `0x14157210F` | Japanese extended page 3 |
| `FD` | `+0x60` | `0x14157211B` | Japanese extended page 4 |
| `FE` | `+0x74` | `0x141572127` | Japanese extended page 5 |
| `C0` | no object slot | `0x14157233D` | current default/no-draw path |

Therefore `C0 00` must not be passed to `FUN_141571ec0` as `0xC000`. It must be normalized before renderer entry so the renderer sees `0xFA00`.

### Glyph index and UV

`FUN_141571ec0` uses the low byte as the glyph index. For `FA 00`, the high byte selects object slot `+0x24`, and the low byte `0x00` selects glyph row/column from the low-byte nibbles:

- column-like component: `(glyph & 0x0F) << 6`
- row-like component: `(glyph >> 4) << 6`

Thus `C0 00 -> FA 00` means: page `FA`, glyph index `00`, object slot `+0x24`.

### Width

`FUN_141571220(0xFA00)` falls through to the default width `0x40`. The POC should make width scanners pass `0xFA00`, not `0xC000`.

## Mandatory POC patch sites

These are sufficient for the first visible menu POC, assuming the test string reaches the confirmed FF7 text scanners directly and is not first converted by a C-string helper.

### `FUN_1415724a0` common string scanner

Patch block:

- VA: `0x141572577`
- RVA: `0x1572577`
- file offset: `0x1571977`
- original bytes:
  `8D 41 06 44 0F B7 C9 3C 04 77 0C 0F B7 D9 66 C1 E3 08 E9 49 01 00 00`

Current behavior:

```asm
lea eax,[rcx+0x6]
movzx r9d,cx
cmp al,0x4
ja  0x14157258E
movzx ebx,cx
shl bx,0x8
jmp 0x1415726D7
```

POC replacement logic:

```c
if (CL == 0xC0) {
    EBX = 0xFA00;
    goto loop_continue_1415726D7;
}
/* original FA-FE logic */
if ((uint8_t)(CL + 6) <= 4) {
    EBX = (uint16_t)CL << 8;
    goto loop_continue_1415726D7;
}
goto not_prefix_14157258E;
```

Effect: `C0` becomes a pending `FA00` prefix. The next byte, including `00`, is consumed by the existing second-byte path at `0x1415726AC`, which calls `FUN_141571ec0(..., second | BX, ...)`.

### `FUN_1415712b0` common width scanner

Patch block:

- VA: `0x141571336`
- RVA: `0x1571336`
- file offset: `0x1570736`
- original bytes:
  `8D 48 06 80 F9 04 77 0E 44 0F B7 C0 66 41 C1 E0 08 E9 03 01 00 00`

Current behavior:

```asm
lea ecx,[rax+0x6]
cmp cl,0x4
ja  0x14157134C
movzx r8d,ax
shl r8w,0x8
jmp 0x14157144F
```

POC replacement logic:

```c
if (AL == 0xC0) {
    R8D = 0xFA00;
    goto loop_continue_14157144F;
}
if ((uint8_t)(AL + 6) <= 4) {
    R8D = (uint16_t)AL << 8;
    goto loop_continue_14157144F;
}
goto not_prefix_14157134C;
```

Effect: width scanning consumes `C0 00` as one two-byte glyph and computes the same width as `FA 00`.

## Optional coverage sites

These are not required for the first menu POC, but they are required for broader field/input/legacy coverage.

### `FUN_14156e430` field independent render scanner

- VA: `0x14156F996`
- file offset: `0x156ED96`
- original bytes:
  `8D 41 06 3C 04 77 0E 44 0F B7 F1 66 41 C1 E6 08 E9 6F 02 00 00`

Use the same logic, with current byte `CL` and pending prefix register `R14W/R14D`:

```c
if (CL == 0xC0) {
    R14D = 0xFA00;
    goto loop_continue_14156FC1A;
}
```

Without this, field text can render `C0` as an ordinary byte and then process the following `00` separately.

### `FUN_1415714b0` field textbox wrapping/layout scanner

- VA: `0x1415716D4`
- file offset: `0x1570AD4`
- original bytes:
  `8D 43 06 3C 04 77 0C 0F B7 F3 66 C1 E6 08 E9 58 02 00 00`

Use the same logic, with current byte `BL` and pending prefix register `SI/ESI`:

```c
if (BL == 0xC0) {
    ESI = 0xFA00;
    goto loop_continue_14157193F;
}
```

Without this, field textbox layout can disagree with render output.

### `FUN_14156e120` name/input or centered text preprocess scanner

- VA: `0x14156E1E2`
- file offset: `0x156D5E2`
- original bytes:
  `8D 42 06 3C 04 77 09 0F B7 CA 66 C1 E1 08 EB 0C`

Use the same logic, with current byte `DL` and pending prefix register `CX/ECX`:

```c
if (DL == 0xC0) {
    ECX = 0xFA00;
    goto loop_continue_14156E1FE;
}
```

This function later calls both `FUN_1415712b0` and `FUN_1415724a0`, but its own pre-scan still needs to consume the second byte correctly if this UI path is tested.

### `FUN_14115af00` legacy/common consumer

- VA: `0x14115B0C7`
- file offset: `0x115A4C7`
- original bytes at index conversion:
  `81 C3 06 FF FF FF 83 C1 F0 89 1D E6 E4 ED 00 E8 C5 3F EE FE 89 18`

Current behavior:

```asm
; EBX = current byte
add ebx,0xFFFFFF06 ; byte - 0xFA
...
cmp eax,0x5
jc  dispatch_fa_fe_case
```

POC replacement logic:

```c
if ((uint8_t)EBX == 0xC0) {
    EBX = 0; /* same table index as FA */
} else {
    EBX = EBX - 0xFA;
}
```

This is optional for the first menu POC because confirmed JP menu render/width paths enter `FUN_1415724a0` and `FUN_1415712b0`. It remains a high-risk complete-coverage site because it independently advances the input index for FA-FE cases.

## Sites that should not be patched for this POC

### `FUN_141571ec0` glyph renderer

Do not patch the glyph renderer for the first POC.

Evidence block:

- VA: `0x1415720BB`
- file offset: `0x15714BB`
- original bytes begin:
  `4C 89 BC 24 E0 00 00 00 3D FE 00 00 00 0F 87 6F 02 00 00 ...`

Reason: the renderer already draws `FA 00` correctly. If scanners normalize `C0` to `FA00`, no renderer change is needed. Changing the renderer table to make high byte `C0` select the FA slot would not fix cursor consumption; `C0 00` would still be read as two independent bytes by unpatched scanners.

### `FUN_141571220` width helper

Do not patch the width helper for the first POC.

Reason: `FUN_141571220(0xFA00)` already returns the same default width used for `FA 00`. The scanner must pass `0xFA00`.

## Unresolved risks

- `FUN_1410732b0` is not a decoder, but it reads source bytes until `0x00` and appends `0xFF`. A literal source sequence `C0 00` cannot pass through this helper unchanged because `00` terminates the source string. The first runtime test should avoid this conversion helper and inject into an already `0xFF`-terminated FF7 text buffer.
- `FUN_14115af00` may still be reached by non-JP or legacy/common fallbacks. If it sees `C0` before being patched, it will not consume `00` as a second byte.
- Additional unconfirmed cursor-advancing consumers may exist outside the confirmed set.
- The design intentionally does not prove that the six-slot font resource structure can be expanded.

## Safest first runtime test

Use a menu text path.

Reason:

- The menu has a confirmed short render chain into `FUN_1415724a0`.
- The same menu helper has a confirmed short width chain into `FUN_1415712b0`.
- This requires only the two mandatory scanner normalizations for a visible result.
- Battle has a confirmed helper path, but the top-level battle root still has unresolved indirect submission/callback edges.
- Field requires the independent scanner `FUN_14156e430` and wrapping/layout `FUN_1415714b0`, increasing the number of patch sites for a first test.

Test constraint: choose a menu string path that reaches `FUN_14106b4a0 -> FUN_141570730/FUN_141570050` with an FF7 `0xFF`-terminated buffer. Do not use a string source that passes through `FUN_1410732b0` for this first `C0 00` test.

## Rollback plan

For any future runtime patch:

1. Record process module base, runtime file hash, and every patched VA.
2. Save the exact original bytes from `findings/poc_patch_sites.csv`.
3. Apply each hook/trampoline independently, starting with only the two mandatory menu POC sites.
4. To roll back, restore the recorded original bytes at each patched VA and disable any allocated code cave/trampoline.
5. Re-test that existing `FA 00` still renders and measures exactly as before.

No Ghidra database edits are needed for rollback because this phase does not rename, type, comment, or patch inside Ghidra.
