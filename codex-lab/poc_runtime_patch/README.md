# FFVII 2026 menu_ja jafont extension patcher

This package extends the Japanese font resource path through `menu_ja.lgp`.

It does not copy Korean font files beside `FFVII.exe`, does not load a loose `korean_c0_page.tim`, does not use overlay smoke-test drawing, and does not modify `FFVII.exe` on disk.

## Required resource setup

Insert the generated TEX files from:

```text
codex-lab/generated/menu_ja_jafont_ext_tbl_exact/
```

into `menu_ja.lgp` as:

```text
jafont_7.tex
jafont_8.tex
...
jafont_19.tex
```

The active encoding and page policy is fixed as follows:

```text
C0 xx -> jafont_7.tex slot xx
C1 xx -> jafont_8.tex slot xx
C2 xx -> jafont_9.tex slot xx
C3 xx -> jafont_10.tex slot xx
C4 xx -> jafont_11.tex slot xx
C5 xx -> jafont_12.tex slot xx
C6 xx -> jafont_13.tex slot xx
C7 xx -> jafont_14.tex slot xx
C8 xx -> jafont_15.tex slot xx
C9 xx -> jafont_16.tex slot xx
CA xx -> jafont_17.tex slot xx
CB xx -> jafont_18.tex slot xx
CC xx -> jafont_19.tex slot xx
```

The trail byte remains the exact slot index within the selected 16x16 page. No table entry is compacted, shifted, or reordered.

## Scope and success criteria

This project is not considered successful merely because one page or one glyph appears.

A successful build and runtime test must satisfy the complete extension contract:

- all 13 pages `jafont_7.tex` through `jafont_19.tex` are present;
- all leads `C0..CC` are recognized as two-byte leads;
- every lead selects the corresponding page shown above;
- the trail byte selects the same numbered slot on that page;
- common rendering, field rendering, width calculation, and field layout use the same byte-consumption rule;
- all 13 added page handles are created successfully;
- original `FA..FE` Japanese behavior remains intact;
- the generated mapping JSON matches every entry in `ff7K(PC).tbl`, not only a few sample characters.

## Current runtime architecture

The current runtime patch does not expand the original six-page font structure in place.

Instead it:

1. hooks the existing Japanese font-loader lifecycle;
2. asks the game's native resource command to resolve logical names `jafont_7.tim` through `jafont_19.tim`;
3. relies on the existing `menu_ja.lgp` resolver to find the corresponding `.tex` resources;
4. stores the 13 returned handles in patcher-owned remote state;
5. redirects `C0..CC` page selection to those handles.

This is a runtime extension built on the game's existing resource mechanism. It must not be described as an in-place expansion of the original `1..6` loop unless that implementation is added and verified separately.

## Build

From this directory in a Visual Studio x64 developer command prompt:

```bat
build_msvc_x64.bat
```

The script uses `cl.exe` if it is already available. Otherwise it locates Visual Studio Build Tools through `vswhere.exe` and initializes the x64 toolchain.

## GitHub Actions

Workflow:

```text
.github/workflows/build-c0-poc-patcher.yml
```

The workflow performs these checks before uploading an artifact:

1. parses all `ff7K(PC).tbl` entries;
2. confirms that the table covers every lead from `C0` through `CC`;
3. rejects any mapping with trail byte `FF`;
4. confirms the exact 13-file TEX set `jafont_7.tex` through `jafont_19.tex`;
5. confirms the expected TEX file size for every page;
6. compares every JSON mapping entry with the table, page number, slot, row, and column;
7. builds the x64 patcher;
8. stages exactly 13 TEX pages and the required mapping metadata.

The uploaded artifact is:

```text
ff7-2026-menu-ja-jafont-extension-windows-x64
```

## Install

Start the patcher before launching the game so the loader hook is present before the Japanese font-loader lifecycle completes:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log menu_jafont_extension_patch.log
```

Then launch the game normally.

The installation fails if any one of the 13 added handles is missing. A partial page load is not treated as success.

## Restore

To disable the runtime patch in the current process:

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log menu_jafont_extension_patch.log
```

Restore writes back the original bytes for all patch sites. Remote allocations remain inert until `FFVII.exe` exits.

## Field text testing

The patcher does not modify `jfleve.lgp`. Field text is edited manually with Makou Reactor.

Current example location:

```text
Field: md1stin
Text: 30
```

For:

```text
가다신참
날따라와
```

the table-exact byte sequence is:

```text
C0 1A C2 60 C7 02 C9 A9 0A C1 91 C3 26 C3 80 C8 10 FF
```

Preserve Makou Reactor's actual field line-break/control bytes if they differ from raw `0A`.

Temporary Japanese-table input equivalents before Makou Reactor encoding is updated:

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

## Known limit

The 13 added handles live in patcher-owned remote state rather than an expanded original font-page array. This avoids writing beyond the original six-page structure, but the loader bridge and all decoder paths still require full in-game verification across `C0..CC`.
