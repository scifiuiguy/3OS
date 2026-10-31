# Installs the newest Final Demo APK on the Quest, clears logcat, waits while you test, then saves the log.
# Saved log is filtered to the installed applicationId (see final_demo_android_package.txt), not the whole device buffer.
$ErrorActionPreference = "Stop"
$repo = "F:\3OS"
. (Join-Path $repo "tools\_qa_seed_lib.ps1")

$LegacyFinalDemoPackages = @(
  "com.scifiuiguy.threeos.sample"
)

function Resolve-QuestAdbPath {
  foreach ($c in @(
    "C:\Program Files\Unity\Hub\Editor\6000.0.60f1\Editor\Data\PlaybackEngines\AndroidPlayer\SDK\platform-tools\adb.exe",
    (Get-Command adb -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
    "F:\Android\platform-tools\adb.exe",
    "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
    "$env:ANDROID_HOME\platform-tools\adb.exe",
    "$env:ANDROID_SDK_ROOT\platform-tools\adb.exe"
  )) {
    if ($c -and (Test-Path $c)) { return $c }
  }
  throw "adb not found. Install Android platform-tools or set ANDROID_HOME."
}

function Get-LogcatPidFromLine {
  param([string]$Line)
  if ($Line -match '^\S+\s+\S+\s+(\d+)\s+\d+\s') { return [int]$Matches[1] }
  return $null
}

function Get-LogcatTimeFromLine {
  param([string]$Line)
  if ($Line -match '^(\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\.\d{3})') { return $Matches[1] }
  return $null
}

function Get-PackageSessionPids {
  param([string[]]$Lines, [string]$Package)
  $pids = [System.Collections.Generic.HashSet[int]]::new()
  $pkgEsc = [regex]::Escape($Package)
  foreach ($line in $Lines) {
    if ($line -notmatch $pkgEsc) { continue }
    $linePid = Get-LogcatPidFromLine $line
    if ($null -ne $linePid) { [void]$pids.Add($linePid) }
  }
  return ,@($pids)
}

function Test-ShellSplashLine {
  param([string]$Line)
  return $Line -match '(?i)(Successfully loaded splash|InterstitialRenderer|InterstitialController|ossplash|vr_splash|SWM-rendered interstitial)'
}

function Filter-LogcatForPackage {
  param(
    [string[]]$Lines,
    [string]$Package
  )
  $pids = Get-PackageSessionPids -Lines $Lines -Package $Package
  if ($pids.Count -eq 0) {
    return ,@(
      "----- filtered logcat: no lines matched applicationId $Package -----",
      "Launch the app from the Quest library icon for this build (package $Package), then capture again.",
      ""
    ) + $Lines | Select-Object -Last 40
  }

  $sessionStart = $null
  $sessionEnd = $null
  foreach ($line in $Lines) {
    $linePid = Get-LogcatPidFromLine $line
    if ($null -ne $linePid -and $pids.Contains($linePid)) {
      $t = Get-LogcatTimeFromLine $line
      if ($t) {
        if (-not $sessionStart -or $t -lt $sessionStart) { $sessionStart = $t }
        if (-not $sessionEnd -or $t -gt $sessionEnd) { $sessionEnd = $t }
      }
    }
  }

  $out = New-Object System.Collections.Generic.List[string]
  [void]$out.Add("# applicationId=$Package pids=$($pids -join ',') session=$sessionStart..$sessionEnd")
  $seen = [System.Collections.Generic.HashSet[string]]::new()
  foreach ($line in $Lines) {
    $keep = $false
    $linePid = Get-LogcatPidFromLine $line
    if ($null -ne $linePid -and $pids.Contains($linePid)) { $keep = $true }
    elseif (Test-ShellSplashLine $line) { $keep = $true }
    elseif ($line -match [regex]::Escape($Package)) { $keep = $true }
    if (-not $keep) { continue }
    if ($seen.Add($line)) { [void]$out.Add($line) }
  }
  return ,$out
}

$adb = Resolve-QuestAdbPath
$serial = "1WMHH822FA0346"
$pkg = Get-FinalDemoAndroidPackage -RepoRoot $repo
$builds = Join-Path $repo "3OS_Unity_Sample_Project\Builds\Android"

$apk = Get-ChildItem "$builds\3OS_Final_Demo_*.apk" | Sort-Object LastWriteTime | Select-Object -Last 1
if (-not $apk) { throw "No Final Demo APK found in $builds" }
$n = [regex]::Match($apk.Name, '_(\d+)\.apk$').Groups[1].Value

& $adb -s $serial get-state
if ($LASTEXITCODE -ne 0) { throw "Quest not connected over USB." }

foreach ($legacy in $LegacyFinalDemoPackages) {
  if ($legacy -eq $pkg) { continue }
  & $adb -s $serial shell am force-stop $legacy 2>$null | Out-Null
}
& $adb -s $serial shell am force-stop $pkg 2>$null | Out-Null

Write-Host "Installing $($apk.Name) as $pkg ..."
& $adb -s $serial install -r $apk.FullName
if ($LASTEXITCODE -ne 0) { throw "Install failed." }

& $adb -s $serial logcat -G 16M | Out-Null
& $adb -s $serial logcat -c
Write-Host ""
Write-Host "Installed build $n (package $pkg)."
Write-Host "Logcat cleared. On the headset, launch THIS app (not $($LegacyFinalDemoPackages -join ' / '))."
Write-Host "Unplug, test, plug USB back, then press Enter."
Read-Host "Press Enter once the Quest is plugged back in"

$logDir = Join-Path $repo "build\logs"
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir "quest-logcat-$n.log"
$logFull = Join-Path $logDir "quest-logcat-$n.full.log"

$raw = (& $adb -s $serial logcat -d -v threadtime 2>&1) -split "`r?`n"
Set-Content -Path $logFull -Value $raw -Encoding utf8

$filtered = Filter-LogcatForPackage -Lines $raw -Package $pkg
Set-Content -Path $log -Value $filtered -Encoding utf8

$pkgHits = ($filtered | Where-Object { $_ -match [regex]::Escape($pkg) }).Count
Write-Host "Saved $log ($([int]((Get-Item $log).Length/1KB)) KB, $pkgHits lines mention $pkg)"
Write-Host "Full device buffer: $logFull ($([int]((Get-Item $logFull).Length/1KB)) KB)"
if ($pkgHits -eq 0) {
  Write-Host "WARNING: filtered log has no package hits - you may have launched the wrong library app." -ForegroundColor Yellow
}
