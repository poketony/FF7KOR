# menu_ja.lgp jafont Extension Assets

Status: regenerated with exact `ff7K(PC).tbl` byte positions.

The previous compact/shifted output was wrong for the table-driven workflow. If `ff7K(PC).tbl` is the source, each byte sequence directly identifies the font page and the slot inside that page:

```text
C0 1B => C0 page, slot 0x1B
```

With the new `menu_ja.lgp` insertion plan this means:

```text
C0 1B => jafont_7.tex, slot 0x1B, row 2, column 12
```

No glyph is compacted, shifted, sorted by Unicode, or moved away from its table byte.

## Generated Asset Set To Use

Use:

`C:\Users\JO\FF7KOR\codex-lab\generated\menu_ja_jafont_ext_tbl_exact`

Insert these TEX files into `menu_ja.lgp`:

| file | lead byte |
| --- | --- |
| `jafont_7.tex` | `C0` |
| `jafont_8.tex` | `C1` |
| `jafont_9.tex` | `C2` |
| `jafont_10.tex` | `C3` |
| `jafont_11.tex` | `C4` |
| `jafont_12.tex` | `C5` |
| `jafont_13.tex` | `C6` |
| `jafont_14.tex` | `C7` |
| `jafont_15.tex` | `C8` |
| `jafont_16.tex` | `C9` |
| `jafont_17.tex` | `CA` |
| `jafont_18.tex` | `CB` |
| `jafont_19.tex` | `CC` |

## Encoding Source

Primary mapping source:

`C:\Users\JO\FF7KOR\ff7K(PC).tbl`

The table is read as CP949. It contains 2,354 entries across `C0..CC`.

Important examples:

| char | bytes | file | slot | row | column |
| --- | --- | --- | --- | --- | --- |
| `가` | `C0 1A` | `jafont_7.tex` | `1A` | 2 | 11 |
| `각` | `C0 1B` | `jafont_7.tex` | `1B` | 2 | 12 |
| `갊` | `C0 21` | `jafont_7.tex` | `21` | 3 | 2 |
| `다` | `C2 60` | `jafont_9.tex` | `60` | 7 | 1 |
| `신` | `C7 02` | `jafont_14.tex` | `02` | 1 | 3 |
| `참` | `C9 A9` | `jafont_16.tex` | `A9` | 11 | 10 |
| `날` | `C1 91` | `jafont_8.tex` | `91` | 10 | 2 |
| `따` | `C3 26` | `jafont_10.tex` | `26` | 3 | 7 |
| `라` | `C3 80` | `jafont_10.tex` | `80` | 9 | 1 |
| `와` | `C8 10` | `jafont_15.tex` | `10` | 2 | 1 |
| `羅` | `CC BF` | `jafont_19.tex` | `BF` | 12 | 16 |

## Current Field Phrase Bytes

For:

```text
가다신참
날따라와
```

The table-exact byte sequence is:

```text
C0 1A C2 60 C7 02 C9 A9 0A C1 91 C3 26 C3 80 C8 10 FF
```

`0A` is shown only as the conceptual line break byte from existing FF7 text handling. Preserve whatever Makou Reactor actually emits for the field line break/control sequence. `FF` remains the FF7 string terminator.

## Current Japanese Makou Input Before Makou Encoding Update

Until Makou Reactor is updated with the new table, these can be typed through the current Japanese table to emit raw bytes:

| target | bytes | current Japanese Makou input |
| --- | --- | --- |
| `가` | `C0 1A` | `Ｍゼ` |
| `각` | `C0 1B` | `Ｍぜ` |
| `갊` | `C0 21` | `Ｍぢ` |
| `다` | `C2 60` | `Ｏチ` |
| `신` | `C7 02` | `Ｔビ` |
| `참` | `C9 A9` | `Ｖぅ` |
| `날` | `C1 91` | `Ｎや` |
| `따` | `C3 26` | `Ｐド` |
| `라` | `C3 80` | `Ｐム` |
| `와` | `C8 10` | `Ｕゲ` |

## Makou Reactor Handoff

Copied to:

`C:\Users\JO\makoureactor\codex-lab\ff7kor_menu_jafont_ext`

Files:

- `ff7kor_menu_jafont_ext_utf8.tbl`
- `ff7kor_menu_jafont_ext_cp949.tbl`
- `menu_jafont_extension_mapping.csv`
- `menu_jafont_extension_mapping.json`
- `README.md`

## Verification

Local verification performed:

- Generated with `PyFF7-master\PyFF7\tex.py`.
- `jafont_7.tex` through `jafont_19.tex` reopen through `PyFF7.tex.TEX`.
- All generated TEX files report `1024x1024`.
- Each generated TEX file is nonzero and has size `4194540` bytes.
- `C0 1B` resolves to `jafont_7.tex`, slot `0x1B`, row 2, column 12.
- Fallback-rendered glyphs not found in the supplied KS X 1001 atlas: `改`, `男`, `道`, `羅`.

## Required FFVII.exe Patch Next

The generated assets alone will not display until the native executable is patched.

The next patch should be minimal and aligned with the existing game mechanism:

1. Extend the native `jafont_%d` load loop from pages `1..6` to `1..19`.
2. Ensure the added pages are read from `menu_ja.lgp` as `jafont_7.tex` through `jafont_19.tex`.
3. Patch the scanner/decoder so `C0..CC` are two-byte leads and consume exactly one trail byte.
4. Patch page selection so `C0` selects `jafont_7`, `C1` selects `jafont_8`, ..., `CC` selects `jafont_19`.
5. Apply equivalent width handling for the same `C0..CC` sequences.
6. Preserve original `FA..FE` Japanese behavior.

Do not revive the loose-file `korean_c0_page.tim` staging path.
