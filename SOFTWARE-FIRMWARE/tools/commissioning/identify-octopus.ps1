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

try {
    $serialPort.Open()
    Start-Sleep -Milliseconds 250
    $serialPort.DiscardInBuffer()
    $serialPort.Write("IDENTIFY`n")
    $response = $serialPort.ReadLine().Trim()
} finally {
    if ($serialPort.IsOpen) { $serialPort.Close() }
    $serialPort.Dispose()
}

$required = @(
    "PAROL6_IDENTIFY",
    "firmware=safe_identity",
    "board=BTT_OCTOPUS_PRO_V1_1_H723ZE",
    "outputs=disabled",
    "motion=disabled"
)
foreach ($token in $required) {
    if ($response.IndexOf($token, [StringComparison]::Ordinal) -lt 0) {
        throw "Unexpected firmware response; missing '$token': $response"
    }
}

Write-Output $response
Write-Output "PASS: known PAROL6 identity-only firmware; physical outputs remain prohibited."
