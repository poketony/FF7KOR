# Korean Mapping Validation

Source: `codex-lab\references\old_encoding_assets_for_mk\txt.cpp`
Atlas: `codex-lab\work\hangulfont-ksx1001-gulim-shadow2x`

## Summary

- Pages parsed: c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, ca, cb, cc
- Missing pages: none
- Wrong entry counts: none
- Non-empty mappings: 2313
- Empty slots: 1015
- Duplicate Unicode characters: 0
- Adjacent string literal warnings: 0

## Missing Comma / Adjacent Literal Check

No adjacent C/C++ string literal concatenation was detected in C0-CC.

The reported C0 sequence around U+AD64..U+AE0D is parsed as normal comma-separated entries.

## Artwork Spot Checks

| Byte | Character | txt.cpp Cell | Source Atlas | Source Cell | Status | Non-bg pixels |
|---|---:|---:|---|---:|---|---:|
| C0 21 | 가 | 33 | hangulfont_1.tex | 00 | nonempty_artwork | 777 |
| C0 20 | 갉 | 32 | hangulfont_1.tex | 05 | nonempty_artwork | 1058 |
| C0 87 | 괴 | 135 | hangulfont_1.tex | 68 | nonempty_artwork | 1016 |
| C1 00 | 긔 | 0 | hangulfont_1.tex | 9E | nonempty_artwork | 872 |
| C4 00 | 롬 | 0 | hangulfont_3.tex | C3 | nonempty_artwork | 1310 |
| C8 00 | 옛 | 0 | hangulfont_6.tex | 9F | nonempty_artwork | 1108 |

## Notes

- The mapping preserves the exact source page/index order from `txt.cpp`.
- Empty strings are treated as unassigned slots and are listed in `korean_mapping_empty_slots.csv`.
- Duplicate characters are not collapsed; all byte sequences remain available in `korean_mapping.json`.
- The supplied Gulim source atlas is packed KS X 1001 order, so artwork checks resolve by Unicode character before reading source cells.
- The generated native C0 page repositions that artwork into the exact `txt.cpp` byte slots.
