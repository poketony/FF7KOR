[CmdletBinding()]
param(
    [string]$InputLgp = "",
    [string]$SourceDir = "",
    [string]$OutputLgp = "",
    [string]$MakouCli = "",
    [string[]]$Include = @()
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($InputLgp)) {
    $InputLgp = "C:\Program Files (x86)\Steam\steamapps\common\FINAL FANTASY VII Steam Edition\ff7\workingdir\data\field\jfleve.lgp"
}

if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $PSScriptRoot "jfleve"
}

if ([string]::IsNullOrWhiteSpace($OutputLgp)) {
    $OutputLgp = Join-Path $PSScriptRoot "_build\jfleve.lgp"
}

if ([string]::IsNullOrWhiteSpace($MakouCli)) {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
    $MakouCli = Join-Path $repoRoot "tools\makoureactor-2.2.0-win64_cli\makoureactor.exe"
}

if (!(Test-Path -LiteralPath $MakouCli)) {
    throw "Makou Reactor CLI not found: $MakouCli"
}

if (!(Test-Path -LiteralPath $InputLgp)) {
    throw "Input LGP not found: $InputLgp"
}

if (!(Test-Path -LiteralPath $SourceDir)) {
    throw "Source TXT directory not found: $SourceDir"
}

$txtFiles = Get-ChildItem -LiteralPath $SourceDir -Filter "*.txt" -File | Sort-Object Name
if ($Include.Count -gt 0) {
    $includeNames = $Include | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_) }
} else {
    $includeNames = $txtFiles | ForEach-Object { $_.BaseName }
}

if ($includeNames.Count -eq 0) {
    throw "No TXT files found to import: $SourceDir"
}

$outputDir = Split-Path -Parent $OutputLgp
if (![string]::IsNullOrWhiteSpace($outputDir) -and !(Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$iniPath = Join-Path (Split-Path -Parent $MakouCli) "Makou_Reactor.ini"
if (!(Test-Path -LiteralPath $iniPath)) {
    Set-Content -LiteralPath $iniPath -Encoding UTF8 -Value @("[General]", "jp_txt=true")
}

$includeArgs = foreach ($name in $includeNames) {
    "--include"
    "^" + [regex]::Escape($name) + "$"
}

$arguments = @(
    "import",
    "--input-format", "lgp",
    "--text", "txt",
    "--autosize-text-windows"
) + $includeArgs + @(
    $InputLgp,
    $SourceDir,
    $OutputLgp
)

Write-Host "Importing $($includeNames.Count) TXT files from: $SourceDir"
& $MakouCli @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Makou Reactor import failed with exit code $LASTEXITCODE"
}

if (!(Test-Path -LiteralPath $OutputLgp)) {
    throw "Makou Reactor finished, but output was not created: $OutputLgp"
}

Write-Host "Wrote: $OutputLgp"
