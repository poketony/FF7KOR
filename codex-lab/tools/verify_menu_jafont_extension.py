from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TBL_PATH = ROOT / "ff7K(PC).tbl"
ASSET_ROOT = ROOT / "codex-lab" / "generated" / "menu_ja_jafont_ext_tbl_exact"
MAPPING_JSON = ASSET_ROOT / "menu_jafont_extension_mapping.json"

FIRST_LEAD = 0xC0
LAST_LEAD = 0xCC
FIRST_JAFONT = 7
EXPECTED_PAGE_COUNT = LAST_LEAD - FIRST_LEAD + 1
EXPECTED_TEX_SIZE = 4_194_540


def fail(message: str) -> None:
    raise RuntimeError(message)


def parse_tbl() -> dict[str, str]:
    if not TBL_PATH.is_file():
        fail(f"missing table: {TBL_PATH}")

    mapping: dict[str, str] = {}
    for line_no, raw_line in enumerate(TBL_PATH.read_text(encoding="cp949").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            fail(f"{TBL_PATH}:{line_no}: missing '='")

        key, value = line.split("=", 1)
        key = key.strip().upper()
        if len(key) != 4:
            fail(f"{TBL_PATH}:{line_no}: expected four hex digits, got {key!r}")
        try:
            lead = int(key[:2], 16)
            trail = int(key[2:], 16)
        except ValueError as exc:
            fail(f"{TBL_PATH}:{line_no}: invalid hex key {key!r}: {exc}")

        if not (FIRST_LEAD <= lead <= LAST_LEAD):
            fail(f"{TBL_PATH}:{line_no}: lead outside C0..CC: {key}")
        if trail == 0xFF:
            fail(f"{TBL_PATH}:{line_no}: trail FF conflicts with the FF7 terminator: {key}")
        if len(value) != 1:
            fail(f"{TBL_PATH}:{line_no}: expected exactly one Unicode character for {key}")
        if key in mapping:
            fail(f"{TBL_PATH}:{line_no}: duplicate byte sequence: {key}")
        mapping[key] = value

    if not mapping:
        fail("ff7K(PC).tbl contains no mappings")

    leads_present = {int(key[:2], 16) for key in mapping}
    expected_leads = set(range(FIRST_LEAD, LAST_LEAD + 1))
    if leads_present != expected_leads:
        missing = sorted(expected_leads - leads_present)
        extra = sorted(leads_present - expected_leads)
        fail(f"table page coverage mismatch; missing={missing}, extra={extra}")

    return mapping


def verify_tex_pages() -> None:
    if not ASSET_ROOT.is_dir():
        fail(f"missing generated asset directory: {ASSET_ROOT}")

    expected_names = {f"jafont_{n}.tex" for n in range(FIRST_JAFONT, FIRST_JAFONT + EXPECTED_PAGE_COUNT)}
    actual_names = {path.name for path in ASSET_ROOT.glob("jafont_*.tex") if path.is_file()}
    if actual_names != expected_names:
        fail(
            "TEX page set mismatch; "
            f"missing={sorted(expected_names - actual_names)}, "
            f"extra={sorted(actual_names - expected_names)}"
        )

    for name in sorted(expected_names):
        path = ASSET_ROOT / name
        size = path.stat().st_size
        if size != EXPECTED_TEX_SIZE:
            fail(f"unexpected TEX size for {name}: {size}, expected {EXPECTED_TEX_SIZE}")


def verify_mapping_json(tbl: dict[str, str]) -> None:
    if not MAPPING_JSON.is_file():
        fail(f"missing mapping JSON: {MAPPING_JSON}")

    data = json.loads(MAPPING_JSON.read_text(encoding="utf-8"))
    byte_to_unicode = data.get("byte_to_unicode")
    if not isinstance(byte_to_unicode, dict):
        fail("mapping JSON has no byte_to_unicode object")

    json_keys = set(byte_to_unicode)
    tbl_keys = set(tbl)
    if json_keys != tbl_keys:
        fail(
            "mapping key set differs from ff7K(PC).tbl; "
            f"missing={sorted(tbl_keys - json_keys)[:20]}, "
            f"extra={sorted(json_keys - tbl_keys)[:20]}"
        )

    for key, char in tbl.items():
        entry = byte_to_unicode[key]
        if not isinstance(entry, dict):
            fail(f"mapping entry is not an object: {key}")

        expected_unicode = f"U+{ord(char):04X}"
        expected_page = FIRST_JAFONT + (int(key[:2], 16) - FIRST_LEAD)
        expected_jafont = f"jafont_{expected_page}.tex"
        expected_slot = key[2:]
        expected_row = int(expected_slot, 16) // 16 + 1
        expected_column = int(expected_slot, 16) % 16 + 1

        checks = {
            "char": char,
            "unicode": expected_unicode,
            "jafont": expected_jafont,
            "slot_hex": expected_slot,
            "row_1based": expected_row,
            "column_1based": expected_column,
        }
        for field, expected in checks.items():
            actual = entry.get(field)
            if actual != expected:
                fail(f"{key}: {field}={actual!r}, expected {expected!r}")


def main() -> None:
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except AttributeError:
        pass

    tbl = parse_tbl()
    verify_tex_pages()
    verify_mapping_json(tbl)

    print("menu_ja jafont extension verification passed")
    print(f"pages: {EXPECTED_PAGE_COUNT} (jafont_7.tex .. jafont_19.tex)")
    print(f"mapping entries: {len(tbl)}")
    print("lead coverage: C0..CC")
    print("trail FF mappings: 0")


if __name__ == "__main__":
    main()
