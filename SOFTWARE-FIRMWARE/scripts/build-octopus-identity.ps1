[CmdletBinding()]
param(
    [string]$PlatformIO = "C:\Users\mattj\Documents\PAROL 6\tmp\pio-env\Scripts\platformio.exe"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$firmwareRoot = Join-Path $projectRoot "firmware\octopus_h723"

if (-not (Test-Path -LiteralPath $PlatformIO -PathType Leaf)) {
    throw "PlatformIO executable not found: $PlatformIO"
}

$env:PYTHONUTF8 = "1"
$env:PLATFORMIO_CORE_DIR = Join-Path $env:USERPROFILE ".platformio"
& $PlatformIO run --project-dir $firmwareRoot -e octopus_h723_identity -t clean
if ($LASTEXITCODE -ne 0) { throw "Could not clean the Octopus identity build." }
& $PlatformIO run --project-dir $firmwareRoot -e octopus_h723_identity
if ($LASTEXITCODE -ne 0) { throw "Octopus identity firmware build failed." }

$artifact = Join-Path $firmwareRoot ".pio\build\octopus_h723_identity\firmware.bin"
$artifactHash = Get-FileHash -Algorithm SHA256 -LiteralPath $artifact
$expectedHash = "25A16D997590C00BC158DF40061970802BEBB5B8A1845C57A2F8734748FC94BD"
if ($artifactHash.Hash -ne $expectedHash) {
    throw "Identity firmware hash mismatch. Expected $expectedHash; got $($artifactHash.Hash)."
}
$packageRoot = Join-Path $projectRoot "dist\octopus-h723-identity-0.1.0"
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
$packageArtifact = Join-Path $packageRoot "firmware.bin"
Copy-Item -LiteralPath $artifact -Destination $packageArtifact -Force
$checksumLine = "$($artifactHash.Hash)  firmware.bin`n"
Set-Content -LiteralPath (Join-Path $packageRoot "SHA256SUMS.txt") -Value $checksumLine -NoNewline -Encoding ascii

Write-Output "Artifact: $artifact"
Write-Output "Flash package: $packageArtifact"
Write-Output "SHA-256: $($artifactHash.Hash)"
