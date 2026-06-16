from __future__ import annotations

import base64
import json
from io import BytesIO
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = ROOT / "codex-lab" / "generated" / "menu_ja_jafont_compact_d1"
REFERENCE_ROOT = ROOT / "codex-lab" / "references" / "original_jafont_png_b64"
MAPPING_JSON = OUTPUT_ROOT / "menu_jafont_compact_d1_mapping.json"

CELL_SIZE = 64
GRID_SIZE = 16
PAGE_SIZE = 1024
TEX_HEADER_SIZE = 236
SAFE_SLOT_LAST = 0xD1
EXPECTED_MAPPING_COUNT = 2354
EXPECTED_PAGES = {4, 5, 6, *range(7, 16)}
PHYSICAL_PAGE_BY_LEAD = {
    "C0": 7, "C1": 8, "C2": 9, "C3": 10, "C4": 11, "C5": 12,
    "C6": 13, "C7": 14, "C8": 15, "C9": 4, "CA": 5, "CB": 6,
}


def fail(message: str) -> None:
    raise RuntimeError(message)


def load_tex(path: Path) -> Image.Image:
    data = path.read_bytes()
    expected = TEX_HEADER_SIZE + PAGE_SIZE * PAGE_SIZE * 4
    if len(data) != expected:
        fail(f"unexpected TEX size for {path.name}: {len(data)}, expected {expected}")
    return Image.frombytes("RGBA", (PAGE_SIZE, PAGE_SIZE), data[TEX_HEADER_SIZE:], "raw", "BGRA")


def load_original(page: int) -> Image.Image:
    text = (REFERENCE_ROOT / f"jafont_{page}.tex.png.b64").read_text(encoding="ascii")
    return Image.open(BytesIO(base64.b64decode("".join(text.split())))).convert("RGBA")


def cell_bytes(image: Image.Image, slot: int) -> bytes:
    x = (slot % GRID_SIZE) * CELL_SIZE
    y = (slot // GRID_SIZE) * CELL_SIZE
    return image.crop((x, y, x + CELL_SIZE, y + CELL_SIZE)).tobytes()


def main() -> None:
    actual_pages = {
        int(path.stem.split("_")[1])
        for path in OUTPUT_ROOT.glob("jafont_*.tex")
        if path.is_file()
    }
    if actual_pages != EXPECTED_PAGES:
        fail(f"page set mismatch: actual={sorted(actual_pages)}, expected={sorted(EXPECTED_PAGES)}")

    data = json.loads(MAPPING_JSON.read_text(encoding="utf-8"))
    mapping = data.get("byte_to_unicode")
    if not isinstance(mapping, dict) or len(mapping) != EXPECTED_MAPPING_COUNT:
        fail(f"mapping count mismatch: {0 if not isinstance(mapping, dict) else len(mapping)}")

    expected_keys: list[str] = []
    for ordinal in range(EXPECTED_MAPPING_COUNT):
        lead = 0xC0 + ordinal // (SAFE_SLOT_LAST + 1)
        slot = ordinal % (SAFE_SLOT_LAST + 1)
        expected_keys.append(f"{lead:02X}{slot:02X}")
    if list(mapping.keys()) != expected_keys:
        fail("compact mapping keys are not contiguous C000..CBD1 order")

    for key, entry in mapping.items():
        lead = key[:2]
        slot = int(key[2:], 16)
        if slot > SAFE_SLOT_LAST:
            fail(f"unsafe trail used: {key}")
        expected_page = PHYSICAL_PAGE_BY_LEAD[lead]
        if entry.get("jafont") != f"jafont_{expected_page}.tex":
            fail(f"wrong physical page for {key}: {entry.get('jafont')}")
        if entry.get("slot_hex") != f"{slot:02X}":
            fail(f"wrong slot for {key}: {entry.get('slot_hex')}")

    for page in (4, 5, 6):
        original = load_original(page)
        rebuilt = load_tex(OUTPUT_ROOT / f"jafont_{page}.tex")
        for slot in range(SAFE_SLOT_LAST + 1, 256):
            if cell_bytes(original, slot) != cell_bytes(rebuilt, slot):
                fail(f"jafont_{page} original tail changed at slot {slot:02X}")

    print("compact D1-safe font verification passed")
    print(f"mapping entries: {len(mapping)}")
    print("logical leads: C0..CB")
    print("safe slots: 00..D1")
    print("preserved original tails: jafont_4..6 D2..FF")


if __name__ == "__main__":
    main()
