# menu_ja jafont Extension Runtime Test

This replaces the old `C0 00 -> FA 00` and loose `korean_c0_page.tim` test plan.

## Required Resource Setup

Insert these generated files into the game's `menu_ja.lgp`:

```text
C:\Users\JO\FF7KOR\codex-lab\generated\menu_ja_jafont_ext_tbl_exact\jafont_7.tex
...
C:\Users\JO\FF7KOR\codex-lab\generated\menu_ja_jafont_ext_tbl_exact\jafont_19.tex
```

Do not copy them beside `FFVII.exe`.

## Field Text To Edit

Edit:

```text
C:\Program Files (x86)\Steam\steamapps\common\FINAL FANTASY VII Steam Edition\ff7\workingdir\data\field\jfleve.lgp
```

Makou Reactor target:

```text
Field: md1stin
Text: 30
```

Target phrase:

```text
가다신참
날따라와
```

Expected byte sequence:

```text
C0 1A C2 60 C7 02 C9 A9 0A C1 91 C3 26 C3 80 C8 10 FF
```

If Makou Reactor emits a field-specific line-break control instead of raw `0A`, preserve that control sequence.

## Patcher Launch Procedure

Start the patcher before launching the game:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log menu_jafont_extension_patch.log
```

When the patcher says it is waiting for `FFVII.exe`, launch the game.

Continue only after the log shows:

```text
loader runtime signature validated
extra menu jafont handles:
scanner/renderer runtime signatures validated
completed successfully
```

Then enter the field scene containing `md1stin` Text 30.

## Expected Visual Result

The dialogue box should show:

```text
가다신참
날따라와
```

The glyphs must come from `jafont_7.tex` through `jafont_19.tex`, not from substituted Japanese glyphs.

## Checks

- The first character `가` renders from `C0 1A`.
- `다`, `신`, `참`, `날`, `따`, `라`, and `와` render from their mapped pages.
- Each Korean glyph consumes exactly two bytes.
- The character after each Korean glyph is not shifted or swallowed.
- Line break behavior is preserved.
- `FF` terminates the string normally.
- Existing Japanese `FA..FE` multibyte rendering still works.
- The game does not return to the launcher after entering the scene.

## Restore

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log menu_jafont_extension_patch.log
```

Closing `FFVII.exe` also removes runtime patches because the original executable is not modified on disk.
