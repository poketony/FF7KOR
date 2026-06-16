# FFVII 2026 first Korean glyph POC patcher

This folder builds an external Windows x64 runtime patcher for the native 2026 `FFVII.exe`.

It does not modify `FFVII.exe` on disk. It validates runtime-decrypted code bytes, installs reversible detours in the live process, loads one Korean C0 font page through the native font-loader path when that path next runs, and maps Korean `C0 xx` text to that page.

The first test glyph is:

- Character: `가`
- Encoding: `C0 21`
- Page: Korean C0 page 0
- Glyph index: `0x21`
- Width: `64`

## Build

From this directory in a Visual Studio x64 developer command prompt:

```bat
build_msvc_x64.bat
```

The script uses `cl.exe` if it is already on `PATH`. Otherwise it tries to find Visual Studio Build Tools through `vswhere.exe` and loads `vcvars64.bat`.

## GitHub Actions Build

The repository includes a manual workflow at `.github/workflows/build-c0-poc-patcher.yml`.

To run it from GitHub:

1. Open the repository's Actions tab.
2. Select `Build first Korean glyph POC patcher`.
3. Click `Run workflow`.
4. Download the `ff7-2026-first-korean-glyph-poc-windows-x64` artifact from the workflow run page.

Windows may warn that `c0_poc_patcher.exe` is unsigned. This POC patcher must only be run against your own live `FFVII.exe` process. It patches runtime memory only; it does not modify the original on-disk executable.

## Artifact Layout

The workflow artifact contains:

- `c0_poc_patcher.exe`
- `README.md`
- `resources/korean_font/korean_c0_page.tim`
- `resources/korean_font/korean_c0_page.tex`
- `resources/korean_font/korean_c0_page.png`
- `resources/korean_font/korean_c0_page_preview.png`
- `resources/korean_font/selected_glyph_ga_c021.png`
- `resources/korean_font/korean_c0_manifest.json`
- `generated/korean_mapping.json`
- selected findings/test-vector files when present
- `build-info.txt`

Do not add `FFVII.exe`, game archives, runtime dumps, or Ghidra projects to the artifact.

## Install Patch

Extract the artifact. Keep `resources/korean_font/korean_c0_page.tim` beside the patcher in the artifact layout.

Launch FFVII first. Then run:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log first_korean_glyph_patch.log
```

The patcher:

- finds the live `FFVII.exe` process
- validates the Korean C0 font resource header
- copies `resources/korean_font/korean_c0_page.tim` beside the target `FFVII.exe` as `korean_c0_page.tim` if it is not already there
- also copies the sibling `korean_c0_page.tex` alias beside the game when present, because the native resolver may map logical `.tim` names to TEX containers
- locates the `FFVII.exe` module base
- waits for runtime-decrypted signatures
- writes `korean_c0_page.tim` into confirmed unused space in the native font-name allocation
- installs a temporary hook at the existing native Japanese font loader
- waits until the native loader creates a Korean C0 page handle
- installs scanner and renderer detours only after that handle exists

If the file copy fails because the Steam game folder is not writable, copy `resources/korean_font/korean_c0_page.tim` and `resources/korean_font/korean_c0_page.tex` manually into the same folder as `FFVII.exe` and rerun the patcher. If the loader hook does not run before timeout, the patcher restores that hook and reports failure. In that case, start the game again, run the patcher earlier, and stay on a screen where the menu/font resource lifecycle can run.

## Restore

To disable the POC in the current process:

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log first_korean_glyph_patch.log
```

Restore writes back the original overwrite bytes for all patch sites. Remote stub/state allocations remain inert until `FFVII.exe` exits.

## Runtime Behavior

After successful install:

- `C0 21` renders as the Korean glyph `가` from `korean_c0_page.tim`.
- `C0 xx` is consumed as a two-byte Korean sequence in the common scanner and field scanner.
- The field layout scanner consumes the same two-byte sequence and uses width `64`.
- Original `FA-FE` Japanese multibyte behavior remains on the original Japanese pages.
- Ordinary non-C0 single-byte behavior is preserved, except that the Korean build intentionally reserves `C0-CC` as lead bytes.

The first field-string test vector is documented in `findings/first_korean_glyph_test_vector.md`.
