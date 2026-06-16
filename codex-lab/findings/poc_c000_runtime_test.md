# First Korean Glyph Runtime Test Plan

## Current test target

This supersedes the earlier `C0 00 -> FA 00` menu-only POC test. The current vertical slice targets native 2026 field text from `jfleve.lgp`.

Selected field payload:

```text
B4 C0 21 B5 FF
```

Expected output:

```text
Ａ가Ｂ
```

Requirements:

- `B4` renders first as full-width `Ａ`.
- `C0 21` renders `가` from `resources/korean_font/korean_c0_page.tim`.
- `B5` renders immediately after `가`, proving cursor advance is exactly two bytes for the Korean glyph.
- `FF` terminates the FF7 text stream.
- The `가` advance is one full native cell, width `0x40`.
- Existing Japanese `FA-FE` rendering remains unchanged.

Patch command after launching FFVII:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log first_korean_glyph_patch.log
```

Do not enter the modified field scene until the log reports:

```text
Korean C0 page native handle: 0x...
completed successfully
```

Rollback:

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log first_korean_glyph_patch.log
```

Detailed byte boundaries and edit guidance are in `codex-lab/findings/first_korean_glyph_test_vector.md`.

---

# Historical C0 00 runtime test plan

Goal: prove that `C0 00` behaves exactly like `FA 00` in the confirmed common menu render and width paths.

## Test target

Use the confirmed menu text path:

- Render: `FUN_14106b4a0 -> FUN_141570730 -> FUN_1415724a0 -> FUN_141571ec0`
- Width: `FUN_14106b4a0 -> FUN_141570050 -> FUN_1415712b0 -> FUN_141571220`

Exact text resource for the first test:

- Runtime FF7 text buffer resolved from the menu helper's text handle `0x91AA28`.
- This handle is used in `FUN_14106b4a0` before calls to both `FUN_141570730` and `FUN_141570050`.

Do not use a source string that passes through `FUN_1410732b0`, because that helper treats `00` as a terminator while normal FF7 text streams treat `00` as printable.

## Patch install

1. Build `poc_runtime_patch\c0_poc_patcher.exe`.
2. Start FFVII and reach the menu path that exercises `FUN_14106b4a0`.
3. Run:

```bat
poc_runtime_patch\c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log poc_c000_patch.log
```

Expected install log:

- target PID
- module base
- validated runtime signatures for both mandatory sites
- two remote stub addresses
- two detour byte sequences
- success for both patch sites

## Test bytes

Use a short FF7 `0xFF`-terminated buffer. Keep one following visible single-byte character after the two-byte glyph to prove cursor behavior.

Baseline original sequence:

```text
41 41 FF
```

Control sequence:

```text
FA 00 41 FF
```

POC sequence:

```text
C0 00 41 FF
```

Where:

- `FA 00` is the existing Japanese two-byte control case.
- `C0 00` is the new POC case.
- `41` is the following character used to prove the cursor advanced to `p+2`.
- `FF` terminates the FF7 text stream.

## Expected results

Visual result:

- `C0 00` displays the same glyph as `FA 00`.
- The following `41` glyph renders immediately after the same advance as in the `FA 00 41 FF` control case.
- No extra blank/space glyph appears from the `00` byte.

Width result:

- `C0 00 41 FF` measures exactly the same as `FA 00 41 FF`.
- The raw glyph helper behavior for `FA00` is default width `0x40`; wrapper-scaled width must match the control sequence.

Cursor result:

- Starting cursor `p` at the lead byte:
  - after `C0`: pending prefix is `FA00`, cursor is `p+1`
  - after following `00`: encoded glyph is `FA00`, cursor is `p+2`
  - following `41` is processed as the next character

No acceptable outcomes:

- `C0` renders as a single-byte glyph.
- `00` renders as a separate space after `C0`.
- Cursor advances to `p+1` or `p+3`.
- `FA-FE` behavior changes.
- Non-`C0` single-byte strings change.

## Manual runtime buffer procedure

Because the first test must avoid `FUN_1410732b0`, modify an already-resolved FF7 text buffer at runtime, not a C-string source.

Recommended debugger/memory-editor flow:

1. Break on `FFVII.exe + 0x106BCEA`, the direct `FUN_14106b4a0 -> FUN_141570730` render wrapper call.
2. Confirm the call is using the text handle/pointer family associated with `0x91AA28`.
3. Resolve the actual buffer pointer through the same runtime state before the wrapper call, or inspect the resolved text pointer inside `FUN_141570730` after its `FUN_14003f0a0` call.
4. Save the original bytes from that buffer.
5. Replace the buffer with `FA 00 41 FF` and observe the control result.
6. Replace the same buffer with `C0 00 41 FF` and observe the POC result.
7. Restore the saved original buffer bytes.

## Crash and corruption checks

- The game must not crash when entering the menu.
- The patcher log must show both mandatory sites succeeded.
- Existing `FA 00 41 FF` must still work after patch install.
- A normal single-byte sequence such as `41 41 FF` must still render normally.
- Width and visual advance of the following `41` must match between `FA 00 41 FF` and `C0 00 41 FF`.

## Rollback

To disable the runtime patch:

```bat
poc_runtime_patch\c0_poc_patcher.exe restore --process FFVII.exe --log poc_c000_patch.log
```

Then restore the menu text buffer bytes saved before the test.

Full rollback is also achieved by closing `FFVII.exe`, because no on-disk executable bytes are modified.
