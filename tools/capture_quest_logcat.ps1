# Capture Quest logcat around a 3OS launch. Requires adb on PATH or ANDROID_HOME.
param(
  [string]$OutDir = "F:\3OS\3OS_Unity_Sample_Project\Builds\Android\logs",
  [string]$Package = "com.scifiuiguy.threeos.sample",
  [int]$Seconds = 45
)

$ErrorActionPreference = "Stop"
$adb = $null
foreach ($c in @(
  (Get-Command adb -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
  "F:\Android\platform-tools\adb.exe",
  "F:\Android\platform-tools\adb\adb.exe",
  "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
  "$env:ANDROID_HOME\platform-tools\adb.exe",
  "$env:ANDROID_SDK_ROOT\platform-tools\adb.exe"
)) {
  if ($c -and (Test-Path $c)) { $adb = $c; break }
}
if (-not $adb) { throw "adb not found. Install platform-tools or add adb to PATH." }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$out = Join-Path $OutDir "quest_logcat_$stamp.txt"

Write-Host "adb: $adb"
& $adb devices
& $adb logcat -c
Write-Host "Launch the app now (or it will capture whatever is running). Collecting ${Seconds}s -> $out"
$p = Start-Process -FilePath $adb -ArgumentList @("logcat","-v","threadtime") -RedirectStandardOutput $out -NoNewWindow -PassThru
Start-Sleep -Seconds $Seconds
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Write-Host "Wrote $out ($((Get-Item $out).Length) bytes)"
Write-Host "--- crash / unity / openxr hits ---"
Select-String -Path $out -Pattern "FATAL|AndroidRuntime|DEBUG : \*\*\*|Unity|OpenXR|lib3os|CRASH|signal |Abort message" |
  Select-Object -First 80 | ForEach-Object { $_.Line }
