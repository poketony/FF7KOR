from __future__ import annotations

import json
import runpy
import struct
import sys
import zlib
from pathlib import Path

# Windows / GitHub Actions 콘솔 출력을 UTF-8로 강제
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")


LAB_ROOT = Path(__file__).resolve().parents[1]
MAPPING_JSON = LAB_ROOT / "generated" / "korean_mapping.json"
FONT_DIR = LAB_ROOT / "resources" / "korean_font"
MANIFEST_JSON = FONT_DIR / "korean_c0_manifest.json"
PNG_PATH = FONT_DIR / "korean_c0_page.png"
TIM_PATH = FONT_DIR / "korean_c0_page.tim"

SELECTED_SEQUENCE = "C0 21"
SELECTED_CHAR = "가"
CELL_SIZE = 64
GRID = 16


def read_png_rgba(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"{path} is not a PNG")

    pos = 8
    width = height = color_type = bit_depth = None
    compressed = bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        chunk_type = data[pos + 4 : pos + 8]
        chunk_data = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(">IIBB", chunk_data[:10])
        elif chunk_type == b"IDAT":
            compressed.extend(chunk_data)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None:
        raise AssertionError("PNG missing IHDR")
    if bit_depth != 8 or color_type != 6:
        raise AssertionError(f"Expected 8-bit RGBA PNG, got bit_depth={bit_depth}, color_type={color_type}")

    raw = zlib.decompress(bytes(compressed))
    stride = width * 4
    rows: list[bytearray] = []
    offset = 0
    prev = bytearray(stride)
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        row = bytearray(raw[offset : offset + stride])
        offset += stride
        for i in range(stride):
            left = row[i - 4] if i >= 4 else 0
            up = prev[i]
            up_left = prev[i - 4] if i >= 4 else 0
            if filter_type == 1:
                row[i] = (row[i] + left) & 0xFF
            elif filter_type == 2:
                row[i] = (row[i] + up) & 0xFF
            elif filter_type == 3:
                row[i] = (row[i] + ((left + up) >> 1)) & 0xFF
            elif filter_type == 4:
                p = left + up - up_left
                pa = abs(p - left)
                pb = abs(p - up)
                pc = abs(p - up_left)
                predictor = left if pa <= pb and pa <= pc else up if pb <= pc else up_left
                row[i] = (row[i] + predictor) & 0xFF
            elif filter_type != 0:
                raise AssertionError(f"Unsupported PNG filter {filter_type}")
        rows.append(row)
        prev = row
    return width, height, b"".join(rows)


def u32le(buf: bytes, index: int) -> int:
    offset = index * 4
    return struct.unpack("<I", buf[offset : offset + 4])[0]


def main() -> None:
    if not TIM_PATH.exists():
        runpy.run_path(str(FONT_DIR / "build_tex_from_png.py"), run_name="__main__")

    mapping = json.loads(MAPPING_JSON.read_text(encoding="utf-8"))
    entry = mapping["byte_to_unicode"][SELECTED_SEQUENCE]
    if entry["char"] != SELECTED_CHAR:
        raise AssertionError(f"{SELECTED_SEQUENCE} maps to {entry['char']!r}, not {SELECTED_CHAR!r}")
    if entry["index"] != 0x21 or entry["page"] != "c0":
        raise AssertionError(f"Unexpected selected glyph placement: {entry}")

    manifest = json.loads(MANIFEST_JSON.read_text(encoding="utf-8"))
    if manifest["selected_sequence"] != SELECTED_SEQUENCE or manifest["selected_character"] != SELECTED_CHAR:
        raise AssertionError("Manifest selected glyph does not match mapping")

    tex_head = TIM_PATH.read_bytes()[:68]
    if u32le(tex_head, 0) != 1 or u32le(tex_head, 15) != 1024 or u32le(tex_head, 16) != 1024:
        raise AssertionError("TEX/TIM alias header probe failed")

    width, height, pixels = read_png_rgba(PNG_PATH)
    if width != 1024 or height != 1024:
        raise AssertionError(f"Unexpected PNG size: {width}x{height}")

    cell_index = entry["index"]
    x0 = (cell_index % GRID) * CELL_SIZE
    y0 = (cell_index // GRID) * CELL_SIZE
    nontransparent = 0
    nonblack = 0
    for y in range(y0, y0 + CELL_SIZE):
        row = y * width * 4
        for x in range(x0, x0 + CELL_SIZE):
            i = row + x * 4
            r, g, b, a = pixels[i : i + 4]
            if a:
                nontransparent += 1
                if r or g or b:
                    nonblack += 1
    if nontransparent < 100 or nonblack < 100:
        raise AssertionError(
            f"Selected glyph cell looks empty: nontransparent={nontransparent}, nonblack={nonblack}"
        )
    print(
        f"verified {SELECTED_SEQUENCE} {SELECTED_CHAR} at cell {cell_index}: "
        f"nontransparent={nontransparent}, nonblack={nonblack}"
    )


if __name__ == "__main__":
    main()
