[CmdletBinding()]
param([string]$Python = "python")

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$venv = Join-Path $projectRoot ".venv"
& $Python -m venv $venv
if ($LASTEXITCODE -ne 0) { throw "Could not create the development environment." }
Write-Host "Created $venv. The simulation core has no third-party runtime dependencies."

