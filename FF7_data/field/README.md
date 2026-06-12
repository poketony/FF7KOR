# jfleve field text workspace

Edit the individual TXT files in:

```text
C:\Users\JO\FF7KOR\FF7_data\field\jfleve
```

The files were copied from:

```text
C:\Users\JO\FF7KOR\codex-lab\work\jfleve-text-export\makou-txt-jp-fixed
```

To rebuild `jfleve.lgp` from the edited TXT files:

```powershell
C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe -ExecutionPolicy Bypass -File .\reimport_jfleve_from_txt.ps1
```

Default output:

```text
C:\Users\JO\FF7KOR\FF7_data\field\_build\jfleve.lgp
```

The script uses Makou Reactor CLI with:

```text
import --input-format lgp --text txt --autosize-text-windows --include <exact txt field name>
```

It reads the original game `jfleve.lgp` as input and writes a new LGP under `_build`. It does not overwrite the game file.

The script imports only TXT files that exist in this folder, using exact field-name matches. This avoids Makou Reactor's all-field import failure when the archive contains fields that should not be imported from this workspace.

For a quick single-field test:

```powershell
.\reimport_jfleve_from_txt.ps1 -Include startmap -OutputLgp .\_build\jfleve_startmap_test.lgp
```
