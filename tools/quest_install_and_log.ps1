# Installs the newest Final Demo APK on the Quest, clears logcat, waits while you test, then saves the log.
$ErrorActionPreference = "Stop"
$adb = "C:\Program Files\Unity\Hub\Editor\6000.0.60f1\Editor\Data\PlaybackEngines\AndroidPlayer\SDK\platform-tools\adb.exe"
$serial = "1WMHH822FA0346"
$pkg = "com.scifiuiguy.threeos.sample"
$builds = "F:\3OS\3OS_Unity_Sample_Project\Builds\Android"

$apk = Get-ChildItem "$builds\3OS_Final_Demo_*.apk" | Sort-Object LastWriteTime | Select-Object -Last 1
if (-not $apk) { throw "No Final Demo APK found in $builds" }
$n = [regex]::Match($apk.Name, '_(\d+)\.apk$').Groups[1].Value

& $adb -s $serial get-state
if ($LASTEXITCODE -ne 0) { throw "Quest not connected over USB." }

Write-Host "Installing $($apk.Name)..."
& $adb -s $serial install -r $apk.FullName
if ($LASTEXITCODE -ne 0) { throw "Install failed." }

& $adb -s $serial logcat -G 16M
& $adb -s $serial logcat -c
Write-Host ""
Write-Host "Installed build $n and cleared logcat. Unplug, test, then plug back in."
Read-Host "Press Enter once the Quest is plugged back in"

$log = "F:\3OS\build\logs\quest-logcat-$n.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
& $adb -s $serial logcat -d -v threadtime | Out-File -Encoding utf8 $log
Write-Host "Saved $log ($([int]((Get-Item $log).Length/1KB)) KB)"
