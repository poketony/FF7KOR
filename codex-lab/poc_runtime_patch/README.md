# FFVII 2026 native-lead reuse menu_ja patcher

This build keeps the original Japanese `FA..FE` lead codes and adds only six new Korean leads. It is designed to reduce the font heap from fifteen loaded pages to twelve.

## Page and lead layout

```text
C0 -> jafont_7   slots 00..D1
C1 -> jafont_8   slots 00..D1
C2 -> jafont_9   slots 00..D1
C3 -> jafont_10  slots 00..D1
C4 -> jafont_11  slots 00..D1
C5 -> jafont_12  slots 00..D1

FA -> jafont_2   slots 00..D1 replaced; D2..FF original
FB -> jafont_3   slots 00..FE replaced; FF original
FC -> jafont_4   slots 00..D1 replaced; D2..FF original
FD -> jafont_5   slots 00..D1 replaced; D2..FF original
FE -> jafont_6   slots 00..D1 replaced; D2..FF original
```

`jafont_1` remains completely untouched and is not included in the font package.

Capacity is 2,355 glyphs for the 2,354-character mapping, leaving one unused slot. The original Japanese control region beginning at `FE D2` remains in the preserved part of `jafont_6`.

The patcher never modifies an LGP archive. Replace or add the TEX files manually in `menu_ja.lgp`.

## Required resources

Replace or add:

```text
jafont_2.tex
jafont_3.tex
jafont_4.tex
jafont_5.tex
jafont_6.tex
jafont_7.tex
jafont_8.tex
jafont_9.tex
jafont_10.tex
jafont_11.tex
jafont_12.tex
```

Do not replace `jafont_1.tex`.

## Runtime behavior

The patcher:

1. waits for the runtime-decrypted font-loader signature;
2. asks the native loader for `jafont_7.tim` through `jafont_12.tim` only;
3. requires all six additional handles to be nonzero;
4. recognizes only `C0..C5` as added two-byte leads;
5. routes `C0..C5` to the six additional handles;
6. leaves native `FA..FE` decoding and page selection on the original game path;
7. applies the same `C0..C5` range to common rendering, common width, field rendering, and field layout.

This removes the previous custom `C9..CB -> native page` routing. The original Japanese lead path is now used directly.

## Install

The loader signature was most reliably observed when the game was launched and the patcher was started immediately afterward:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log menu_jafont_extension_patch.log
```

A successful loader phase must show exactly six nonzero additional handles:

```text
C0/jafont_7
C1/jafont_8
C2/jafont_9
C3/jafont_10
C4/jafont_11
C5/jafont_12
```

The log must not wait for `jafont_13` or later pages.

## Restore

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log menu_jafont_extension_patch.log
```

## Mapping files

The font package includes:

```text
ff7K_native_lead_reuse_utf8.tbl
ff7K_native_lead_reuse_cp949.tbl
menu_jafont_native_lead_reuse_mapping.csv
menu_jafont_native_lead_reuse_mapping.json
```

This encoding is not byte-compatible with either of the earlier sparse or compact mappings. Field text must be re-encoded.

## Field test

Test text:

```text
간다신참날따라와
```

Exact bytes:

```text
C0 02 C2 0C C5 D0 FC 3A C1 56 C2 8C C3 10 FA C2 FF
```

Temporary Makou Reactor input under the original Japanese table:

```text
ＭビＯギＲー廃ＮシＯレＰゲ目
```

Character mapping:

```text
간 -> C0 02 -> Ｍビ
다 -> C2 0C -> Ｏギ
신 -> C5 D0 -> Ｒー
참 -> FC 3A -> 廃
날 -> C1 56 -> Ｎシ
따 -> C2 8C -> Ｏレ
라 -> C3 10 -> Ｐゲ
와 -> FA C2 -> 目
```

Makou Reactor writes the final string terminator automatically.

## GitHub Actions artifact

```text
ff7-2026-native-lead-reuse-patcher-windows-x64
```

The workflow validates that the generated patch source uses only `C0..C5`, requests only `jafont_7..12`, compiles the x64 executable, and uploads it. Font TEX generation is separate and does not edit an LGP archive.
