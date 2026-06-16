# Existing Korean Font Mapping

Current source of truth for generated `menu_ja.lgp` font pages:

```text
C:\Users\JO\FF7KOR\ff7K(PC).tbl
```

The file is read as CP949.

## Interpretation

Each four-hex-digit key is used directly:

```text
C0 1B -> lead C0, slot 0x1B
```

With the current `menu_ja.lgp` extension this becomes:

```text
C0 1B -> jafont_7.tex, slot 0x1B
```

No glyph is compacted, moved, or sorted by Unicode order.

## Generated Pages

Output:

```text
codex-lab/generated/menu_ja_jafont_ext_tbl_exact
```

Page mapping:

| lead | file |
| --- | --- |
| `C0` | `jafont_7.tex` |
| `C1` | `jafont_8.tex` |
| `C2` | `jafont_9.tex` |
| `C3` | `jafont_10.tex` |
| `C4` | `jafont_11.tex` |
| `C5` | `jafont_12.tex` |
| `C6` | `jafont_13.tex` |
| `C7` | `jafont_14.tex` |
| `C8` | `jafont_15.tex` |
| `C9` | `jafont_16.tex` |
| `CA` | `jafont_17.tex` |
| `CB` | `jafont_18.tex` |
| `CC` | `jafont_19.tex` |

The trail byte is the exact slot index in the selected 16x16 page.

## Artwork Source

Style source:

```text
codex-lab/work/hangulfont-ksx1001-gulim-shadow2x
```

That atlas is not itself the final byte layout. The generator copies matching glyph pixels from the source atlas into the exact `ff7K(PC).tbl` destination slot.

Fallback-rendered glyphs not found in the supplied KS X 1001 source atlas:

```text
改
男
道
羅
```

## Confirmed Examples

| character | bytes | file | slot | row | column |
| --- | --- | --- | --- | --- | --- |
| 가 | `C0 1A` | `jafont_7.tex` | `0x1A` | 2 | 11 |
| 각 | `C0 1B` | `jafont_7.tex` | `0x1B` | 2 | 12 |
| 갊 | `C0 21` | `jafont_7.tex` | `0x21` | 3 | 2 |
| 다 | `C2 60` | `jafont_9.tex` | `0x60` | 7 | 1 |
| 신 | `C7 02` | `jafont_14.tex` | `0x02` | 1 | 3 |
| 참 | `C9 A9` | `jafont_16.tex` | `0xA9` | 11 | 10 |
| 날 | `C1 91` | `jafont_8.tex` | `0x91` | 10 | 2 |
| 따 | `C3 26` | `jafont_10.tex` | `0x26` | 3 | 7 |
| 라 | `C3 80` | `jafont_10.tex` | `0x80` | 9 | 1 |
| 와 | `C8 10` | `jafont_15.tex` | `0x10` | 2 | 1 |
| 羅 | `CC BF` | `jafont_19.tex` | `0xBF` | 12 | 16 |

## Generated Mapping Files

- `codex-lab/generated/menu_ja_jafont_ext_tbl_exact/menu_jafont_extension_mapping.csv`
- `codex-lab/generated/menu_ja_jafont_ext_tbl_exact/menu_jafont_extension_mapping.json`
- `codex-lab/generated/menu_ja_jafont_ext_tbl_exact/menu_jafont_extension_duplicates.csv`
- `codex-lab/generated/menu_ja_jafont_ext_tbl_exact/makou_reactor_handoff/ff7kor_menu_jafont_ext_utf8.tbl`
- `codex-lab/generated/menu_ja_jafont_ext_tbl_exact/makou_reactor_handoff/ff7kor_menu_jafont_ext_cp949.tbl`

The Makou Reactor handoff copy is also placed at:

```text
C:\Users\JO\makoureactor\codex-lab\ff7kor_menu_jafont_ext
```
