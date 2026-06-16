# FFVII 2026 menu_ja jafont extension patcher

This patcher is now aligned with the `menu_ja.lgp` resource-extension plan.

It does not copy Korean font files beside `FFVII.exe`, does not load a loose `korean_c0_page.tim`, and does not modify `FFVII.exe` on disk.

## Required Game Resource Setup

Insert the generated TEX files into `menu_ja.lgp`:

```text
C:\Users\JO\FF7KOR\codex-lab\generated\menu_ja_jafont_ext_tbl_exact\jafont_7.tex
...
C:\Users\JO\FF7KOR\codex-lab\generated\menu_ja_jafont_ext_tbl_exact\jafont_19.tex
```

The intended mapping is:

```text
C0 xx -> jafont_7.tex slot xx
C1 xx -> jafont_8.tex slot xx
...
CC xx -> jafont_19.tex slot xx
```

The native loader still receives logical names `jafont_7.tim` through `jafont_19.tim`, matching the original `jafont_%d.tim` path. The resources themselves should be inserted as `.tex` files in `menu_ja.lgp`, consistent with the existing extracted Japanese font resources.

## Build

From this directory in a Visual Studio x64 developer command prompt:

```bat
build_msvc_x64.bat
```

The script uses `cl.exe` if it is already on `PATH`. Otherwise it tries to find Visual Studio Build Tools through `vswhere.exe` and loads `vcvars64.bat`.

## GitHub Actions Artifact

The workflow is:

```text
.github/workflows/build-c0-poc-patcher.yml
```

Run it manually from GitHub Actions with **Build menu_ja jafont extension patcher** -> **Run workflow**.

Download the artifact named:

```text
ff7-2026-menu-ja-jafont-extension-windows-x64
```

Windows may warn that `c0_poc_patcher.exe` is unsigned. The patcher must only be run against your own live `FFVII.exe` process. It patches runtime memory only; the original on-disk `FFVII.exe` is not modified.

## Install Patch

Run the patcher before launching the game, so the hook is installed before the native Japanese font loader finishes:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log menu_jafont_extension_patch.log
```

When it prints that it is waiting for `FFVII.exe`, launch the game normally.

The patcher:

- waits for the live `FFVII.exe` process;
- validates runtime-decrypted patch-site bytes;
- hooks the existing native font-loader epilogue;
- asks the native loader to resolve `jafont_7.tim` through `jafont_19.tim`;
- stores the returned handles in patcher-owned remote state;
- patches the common and field scanners so `C0..CC` consume one trail byte;
- patches the glyph renderer so `C0..CC` select the newly loaded handles;
- patches the common/field width scanner prefix handling so measurement consumes the same byte pairs.

If install times out waiting for handles, either the patcher was started too late or `jafont_7.tex` through `jafont_19.tex` are not present in `menu_ja.lgp`.

## Restore

To disable the runtime patch in the current process:

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log menu_jafont_extension_patch.log
```

Restore writes back the original overwrite bytes for all patch sites. Remote stub/state allocations remain inert until `FFVII.exe` exits.

## Test Text Location

The current field test target is:

```text
C:\Program Files (x86)\Steam\steamapps\common\FINAL FANTASY VII Steam Edition\ff7\workingdir\data\field\jfleve.lgp
```

Inside Makou Reactor:

```text
Field: md1stin
Text: 30
```

So the test bytes belong to `jfleve.lgp`, field `md1stin`, text entry `Text 30`. The font pages belong to `menu_ja.lgp`; do not put the test sentence bytes into `menu_ja.lgp`.

For:

```text
가다신참
날따라와
```

the table-exact byte sequence is:

```text
C0 1A C2 60 C7 02 C9 A9 0A C1 91 C3 26 C3 80 C8 10 FF
```

Preserve Makou Reactor's actual line-break/control bytes if it emits something other than raw `0A`.

Temporary Japanese-table input equivalents before Makou encoding is updated:

```text
가 C0 1A -> Ｍゼ
다 C2 60 -> Ｏチ
신 C7 02 -> Ｔビ
참 C9 A9 -> Ｖぅ
날 C1 91 -> Ｎや
따 C3 26 -> Ｐド
라 C3 80 -> Ｐム
와 C8 10 -> Ｕゲ
```

## Known Limit

This runtime patch does not expand the original six-slot struct in place. That would overwrite fixed data after `+0x78` and run past the original allocation for page 19. Instead, extra `jafont_7..19` handles are stored in patcher-owned remote state while the native resolver still loads the resources from `menu_ja.lgp`.
