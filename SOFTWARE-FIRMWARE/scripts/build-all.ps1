[CmdletBinding()]
param(
    [switch]$Offline,
    [string]$Python = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
Write-Host "Simulation milestone: validating generated protocol artifacts."
& (Join-Path $PSScriptRoot "test-all.ps1") -Python $Python
if ($LASTEXITCODE -ne 0) { throw "Build validation failed." }
Write-Host "No hardware firmware or release binary is produced by this milestone."
