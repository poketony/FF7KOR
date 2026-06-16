# FFVII 2026 compact D1-safe menu_ja patcher

This package uses a compact Korean mapping designed around the observed loader ceiling.

The patcher loads only `jafont_7.tim` through `jafont_15.tim` as additional resources. Three native pages are reused for the final Korean blocks:

```text
C0 -> jafont_7
C1 -> jafont_8
C2 -> jafont_9
C3 -> jafont_10
C4 -> jafont_11
C5 -> jafont_12
C6 -> jafont_13
C7 -> jafont_14
C8 -> jafont_15
C9 -> jafont_4
CA -> jafont_5
CB -> jafont_6
```

Only trail bytes `00..D1` are assigned to Korean glyphs. `D2..FF` remain reserved. On `jafont_4`, `jafont_5`, and `jafont_6`, the original `D2..FF` cells are preserved.

The patcher does not modify any LGP archive. The user replaces/adds generated TEX files manually in `menu_ja.lgp`.

## Required font resources

Use the compact font package generated for this branch and insert or replace:

```text
jafont_4.tex
jafont_5.tex
jafont_6.tex
jafont_7.tex
jafont_8.tex
jafont_9.tex
jafont_10.tex
jafont_11.tex
jafont_12.tex
jafont_13.tex
jafont_14.tex
jafont_15.tex
```

Do not use the old compact-incompatible `jafont_16.tex` through `jafont_19.tex` mapping.

## Runtime behavior

The patcher:

1. waits for the runtime-decrypted loader signature;
2. hooks the existing font-loader lifecycle;
3. requests native logical names `jafont_7.tim` through `jafont_15.tim`;
4. requires all nine added handles to be nonzero;
5. treats `C0..CB` as Korean two-byte leads;
6. routes `C0..C8` to the nine added handles;
7. routes `C9..CB` to the existing native handles for pages 4, 5, and 6;
8. patches common rendering, field rendering, width calculation, and field layout with the same lead range.

The original `FA..FE` Japanese prefix handling remains on its original path. Reusing pages 4..6 means their original `00..D1` glyphs are replaced, while their original `D2..FF` cells remain available.

## Install

Run the patcher around the point where the loader signature becomes available. The previously observed reliable order was launching the game and then starting the patcher immediately:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log menu_jafont_extension_patch.log
```

A successful loader phase must show nonzero handles for every page from `jafont_7` through `jafont_15`. There should be no wait for `jafont_16` through `jafont_19` in this build.

## Restore

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log menu_jafont_extension_patch.log
```

## Compact mapping files

The compact font package includes:

```text
ff7K_compact_D1_utf8.tbl
ff7K_compact_D1_cp949.tbl
menu_jafont_compact_d1_mapping.csv
menu_jafont_compact_d1_mapping.json
```

The compact mapping is not byte-compatible with the previous sparse `C0..CC` table. Field text must be re-encoded with the new table.

## Field test example

For:

```text
간다신참날따라와
```

the new compact bytes are:

```text
C0 02 C2 0C C5 D0 C8 67 C1 56 C2 8C C3 10 C6 C2 FF
```

With the current Japanese Makou Reactor table, the temporary input string producing those bytes is:

```text
ＭビＯギＲーＵとＮシＯレＰゲＳＯ
```

Character mapping:

```text
간 -> C0 02 -> Ｍビ
다 -> C2 0C -> Ｏギ
신 -> C5 D0 -> Ｒー
참 -> C8 67 -> Ｕと
날 -> C1 56 -> Ｎシ
따 -> C2 8C -> Ｏレ
라 -> C3 10 -> Ｐゲ
와 -> C6 C2 -> ＳＯ
```

Makou Reactor supplies the final `FF` terminator automatically. Preserve its real line-break/control codes when using multiple lines.

## GitHub Actions

Workflow:

```text
.github/workflows/build-c0-poc-patcher.yml
```

It validates the Python transformation sources, generates the compact C++ source, compiles the x64 patcher, verifies the executable, stages the package, and uploads:

```text
ff7-2026-compact-d1-patcher-windows-x64
```

The font TEX package itself is generated separately from the supplied original page images. No LGP editing is performed by the workflow or the scripts.
