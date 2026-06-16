# C0 00 runtime patch POC

This folder contains a minimal external runtime patcher for the FFVII 2026 native 64-bit Japanese font decoder path.

It does not modify `FFVII.exe` on disk. It waits for the running process to expose the runtime-decrypted bytes, validates both mandatory signatures, installs two detours, and logs every address and byte sequence it writes.

## Build

Use a normal command prompt:

```bat
build_msvc_x64.bat
```

The script uses `cl.exe` if it is already on `PATH`. Otherwise it tries to find Visual Studio Build Tools through `vswhere.exe` and loads `vcvars64.bat`.

## GitHub Actions Build

The repository includes a manual workflow at `.github/workflows/build-c0-poc-patcher.yml`.

To run it from GitHub:

1. Open the repository's Actions tab.
2. Select `Build C0 POC patcher`.
3. Click `Run workflow`.

When the run finishes, download the `c0-poc-patcher-windows-x64` artifact from the workflow run page. The artifact contains the built patcher, this README, build metadata, and `poc_patch_sites.csv` when that file is present in the repository.

Windows may warn that `c0_poc_patcher.exe` is unsigned. This POC patcher must only be run against your own live `FFVII.exe` process. It validates and patches runtime memory only; it does not modify the original on-disk executable.

## Install Patch

Start FFVII first and reach a point where the runtime code has been decrypted. Then run:

```bat
c0_poc_patcher.exe install --process FFVII.exe --wait-ms 120000 --log poc_c000_patch.log
```

The patcher:

- finds the `FFVII.exe` process
- locates the module base
- resolves mandatory patch sites by RVA
- validates the expected runtime-decrypted bytes
- suspends target threads while writing the detours
- writes remote stubs and absolute indirect jumps
- restores page protection
- flushes the instruction cache

## Restore

To disable the POC in the current process:

```bat
c0_poc_patcher.exe restore --process FFVII.exe --log poc_c000_patch.log
```

Restore writes back the original overwrite bytes for both mandatory sites. Remote stub allocations are not freed by a later restore run, but they become unreachable once the original bytes are restored and disappear when `FFVII.exe` exits.

## Patch Sites

Mandatory sites only:

- `FUN_1415724a0`, RVA `0x1572577`: common render scanner
- `FUN_1415712b0`, RVA `0x1571336`: common width scanner

The glyph renderer is intentionally not patched.

## Expected Behavior

After install, `C0 00` should behave like `FA 00` in the common menu/battle render and width paths:

- same font page: `FA`
- same object slot: `*0x14207CE08 + 0x24`
- same glyph index: `00`
- same width behavior
- exactly two source bytes consumed

## Safety Notes

- Abort if either mandatory signature does not match.
- Do not use this on a different FFVII build.
- Do not use strings that pass through `FUN_1410732b0` for the first `C0 00` test, because that helper treats `00` as a terminator.
