from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "codex-lab" / "poc_runtime_patch" / "c0_poc_patcher.cpp"
OUTPUT = ROOT / "codex-lab" / "poc_runtime_patch" / "c0_poc_patcher_compact.cpp"


def replace_exact(text: str, old: str, new: str, expected: int = 1) -> str:
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"expected {expected} occurrences, found {count}: {old!r}")
    return text.replace(old, new)


def replace_regex(text: str, pattern: str, replacement: str) -> str:
    new_text, count = re.subn(pattern, replacement, text, flags=re.S)
    if count != 1:
        raise RuntimeError(f"expected one regex replacement, found {count}: {pattern}")
    return new_text


def main() -> None:
    text = SOURCE.read_text(encoding="utf-8")
    text = replace_exact(
        text,
        "constexpr uint32_t kLastKoreanLead = 0xCC;\n"
        "constexpr uint32_t kFirstExtraJafontPage = 7;\n"
        "constexpr size_t kExtraJafontPageCount = kLastKoreanLead - kFirstKoreanLead + 1;",
        "constexpr uint32_t kLastKoreanLead = 0xCB;\n"
        "constexpr uint32_t kLastExtraKoreanLead = 0xC8;\n"
        "constexpr uint32_t kFirstReusedKoreanLead = 0xC9;\n"
        "constexpr uint32_t kFirstExtraJafontPage = 7;\n"
        "constexpr size_t kExtraJafontPageCount = kLastExtraKoreanLead - kFirstKoreanLead + 1;",
    )
    text = text.replace("{0x80, 0xf9, 0xcc}", "{0x80, 0xf9, 0xcb}")
    text = text.replace("{0x3c, 0xcc}", "{0x3c, 0xcb}")
    text = text.replace("{0x80, 0xfb, 0xcc}", "{0x80, 0xfb, 0xcb}")

    glyph_renderer = r'''std::vector<uint8_t> MakeGlyphRendererStub(uint64_t module_base, uint64_t remote_state) {
    const uint64_t original_branch = module_base + 0x15720c8;
    const uint64_t common_handle_test = module_base + 0x1572131;
    const uint64_t default_return = module_base + 0x157233d;
    const uint64_t font_struct_ptr_global = module_base + kGlobalFontStructPtrRva;
    std::vector<uint8_t> b = {
        0x41, 0x0f, 0xb7, 0xc0,
        0x4c, 0x89, 0xbc, 0x24, 0xe0, 0x00, 0x00, 0x00,
        0x3d, 0xc0, 0x00, 0x00, 0x00
    };
    size_t jb_original = AppendJcc32(b, 0x82);
    b.insert(b.end(), {0x3d, 0xc8, 0x00, 0x00, 0x00});
    size_t ja_extra = AppendJcc32(b, 0x87);
    b.insert(b.end(), {0x2d, 0xc0, 0x00, 0x00, 0x00});
    AppendMovR10Imm64(b, remote_state);
    b.insert(b.end(), {0x41, 0x8b, 0x3c, 0x82, 0x85, 0xff});
    size_t jz_default_extra = AppendJcc32(b, 0x84);
    AppendAbsJmp(b, common_handle_test);

    size_t reused_label = b.size();
    b.insert(b.end(), {0x3d, 0xcb, 0x00, 0x00, 0x00});
    size_t ja_original_reused = AppendJcc32(b, 0x87);
    b.insert(b.end(), {
        0x2d, 0xc9, 0x00, 0x00, 0x00,
        0x6b, 0xc0, 0x14,
        0x83, 0xc0, 0x4c
    });
    AppendMovR10Imm64(b, font_struct_ptr_global);
    b.insert(b.end(), {0x4d, 0x8b, 0x12, 0x4d, 0x85, 0xd2});
    size_t jz_default_struct = AppendJcc32(b, 0x84);
    b.insert(b.end(), {0x41, 0x8b, 0x3c, 0x02, 0x85, 0xff});
    size_t jz_default_reused = AppendJcc32(b, 0x84);
    AppendAbsJmp(b, common_handle_test);

    size_t original = b.size();
    b.insert(b.end(), {0x3d, 0xfe, 0x00, 0x00, 0x00});
    AppendAbsJmp(b, original_branch);
    size_t default_label = b.size();
    AppendAbsJmp(b, default_return);

    PatchJcc32(b, jb_original, original);
    PatchJcc32(b, ja_extra, reused_label);
    PatchJcc32(b, ja_original_reused, original);
    PatchJcc32(b, jz_default_extra, default_label);
    PatchJcc32(b, jz_default_struct, default_label);
    PatchJcc32(b, jz_default_reused, default_label);
    return b;
}
'''
    text = replace_regex(
        text,
        r"std::vector<uint8_t> MakeGlyphRendererStub\(uint64_t module_base, uint64_t remote_state\) \{.*?\n\}\n\nstd::vector<uint8_t> MakeFontLoaderHookStub",
        glyph_renderer + "\nstd::vector<uint8_t> MakeFontLoaderHookStub",
    )
    text = text.replace("jafont_7.tim .. jafont_19.tim", "jafont_7.tim .. jafont_15.tim")
    text = text.replace("jafont_7..19", "jafont_7..15")
    text = text.replace("jafont_7.tex..jafont_19.tex", "jafont_7.tex..jafont_15.tex")
    text = text.replace("menu_jafont_7_19_loader_hook", "menu_jafont_7_15_loader_hook")
    OUTPUT.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
