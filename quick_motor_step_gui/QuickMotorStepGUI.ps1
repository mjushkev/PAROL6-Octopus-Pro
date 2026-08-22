param([switch]$SelfTest)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$script:port = $null
$script:buffer = ''
$script:driverReady = $false
$script:pendingMove = $null
$script:moving = $false

function Convert-StepsToDegrees {
    param([decimal]$Count, [string]$Mode)
    if ($Mode -eq 'Full motor steps (1.8 deg each)') {
        return [double]$Count * 1.8
    }
    return [double]$Count * (360.0 / 3200.0)
}

if ($SelfTest) {
    $micro = Convert-StepsToDegrees 16 'Microsteps (1/16)'
    $full = Convert-StepsToDegrees 10 'Full motor steps (1.8 deg each)'
    if ([Math]::Abs($micro - 1.8) -gt 0.000001) { throw 'Microstep conversion failed.' }
    if ([Math]::Abs($full - 18.0) -gt 0.000001) { throw 'Full-step conversion failed.' }
    Write-Output 'GUI_SELFTEST_OK'
    exit 0
}

function Add-Log {
    param([string]$Text)
    if ([string]::IsNullOrWhiteSpace($Text)) { return }
    $stamp = [DateTime]::Now.ToString('HH:mm:ss')
    $log.AppendText("[$stamp] $Text`r`n")
    $log.SelectionStart = $log.TextLength
    $log.ScrollToCaret()
}

function Send-Command {
    param([string]$Command)
    if ($null -eq $script:port -or -not $script:port.IsOpen) {
        Add-Log 'Not connected.'
        return $false
    }
    try {
        $script:port.WriteLine($Command)
        Add-Log "> $Command"
        return $true
    } catch {
        Add-Log ("Serial write failed: " + $_.Exception.Message)
        return $false
    }
}

function Update-Controls {
    $connected = $null -ne $script:port -and $script:port.IsOpen
    $connect.Enabled = -not $connected
    $disconnect.Enabled = $connected
    $check.Enabled = $connected -and -not $script:moving
    $stop.Enabled = $connected
    $movePositive.Enabled = $connected -and $script:driverReady -and -not $script:moving
    $moveNegative.Enabled = $movePositive.Enabled
    $portList.Enabled = -not $connected
    $refresh.Enabled = -not $connected
}

function Set-State {
    param([string]$Text, [System.Drawing.Color]$Color)
    $state.Text = $Text
    $state.BackColor = $Color
}

function Process-Line {
    param([string]$Line)
    $Line = $Line.Trim()
    if (-not $Line) { return }
    Add-Log $Line

    if ($Line -match 'CHECK OK DRIVER=ONLINE') {
        $script:driverReady = $true
        Set-State 'DRIVER READY' ([System.Drawing.Color]::FromArgb(55, 155, 90))
    } elseif ($Line -match 'CHECK FAILED|CONFIG FAILED|DRIVER=OFFLINE') {
        $script:driverReady = $false
        $script:pendingMove = $null
        Set-State 'DRIVER NOT READY' ([System.Drawing.Color]::FromArgb(190, 65, 65))
    } elseif ($Line -match 'ARMED FOR 10 SECONDS') {
        if ($null -ne $script:pendingMove) {
            $command = $script:pendingMove
            $script:pendingMove = $null
            [void](Send-Command $command)
        }
    } elseif ($Line -match '^MOVE START') {
        $script:moving = $true
        Set-State 'MOVING' ([System.Drawing.Color]::FromArgb(220, 145, 45))
    } elseif ($Line -match 'SAFE DISABLED REASON=MOVE_COMPLETE') {
        $script:moving = $false
        Set-State 'DRIVER READY - DISABLED' ([System.Drawing.Color]::FromArgb(55, 155, 90))
    } elseif ($Line -match 'SAFE DISABLED REASON=') {
        $script:moving = $false
        $script:pendingMove = $null
        Set-State 'STOPPED / DISABLED' ([System.Drawing.Color]::FromArgb(85, 105, 130))
    } elseif ($Line -match 'REFUSED|UNKNOWN COMMAND') {
        $script:moving = $false
        $script:pendingMove = $null
        Set-State 'COMMAND REFUSED' ([System.Drawing.Color]::FromArgb(190, 65, 65))
    }
    Update-Controls
}

function Read-Serial {
    if ($null -eq $script:port -or -not $script:port.IsOpen) { return }
    try {
        $incoming = $script:port.ReadExisting()
        if (-not $incoming) { return }
        $script:buffer += $incoming
        while ($script:buffer -match "^(.*?)(\r?\n)([\s\S]*)$") {
            $line = $Matches[1]
            $script:buffer = $Matches[3]
            Process-Line $line
        }
    } catch {
        Add-Log ("Serial read failed: " + $_.Exception.Message)
    }
}

function Refresh-Ports {
    $selected = $portList.SelectedItem
    $portList.Items.Clear()
    foreach ($name in [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object) {
        [void]$portList.Items.Add($name)
    }
    if ($selected -and $portList.Items.Contains($selected)) {
        $portList.SelectedItem = $selected
    } elseif ($portList.Items.Contains('COM4')) {
        $portList.SelectedItem = 'COM4'
    } elseif ($portList.Items.Count -gt 0) {
        $portList.SelectedIndex = 0
    }
}

function Queue-Move {
    param([int]$Direction)
    if (-not $script:driverReady -or $script:moving -or $null -ne $script:pendingMove) { return }
    $degrees = Convert-StepsToDegrees $stepCount.Value ([string]$stepMode.SelectedItem)
    $degrees *= $Direction
    if ([Math]::Abs($degrees) -gt 720.0) {
        [System.Windows.Forms.MessageBox]::Show(
            'That request exceeds the firmware limit of 720 degrees.',
            'Move refused', 'OK', 'Warning') | Out-Null
        return
    }
    $profile = [string]$profileList.SelectedItem
    $script:pendingMove = 'MOVE {0:F6} {1:F2} {2:F2} {3}' -f $degrees, $speed.Value, $accel.Value, $profile
    Set-State 'ARMING...' ([System.Drawing.Color]::FromArgb(70, 120, 175))
    if (-not (Send-Command 'ARM')) { $script:pendingMove = $null }
    Update-Controls
}

$form = [System.Windows.Forms.Form]::new()
$form.Text = 'PAROL6 Quick Motor Step Control'
$form.Size = [System.Drawing.Size]::new(650, 570)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedSingle'
$form.MaximizeBox = $false
$form.Font = [System.Drawing.Font]::new('Segoe UI', 9)

$title = [System.Windows.Forms.Label]::new()
$title.Text = 'MOTOR0 - TMC2209 step control'
$title.Font = [System.Drawing.Font]::new('Segoe UI', 15, [System.Drawing.FontStyle]::Bold)
$title.Location = [System.Drawing.Point]::new(18, 14)
$title.Size = [System.Drawing.Size]::new(400, 32)
$form.Controls.Add($title)

$state = [System.Windows.Forms.Label]::new()
$state.Text = 'DISCONNECTED'
$state.ForeColor = [System.Drawing.Color]::White
$state.TextAlign = 'MiddleCenter'
$state.Location = [System.Drawing.Point]::new(430, 14)
$state.Size = [System.Drawing.Size]::new(185, 32)
$state.BackColor = [System.Drawing.Color]::FromArgb(85, 105, 130)
$form.Controls.Add($state)

$portLabel = [System.Windows.Forms.Label]::new(); $portLabel.Text = 'Port'; $portLabel.Location = '20,62'; $portLabel.Size = '35,24'; $form.Controls.Add($portLabel)
$portList = [System.Windows.Forms.ComboBox]::new(); $portList.DropDownStyle = 'DropDownList'; $portList.Location = '58,59'; $portList.Size = '90,26'; $form.Controls.Add($portList)
$refresh = [System.Windows.Forms.Button]::new(); $refresh.Text = 'Refresh'; $refresh.Location = '155,58'; $refresh.Size = '75,28'; $form.Controls.Add($refresh)
$connect = [System.Windows.Forms.Button]::new(); $connect.Text = 'Connect'; $connect.Location = '238,58'; $connect.Size = '82,28'; $form.Controls.Add($connect)
$disconnect = [System.Windows.Forms.Button]::new(); $disconnect.Text = 'Disconnect'; $disconnect.Location = '327,58'; $disconnect.Size = '88,28'; $form.Controls.Add($disconnect)
$check = [System.Windows.Forms.Button]::new(); $check.Text = 'Check driver'; $check.Location = '423,58'; $check.Size = '95,28'; $form.Controls.Add($check)

$group = [System.Windows.Forms.GroupBox]::new()
$group.Text = 'Move by step count'
$group.Location = '18,102'
$group.Size = '597,190'
$form.Controls.Add($group)

$l1 = [System.Windows.Forms.Label]::new(); $l1.Text = 'Step count'; $l1.Location = '16,30'; $l1.Size = '80,22'; $group.Controls.Add($l1)
$stepCount = [System.Windows.Forms.NumericUpDown]::new(); $stepCount.Location = '100,28'; $stepCount.Size = '110,26'; $stepCount.Minimum = 1; $stepCount.Maximum = 10000; $stepCount.Value = 10; $group.Controls.Add($stepCount)
$stepMode = [System.Windows.Forms.ComboBox]::new(); $stepMode.DropDownStyle = 'DropDownList'; $stepMode.Location = '220,28'; $stepMode.Size = '220,26'; [void]$stepMode.Items.Add('Full motor steps (1.8 deg each)'); [void]$stepMode.Items.Add('Driver microsteps (1/16 step)'); $stepMode.SelectedIndex = 0; $group.Controls.Add($stepMode)
$equivalent = [System.Windows.Forms.Label]::new(); $equivalent.Location = '445,30'; $equivalent.Size = '135,22'; $group.Controls.Add($equivalent)

$l2 = [System.Windows.Forms.Label]::new(); $l2.Text = 'Speed RPM'; $l2.Location = '16,72'; $l2.Size = '80,22'; $group.Controls.Add($l2)
$speed = [System.Windows.Forms.NumericUpDown]::new(); $speed.Location = '100,70'; $speed.Size = '90,26'; $speed.Minimum = 1; $speed.Maximum = 30; $speed.DecimalPlaces = 1; $speed.Value = 5; $group.Controls.Add($speed)
$l3 = [System.Windows.Forms.Label]::new(); $l3.Text = 'Acceleration'; $l3.Location = '210,72'; $l3.Size = '82,22'; $group.Controls.Add($l3)
$accel = [System.Windows.Forms.NumericUpDown]::new(); $accel.Location = '296,70'; $accel.Size = '90,26'; $accel.Minimum = 1; $accel.Maximum = 100; $accel.Value = 30; $group.Controls.Add($accel)
$profileList = [System.Windows.Forms.ComboBox]::new(); $profileList.DropDownStyle = 'DropDownList'; $profileList.Location = '405,70'; $profileList.Size = '120,26'; [void]$profileList.Items.Add('CONSTANT'); [void]$profileList.Items.Add('SMOOTH'); [void]$profileList.Items.Add('LINEAR'); $profileList.SelectedIndex = 0; $group.Controls.Add($profileList)

$moveNegative = [System.Windows.Forms.Button]::new(); $moveNegative.Text = [char]0x2190 + '  STEP NEGATIVE'; $moveNegative.Location = '45,118'; $moveNegative.Size = '220,48'; $moveNegative.BackColor = [System.Drawing.Color]::FromArgb(220,225,232); $group.Controls.Add($moveNegative)
$movePositive = [System.Windows.Forms.Button]::new(); $movePositive.Text = 'STEP POSITIVE  ' + [char]0x2192; $movePositive.Location = '325,118'; $movePositive.Size = '220,48'; $movePositive.BackColor = [System.Drawing.Color]::FromArgb(220,225,232); $group.Controls.Add($movePositive)

$stop = [System.Windows.Forms.Button]::new()
$stop.Text = 'STOP AND DISABLE'
$stop.Font = [System.Drawing.Font]::new('Segoe UI', 13, [System.Drawing.FontStyle]::Bold)
$stop.ForeColor = [System.Drawing.Color]::White
$stop.BackColor = [System.Drawing.Color]::FromArgb(190, 45, 45)
$stop.FlatStyle = 'Flat'
$stop.Location = '18,304'
$stop.Size = '597,50'
$form.Controls.Add($stop)

$warning = [System.Windows.Forms.Label]::new()
$warning.Text = 'Remove the Allen key before motion. Power off and wait for dark LEDs before inserting a tool.'
$warning.ForeColor = [System.Drawing.Color]::FromArgb(155, 60, 25)
$warning.Location = '20,362'
$warning.Size = '590,24'
$warning.TextAlign = 'MiddleCenter'
$form.Controls.Add($warning)

$log = [System.Windows.Forms.TextBox]::new()
$log.Multiline = $true
$log.ReadOnly = $true
$log.ScrollBars = 'Vertical'
$log.Font = [System.Drawing.Font]::new('Consolas', 8.5)
$log.Location = '18,392'
$log.Size = '597,125'
$form.Controls.Add($log)

$timer = [System.Windows.Forms.Timer]::new(); $timer.Interval = 50; $timer.Add_Tick({ Read-Serial }); $timer.Start()

$refresh.Add_Click({ Refresh-Ports })
$connect.Add_Click({
    if (-not $portList.SelectedItem) { return }
    try {
        $script:port = [System.IO.Ports.SerialPort]::new([string]$portList.SelectedItem, 115200, 'None', 8, 'One')
        $script:port.NewLine = "`n"
        $script:port.DtrEnable = $true
        $script:port.Open()
        $script:buffer = ''
        $script:driverReady = $false
        Set-State 'CHECKING DRIVER...' ([System.Drawing.Color]::FromArgb(70, 120, 175))
        Add-Log ("Connected to " + $portList.SelectedItem)
        Start-Sleep -Milliseconds 250
        [void](Send-Command 'VERSION')
        [void](Send-Command 'CHECK')
    } catch {
        Add-Log ("Connect failed: " + $_.Exception.Message)
        if ($null -ne $script:port) { $script:port.Dispose(); $script:port = $null }
        Set-State 'CONNECTION FAILED' ([System.Drawing.Color]::FromArgb(190, 65, 65))
    }
    Update-Controls
})
$disconnect.Add_Click({
    if ($null -ne $script:port) {
        if ($script:port.IsOpen) { [void](Send-Command 'STOP'); Start-Sleep -Milliseconds 100; $script:port.Close() }
        $script:port.Dispose(); $script:port = $null
    }
    $script:driverReady = $false; $script:moving = $false; $script:pendingMove = $null
    Set-State 'DISCONNECTED' ([System.Drawing.Color]::FromArgb(85, 105, 130)); Update-Controls
})
$check.Add_Click({ $script:driverReady = $false; Set-State 'CHECKING DRIVER...' ([System.Drawing.Color]::FromArgb(70,120,175)); [void](Send-Command 'CHECK'); Update-Controls })
$stop.Add_Click({ $script:pendingMove = $null; $script:moving = $false; [void](Send-Command 'STOP'); Update-Controls })
$movePositive.Add_Click({ Queue-Move 1 })
$moveNegative.Add_Click({ Queue-Move -1 })
$updateEquivalent = {
    $deg = Convert-StepsToDegrees $stepCount.Value ([string]$stepMode.SelectedItem)
    $equivalent.Text = ('= {0:F4} motor deg' -f $deg)
}
$stepCount.Add_ValueChanged($updateEquivalent)
$stepMode.Add_SelectedIndexChanged($updateEquivalent)
$form.Add_FormClosing({
    $timer.Stop()
    if ($null -ne $script:port) {
        try { if ($script:port.IsOpen) { $script:port.WriteLine('STOP'); Start-Sleep -Milliseconds 100; $script:port.Close() } } catch {}
        $script:port.Dispose()
    }
})

Refresh-Ports
& $updateEquivalent
Update-Controls
[void]$form.ShowDialog()
