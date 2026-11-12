# makcu_probe.ps1 - what Windows sees on the serial side, and what each COM port answers to km.version()
# run: powershell -ExecutionPolicy Bypass -File makcu_probe.ps1
$out = New-Object System.Collections.Generic.List[string]
function Log($s) { Write-Host $s; $out.Add($s) }

Log "== serial / CH343 / CH340 devices (Windows view) =="
$dev = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '\(COM\d+\)' -or $_.PNPDeviceID -match 'VID_1A86|VID_2E8A|VID_303A' }
if (-not $dev) { Log "NONE: no COM port and no 1A86 device -> Windows does not see the Makcu at all (cable / port / driver)" }
foreach ($d in $dev) { Log ("{0} | {1} | status={2} | errcode={3}" -f $d.Name, $d.PNPDeviceID, $d.Status, $d.ConfigManagerErrorCode) }

Log ""
Log "== processes that usually hold the Makcu port =="
$p = Get-Process | Where-Object { $_.Name -match 'makcu|relay|arduino|putty|serial|termite|realterm|hhd|kmbox|medius' }
if ($p) { $p | ForEach-Object { Log ("{0} (pid {1})" -f $_.Name, $_.Id) } } else { Log "none found" }

Log ""
Log "== probe: km.version() on every COM port x baud =="
$bauds = 115200, 4000000, 2000000, 1000000, 921600, 460800
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if (-not $ports) { Log "GetPortNames() is empty" }
foreach ($name in $ports) {
    foreach ($b in $bauds) {
        $sp = New-Object System.IO.Ports.SerialPort $name, $b, 'None', 8, 'One'
        $sp.ReadTimeout = 300
        $sp.WriteTimeout = 300
        $sp.DtrEnable = $true
        try { $sp.Open() }
        catch { Log ("{0} @{1} OPEN FAIL: {2}" -f $name, $b, $_.Exception.Message); $sp.Dispose(); break }
        try {
            $sp.DiscardInBuffer()
            $sp.Write("km.version()`r`n")
            Start-Sleep -Milliseconds 400
            $r = $sp.ReadExisting()
            $hex = (($r.ToCharArray() | ForEach-Object { '{0:X2}' -f [int]$_ }) -join ' ')
            if ($hex.Length -gt 90) { $hex = $hex.Substring(0, 90) + '...' }
            Log ("{0} @{1} -> len={2} text=[{3}] hex={4}" -f $name, $b, $r.Length, ($r -replace "[\r\n]", ' '), $hex)
        }
        catch { Log ("{0} @{1} ERR: {2}" -f $name, $b, $_.Exception.Message) }
        finally { $sp.Close(); $sp.Dispose() }
    }
}

$file = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'makcu_probe.txt'
$out | Out-File -Encoding utf8 $file
Log ""
Log "saved: $file"
