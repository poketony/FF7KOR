# First Korean Glyph Test Vector

Goal: after the runtime patcher reports successful Korean C0 page loading, edit one field text entry in `jfleve.lgp` so the native 2026 field renderer displays one genuine Korean glyph.

## Selected Glyph

| Property | Value |
|---|---|
| Character | `가` |
| Unicode | `U+AC00` |
| Korean encoding | `C0 21` |
| Korean page | `C0`, page 0 |
| Glyph index | `0x21` |
| Width/advance | `64` native glyph units |

`C0 21` comes directly from `txt.cpp` table `FF7Text::caract_jp_c0[0x21]`.

## Surrounding Characters

The surrounding visible characters use the confirmed Japanese FF7 table, not PC ASCII:

| Intended display | FF7 byte | Unicode |
|---|---:|---|
| `Ａ` | `B4` | `U+FF21` |
| `Ｂ` | `B5` | `U+FF22` |
| terminator | `FF` | FF7 text terminator |

## Exact Test Bytes

Recommended field text payload:

```text
B4 C0 21 B5 FF
```

Byte boundaries:

- `B4`: leading visible control character `Ａ`
- `C0 21`: Korean two-byte glyph `가`
- `B5`: following visible character `Ｂ`
- `FF`: FF7 string terminator

Expected rendered output:

```text
Ａ가Ｂ
```

Expected cursor behavior:

- `B4` renders first.
- `C0` sets Korean pending prefix and advances cursor to `p+1`.
- `21` completes glyph `C0 21`, renders `가`, and advances cursor to `p+2`.
- `B5` renders as the next character, proving the trail byte was not processed again.
- `FF` terminates the FF7 string.

## Control Checks

Before editing the target text permanently, keep a copy of the original bytes.

Recommended checks:

- Baseline: original field text loads without the patch.
- Patched: `B4 C0 21 B5 FF` renders `Ａ가Ｂ`.
- Following character check: `Ｂ` must appear immediately after `가`.
- Width check: the gap after `가` should match a 64-unit full-cell glyph.
- Terminator check: no garbage text appears after `Ｂ`.
- Japanese regression check: existing `FA-FE` Japanese multibyte text still renders on original pages.

Avoid trail bytes `00` and `FF` for this first test. The selected trail byte `21` is safe for normal FF7 text streams and avoids known NUL/terminator-sensitive helper paths.

## Runtime Command

After launching the game:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log first_korean_glyph_patch.log
```

Only enter the modified field scene after the patcher log says `completed successfully`.

Rollback:

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log first_korean_glyph_patch.log
```

Closing `FFVII.exe` also removes all runtime patches because the original executable is never modified on disk.
