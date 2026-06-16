from __future__ import annotations

import csv
import json
import sys
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
CODEX_LAB = ROOT / "codex-lab"
TBL_PATH = ROOT / "ff7K(PC).tbl"
TXT_CPP = CODEX_LAB / "references" / "old_encoding_assets_for_mk" / "txt.cpp"
JAPANESE_ENCODING_JSON = (
    CODEX_LAB
    / "references"
    / "makou_reactor_japanese_encoding"
    / "ff7_japanese_encoding.json"
)
SOURCE_ATLAS_ROOT = CODEX_LAB / "work" / "hangulfont-ksx1001-gulim-shadow2x"
SOURCE_ATLAS_JSON = SOURCE_ATLAS_ROOT / "ff7_kr_ksx1001_2350.json"
PYFF7_ROOT = ROOT / "PyFF7-master"
OUTPUT_ROOT = CODEX_LAB / "generated" / "menu_ja_jafont_ext_tbl_exact"
MAKOU_HANDOFF = OUTPUT_ROOT / "makou_reactor_handoff"

if str(PYFF7_ROOT) not in sys.path:
    sys.path.insert(0, str(PYFF7_ROOT))

from PyFF7.tex import TEX

try:
    sys.stdout.reconfigure(encoding="utf-8")
except AttributeError:
    pass

CELL_SIZE = 64
GRID_SIZE = 16
PAGE_SIZE = CELL_SIZE * GRID_SIZE
FIRST_LEAD = 0xC0
LAST_LEAD = 0xCC
FIRST_JAFONT = 7

FILL = (255, 255, 255, 230)
SHADOW = (0, 0, 0, 231)
SHADOW_OFFSETS = [
    (2, 2),
    (3, 2),
    (4, 2),
    (5, 2),
    (6, 2),
    (2, 3),
    (3, 3),
    (4, 3),
    (5, 3),
    (6, 3),
    (2, 4),
    (3, 4),
    (4, 4),
    (5, 4),
    (6, 4),
    (2, 5),
    (3, 5),
    (4, 5),
    (5, 5),
    (2, 6),
    (3, 6),
    (4, 6),
]


def load_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/gulim.ttc"),
        Path("C:/Windows/Fonts/malgun.ttf"),
        Path("C:/Windows/Fonts/arial.ttf"),
    ]
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def draw_centered_glyph(
    page: Image.Image,
    slot: int,
    char: str,
    font: ImageFont.FreeTypeFont | ImageFont.ImageFont,
    y_adjust: int = -1,
) -> None:
    draw = ImageDraw.Draw(page)
    cell_x = (slot % GRID_SIZE) * CELL_SIZE
    cell_y = (slot // GRID_SIZE) * CELL_SIZE
    bbox = draw.textbbox((0, 0), char, font=font)
    width = bbox[2] - bbox[0]
    height = bbox[3] - bbox[1]
    x = cell_x + (CELL_SIZE - width) // 2 - bbox[0]
    y = cell_y + (CELL_SIZE - height) // 2 - bbox[1] + y_adjust
    for dx, dy in SHADOW_OFFSETS:
        draw.text((x + dx, y + dy), char, font=font, fill=SHADOW)
    draw.text((x, y), char, font=font, fill=FILL)


def parse_tbl_exact() -> dict[tuple[int, int], str]:
    if not TBL_PATH.exists():
        raise RuntimeError(f"Missing table: {TBL_PATH}")

    mapping: dict[tuple[int, int], str] = {}
    for line_no, raw_line in enumerate(TBL_PATH.read_text(encoding="cp949").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise RuntimeError(f"{TBL_PATH}:{line_no}: missing '='")
        key, value = line.split("=", 1)
        key = key.strip().upper()
        if len(key) != 4:
            raise RuntimeError(f"{TBL_PATH}:{line_no}: expected 4 hex digits, got {key!r}")
        lead = int(key[:2], 16)
        trail = int(key[2:], 16)
        if lead < FIRST_LEAD or lead > LAST_LEAD:
            raise RuntimeError(f"{TBL_PATH}:{line_no}: lead out of range: {key}")
        if (lead, trail) in mapping:
            raise RuntimeError(f"{TBL_PATH}:{line_no}: duplicate byte sequence: {key}")
        if len(value) != 1:
            raise RuntimeError(f"{TBL_PATH}:{line_no}: expected one Unicode character for {key}")
        mapping[(lead, trail)] = value
    return mapping


def load_source_atlas() -> dict[int, tuple[Path, int]]:
    data = json.loads(SOURCE_ATLAS_JSON.read_text(encoding="utf-8"))
    atlas: dict[int, tuple[Path, int]] = {}
    for item in data:
        codepoint = int(item["unicode"][2:], 16)
        font_file = SOURCE_ATLAS_ROOT / f"{item['font_file']}.png"
        if not font_file.exists():
            raise RuntimeError(f"Missing source atlas PNG: {font_file}")
        atlas[codepoint] = (font_file, int(item["slot_hex"], 16))
    return atlas


def crop_source_cell(source_path: Path, slot: int) -> Image.Image:
    image = Image.open(source_path).convert("RGBA")
    x = (slot % GRID_SIZE) * CELL_SIZE
    y = (slot // GRID_SIZE) * CELL_SIZE
    return image.crop((x, y, x + CELL_SIZE, y + CELL_SIZE))


def write_page(page: Image.Image, jafont_number: int) -> None:
    rgba_page = page.convert("RGBA")
    png_path = OUTPUT_ROOT / f"jafont_{jafont_number}.tex.png"
    tex_path = OUTPUT_ROOT / f"jafont_{jafont_number}.tex"
    rgba_page.save(png_path)
    tex_path.write_bytes(TEX(rgba_page).get_bytes(bmp_mode=True))


def japanese_raw_input_map() -> dict[int, str]:
    data = json.loads(JAPANESE_ENCODING_JSON.read_text(encoding="utf-8"))
    mapping: dict[int, str] = {}
    for entry in data["entries"]:
        if entry.get("table_name") != "jap":
            continue
        encoded = entry.get("encoded_hex", "")
        if len(encoded) != 2:
            continue
        unicode_name = entry.get("unicode", "")
        if unicode_name.startswith("U+"):
            mapping[int(encoded, 16)] = chr(int(unicode_name[2:], 16))
    return mapping


def current_japanese_input_for(sequence: str) -> str:
    jap = japanese_raw_input_map()
    out = []
    for part in sequence.split():
        value = int(part, 16)
        out.append(jap.get(value, f"<{part}>"))
    return "".join(out)


def generate_pages(mapping: dict[tuple[int, int], str]) -> tuple[list[dict[str, str]], list[str]]:
    source_atlas = load_source_atlas()
    fallback_font = load_font(56)
    pages = {
        lead: Image.new("RGBA", (PAGE_SIZE, PAGE_SIZE), (0, 0, 0, 0))
        for lead in range(FIRST_LEAD, LAST_LEAD + 1)
    }
    rows: list[dict[str, str]] = []
    fallback_notes: list[str] = []

    for (lead, slot), char in sorted(mapping.items()):
        page = pages[lead]
        codepoint = ord(char)
        if codepoint in source_atlas:
            source_path, source_slot = source_atlas[codepoint]
            page.alpha_composite(
                crop_source_cell(source_path, source_slot),
                ((slot % GRID_SIZE) * CELL_SIZE, (slot // GRID_SIZE) * CELL_SIZE),
            )
            source_note = f"{source_path.name}:{source_slot:02X}"
        else:
            draw_centered_glyph(page, slot, char, fallback_font)
            source_note = "rendered_gulim_fallback"
            fallback_notes.append(f"{lead:02X} {slot:02X} {char} U+{codepoint:04X}")

        jafont_number = FIRST_JAFONT + (lead - FIRST_LEAD)
        rows.append(
            {
                "bytes": f"{lead:02X} {slot:02X}",
                "jafont": f"jafont_{jafont_number}.tex",
                "lead_hex": f"{lead:02X}",
                "slot_hex": f"{slot:02X}",
                "slot_decimal": str(slot),
                "row_1based": str((slot // GRID_SIZE) + 1),
                "column_1based": str((slot % GRID_SIZE) + 1),
                "unicode": f"U+{codepoint:04X}",
                "char": char,
                "source_glyph": source_note,
                "notes": "exact ff7K(PC).tbl byte position",
            }
        )

    for lead, page in pages.items():
        write_page(page, FIRST_JAFONT + (lead - FIRST_LEAD))
    return rows, fallback_notes


def write_preview_grid() -> None:
    font = load_font(12)
    for png_path in sorted(OUTPUT_ROOT.glob("jafont_*.tex.png")):
        image = Image.open(png_path).convert("RGBA")
        overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        for i in range(GRID_SIZE + 1):
            pos = i * CELL_SIZE
            draw.line((pos, 0, pos, PAGE_SIZE), fill=(255, 0, 0, 90))
            draw.line((0, pos, PAGE_SIZE, pos), fill=(255, 0, 0, 90))
        for slot in range(256):
            x = (slot % GRID_SIZE) * CELL_SIZE + 2
            y = (slot // GRID_SIZE) * CELL_SIZE + 2
            draw.text((x, y), f"{slot:02X}", font=font, fill=(255, 255, 0, 210))
        Image.alpha_composite(image, overlay).save(png_path.with_name(png_path.stem + "_grid.png"))


def write_mapping_files(rows: list[dict[str, str]], fallback_notes: list[str]) -> None:
    fieldnames = [
        "bytes",
        "jafont",
        "lead_hex",
        "slot_hex",
        "slot_decimal",
        "row_1based",
        "column_1based",
        "unicode",
        "char",
        "source_glyph",
        "notes",
    ]
    csv_path = OUTPUT_ROOT / "menu_jafont_extension_mapping.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    byte_to_unicode = {}
    unicode_to_bytes: dict[str, list[str]] = defaultdict(list)
    for row in rows:
        key = row["bytes"].replace(" ", "")
        byte_to_unicode[key] = {
            "char": row["char"],
            "unicode": row["unicode"],
            "jafont": row["jafont"],
            "slot_hex": row["slot_hex"],
            "row_1based": int(row["row_1based"]),
            "column_1based": int(row["column_1based"]),
        }
        unicode_to_bytes[row["unicode"]].append(key)

    json_path = OUTPUT_ROOT / "menu_jafont_extension_mapping.json"
    json_path.write_text(
        json.dumps(
            {
                "policy": {
                    "source": str(TBL_PATH),
                    "encoding": "exact ff7K(PC).tbl byte positions",
                    "page_mapping": "C0->jafont_7, C1->jafont_8, ..., CC->jafont_19",
                    "slot_mapping": "trail byte is the 0..255 slot index within the page",
                    "trail_FF": "unused because ff7K(PC).tbl has no *FF entries",
                },
                "byte_to_unicode": byte_to_unicode,
                "unicode_to_bytes": unicode_to_bytes,
                "fallback_rendered_glyphs": fallback_notes,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    MAKOU_HANDOFF.mkdir(parents=True, exist_ok=True)
    tbl_lines = [f"{row['bytes'].replace(' ', '')}={row['char']}" for row in rows]
    (MAKOU_HANDOFF / "ff7kor_menu_jafont_ext_utf8.tbl").write_text(
        "\n".join(tbl_lines) + "\n",
        encoding="utf-8",
    )
    (MAKOU_HANDOFF / "ff7kor_menu_jafont_ext_cp949.tbl").write_text(
        "\n".join(tbl_lines) + "\n",
        encoding="cp949",
        errors="replace",
    )
    (MAKOU_HANDOFF / "ff7kor_menu_jafont_ext_mapping.csv").write_text(
        csv_path.read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    (MAKOU_HANDOFF / "ff7kor_menu_jafont_ext_mapping.json").write_text(
        json_path.read_text(encoding="utf-8"),
        encoding="utf-8",
    )

    duplicates = [
        (unicode_value, byte_values)
        for unicode_value, byte_values in sorted(unicode_to_bytes.items())
        if len(byte_values) > 1
    ]
    with (OUTPUT_ROOT / "menu_jafont_extension_duplicates.csv").open(
        "w", encoding="utf-8", newline=""
    ) as f:
        writer = csv.writer(f)
        writer.writerow(["unicode", "char", "bytes"])
        for unicode_value, byte_values in duplicates:
            char = chr(int(unicode_value[2:], 16))
            writer.writerow([unicode_value, char, " ".join(byte_values)])


def write_readme(rows: list[dict[str, str]], fallback_notes: list[str]) -> None:
    by_char: dict[str, dict[str, str]] = {}
    for row in rows:
        by_char.setdefault(row["char"], row)

    targets = ["가", "각", "갊", "다", "신", "참", "날", "따", "라", "와", "羅"]
    lines = []
    for char in targets:
        row = by_char.get(char)
        if row is None:
            lines.append(f"| {char} | missing | missing | missing | missing |")
            continue
        lines.append(
            "| "
            + " | ".join(
                [
                    char,
                    f"`{row['bytes']}`",
                    f"`{row['jafont']}`",
                    f"`{row['slot_hex']}`",
                    f"`{current_japanese_input_for(row['bytes'])}`",
                ]
            )
            + " |"
        )

    readme = f"""# menu_ja.lgp jafont extension assets

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

`{TBL_PATH}`

The file is read as CP949. Generated rows: {len(rows)}.

## Files To Insert Into menu_ja.lgp

Insert `jafont_7.tex` through `jafont_19.tex` into `menu_ja.lgp`.

Do not copy these files beside `FFVII.exe`.

## Test Character Bytes

| char | bytes | font sheet | slot | current Japanese Makou input |
| --- | --- | --- | --- | --- |
{chr(10).join(lines)}

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

- Generated with `PyFF7-master\\PyFF7\\tex.py`.
- Fallback-rendered glyphs not found in the supplied KS X 1001 atlas: {len(fallback_notes)}.
- Fallback list: `{'; '.join(fallback_notes) if fallback_notes else 'none'}`.
"""
    (OUTPUT_ROOT / "README.md").write_text(readme, encoding="utf-8")


def main() -> None:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    MAKOU_HANDOFF.mkdir(parents=True, exist_ok=True)
    mapping = parse_tbl_exact()
    rows, fallback_notes = generate_pages(mapping)
    write_preview_grid()
    write_mapping_files(rows, fallback_notes)
    write_readme(rows, fallback_notes)

    print(f"wrote {OUTPUT_ROOT}")
    print("pages: jafont_7.tex .. jafont_19.tex")
    print(f"mapping rows: {len(rows)}")
    print(f"fallback-rendered glyphs: {len(fallback_notes)}")
    for note in fallback_notes:
        print(f"  {note}")


if __name__ == "__main__":
    main()
