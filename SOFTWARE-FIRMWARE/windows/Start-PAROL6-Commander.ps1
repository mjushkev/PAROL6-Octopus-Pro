[CmdletBinding()]
param(
    [ValidateSet("Simulator", "Hardware")]
    [string]$Mode = "Simulator",
    [string]$ComPort = "COM4",
    [int]$WebPort = 8080,
    [int]$ControllerPort = 5001,
    [switch]$EnableWorkspaceEnvelope,
    [switch]$NoBrowser
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$apiRoot = Join-Path $PSScriptRoot "parol6_api_octopus"
$commanderRoot = Join-Path $PSScriptRoot "waldo_commander"
$runtimeRoot = Join-Path $env:USERPROFILE ".cache\parol6-waldo"
$python = Join-Path $runtimeRoot ".venv\Scripts\python.exe"
$profile = Join-Path $projectRoot "config\robot.mattj.calibrated.json"

if (-not (Test-Path -LiteralPath $python)) {
    throw "Commander is not installed. Run Install-PAROL6-Commander.ps1 first."
}
if (-not (Test-Path -LiteralPath $profile)) {
    throw "The owner calibration profile is missing: $profile"
}
New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null
$legacyStorage = Join-Path $commanderRoot ".nicegui\storage-general.json"
$runtimeStorage = Join-Path $runtimeRoot ".nicegui\storage-general.json"
if ((Test-Path -LiteralPath $legacyStorage) -and -not (Test-Path -LiteralPath $runtimeStorage)) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $runtimeStorage) | Out-Null
    Copy-Item -LiteralPath $legacyStorage -Destination $runtimeStorage
}
$env:PAROL6_HARDWARE_PROFILE = $profile
$env:PAROL6_RUNTIME_CACHE = $runtimeRoot
$env:PAROL6_COLLISION_CHECK = "0"
$env:PAROL6_NOAUTOHOME = "1"
$env:PAROL6_COMMANDER_MODE = $Mode.ToUpperInvariant()
$env:WALDO_SKIP_ENVELOPE = if ($EnableWorkspaceEnvelope) { "" } else { "1" }
$env:WALDO_SKIP_PROCESS_POOL_WARMUP = "1"
$env:WALDO_EXCLUSIVE_START = "0"

if ($Mode -eq "Hardware") {
    if ($ComPort -notmatch '^COM\d+$') {
        throw "Use a Windows COM port such as COM4. Received: $ComPort"
    }
    $env:PAROL6_FAKE_SERIAL = "0"
    $env:PAROL6_TRANSPORT = "octopus-buffered"
    $env:PAROL6_COM_PORT = $ComPort
    $env:PAROL6_ALLOW_NO_COLLISION = "1"
} else {
    $env:PAROL6_FAKE_SERIAL = "1"
    $env:PAROL6_TRANSPORT = "mock"
    $env:PAROL6_COM_PORT = ""
    Remove-Item Env:PAROL6_ALLOW_NO_COLLISION -ErrorAction SilentlyContinue
}

$existingWeb = Get-NetTCPConnection -LocalPort $WebPort -State Listen -ErrorAction SilentlyContinue
if (-not $existingWeb) {
    $controllerArgs = @(
        "-u", "-m", "parol6.server.cli",
        "--host=127.0.0.1", "--port=$ControllerPort", "--log-level=INFO"
    )
    if ($Mode -eq "Hardware") {
        $controllerArgs += @("--serial=$ComPort", "--baudrate=2000000")
    }
    $controllerProcess = Start-Process -FilePath $python -ArgumentList $controllerArgs `
        -WorkingDirectory $apiRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $runtimeRoot "controller.out.log") `
        -RedirectStandardError (Join-Path $runtimeRoot "controller.err.log") -PassThru

    $deadline = (Get-Date).AddSeconds(45)
    do {
        Start-Sleep -Milliseconds 250
        $udp = Get-NetUDPEndpoint -LocalPort $ControllerPort -ErrorAction SilentlyContinue
    } until ($udp -or (Get-Date) -ge $deadline)
    if (-not $udp) {
        throw "The simulated controller did not start. See $runtimeRoot\controller.err.log"
    }

    $commanderArgs = @(
        "-u", "-m", "waldo_commander.main",
        "--host=127.0.0.1", "--port=$WebPort",
        "--controller-host=127.0.0.1", "--controller-port=$ControllerPort",
        "--robot=parol6", "--log-level=INFO"
    )
    $commanderProcess = Start-Process -FilePath $python -ArgumentList $commanderArgs `
        -WorkingDirectory $runtimeRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $runtimeRoot "commander.out.log") `
        -RedirectStandardError (Join-Path $runtimeRoot "commander.err.log") -PassThru

    $deadline = (Get-Date).AddSeconds(45)
    do {
        Start-Sleep -Milliseconds 250
        $existingWeb = Get-NetTCPConnection -LocalPort $WebPort -State Listen -ErrorAction SilentlyContinue
    } until ($existingWeb -or (Get-Date) -ge $deadline)
    if (-not $existingWeb) {
        throw "Commander did not start. See $runtimeRoot\commander.err.log"
    }

    @{
        mode = $Mode
        controller_pid = $controllerProcess.Id
        commander_pid = $commanderProcess.Id
        com_port = if ($Mode -eq "Hardware") { $ComPort } else { $null }
        started_utc = [DateTime]::UtcNow.ToString("o")
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runtimeRoot "session.json") -Encoding utf8
}

$url = "http://127.0.0.1:$WebPort/"
if (-not $NoBrowser) {
    Start-Process $url | Out-Null
}
Write-Host "PAROL6 Commander is running at $url"
Write-Host "Robot profile: PAROL6-MATTJ-001"
if ($Mode -eq "Hardware") {
    Write-Host "Hardware mode: $ComPort, checksum-gated P6B1 transport, 80% motion stage with protected J1/J2 Servo42C caps."
    Write-Warning "Physical collision checking is not yet available on this Windows runtime. Use no tool, clear the work area, and keep the main-power E-stop within reach during acceptance testing."
} else {
    Write-Host "Mode: safe simulator (no physical motion)."
}
