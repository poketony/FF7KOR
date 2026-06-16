# C0 POC regression diagnosis

Observed regression:

- Original menu text `NEW GAME` renders as `NEW GA[U+9CE5]` after installing the first C0 POC patch.
- The original `ME` bytes were not intentionally edited.

## Diagnosis

The installed POC changes every `C0 xx` pair into the existing `FA xx` Japanese two-byte page, not only `C0 00` into `FA 00`.

This collides with valid existing Japanese single-byte text. The local Makou Reactor / ff7tk Japanese table confirms:

| encoded | value |
|---|---|
| `B8` | full-width `E` (`U+FF25`) |
| `C0` | full-width `M` (`U+FF2D`) |
| `FA00` | existing JP glyph `U+5FC5` |
| `FAB8` | existing JP glyph `U+9CE5` |

Therefore an unmodified Japanese `NEW GAME` string can contain this byte sequence:

```text
C1 B8 CA 3F BA B4 C0 B8 FF
 N  E  W  sp G  A  M  E  end
```

The current render stub sees `C0` at the original `M`, sets pending prefix to `FA00`, consumes the following `B8` at the original `E`, and renders the combined glyph `FAB8`, which is `U+9CE5`.

This exactly explains `NEW GAME -> NEW GA[U+9CE5]`.

## Original control flow

### Render scanner, RVA `0x1572577`

Original flow around `FUN_1415724a0`:

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

For source byte `C0`:

- Original: `C0 + 6 = C6`, so `cmp al,4` followed by `ja 14157258e` takes the single-byte path.
- Current POC: `cmp cl,C0` takes the special branch before the original range check and sets `EBX=FA00`.

At this site, `CL` is truly the current source byte when execution arrives from the normal loop head. The actual current source address is `R13 - 1`; `R13` has already advanced to the next source byte.

### Width scanner, RVA `0x1571336`

Original flow around `FUN_1415712b0`:

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

For source byte `C0`:

- Original: `C0 + 6 = C6`, so `cmp cl,4` followed by `ja 14157134c` takes the single-byte width path.
- Current POC: `cmp al,C0` takes the special branch before the original range check and sets `R8D=FA00`.

At this site, `AL` is truly the current source byte when execution arrives from the normal loop head. The actual current source address is `R14 - 1`; `R14` has already advanced to the next source byte.

## Detour destination check

The installed absolute-jump destinations from the reported runtime log are internally consistent:

| site | purpose | destination |
|---|---|---|
| render | stolen-prefix continuation | `module_base + 0x1572585` |
| render | original non-prefix path | `module_base + 0x157258E` |
| render | loop continuation after pending prefix | `module_base + 0x15726D7` |
| width | original non-prefix path | `module_base + 0x157134C` |
| width | loop continuation after pending prefix | `module_base + 0x157144F` |

Ghidra MCP returned no incoming xrefs to the overwritten entry addresses, to every byte in the overwritten ranges, or to the immediate stolen-instruction continuation addresses `0x141572585` and `0x141571347`.

This does not prove there is no indirect jump, but the observed `NEW GAME` failure does not require an indirect-entry explanation. It is explained by the current source byte actually being `C0`.

## Current bad stub behavior

Render stub, simplified:

```asm
cmp cl, 0c0h
je  c0_special
; otherwise replay original prefix test

c0_special:
mov ebx, 0fa00h
jmp loop_continue
```

Width stub, simplified:

```asm
cmp al, 0c0h
je  c0_special
; otherwise replay original prefix test

c0_special:
mov r8d, 0fa00h
jmp loop_continue
```

The bad behavior is not that `CL` or `AL` are unrelated parameters. They are the current source byte in the normal path. The bad behavior is that `C0` is already a valid ordinary single-byte character, full-width `M` (`U+FF2D`), and the POC made it an unconditional lead byte.

## Temporary debugger breakpoints

Using the runtime addresses from the observed install log:

```text
render stub base: 0x2060FE70000
render C0 branch: 0x2060FE7002F

width stub base:  0x2060FE80000
width C0 branch:  0x2060FE80031
```

Break at those two C0 branch addresses while rendering `NEW GAME`.

Expected render breakpoint state:

- `CL = C0`
- `R13 - 1` points at the source byte for full-width `M`
- bytes around `R13 - 1` include `... BA B4 C0 B8 FF ...`
- `RBX` has not yet been changed by the C0 branch if the breakpoint is placed at the `mov ebx,0xFA00`
- flags still reflect the taken `cmp cl,0xC0` (`ZF=1`)

Expected width breakpoint state:

- `AL = C0`
- `R14 - 1` points at the source byte for full-width `M`
- bytes around `R14 - 1` include `... BA B4 C0 B8 FF ...`
- `R8` has not yet been changed by the C0 branch if the breakpoint is placed at the `mov r8d,0xFA00`
- flags still reflect the taken `cmp al,0xC0` (`ZF=1`)

If the next source byte is `B8`, stepping the current POC will produce `FA B8`, which the table identifies as `U+9CE5`.

## Replacement design constraint

Do not make `C0` an unconditional lead byte.

The next design must compare against the actual text cursor memory, not just the current-byte register:

- Render: current byte at `[R13 - 1]`, next byte at `[R13]`.
- Width: current byte at `[R14 - 1]`, next byte at `[R14]`.

For a strict `C0 00 -> FA 00` POC, the special path must require both bytes:

```text
source[0] == C0 && source[1] == 00
```

If the second byte is anything other than `00`, the code must replay the original non-prefix behavior byte-for-byte, so existing single-byte `C0` text such as full-width `ME` remains unchanged.

This still reserves the literal existing sequence `C0 00`, which can collide with valid single-byte `C0` followed by valid printable `00`. That collision is a separate encoding-design problem for the full Korean extension.
