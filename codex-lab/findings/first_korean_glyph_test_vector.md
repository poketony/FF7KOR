# Field Text Test Vector

This is the current visible Korean field test.

## File To Edit

Edit this game archive:

```text
C:\Program Files (x86)\Steam\steamapps\common\FINAL FANTASY VII Steam Edition\ff7\workingdir\data\field\jfleve.lgp
```

Makou Reactor target:

```text
Field: md1stin
Text: 30
```

These bytes are field dialogue bytes. They do not go into `menu_ja.lgp`.

## Font Resource File Set

The matching font pages are:

```text
C:\Users\JO\FF7KOR\codex-lab\generated\menu_ja_jafont_ext_tbl_exact\jafont_7.tex
...
C:\Users\JO\FF7KOR\codex-lab\generated\menu_ja_jafont_ext_tbl_exact\jafont_19.tex
```

Insert those into `menu_ja.lgp`.

## Phrase

Target text:

```text
가다신참
날따라와
```

Table-exact bytes from `ff7K(PC).tbl`:

```text
C0 1A C2 60 C7 02 C9 A9 0A C1 91 C3 26 C3 80 C8 10 FF
```

Breakdown:

| character | bytes | font page | slot |
| --- | --- | --- | --- |
| 가 | `C0 1A` | `jafont_7.tex` | `0x1A` |
| 다 | `C2 60` | `jafont_9.tex` | `0x60` |
| 신 | `C7 02` | `jafont_14.tex` | `0x02` |
| 참 | `C9 A9` | `jafont_16.tex` | `0xA9` |
| line break | `0A` | not a glyph | not a glyph |
| 날 | `C1 91` | `jafont_8.tex` | `0x91` |
| 따 | `C3 26` | `jafont_10.tex` | `0x26` |
| 라 | `C3 80` | `jafont_10.tex` | `0x80` |
| 와 | `C8 10` | `jafont_15.tex` | `0x10` |
| terminator | `FF` | not a glyph | not a glyph |

If Makou Reactor emits a field-specific line-break/control sequence instead of raw `0A`, preserve Makou's actual line-break bytes and only replace the visible character byte pairs.

## Temporary Japanese Input Before Makou Encoding Update

Until Makou Reactor is updated to use the new Korean table, the current Japanese table can be used to emit the same bytes:

| Korean | bytes | current Japanese-table input |
| --- | --- | --- |
| 가 | `C0 1A` | `Ｍゼ` |
| 다 | `C2 60` | `Ｏチ` |
| 신 | `C7 02` | `Ｔビ` |
| 참 | `C9 A9` | `Ｖぅ` |
| 날 | `C1 91` | `Ｎや` |
| 따 | `C3 26` | `Ｐド` |
| 라 | `C3 80` | `Ｐム` |
| 와 | `C8 10` | `Ｕゲ` |

## Expected Runtime Behavior

- `C0..CC` bytes are treated as Korean page leads.
- The next byte is consumed as the slot inside that page.
- Cursor advance is exactly two bytes per Korean glyph.
- The following glyph is not shifted or consumed as a trail byte.
- `FF` remains the FF7 string terminator.
- Existing `FA..FE` Japanese multibyte rendering remains unchanged.
