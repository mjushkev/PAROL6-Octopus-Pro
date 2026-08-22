[CmdletBinding()]
param(
    [string]$Port = "COM4",
    [ValidateSet("+", "-")][string]$Direction = "+",
    [switch]$Execute,
    [string]$ConfirmText = ""
)

$ErrorActionPreference = "Stop"
if ($Execute -and $ConfirmText -cne "J6 CLEAR ESTOP READY") {
    throw "Execution requires -ConfirmText 'J6 CLEAR ESTOP READY'."
}

$serial = [System.IO.Ports.SerialPort]::new($Port, 3000000)
$serial.DtrEnable = $true
$serial.RtsEnable = $false
$serial.NewLine = "`n"
$serial.ReadTimeout = 3000
$serial.WriteTimeout = 1000

function Invoke-Line([string]$Command) {
    $serial.Write("$Command`n")
    return $serial.ReadLine().Trim()
}

try {
    $serial.Open()
    Start-Sleep -Milliseconds 250
    $serial.DiscardInBuffer()
    $identity = Invoke-Line "IDENTIFY"
    if ($identity -notmatch 'version=0\.4\.2-j6-diag' -or
        $identity -notmatch 'driver_disabled=1' -or
        $identity -notmatch 'token=([0-9A-F]{8})') {
        throw "Unexpected J6 diagnostic identity: $identity"
    }
    $token = $Matches[1]
    $check = Invoke-Line "CHECK"
    if ($check -notmatch 'ready=1' -or $check -notmatch 'version=0x21' -or
        $check -notmatch 'current_ma=250' -or
        $check -notmatch 'driver_disabled=1') {
        throw "J6 TMC2209 preflight failed: $check"
    }
    Write-Output $identity
    Write-Output $check
    if (-not $Execute) {
        Write-Output "PASS: J6 driver preflight only; no STEP pulses sent."
        return
    }
    $result = Invoke-Line "J6 $token $Direction"
    if ($result -notmatch 'PAROL6_J6_JOG_COMPLETE' -or
        $result -notmatch 'pulses=1600' -or
        $result -notmatch 'driver_disabled=1') {
        throw "J6 bounded jog failed: $result"
    }
    Write-Output $result
    Write-Output "PASS: one bounded J6 jog completed and driver returned disabled."
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
