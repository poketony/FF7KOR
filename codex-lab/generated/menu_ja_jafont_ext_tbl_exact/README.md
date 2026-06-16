# menu_ja.lgp jafont extension assets

These assets preserve `ff7K(PC).tbl` byte positions exactly.

## Page Policy

- `C0 xx` -> `jafont_7.tex`
- `C1 xx` -> `jafont_8.tex`
- ...
- `CC xx` -> `jafont_19.tex`
- The trail byte is the exact slot index inside the selected 16x16 page.
- Example: `C0 1B` means `jafont_7.tex`, slot `0x1B`, row 2, column 12.
- No entry is compacted, shifted, sorted by Unicode, or moved away from its table byte.

## Source

Primary source:

`C:\Users\JO\FF7KOR\ff7K(PC).tbl`

The file is read as CP949. Generated rows: 2354.

## Files To Insert Into menu_ja.lgp

Insert `jafont_7.tex` through `jafont_19.tex` into `menu_ja.lgp`.

Do not copy these files beside `FFVII.exe`.

## Test Character Bytes

| char | bytes | font sheet | slot | current Japanese Makou input |
| --- | --- | --- | --- | --- |
| 가 | `C0 1A` | `jafont_7.tex` | `1A` | `Ｍゼ` |
| 각 | `C0 1B` | `jafont_7.tex` | `1B` | `Ｍぜ` |
| 갊 | `C0 21` | `jafont_7.tex` | `21` | `Ｍぢ` |
| 다 | `C2 60` | `jafont_9.tex` | `60` | `Ｏチ` |
| 신 | `C7 02` | `jafont_14.tex` | `02` | `Ｔビ` |
| 참 | `C9 A9` | `jafont_16.tex` | `A9` | `Ｖぅ` |
| 날 | `C1 91` | `jafont_8.tex` | `91` | `Ｎや` |
| 따 | `C3 26` | `jafont_10.tex` | `26` | `Ｐド` |
| 라 | `C3 80` | `jafont_10.tex` | `80` | `Ｐム` |
| 와 | `C8 10` | `jafont_15.tex` | `10` | `Ｕゲ` |
| 羅 | `CC BF` | `jafont_19.tex` | `BF` | `ＹＬ` |

For the field phrase:

```text
가다신참
날따라와
```

The table-exact bytes are:

```text
C0 1A C2 60 C7 02 C9 A9 0A C1 91 C3 26 C3 80 C8 10 FF
```

Preserve Makou Reactor's actual field line-break/control bytes instead of blindly replacing them with `0A` if the field script uses a different control sequence. `FF` remains the FF7 string terminator.

## Runtime Patch Direction

Patch `FFVII.exe` minimally so the native `jafont_%d` mechanism loads pages 1..19, then select pages by lead byte:

- original `FA..FE` Japanese behavior remains unchanged;
- `C0..CC` become two-byte leads;
- page number is `7 + (lead - C0)`;
- glyph slot is the trail byte;
- source cursor advances by exactly two bytes;
- width calculation uses the same lead/trail contract.

## Validation

- Generated with `PyFF7-master\PyFF7\tex.py`.
- Fallback-rendered glyphs not found in the supplied KS X 1001 atlas: 4.
- Fallback list: `CC BC 改 U+6539; CC BD 男 U+7537; CC BE 道 U+9053; CC BF 羅 U+7F85`.
