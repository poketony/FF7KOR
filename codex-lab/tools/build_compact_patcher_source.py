from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "codex-lab" / "poc_runtime_patch" / "c0_poc_patcher.cpp"
OUTPUT = ROOT / "codex-lab" / "poc_runtime_patch" / "c0_poc_patcher_compact.cpp"


def replace_exact(text: str, old: str, new: str, expected: int = 1) -> str:
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"expected {expected} occurrences, found {count}: {old!r}")
    return text.replace(old, new)


def main() -> None:
    text = SOURCE.read_text(encoding="utf-8")

    text = replace_exact(
        text,
        "constexpr uint32_t kLastKoreanLead = 0xCC;\n"
        "constexpr uint32_t kFirstExtraJafontPage = 7;\n"
        "constexpr size_t kExtraJafontPageCount = kLastKoreanLead - kFirstKoreanLead + 1;",
        "constexpr uint32_t kLastKoreanLead = 0xC5;\n"
        "constexpr uint32_t kFirstExtraJafontPage = 7;\n"
        "constexpr size_t kExtraJafontPageCount = kLastKoreanLead - kFirstKoreanLead + 1;",
    )

    text = text.replace("{0x80, 0xf9, 0xcc}", "{0x80, 0xf9, 0xc5}")
    text = text.replace("{0x3c, 0xcc}", "{0x3c, 0xc5}")
    text = text.replace("{0x80, 0xfb, 0xcc}", "{0x80, 0xfb, 0xc5}")
    text = text.replace("0x3d, 0xcc, 0x00, 0x00, 0x00", "0x3d, 0xc5, 0x00, 0x00, 0x00")

    text = text.replace("jafont_7.tim .. jafont_19.tim", "jafont_7.tim .. jafont_12.tim")
    text = text.replace("jafont_7..19", "jafont_7..12")
    text = text.replace("jafont_7.tex..jafont_19.tex", "jafont_7.tex..jafont_12.tex")
    text = text.replace("menu_jafont_7_19_loader_hook", "menu_jafont_7_12_loader_hook")

    OUTPUT.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
