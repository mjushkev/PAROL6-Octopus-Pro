[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $repoRoot "firmware\octopus_h723_commander"
$python = Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
$pioPackages = Join-Path (Split-Path -Parent $repoRoot) "tmp\pio-env\Lib\site-packages"
$elf = Join-Path $project ".pio\build\octopus_h723_commander\firmware.elf"
$release = Join-Path $repoRoot "dist\commander-1.0.0-rc7"

if (-not (Test-Path -LiteralPath $python)) {
    throw "The pinned Python runtime is missing: $python"
}
if (-not (Test-Path -LiteralPath $pioPackages)) {
    throw "The pinned PlatformIO environment is missing: $pioPackages"
}

$env:PLATFORMIO_CORE_DIR = Join-Path $env:USERPROFILE ".platformio"
$env:PYTHONPATH = $pioPackages
& $python -m platformio run --project-dir $project --target checkprogsize --jobs 1
if ($LASTEXITCODE -ne 0) { throw "Commander firmware compile/verification failed" }

New-Item -ItemType Directory -Path $release -Force | Out-Null
& $python (Join-Path $project "make_firmware_bin.py") `
    --elf $elf `
    --output (Join-Path $release "firmware.bin") `
    --manifest (Join-Path $release "firmware-manifest.json")
if ($LASTEXITCODE -ne 0) { throw "Commander firmware packaging failed" }

Write-Host "Commander RC image created in $release"
Write-Host "Do not flash until USB logic-only and HIL validation are complete."
