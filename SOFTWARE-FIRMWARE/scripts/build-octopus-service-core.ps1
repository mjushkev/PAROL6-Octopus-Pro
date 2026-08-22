[CmdletBinding()]
param(
    [string]$PlatformIO = "C:\Users\mattj\Documents\PAROL 6\tmp\pio-env\Scripts\platformio.exe"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$firmwareRoot = Join-Path $projectRoot "firmware\octopus_h723"
$environment = "octopus_h723_service_core"
$expectedHash = "3C176B9D75EE10711DB818E37B65F547089A30EC7E5BB4EE4096DB0ADB1B4FB1"

if (-not (Test-Path -LiteralPath $PlatformIO -PathType Leaf)) {
    throw "PlatformIO executable not found: $PlatformIO"
}

$env:PYTHONUTF8 = "1"
$env:PLATFORMIO_CORE_DIR = Join-Path $env:USERPROFILE ".platformio"

function Invoke-CleanBuild {
    & $PlatformIO run --project-dir $firmwareRoot -e $environment -t clean | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Could not clean the Octopus service-core build." }
    & $PlatformIO run --project-dir $firmwareRoot -e $environment | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Octopus service-core firmware build failed." }
    $artifact = Join-Path $firmwareRoot ".pio\build\$environment\firmware.bin"
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact).Hash
}

$firstHash = Invoke-CleanBuild
$secondHash = Invoke-CleanBuild
if ($firstHash -ne $secondHash) {
    throw "Service-core build is not reproducible: $firstHash != $secondHash"
}
if ($secondHash -ne $expectedHash) {
    throw "Service-core firmware hash mismatch. Expected $expectedHash; got $secondHash."
}

$buildRoot = Join-Path $firmwareRoot ".pio\build\$environment"
$packageRoot = Join-Path $projectRoot "dist\octopus-h723-service-core-0.3.0"
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
foreach ($name in @("firmware.bin", "firmware.elf", "firmware.map")) {
    Copy-Item -LiteralPath (Join-Path $buildRoot $name) -Destination (Join-Path $packageRoot $name) -Force
}
Set-Content -LiteralPath (Join-Path $packageRoot "SHA256SUMS.txt") -Value "$secondHash  firmware.bin`n" -NoNewline -Encoding ascii

Write-Output "Service-core image prepared but not flashed."
Write-Output "Artifact: $(Join-Path $packageRoot 'firmware.bin')"
Write-Output "SHA-256: $secondHash"
