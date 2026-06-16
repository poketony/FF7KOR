from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[3]
PYFF7_ROOT = ROOT / "PyFF7-master"
if str(PYFF7_ROOT) not in sys.path:
    sys.path.insert(0, str(PYFF7_ROOT))

from PyFF7.tex import TEX


SOURCE_ATLAS_DIR = ROOT / "codex-lab" / "work" / "hangulfont-ksx1001-gulim-shadow2x"
MAPPING_JSON = ROOT / "codex-lab" / "generated" / "korean_mapping.json"
OUT_DIR = ROOT / "codex-lab" / "resources" / "korean_font"
FINDINGS_DIR = ROOT / "codex-lab" / "findings"

PAGE = "c0"
LEAD_BYTE = 0xC0
SELECTED_CHAR = "가"
SELECTED_SEQUENCE = "C0 21"
CELL_SIZE = 64
GRID = 16
IMAGE_SIZE = CELL_SIZE * GRID


def load_source_index() -> dict[str, dict[str, object]]:
    rows_by_char: dict[str, dict[str, object]] = {}
    with (SOURCE_ATLAS_DIR / "ff7_kr_ksx1001_2350.csv").open("r", encoding="utf-8-sig", newline="") as f:
        for row in csv.DictReader(f):
            rows_by_char.setdefault(row["char"], row)
    return rows_by_char


def cell_box(index: int) -> tuple[int, int, int, int]:
    x = (index % GRID) * CELL_SIZE
    y = (index // GRID) * CELL_SIZE
    return (x, y, x + CELL_SIZE, y + CELL_SIZE)


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    FINDINGS_DIR.mkdir(parents=True, exist_ok=True)

    mapping = json.loads(MAPPING_JSON.read_text(encoding="utf-8"))
    byte_to_unicode = mapping["byte_to_unicode"]
    selected = byte_to_unicode[SELECTED_SEQUENCE]
    if selected["char"] != SELECTED_CHAR:
        raise RuntimeError(f"{SELECTED_SEQUENCE} maps to {selected['char']!r}, not {SELECTED_CHAR!r}")

    source_index = load_source_index()
    source_images: dict[str, Image.Image] = {}
    page = Image.new("RGBA", (IMAGE_SIZE, IMAGE_SIZE), (0, 0, 0, 0))
    rows: list[dict[str, object]] = []
    missing: list[str] = []

    for seq, entry in sorted(byte_to_unicode.items()):
        if entry["page"] != PAGE:
            continue
        char = entry["char"]
        dest_index = int(entry["index"])
        src = source_index.get(char)
        if not src:
            missing.append(seq)
            rows.append(
                {
                    "unicode_character": char,
                    "unicode_codepoint": " ".join(f"U+{ord(c):04X}" for c in char),
                    "korean_source_code_or_index": "",
                    "intended_lead_byte": f"{LEAD_BYTE:02X}",
                    "intended_trail_byte": f"{dest_index:02X}",
                    "page_number": 0,
                    "glyph_index": dest_index,
                    "width": 64,
                    "source_atlas_coordinates": "",
                    "destination_atlas_coordinates": f"{dest_index % GRID},{dest_index // GRID}",
                    "notes": "No matching source glyph in packed KS X 1001 atlas; destination left blank.",
                }
            )
            continue

        src_file = str(src["font_file"])
        if src_file not in source_images:
            source_images[src_file] = Image.open(SOURCE_ATLAS_DIR / f"{src_file}.png").convert("RGBA")
        src_index = int(src["index"])
        glyph = source_images[src_file].crop(cell_box(src_index))
        page.paste(glyph, cell_box(dest_index)[:2])

        rows.append(
            {
                "unicode_character": char,
                "unicode_codepoint": " ".join(f"U+{ord(c):04X}" for c in char),
                "korean_source_code_or_index": int(src["index"]),
                "intended_lead_byte": f"{LEAD_BYTE:02X}",
                "intended_trail_byte": f"{dest_index:02X}",
                "page_number": 0,
                "glyph_index": dest_index,
                "width": 64,
                "source_atlas_coordinates": f"{src_file}:{int(src['slot_hex'], 16) % GRID},{int(src['slot_hex'], 16) // GRID}",
                "destination_atlas_coordinates": f"{dest_index % GRID},{dest_index // GRID}",
                "notes": "Copied from supplied Gulim shadow2x source atlas and placed at txt.cpp index.",
            }
        )

    png_path = OUT_DIR / "korean_c0_page.png"
    tex_path = OUT_DIR / "korean_c0_page.tex"
    tim_alias_path = OUT_DIR / "korean_c0_page.tim"
    preview_path = OUT_DIR / "korean_c0_page_preview.png"
    glyph_preview_path = OUT_DIR / "selected_glyph_ga_c021.png"

    page.save(png_path)
    tex_bytes = TEX(page).get_bytes(bmp_mode=True)
    tex_path.write_bytes(tex_bytes)
    tim_alias_path.write_bytes(tex_bytes)

    preview = page.copy()
    draw = ImageDraw.Draw(preview)
    for i in range(GRID + 1):
        p = i * CELL_SIZE
        draw.line((p, 0, p, IMAGE_SIZE), fill=(255, 0, 0, 80), width=1)
        draw.line((0, p, IMAGE_SIZE, p), fill=(255, 0, 0, 80), width=1)
    selected_index = int(selected["index"])
    x0, y0, x1, y1 = cell_box(selected_index)
    draw.rectangle((x0, y0, x1 - 1, y1 - 1), outline=(0, 255, 0, 255), width=3)
    draw.text((x0 + 3, y0 + 3), SELECTED_SEQUENCE, fill=(0, 255, 0, 255))
    preview.save(preview_path)
    page.crop(cell_box(selected_index)).save(glyph_preview_path)

    fieldnames = [
        "unicode_character",
        "unicode_codepoint",
        "korean_source_code_or_index",
        "intended_lead_byte",
        "intended_trail_byte",
        "page_number",
        "glyph_index",
        "width",
        "source_atlas_coordinates",
        "destination_atlas_coordinates",
        "notes",
    ]
    with (FINDINGS_DIR / "existing_korean_font_mapping.csv").open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    manifest = {
        "source_mapping": str(MAPPING_JSON.relative_to(ROOT)),
        "source_atlas": str(SOURCE_ATLAS_DIR.relative_to(ROOT)),
        "generated_files": {
            "png": png_path.name,
            "tex": tex_path.name,
            "tim_alias": tim_alias_path.name,
            "preview": preview_path.name,
            "selected_glyph_preview": glyph_preview_path.name,
        },
        "page": PAGE,
        "lead_byte": f"{LEAD_BYTE:02X}",
        "selected_character": SELECTED_CHAR,
        "selected_sequence": SELECTED_SEQUENCE,
        "selected_glyph_index": selected_index,
        "selected_width": 64,
        "cell_size": CELL_SIZE,
        "image_size": [IMAGE_SIZE, IMAGE_SIZE],
        "tex_format": "PyFF7 TEX bmp_mode=True; byte-identical container style to existing 1024x1024 RGBA font pages",
        "missing_source_glyph_sequences": missing,
    }
    (OUT_DIR / "korean_c0_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
