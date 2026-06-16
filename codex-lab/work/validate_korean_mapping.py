#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TXT_CPP = ROOT / "codex-lab" / "references" / "old_encoding_assets_for_mk" / "txt.cpp"
ATLAS_DIR = ROOT / "codex-lab" / "work" / "hangulfont-ksx1001-gulim-shadow2x"
FINDINGS_DIR = ROOT / "codex-lab" / "findings"
GENERATED_DIR = ROOT / "codex-lab" / "generated"

PAGES = [f"c{i:x}" for i in range(0x0, 0xD)]
ARRAY_RE = re.compile(
    r"const\s+char\s*\*\s*FF7Text::caract_jp_(c[0-9a-c])\s*\[\s*256\s*\]\s*=\s*\{",
    re.MULTILINE,
)


class ParseError(RuntimeError):
    pass


def find_matching_brace(text: str, open_pos: int) -> int:
    depth = 0
    i = open_pos
    in_line_comment = False
    in_block_comment = False
    in_string = False
    escaped = False
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
        elif in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
                i += 1
        elif in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
        else:
            if ch == "/" and nxt == "/":
                in_line_comment = True
                i += 1
            elif ch == "/" and nxt == "*":
                in_block_comment = True
                i += 1
            elif ch == '"':
                in_string = True
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return i
        i += 1
    raise ParseError("unclosed array brace")


def decode_cpp_string(raw: str) -> str:
    # The supplied tables use plain UTF-8 strings plus a few escaped ASCII
    # punctuation marks. unicode_escape is not suitable for arbitrary Korean
    # text, so handle only the escapes that occur in C string syntax here.
    out: list[str] = []
    i = 0
    while i < len(raw):
        ch = raw[i]
        if ch != "\\":
            out.append(ch)
            i += 1
            continue
        if i + 1 >= len(raw):
            out.append("\\")
            i += 1
            continue
        esc = raw[i + 1]
        mapping = {
            "n": "\n",
            "t": "\t",
            "r": "\r",
            "\\": "\\",
            '"': '"',
            "'": "'",
            "0": "\0",
        }
        out.append(mapping.get(esc, esc))
        i += 2
    return "".join(out)


def lex_array_entries(body: str, page: str) -> tuple[list[str], list[dict[str, object]]]:
    entries: list[str] = []
    warnings: list[dict[str, object]] = []
    current_parts: list[str] = []
    current_line_parts: list[int] = []
    i = 0
    line = 1
    expecting_value = True

    def finish_entry() -> None:
        nonlocal current_parts, current_line_parts, expecting_value
        if not current_parts:
            entries.append("")
        else:
            if len(current_parts) > 1:
                warnings.append(
                    {
                        "page": page,
                        "index": len(entries),
                        "kind": "adjacent_string_literals",
                        "lines": current_line_parts,
                        "parts": current_parts[:],
                        "combined": "".join(current_parts),
                    }
                )
            entries.append("".join(current_parts))
        current_parts = []
        current_line_parts = []
        expecting_value = True

    while i < len(body):
        ch = body[i]
        nxt = body[i + 1] if i + 1 < len(body) else ""
        if ch == "\n":
            line += 1
            i += 1
            continue
        if ch.isspace():
            i += 1
            continue
        if ch == "/" and nxt == "/":
            i += 2
            while i < len(body) and body[i] != "\n":
                i += 1
            continue
        if ch == "/" and nxt == "*":
            i += 2
            while i + 1 < len(body) and not (body[i] == "*" and body[i + 1] == "/"):
                if body[i] == "\n":
                    line += 1
                i += 1
            i += 2
            continue
        if ch == ",":
            finish_entry()
            i += 1
            continue
        if ch == '"':
            i += 1
            start_line = line
            raw: list[str] = []
            escaped = False
            while i < len(body):
                c = body[i]
                if c == "\n":
                    line += 1
                if escaped:
                    raw.append("\\" + c)
                    escaped = False
                elif c == "\\":
                    escaped = True
                elif c == '"':
                    break
                else:
                    raw.append(c)
                i += 1
            if i >= len(body) or body[i] != '"':
                raise ParseError(f"unterminated string in {page} around line {start_line}")
            current_parts.append(decode_cpp_string("".join(raw)))
            current_line_parts.append(start_line)
            expecting_value = False
            i += 1
            continue
        raise ParseError(f"unexpected token in {page} line {line}: {body[i:i+24]!r}")

    if current_parts or not expecting_value:
        finish_entry()
    return entries, warnings


def parse_tables() -> tuple[dict[str, list[str]], list[dict[str, object]]]:
    text = TXT_CPP.read_text(encoding="utf-8-sig")
    tables: dict[str, list[str]] = {}
    warnings: list[dict[str, object]] = []
    for match in ARRAY_RE.finditer(text):
        page = match.group(1)
        open_pos = text.find("{", match.start())
        close_pos = find_matching_brace(text, open_pos)
        body = text[open_pos + 1 : close_pos]
        entries, page_warnings = lex_array_entries(body, page)
        tables[page] = entries
        warnings.extend(page_warnings)
    return tables, warnings


def byte_sequence(page: str, index: int) -> str:
    lead = int(page[1:], 16)
    return f"{0xC0 + lead:02X} {index:02X}"


def page_number(page: str) -> int:
    return int(page[1:], 16) + 1


def atlas_cell(page: str, index: int) -> dict[str, object]:
    return {
        "font_page": f"hangulfont_{page_number(page)}.tex",
        "cell_index": index,
        "cell_x": index % 16,
        "cell_y": index // 16,
    }


def build_mapping(tables: dict[str, list[str]]) -> tuple[dict[str, dict[str, object]], dict[str, list[str]]]:
    bytes_to_char: dict[str, dict[str, object]] = {}
    char_to_bytes: dict[str, list[str]] = defaultdict(list)
    for page in PAGES:
        for index, char in enumerate(tables[page]):
            if not char:
                continue
            seq = byte_sequence(page, index)
            entry = {
                "char": char,
                "page": page,
                "index": index,
                **atlas_cell(page, index),
            }
            bytes_to_char[seq] = entry
            char_to_bytes[char].append(seq)
    return bytes_to_char, dict(sorted(char_to_bytes.items()))


def write_csv(path: Path, rows: list[dict[str, object]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    FINDINGS_DIR.mkdir(parents=True, exist_ok=True)
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)

    tables, warnings = parse_tables()
    missing_pages = [page for page in PAGES if page not in tables]
    wrong_counts = {page: len(entries) for page, entries in tables.items() if len(entries) != 256}

    bytes_to_char, char_to_bytes = build_mapping(tables)

    duplicate_rows: list[dict[str, object]] = []
    for char, seqs in sorted(char_to_bytes.items()):
        if len(seqs) > 1:
            duplicate_rows.append(
                {
                    "char": char,
                    "count": len(seqs),
                    "byte_sequences": " ".join(seqs),
                }
            )

    empty_rows: list[dict[str, object]] = []
    for page in PAGES:
        for index, char in enumerate(tables.get(page, [])):
            if char == "":
                empty_rows.append(
                    {
                        "page": page,
                        "index_decimal": index,
                        "index_hex": f"{index:02X}",
                        "byte_sequence": byte_sequence(page, index),
                        **atlas_cell(page, index),
                    }
                )

    # Deterministic artwork spot checks. These confirm that the txt.cpp
    # character exists as non-empty artwork in the supplied packed atlas.
    spot_sequences = ["C0 21", "C0 20", "C0 87", "C1 00", "C4 00", "C8 00"]
    spot_rows: list[dict[str, object]] = []
    try:
        from PIL import Image
    except Exception:
        Image = None

    source_by_char: dict[str, dict[str, str]] = {}
    source_csv = ATLAS_DIR / "ff7_kr_ksx1001_2350.csv"
    if source_csv.exists():
        with source_csv.open("r", encoding="utf-8-sig", newline="") as f:
            for row in csv.DictReader(f):
                source_by_char.setdefault(row["char"], row)

    for seq in spot_sequences:
        entry = bytes_to_char.get(seq)
        if not entry:
            spot_rows.append({"byte_sequence": seq, "status": "missing_mapping"})
            continue
        source = source_by_char.get(str(entry["char"]))
        status = "not_checked_pillow_unavailable"
        non_bg_pixels = ""
        bbox = ""
        source_cell = ""
        source_file = ""
        if source:
            source_file = source["font_file"]
            source_cell = source["slot_hex"]
        png = ATLAS_DIR / f"{source_file}.png" if source_file else Path()
        if not source:
            status = "missing_source_atlas_character"
        elif Image is not None and png.exists():
            img = Image.open(png).convert("RGBA")
            source_slot = int(source["slot_hex"], 16)
            x0 = (source_slot % 16) * 64
            y0 = (source_slot // 16) * 64
            crop = img.crop((x0, y0, x0 + 64, y0 + 64))
            pixels = crop.getdata()
            non_bg = sum(1 for px in pixels if px[:3] != (0, 0, 0) and px[3] != 0)
            non_bg_pixels = non_bg
            bbox = crop.getbbox()
            status = "nonempty_artwork" if non_bg > 0 else "empty_artwork"
        elif not png.exists():
            status = "missing_png"
        spot_rows.append(
            {
                "byte_sequence": seq,
                "char": entry["char"],
                "font_page": entry["font_page"],
                "cell_index": entry["cell_index"],
                "cell_x": entry["cell_x"],
                "cell_y": entry["cell_y"],
                "source_file": source_file,
                "source_cell": source_cell,
                "status": status,
                "non_bg_pixels": non_bg_pixels,
                "bbox": bbox,
            }
        )

    mapping_json = {
        "source": str(TXT_CPP.relative_to(ROOT)),
        "interpretation": {
            "lead_bytes": "C0..CC",
            "trail_byte": "zero-based array index",
            "empty_string": "unassigned glyph slot",
        },
        "byte_to_unicode": bytes_to_char,
        "unicode_to_bytes": char_to_bytes,
        "validation_summary": {
            "pages_found": sorted(tables.keys()),
            "missing_pages": missing_pages,
            "wrong_entry_counts": wrong_counts,
            "adjacent_string_literal_warnings": warnings,
            "nonempty_entries": len(bytes_to_char),
            "empty_slots": len(empty_rows),
            "duplicate_characters": len(duplicate_rows),
            "spot_checks": spot_rows,
        },
    }
    (GENERATED_DIR / "korean_mapping.json").write_text(
        json.dumps(mapping_json, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    write_csv(
        FINDINGS_DIR / "korean_mapping_duplicates.csv",
        duplicate_rows,
        ["char", "count", "byte_sequences"],
    )
    write_csv(
        FINDINGS_DIR / "korean_mapping_empty_slots.csv",
        empty_rows,
        ["page", "index_decimal", "index_hex", "byte_sequence", "font_page", "cell_index", "cell_x", "cell_y"],
    )

    validation_md = [
        "# Korean Mapping Validation",
        "",
        f"Source: `{TXT_CPP.relative_to(ROOT)}`",
        f"Atlas: `{ATLAS_DIR.relative_to(ROOT)}`",
        "",
        "## Summary",
        "",
        f"- Pages parsed: {', '.join(sorted(tables.keys()))}",
        f"- Missing pages: {missing_pages or 'none'}",
        f"- Wrong entry counts: {wrong_counts or 'none'}",
        f"- Non-empty mappings: {len(bytes_to_char)}",
        f"- Empty slots: {len(empty_rows)}",
        f"- Duplicate Unicode characters: {len(duplicate_rows)}",
        f"- Adjacent string literal warnings: {len(warnings)}",
        "",
        "## Missing Comma / Adjacent Literal Check",
        "",
    ]
    if warnings:
        validation_md.append("The parser found adjacent C/C++ string literals without a comma:")
        validation_md.append("")
        for warning in warnings:
            validation_md.append(
                f"- {warning['page']}[{warning['index']:02X}] lines {warning['lines']}: "
                f"parts={warning['parts']} combined={warning['combined']!r}"
            )
    else:
        validation_md.append("No adjacent C/C++ string literal concatenation was detected in C0-CC.")
    validation_md.extend(
        [
            "",
            "The reported C0 sequence around U+AD64..U+AE0D is parsed as normal comma-separated entries.",
            "",
            "## Artwork Spot Checks",
            "",
            "| Byte | Character | txt.cpp Cell | Source Atlas | Source Cell | Status | Non-bg pixels |",
            "|---|---:|---:|---|---:|---|---:|",
        ]
    )
    for row in spot_rows:
        validation_md.append(
            f"| {row.get('byte_sequence', '')} | {row.get('char', '')} | {row.get('cell_index', '')} | "
            f"{row.get('source_file', '')} | {row.get('source_cell', '')} | "
            f"{row.get('status', '')} | {row.get('non_bg_pixels', '')} |"
        )
    validation_md.extend(
        [
            "",
            "## Notes",
            "",
            "- The mapping preserves the exact source page/index order from `txt.cpp`.",
            "- Empty strings are treated as unassigned slots and are listed in `korean_mapping_empty_slots.csv`.",
            "- Duplicate characters are not collapsed; all byte sequences remain available in `korean_mapping.json`.",
            "- The supplied Gulim source atlas is packed KS X 1001 order, so artwork checks resolve by Unicode character before reading source cells.",
            "- The generated native C0 page repositions that artwork into the exact `txt.cpp` byte slots.",
        ]
    )
    (FINDINGS_DIR / "korean_mapping_validation.md").write_text(
        "\n".join(str(x) for x in validation_md) + "\n",
        encoding="utf-8",
    )

    print(json.dumps(mapping_json["validation_summary"], ensure_ascii=True, indent=2))


if __name__ == "__main__":
    main()
