# scan_capture.ps1 - captura el escaner de banda del firmware test_nrf24,
# promedia varios barridos completos y guarda un CSV (ch,mhz,avg).
# Uso:  powershell -File scan_capture.ps1 -Out base.csv [-Port COM11] [-Seconds 16]
param(
  [string]$Port = 'COM11',
  [int]$Seconds = 16,
  [Parameter(Mandatory=$true)][string]$Out
)

$p = New-Object System.IO.Ports.SerialPort $Port,115200,'None',8,'One'
$p.ReadTimeout = 400
$p.DtrEnable = $false   # no resetear la placa al abrir
$p.RtsEnable = $false
$p.Open()
Start-Sleep -Milliseconds 400
$p.DiscardInBuffer()

$p.WriteLine('s')       # arranca el escaner continuo
$sb = New-Object System.Text.StringBuilder
$t = [Diagnostics.Stopwatch]::StartNew()
while ($t.Elapsed.TotalSeconds -lt $Seconds) {
  try { $s = $p.ReadExisting(); if ($s) { [void]$sb.Append($s) } } catch {}
  Start-Sleep -Milliseconds 80
}
$p.WriteLine('x')       # para el escaner
Start-Sleep -Milliseconds 200
$p.Close()

$text = $sb.ToString()

# Cada barrido completo imprime cabecera "ch  MHz" y termina con la nota "(vacio".
$blocks = $text -split 'ch\s+MHz\s+actividad \(RPD\)'
$sum = @{}; 0..125 | ForEach-Object { $sum[$_] = 0 }
$nsweep = 0
foreach ($b in $blocks) {
  if ($b -notmatch 'vacio') { continue }   # solo barridos COMPLETOS
  $nsweep++
  foreach ($line in ($b -split "`n")) {
    if ($line -match '^\s*(\d+)\s+(\d+)\s+.*?(\d+)%\s*$') {
      $ch = [int]$Matches[1]
      $pct = [int]$Matches[3]
      if ($ch -ge 0 -and $ch -le 125) { $sum[$ch] += $pct }
    }
  }
}

if ($nsweep -eq 0) { Write-Output "ERROR: no se capturaron barridos completos (sube -Seconds)"; exit 1 }

$rows = 0..125 | ForEach-Object {
  [pscustomobject]@{ ch = $_; mhz = 2400 + $_; avg = [math]::Round($sum[$_] / $nsweep, 1) }
}
$rows | Export-Csv -NoTypeInformation -Path $Out
Write-Output "Barridos promediados: $nsweep  ->  $Out"
