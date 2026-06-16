# FFVII 2026 first Korean glyph POC patcher

This folder builds an external Windows x64 runtime patcher for the native 2026 `FFVII.exe`.

It does not modify `FFVII.exe` on disk. It validates runtime-decrypted code bytes, installs reversible detours in the live process, loads one Korean C0 font page through the native font-loader path, and maps the first selected Korean test sequence, `C0 21`, to that page.

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

Recommended timing for the current POC is to run the patcher first, then launch FFVII while it is waiting for the process. This gives the loader hook a chance to install before the native Japanese font loader runs:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log first_korean_glyph_patch.log
```

After this prints `waiting for process FFVII.exe`, launch the game normally.

If the game is already running, the same command is still valid. In that case the patcher first tries a direct native loader command. If the Japanese font lifecycle has already passed and direct loading returns no handle, the fallback hook may wait until timeout.

The patcher:

- finds the live `FFVII.exe` process
- validates the Korean C0 font resource header
- copies `resources/korean_font/korean_c0_page.tim` beside the target `FFVII.exe` as `korean_c0_page.tim` if it is not already there
- also copies the sibling `korean_c0_page.tex` alias beside the game when present, because the native resolver may map logical `.tim` names to TEX containers
- locates the `FFVII.exe` module base
- waits first only for the runtime-decrypted loader signature
- writes `korean_c0_page.tim` into confirmed unused space in the native font-name allocation
- if the six Japanese font handles are already loaded, first attempts a direct native loader command using the current VM stack arguments, restoring `DAT_1420395C8` afterward
- if Japanese handles are not loaded yet, or direct loading returns no handle, installs a temporary hook at the existing native Japanese font loader
- the loader hook writes `korean_c0_page.tim` into the native font-name allocation at execution time, so it can be installed before that allocation has been initialized
- waits until either path creates a Korean C0 page handle
- waits for the scanner/renderer signatures after the Korean handle exists
- installs scanner and renderer detours only after that handle exists
- installs a temporary field overlay smoke-test detour that draws `C0 21` through the common glyph renderer from field render timing

If the file copy fails because the Steam game folder is not writable, copy `resources/korean_font/korean_c0_page.tim` and `resources/korean_font/korean_c0_page.tex` manually into the same folder as `FFVII.exe` and rerun the patcher. If both direct loading and the loader hook return no handle before timeout, the patcher restores the loader hook and reports failure. In that case, close the game, start the patcher first, then launch the game while the patcher is already waiting.

## Restore

To disable the POC in the current process:

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log first_korean_glyph_patch.log
```

Restore writes back the original overwrite bytes for all patch sites. Remote stub/state allocations remain inert until `FFVII.exe` exits.

## Runtime Behavior

After successful install:

- `C0 21` renders as the Korean glyph `가` from `korean_c0_page.tim`.
- The field overlay smoke test intentionally draws one extra `가` at a fixed coordinate after selected field text rendering helpers. This proves the Korean page and common glyph renderer are live even before the field-specific direct text loop is fully converted.
- The common menu/battle render and width scanners consume only exact `C0 21` for this crash-safe first POC. Other original `C0 xx` bytes continue through the original single-byte path.
- The field scanner hooks still implement the broader C0-page path for the target `jfleve.lgp` test and should be narrowed to `txt.cpp`-validated byte pairs before enabling a full unconverted playthrough.
- The field layout scanner consumes the same two-byte sequence and uses width `64`.
- Original `FA-FE` Japanese multibyte behavior remains on the original Japanese pages.
- Ordinary non-C0 single-byte behavior is preserved. Full `C0-CC` reservation is still the final Korean-resource goal, but it is not safe while unconverted original resources still pass through these scanners.

The first field-string test vector is documented in `findings/first_korean_glyph_test_vector.md`.
