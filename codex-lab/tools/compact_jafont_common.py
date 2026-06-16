from __future__ import annotations

import base64
from io import BytesIO
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "codex-lab" / "generated" / "menu_ja_jafont_ext_tbl_exact"
OUTPUT_ROOT = ROOT / "codex-lab" / "generated" / "menu_ja_jafont_compact_d1"
REFERENCE_ROOT = ROOT / "codex-lab" / "references" / "original_jafont_png_b64"
CELL_SIZE = 64
GRID_SIZE = 16
PAGE_SIZE = 1024
TEX_HEADER_SIZE = 236
SAFE_SLOT_LAST = 0xD1
SLOTS_PER_PAGE = 0xD2
PHYSICAL_PAGE_BY_LEAD = {
    0xC0: 7, 0xC1: 8, 0xC2: 9, 0xC3: 10, 0xC4: 11, 0xC5: 12,
    0xC6: 13, 0xC7: 14, 0xC8: 15, 0xC9: 4, 0xCA: 5, 0xCB: 6,
}


def fail(message: str) -> None:
    raise RuntimeError(message)


def load_tex_image(path: Path) -> tuple[bytes, Image.Image]:
    data = path.read_bytes()
    expected = TEX_HEADER_SIZE + PAGE_SIZE * PAGE_SIZE * 4
    if len(data) != expected:
        fail(f"unexpected TEX size for {path}: {len(data)}, expected {expected}")
    return data[:TEX_HEADER_SIZE], Image.frombytes(
        "RGBA", (PAGE_SIZE, PAGE_SIZE), data[TEX_HEADER_SIZE:], "raw", "BGRA"
    )


def save_tex(path: Path, header: bytes, image: Image.Image) -> None:
    path.write_bytes(header + image.convert("RGBA").tobytes("raw", "BGRA"))


def load_original_page(page: int) -> Image.Image:
    path = REFERENCE_ROOT / f"jafont_{page}.tex.png.b64"
    encoded = "".join(path.read_text(encoding="ascii").split())
    image = Image.open(BytesIO(base64.b64decode(encoded))).convert("RGBA")
    if image.size != (PAGE_SIZE, PAGE_SIZE):
        fail(f"unexpected original page dimensions for jafont_{page}: {image.size}")
    return image


def cell_box(slot: int) -> tuple[int, int, int, int]:
    x = (slot % GRID_SIZE) * CELL_SIZE
    y = (slot // GRID_SIZE) * CELL_SIZE
    return x, y, x + CELL_SIZE, y + CELL_SIZE


def crop_cell(image: Image.Image, slot: int) -> Image.Image:
    return image.crop(cell_box(slot))


def paste_cell(image: Image.Image, slot: int, cell: Image.Image) -> None:
    x, y, _, _ = cell_box(slot)
    image.paste(cell, (x, y))
