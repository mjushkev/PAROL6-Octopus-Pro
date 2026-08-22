[CmdletBinding()]
param([string]$Port = "COM4")

$ErrorActionPreference = "Stop"
$serial = [System.IO.Ports.SerialPort]::new($Port, 3000000)
$serial.DtrEnable = $true
$serial.RtsEnable = $false
$serial.NewLine = "`n"
$serial.ReadTimeout = 5000
$serial.WriteTimeout = 1000

function Invoke-Line([string]$Command) {
    $serial.Write("$Command`n")
    return $serial.ReadLine().Trim()
}

try {
    $serial.Open()
    Start-Sleep -Milliseconds 500
    $serial.DiscardInBuffer()
    $identity = Invoke-Line "IDENTIFY"
    if ($identity -notmatch 'PAROL6_MOTION_RC_READY' -or
        $identity -notmatch 'version=0\.8\.2-calibration-rc' -or
        $identity -notmatch 'joints=6' -or
        $identity -notmatch 'stops=8' -or
        $identity -notmatch 'home_sequence=J2,J3,J4,J6,J5' -or
        $identity -notmatch 'hold_speed_mdeg_s=3000-45000' -or
        $identity -notmatch 'motor_hold=host_supervised' -or
        $identity -notmatch 'j1_home=sensor_or_manual_temporary' -or
        $identity -notmatch 'calibration=dual_slot_crc32c' -or
        $identity -notmatch 'soft_limits=firmware_enforced' -or
        $identity -notmatch 'direction_discovery=raw_2deg' -or
        $identity -notmatch 'manual_zero=j1_runtime_only' -or
        $identity -notmatch 'driver_disabled=1') {
        throw "Unexpected commissioning identity: $identity"
    }
    $sensors = Invoke-Line "SENSORS"
    foreach ($required in @('J1=', 'J2=', 'J3=', 'J4=', 'J5=', 'J6=',
            'STOP6=', 'STOP7=', 'T0=', 'T1=', 'T2=', 'T3=', 'PWR=',
            'moving=0')) {
        if ($sensors -notlike "*$required*") {
            throw "Sensor response is missing $required : $sensors"
        }
    }
    Write-Output $identity
    Write-Output $sensors
    $status = Invoke-Line "STATUS"
    if ($status -notmatch 'moving=0' -or $status -notmatch 'driver_disabled=1') {
        throw "Controller is not idle and disabled: $status"
    }
    Write-Output $status
    $calibrationHeader = Invoke-Line "CALIBRATION"
    if ($calibrationHeader -notmatch '^PAROL6_CALIBRATION_HEADER ' -or
        $calibrationHeader -notmatch 'schema=1' -or
        $calibrationHeader -notmatch 'storage=') {
        throw "Unexpected calibration header: $calibrationHeader"
    }
    Write-Output $calibrationHeader
    Write-Output "PASS: calibration RC identity, sensors, retained calibration header, and idle-disabled status only; no motion command sent."
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
