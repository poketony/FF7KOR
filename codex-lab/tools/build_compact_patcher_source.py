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

    text = replace_exact(
        text,
        "    bool suspend_threads = true;\n"
        "    std::wstring log_path = L\"menu_jafont_extension_patch.log\";",
        "    bool suspend_threads = true;\n"
        "    std::wstring stage = L\"field\";\n"
        "    std::wstring log_path = L\"menu_jafont_extension_patch.log\";",
    )

    text = text.replace("{0x80, 0xf9, 0xcc}", "{0x80, 0xf9, 0xc5}")
    text = text.replace("{0x3c, 0xcc}", "{0x3c, 0xc5}")
    text = text.replace("{0x80, 0xfb, 0xcc}", "{0x80, 0xfb, 0xc5}")
    text = text.replace("0x3d, 0xcc, 0x00, 0x00, 0x00", "0x3d, 0xc5, 0x00, 0x00, 0x00")

    text = text.replace("jafont_7.tim .. jafont_19.tim", "jafont_7.tim .. jafont_12.tim")
    text = text.replace("jafont_7..19", "jafont_7..12")
    text = text.replace("jafont_7.tex..jafont_19.tex", "jafont_7.tex..jafont_12.tex")
    text = text.replace("menu_jafont_7_19_loader_hook", "menu_jafont_7_12_loader_hook")

    text = replace_exact(
        text,
        "bool Install(HANDLE process, DWORD pid, uint64_t module_base, const Options& opt) {",
        "bool ShouldInstallTextSite(PatchKind kind, const std::wstring& stage) {\n"
        "    if (_wcsicmp(stage.c_str(), L\"loader\") == 0) return false;\n"
        "    if (_wcsicmp(stage.c_str(), L\"glyph\") == 0) return kind == PatchKind::GlyphRenderer;\n"
        "    if (_wcsicmp(stage.c_str(), L\"field\") == 0) {\n"
        "        return kind == PatchKind::FieldRender || kind == PatchKind::FieldLayout ||\n"
        "               kind == PatchKind::GlyphRenderer;\n"
        "    }\n"
        "    return kind != PatchKind::ExtraJafontLoaderHook;\n"
        "}\n\n"
        "bool Install(HANDLE process, DWORD pid, uint64_t module_base, const Options& opt) {",
    )

    text = replace_exact(
        text,
        "    std::vector<PatchSite> text_sites;\n"
        "    for (const auto& site : sites) {\n"
        "        if (site.kind != PatchKind::ExtraJafontLoaderHook) text_sites.push_back(site);\n"
        "    }",
        "    if (_wcsicmp(opt.stage.c_str(), L\"loader\") == 0) {\n"
        "        Log(L\"stage=loader: font pages loaded; scanner and renderer detours skipped\");\n"
        "        return true;\n"
        "    }\n\n"
        "    std::vector<PatchSite> text_sites;\n"
        "    for (const auto& site : sites) {\n"
        "        if (ShouldInstallTextSite(site.kind, opt.stage)) text_sites.push_back(site);\n"
        "    }\n"
        "    Log(L\"install stage: \" + opt.stage);",
    )

    text = replace_exact(
        text,
        "        for (const auto& site : sites) {\n"
        "            if (site.kind == PatchKind::ExtraJafontLoaderHook) continue;\n"
        "            if (!InstallOneSite(process, module_base, site, remote_state)) {",
        "        for (const auto& site : sites) {\n"
        "            if (!ShouldInstallTextSite(site.kind, opt.stage)) continue;\n"
        "            if (!InstallOneSite(process, module_base, site, remote_state)) {",
    )

    text = replace_exact(
        text,
        "        << L\"                              [--wait-ms N] [--log path] [--no-suspend]\\n\";",
        "        << L\"                              [--wait-ms N] [--log path] [--no-suspend]\\n\"\n"
        "        << L\"                              [--stage loader|glyph|field|full]\\n\";",
    )

    text = replace_exact(
        text,
        "        else if (a == L\"--log\" && i + 1 < argc) opt.log_path = argv[++i];\n"
        "        else if (a == L\"--no-suspend\") opt.suspend_threads = false;",
        "        else if (a == L\"--log\" && i + 1 < argc) opt.log_path = argv[++i];\n"
        "        else if (a == L\"--stage\" && i + 1 < argc) opt.stage = argv[++i];\n"
        "        else if (a == L\"--no-suspend\") opt.suspend_threads = false;",
    )

    text = replace_exact(
        text,
        "    return opt;\n}\n\n} // namespace",
        "    if (_wcsicmp(opt.stage.c_str(), L\"loader\") != 0 &&\n"
        "        _wcsicmp(opt.stage.c_str(), L\"glyph\") != 0 &&\n"
        "        _wcsicmp(opt.stage.c_str(), L\"field\") != 0 &&\n"
        "        _wcsicmp(opt.stage.c_str(), L\"full\") != 0) {\n"
        "        PrintUsage();\n"
        "        ExitProcess(2);\n"
        "    }\n"
        "    return opt;\n}\n\n} // namespace",
    )

    OUTPUT.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
