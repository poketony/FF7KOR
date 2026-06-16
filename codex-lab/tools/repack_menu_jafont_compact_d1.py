from __future__ import annotations

import csv
import json
from collections import OrderedDict
from pathlib import Path

from PIL import Image

from compact_jafont_common import (
    OUTPUT_ROOT,
    PHYSICAL_PAGE_BY_LEAD,
    SAFE_SLOT_LAST,
    SLOTS_PER_PAGE,
    SOURCE_ROOT,
    crop_cell,
    fail,
    load_original_page,
    load_tex_image,
    paste_cell,
    save_tex,
)

OLD_MAPPING_JSON = SOURCE_ROOT / "menu_jafont_extension_mapping.json"


def load_old_mapping() -> list[tuple[str, dict[str, object]]]:
    data = json.loads(OLD_MAPPING_JSON.read_text(encoding="utf-8"))
    mapping = data.get("byte_to_unicode")
    if not isinstance(mapping, dict):
        fail("old mapping JSON has no byte_to_unicode object")
    return sorted(mapping.items(), key=lambda item: int(item[0], 16))


def verify_preserved_tail(original: Image.Image, rebuilt: Image.Image, page: int) -> None:
    for slot in range(SAFE_SLOT_LAST + 1, 256):
        if crop_cell(original, slot).tobytes() != crop_cell(rebuilt, slot).tobytes():
            fail(f"jafont_{page}: original slot {slot:02X} was not preserved")


def main() -> None:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    for child in OUTPUT_ROOT.iterdir():
        if child.is_file():
            child.unlink()

    old_mapping = load_old_mapping()
    capacity = len(PHYSICAL_PAGE_BY_LEAD) * SLOTS_PER_PAGE
    if len(old_mapping) > capacity:
        fail(f"mapping count {len(old_mapping)} exceeds compact capacity {capacity}")

    old_pages: dict[int, Image.Image] = {}
    tex_header: bytes | None = None
    for page in range(7, 20):
        header, image = load_tex_image(SOURCE_ROOT / f"jafont_{page}.tex")
        if tex_header is None:
            tex_header = header
        elif header != tex_header:
            fail(f"TEX header mismatch on jafont_{page}.tex")
        old_pages[page] = image
    assert tex_header is not None

    original_pages = {page: load_original_page(page) for page in (4, 5, 6)}
    target_pages: dict[int, Image.Image] = {}
    for page in PHYSICAL_PAGE_BY_LEAD.values():
        if page in original_pages:
            target_pages[page] = original_pages[page].copy()
        else:
            target_pages[page] = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))

    rows: list[dict[str, object]] = []
    byte_to_unicode: OrderedDict[str, dict[str, object]] = OrderedDict()
    unicode_to_bytes: OrderedDict[str, list[str]] = OrderedDict()
    tbl_lines: list[str] = []

    for ordinal, (old_key, old_entry) in enumerate(old_mapping):
        lead = 0xC0 + ordinal // SLOTS_PER_PAGE
        slot = ordinal % SLOTS_PER_PAGE
        if lead > 0xCB:
            fail(f"packing overflow at ordinal {ordinal}")
        physical_page = PHYSICAL_PAGE_BY_LEAD[lead]

        old_page_name = str(old_entry["jafont"])
        old_page = int(old_page_name.removeprefix("jafont_").removesuffix(".tex"))
        old_slot = int(str(old_entry["slot_hex"]), 16)
        paste_cell(target_pages[physical_page], slot, crop_cell(old_pages[old_page], old_slot))

        char = str(old_entry["char"])
        unicode_value = str(old_entry["unicode"])
        key = f"{lead:02X}{slot:02X}"
        entry = {
            "char": char,
            "unicode": unicode_value,
            "jafont": f"jafont_{physical_page}.tex",
            "logical_lead": f"{lead:02X}",
            "slot_hex": f"{slot:02X}",
            "row_1based": slot // 16 + 1,
            "column_1based": slot % 16 + 1,
            "source_bytes": old_key,
            "source_jafont": old_page_name,
            "source_slot_hex": f"{old_slot:02X}",
        }
        byte_to_unicode[key] = entry
        unicode_to_bytes.setdefault(unicode_value, []).append(key)
        tbl_lines.append(f"{key}={char}")
        rows.append({"bytes": f"{lead:02X} {slot:02X}", **entry})

    for page in (4, 5, 6):
        verify_preserved_tail(original_pages[page], target_pages[page], page)

    for page in sorted(target_pages):
        image = target_pages[page]
        image.save(OUTPUT_ROOT / f"jafont_{page}.tex.png")
        save_tex(OUTPUT_ROOT / f"jafont_{page}.tex", tex_header, image)

    mapping = {
        "policy": {
            "logical_leads": "C0..CB",
            "safe_slot_range": "00..D1",
            "reserved_slot_range": "D2..FF",
            "physical_page_by_lead": {f"{k:02X}": v for k, v in PHYSICAL_PAGE_BY_LEAD.items()},
            "reused_original_pages": [4, 5, 6],
        },
        "byte_to_unicode": byte_to_unicode,
        "unicode_to_bytes": unicode_to_bytes,
    }
    (OUTPUT_ROOT / "menu_jafont_compact_d1_mapping.json").write_text(
        json.dumps(mapping, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    fieldnames = list(rows[0].keys())
    with (OUTPUT_ROOT / "menu_jafont_compact_d1_mapping.csv").open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    tbl = "\n".join(tbl_lines) + "\n"
    (OUTPUT_ROOT / "ff7K_compact_D1_utf8.tbl").write_text(tbl, encoding="utf-8")
    (OUTPUT_ROOT / "ff7K_compact_D1_cp949.tbl").write_text(tbl, encoding="cp949")

    (OUTPUT_ROOT / "README.md").write_text(
        "# Compact D1-safe font package\n\n"
        "Insert/replace jafont_4.tex through jafont_15.tex in menu_ja.lgp manually.\n"
        "The generator never edits an LGP archive.\n\n"
        "- C0..C8 -> jafont_7..15\n"
        "- C9 -> jafont_4\n"
        "- CA -> jafont_5\n"
        "- CB -> jafont_6\n"
        "- valid trails: 00..D1\n"
        "- reserved trails: D2..FF\n"
        "- jafont_4..6 D2..FF preserve the supplied original pixels\n",
        encoding="utf-8",
    )

    print(f"wrote {OUTPUT_ROOT}")
    print(f"mapping entries: {len(rows)} / capacity {capacity}")
    print("generated pages: jafont_4..15")


if __name__ == "__main__":
    main()
