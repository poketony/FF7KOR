# ff7kor.dll

Minimal helper DLL for the FF7 2026 Japanese engine font extension.

Current stage:

- loaded by patched `ff7_ja`
- writes `ff7kor.log`
- verifies key Japanese-engine addresses
- hooks the first menu font graphics-object load call at `0x6C160C`
- attempts to load `hangulfont_1.tim` through `hangulfont_13.tim`, then falls back to `.tex`
- calls the original blend-mode setup at `0x674659` before each Hangul font page load
- hooks the 640x480 menu/font draw call at `0x6CCAB8` and draws loaded Hangul font pages
- hooks the 640x480 field text font draw call at `0x6ECB8B`, draws Hangul font pages, then resets them
- hooks the field text decode loop at `0x6E70F5`
- treats `C0..CC` plus the following byte as one fixed-width Hangul glyph in field dialogs
- draws Hangul field glyphs as 31x24 screen-space quads, matching the original 24px field glyph height
- calls the original graphics-object prepare routine at `0x66E272` before writing Hangul glyph vertices

Build with an x86 Visual Studio Developer Command Prompt, without CMake:

```bat
build_msvc_win32_cl.bat
```

Or commit and push `.github\workflows\build-ff7kor-dll.yml` to build only this DLL on GitHub Actions.

Or build with CMake:

```bat
build_msvc_win32.bat
```

The output DLL must be named `ff7kor.dll` and placed next to the patched `ff7_ja` when testing.

This project intentionally does not depend on FFNx, BGFX, FFmpeg, or vcpkg.

Expected first test:

1. Put `hangulfont_1.tex` ... `hangulfont_13.tex` into `menu_ja.lgp`.
2. Place `ff7kor.dll` next to the patched `ff7_ja`.
3. Launch the game and inspect `ff7kor.log`.
4. Confirm that the font load hook installs and the Hangul font pages return non-null pointers.
5. Confirm that `font draw hook installed`, `field font draw hook installed`, and `field text decode hook installed` are logged.
6. Confirm that `hangul field draw hook ran` logs a non-zero `drawn_pages` after a dialog is shown.
7. Put a `C0 00` test glyph in a field text and confirm it renders from `hangulfont_1`.

Prepared test asset:

- `test-assets\jfleve-startmap-ga-test.lgp` changes `startmap` text 0 to `U+AC00`.
- The decompressed `startmap.dec` contains `C0 00` at offset `988`.

Prepared smoke-test package:

- `C:\Users\JO\FF7KOR\FF7_data\font_extension_test`
- includes a patched `ff7_ja` copy, `menu_ja.lgp` with `hangulfont_1.tex` through `hangulfont_13.tex`,
  and the `jfleve.lgp` field smoke-test file.
