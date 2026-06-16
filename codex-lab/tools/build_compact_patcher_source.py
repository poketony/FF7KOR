from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "codex-lab" / "poc_runtime_patch" / "c0_poc_patcher.cpp"
OUTPUT = ROOT / "codex-lab" / "poc_runtime_patch" / "c0_poc_patcher_compact.cpp"


def require(text: str, token: str) -> None:
    if token not in text:
        raise RuntimeError(f"required source token is missing: {token}")


def replace_once(text: str, old: str, new: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one occurrence of {old!r}, found {count}")
    return text.replace(old, new, 1)


def remove_site(text: str, site_id: str) -> str:
    pattern = re.compile(
        r"\n\s*\{\s*\"" + re.escape(site_id) + r"\"\s*,.*?\n\s*\},",
        re.DOTALL,
    )
    updated, count = pattern.subn("", text, count=1)
    if count != 1:
        raise RuntimeError(f"failed to remove patch site: {site_id}")
    return updated


def main() -> None:
    text = SOURCE.read_text(encoding="utf-8")

    text = replace_once(
        text,
        "constexpr uint32_t kLastKoreanLead = 0xCC;",
        "constexpr uint32_t kLastKoreanLead = 0xC5;",
    )

    replacements = {
        "{0x80, 0xf9, 0xcc}": "{0x80, 0xf9, 0xc5}",
        "{0x3c, 0xcc}": "{0x3c, 0xc5}",
        "{0x80, 0xfb, 0xcc}": "{0x80, 0xfb, 0xc5}",
        "0x3d, 0xcc, 0x00, 0x00, 0x00": "0x3d, 0xc5, 0x00, 0x00, 0x00",
        "jafont_7.tim .. jafont_19.tim": "jafont_7.tim .. jafont_12.tim",
        "jafont_7..19": "jafont_7..12",
        "jafont_7.tex..jafont_19.tex": "jafont_7.tex..jafont_12.tex",
        "menu_jafont_7_19_loader_hook": "menu_jafont_7_12_loader_hook",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)

    text = remove_site(text, "common_render_scanner_prefix")
    text = remove_site(text, "common_width_scanner_prefix")

    required = [
        "kLastKoreanLead = 0xC5",
        "menu_jafont_7_12_loader_hook",
        "jafont_7.tim .. jafont_12.tim",
        "field_render_scanner_prefix",
        "field_layout_scanner_prefix",
        "glyph_renderer_c0_page_select",
    ]
    forbidden = [
        "common_render_scanner_prefix",
        "common_width_scanner_prefix",
        "kLastKoreanLead = 0xCC",
        "jafont_13.tim",
        "jafont_19.tim",
    ]
    for token in required:
        require(text, token)
    for token in forbidden:
        if token in text:
            raise RuntimeError(f"forbidden token remains in generated source: {token}")

    OUTPUT.write_text(text, encoding="utf-8")
    print("generated field-only patcher source")
    print("included hooks: loader, field render, field layout, glyph renderer")
    print("excluded hooks: common render, common width")


if __name__ == "__main__":
    main()
