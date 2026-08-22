[CmdletBinding()]
param(
    [string]$Port = "COM4",
    [int]$TimeoutMilliseconds = 5000
)

$ErrorActionPreference = "Stop"
$serialPort = [System.IO.Ports.SerialPort]::new(
    $Port,
    3000000,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)
$serialPort.DtrEnable = $true
$serialPort.RtsEnable = $false
$serialPort.Handshake = [System.IO.Ports.Handshake]::None
$serialPort.NewLine = "`n"
$serialPort.ReadTimeout = $TimeoutMilliseconds
$serialPort.WriteTimeout = 1000

function Invoke-SafeQuery {
    param([string]$Command)
    $serialPort.Write("$Command`n")
    return $serialPort.ReadLine().Trim()
}

try {
    $serialPort.Open()
    Start-Sleep -Milliseconds 250
    $serialPort.DiscardInBuffer()
    $identify = Invoke-SafeQuery "IDENTIFY"
    $status = Invoke-SafeQuery "STATUS"
    $diagnostics = Invoke-SafeQuery "DIAGNOSTICS"
    $heartbeat = Invoke-SafeQuery "HEARTBEAT"
    $rejected = Invoke-SafeQuery "MOVE"
} finally {
    if ($serialPort.IsOpen) { $serialPort.Close() }
    $serialPort.Dispose()
}

$checks = @(
    @($identify, "firmware=safe_core"),
    @($identify, "version=0.2.0-safe-core"),
    @($status, "state=not_commissioned"),
    @($diagnostics, "config_valid=1"),
    @($diagnostics, "hardware_watchdog_ready=1"),
    @($heartbeat, "PAROL6_HEARTBEAT_REPLY"),
    @($rejected, "code=command_rejected")
)
foreach ($check in $checks) {
    if ($check[0].IndexOf($check[1], [StringComparison]::Ordinal) -lt 0) {
        throw "Safe-core verification failed; missing '$($check[1])': $($check[0])"
    }
}
foreach ($response in @($identify, $status, $diagnostics, $heartbeat, $rejected)) {
    foreach ($token in @("outputs=disabled", "motion=disabled")) {
        if ($response.IndexOf($token, [StringComparison]::Ordinal) -lt 0) {
            throw "Unsafe or incomplete response; missing '$token': $response"
        }
    }
}

Write-Output $identify
Write-Output $status
Write-Output $diagnostics
Write-Output $heartbeat
Write-Output $rejected
Write-Output "PASS: safe-core diagnostics verified; physical outputs remain prohibited."
