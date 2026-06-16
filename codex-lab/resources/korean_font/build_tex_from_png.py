from __future__ import annotations

import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PNG_PATH = ROOT / "korean_c0_page.png"
TEX_PATH = ROOT / "korean_c0_page.tex"
TIM_ALIAS_PATH = ROOT / "korean_c0_page.tim"

# PyFF7 TEX(image).get_bytes(bmp_mode=True) header for a 1024x1024 RGBA page.
# The pixel payload follows directly at offset 236 as raw RGBA bytes.
TEX_HEADER = bytes.fromhex(
    "0100000000000000010000000100000005000000040000000800000004000000"
    "0800000008000000200000000000000000000000000000002000000000040000"
    "0004000000100000000000000000000000000000000000000000000000000000"
    "50642d012000000004000000080000000800000008000000080000000000ff00"
    "00ffffffff000000000000ff1000000008000000000000001800000008000000"
    "080000000800000008000000ff000000ff000000ff000000ff00000000000000"
    "00000000ff0000000400000001000000000000009c210f020000000000000000"
    "e00100004001000000020000"
)


def read_png_rgba(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"{path} is not a PNG")

    pos = 8
    width = height = bit_depth = color_type = None
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

    if width != 1024 or height != 1024 or bit_depth != 8 or color_type != 6:
        raise RuntimeError(
            f"Expected 1024x1024 8-bit RGBA PNG, got {width}x{height} "
            f"bit_depth={bit_depth} color_type={color_type}"
        )

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
                raise RuntimeError(f"Unsupported PNG filter {filter_type}")
        rows.append(row)
        prev = row
    return width, height, b"".join(rows)


def main() -> None:
    _, _, rgba = read_png_rgba(PNG_PATH)
    tex = TEX_HEADER + rgba
    TEX_PATH.write_bytes(tex)
    TIM_ALIAS_PATH.write_bytes(tex)
    print(f"wrote {TEX_PATH} ({len(tex)} bytes)")
    print(f"wrote {TIM_ALIAS_PATH} ({len(tex)} bytes)")


if __name__ == "__main__":
    main()
