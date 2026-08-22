[CmdletBinding()]
param(
    [string]$PlatformIO = "C:\Users\mattj\Documents\PAROL 6\tmp\pio-env\Scripts\platformio.exe"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$firmwareRoot = Join-Path $projectRoot "firmware\octopus_h723_j6_diag"
$environment = "octopus_h723_j6_diag"
$expectedHash = "7DF04721D4E6DCEC311E81A4DC17FE8905E001D4112A4EAEF27761196C5C892C"

$env:PYTHONUTF8 = "1"
$env:PLATFORMIO_CORE_DIR = Join-Path $env:USERPROFILE ".platformio"

function Invoke-CleanBuild {
    & $PlatformIO run --project-dir $firmwareRoot -e $environment -t clean | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Could not clean the J6 diagnostic build." }
    & $PlatformIO run --project-dir $firmwareRoot -e $environment | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "J6 diagnostic build failed." }
    return (Get-FileHash -Algorithm SHA256 -LiteralPath (
        Join-Path $firmwareRoot ".pio\build\$environment\firmware.bin"
    )).Hash
}

$firstHash = Invoke-CleanBuild
$secondHash = Invoke-CleanBuild
if ($firstHash -ne $secondHash) { throw "J6 diagnostic build is not reproducible." }
if ($secondHash -ne $expectedHash) {
    throw "J6 diagnostic hash mismatch. Expected $expectedHash; got $secondHash."
}

$buildRoot = Join-Path $firmwareRoot ".pio\build\$environment"
$packageRoot = Join-Path $projectRoot "dist\octopus-h723-j6-diag-0.4.0"
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
foreach ($name in @("firmware.bin", "firmware.elf", "firmware.map")) {
    Copy-Item -LiteralPath (Join-Path $buildRoot $name) -Destination (Join-Path $packageRoot $name) -Force
}
Set-Content -LiteralPath (Join-Path $packageRoot "SHA256SUMS.txt") -Value "$secondHash  firmware.bin`n" -NoNewline -Encoding ascii
Write-Output "J6 diagnostic prepared but not flashed."
Write-Output "Artifact: $(Join-Path $packageRoot 'firmware.bin')"
Write-Output "SHA-256: $secondHash"
