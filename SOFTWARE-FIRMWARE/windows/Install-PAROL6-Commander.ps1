[CmdletBinding()]
param(
    [string]$Python = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$apiRoot = Join-Path $PSScriptRoot "parol6_api_octopus"
$commanderRoot = Join-Path $PSScriptRoot "waldo_commander"
$runtimeRoot = Join-Path $env:USERPROFILE ".cache\parol6-waldo"
$venvPython = Join-Path $runtimeRoot ".venv\Scripts\python.exe"

function Resolve-Python {
    if ($Python) {
        return (Resolve-Path -LiteralPath $Python).Path
    }
    if ($env:PAROL6_PYTHON) {
        return (Resolve-Path -LiteralPath $env:PAROL6_PYTHON).Path
    }
    $bundled = Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
    if (Test-Path -LiteralPath $bundled) {
        return $bundled
    }
    $command = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($command -and $command.Source -notlike "*WindowsApps*") {
        return $command.Source
    }
    throw "Python 3.12 was not found. Set PAROL6_PYTHON to a Python 3.12 executable."
}

if (-not (Test-Path -LiteralPath $venvPython)) {
    New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null
    $basePython = Resolve-Python
    & $basePython -m venv (Split-Path -Parent (Split-Path -Parent $venvPython))
    if ($LASTEXITCODE -ne 0) { throw "Could not create the Commander runtime." }
}

& $venvPython -m pip install --disable-pip-version-check --upgrade pip
if ($LASTEXITCODE -ne 0) { throw "Could not update the package installer." }
& $venvPython -m pip install --disable-pip-version-check --upgrade (Join-Path $apiRoot "vendor\toppra-pure")
if ($LASTEXITCODE -ne 0) { throw "Could not install the portable motion-planning dependency." }
& $venvPython -m pip install --disable-pip-version-check --upgrade --force-reinstall --no-deps (Join-Path $apiRoot "vendor\pinokin-pure")
if ($LASTEXITCODE -ne 0) { throw "Could not install the portable kinematics dependency." }
& $venvPython -m pip install --disable-pip-version-check --editable $apiRoot
if ($LASTEXITCODE -ne 0) { throw "Could not install the Octopus PAROL6 backend." }
& $venvPython -m pip install --disable-pip-version-check --editable $commanderRoot
if ($LASTEXITCODE -ne 0) { throw "Could not install Waldo Commander." }

$profile = Join-Path $projectRoot "config\robot.mattj.calibrated.json"
$env:PAROL6_COLLISION_CHECK = "0"
$env:PAROL6_HARDWARE_PROFILE = $profile
& $venvPython -c "from parol6.hardware_profile import PROFILE; from parol6.robot import Robot; r=Robot(); assert r.name == PROFILE.robot_id; print(f'Installed {r.name}')"
if ($LASTEXITCODE -ne 0) { throw "Commander self-check failed." }

Write-Host "PAROL6 Commander is installed."
Write-Host "Use Start-PAROL6-Commander.ps1 to open the safe simulator."
