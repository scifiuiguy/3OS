# Reset Quest Documents demo trees to the Act I snapshot baseline.
# Wipes 3OS_Demo / 3OS_Demo2 / DemonProject leftovers, then recreates snapshot paths
# from StreamingAssets seed files. Run after each Final Demo APK build/install.
param(
  [string]$SnapshotJson = "F:\3OS\3OS_Unity_Sample_Project\Assets\StreamingAssets\3OS\final_demo_fs_snapshot.json",
  [string]$SeedDir = "F:\3OS\3OS_Unity_Sample_Project\Assets\StreamingAssets\3OS\seed",
  [string]$Package = "",
  [string]$DocsRoot = "/sdcard/Documents"
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($Package)) {
  $repo = Split-Path $PSScriptRoot -Parent
  . (Join-Path $PSScriptRoot "_qa_seed_lib.ps1")
  $Package = Get-FinalDemoAndroidPackage -RepoRoot $repo
}

$adb = $null
foreach ($c in @(
  (Get-Command adb -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
  "F:\Android\platform-tools\adb.exe",
  "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
  "$env:ANDROID_HOME\platform-tools\adb.exe",
  "$env:ANDROID_SDK_ROOT\platform-tools\adb.exe"
)) {
  if ($c -and (Test-Path $c)) { $adb = $c; break }
}
if (-not $adb) { throw "adb not found" }
if (-not (Test-Path $SnapshotJson)) { throw "snapshot missing: $SnapshotJson" }
if (-not (Test-Path $SeedDir)) { throw "seed dir missing: $SeedDir" }

$snap = Get-Content -Raw $SnapshotJson | ConvertFrom-Json
$demo1 = if ($snap.demo1_folder) { $snap.demo1_folder } else { "3OS_Demo" }
$demo2 = if ($snap.demo2_folder) { $snap.demo2_folder } else { "3OS_Demo2" }
$demo3 = if ($snap.demo3_folder) { $snap.demo3_folder } else { "3OS_Demo3" }
$demon = if ($snap.demon_project) { $snap.demon_project } else { "DemonProject" }

$d1 = "$DocsRoot/$demo1"
$d2 = "$DocsRoot/$demo2"
$d3 = "$DocsRoot/$demo3"
$demonRoot = "$d3/$demon"
$persist = "/sdcard/Android/data/$Package/files"

function Invoke-AdbOk([string[]]$AdbArgs) {
  & $adb @AdbArgs
  if ($LASTEXITCODE -ne 0) {
    throw "adb $($AdbArgs -join ' ') failed (exit $LASTEXITCODE)"
  }
}

function Invoke-ShellOk([string[]]$CmdArgs) {
  Invoke-AdbOk -AdbArgs (@("shell") + $CmdArgs)
}

# Native stderr must not abort wipe retries (Quest toybox rm is flaky on fat trees).
# Pass args as an array — bare `-p`/`-rf` would be parsed as PowerShell parameters.
function Invoke-ShellSoft([string[]]$CmdArgs) {
  $prev = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  try {
    & $adb (@("shell") + $CmdArgs) 2>$null | Out-Null
  } finally {
    $ErrorActionPreference = $prev
  }
}

Write-Host "adb: $adb"
Invoke-AdbOk -AdbArgs @("devices")

Write-Host "Stopping $Package (ignore if not running)..."
Invoke-ShellSoft -CmdArgs @("am", "force-stop", $Package)

# Without this, adb-pushed / MediaStore-staged Documents seeds are Access Denied
# for paste move/delete (DemonA.blend, simple.glb, Test3/5/6 with nested seeds).
Write-Host "Granting MANAGE_EXTERNAL_STORAGE..."
Invoke-ShellSoft -CmdArgs @("appops", "set", $Package, "MANAGE_EXTERNAL_STORAGE", "allow")
Invoke-ShellSoft -CmdArgs @("cmd", "appops", "set", $Package, "MANAGE_EXTERNAL_STORAGE", "allow")

Write-Host "Wiping demo Documents + persistent mirrors..."
$wipeTargets = @(
  $d1, $d2, $demonRoot,
  "$persist/$demo1", "$persist/$demo2", "$persist/$demo3",
  "/sdcard/.3os_Demo3_BAD_purge",
  "/sdcard/Documents/3OS_Demo_probeA",
  "/sdcard/Documents/3OS_Demo_probeB",
  "/sdcard/Documents/simple.glb"
)
foreach ($path in $wipeTargets) {
  # Children first via find -delete, then the root.
  Invoke-ShellSoft -CmdArgs @("find", $path, "-mindepth", "1", "-delete")
  Invoke-ShellSoft -CmdArgs @("rm", "-rf", $path)
  Invoke-ShellSoft -CmdArgs @("rm", "-r", $path)
}

foreach ($path in @($d1, $d2, $d3, $demonRoot, "$persist/$demo1", "$persist/$demo2", "$persist/$demo3")) {
  Invoke-ShellOk -CmdArgs @("mkdir", "-p", $path)
}

$tmp = Join-Path $env:TEMP "3os_reset_markers"
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
Set-Content -Path (Join-Path $tmp ".3os_mind_palace") -Value "mindpalace" -NoNewline
Set-Content -Path (Join-Path $tmp ".keep") -Value "keep" -NoNewline
Invoke-AdbOk -AdbArgs @("push", (Join-Path $tmp ".3os_mind_palace"), "$d3/.3os_mind_palace") | Out-Null
Invoke-AdbOk -AdbArgs @("push", (Join-Path $tmp ".keep"), "$demonRoot/.keep") | Out-Null

function Ensure-RemoteDir([string]$RemoteDir) {
  Invoke-ShellOk -CmdArgs @("mkdir", "-p", $RemoteDir)
}

function Push-Seed([string]$FileName, [string]$RemoteFile) {
  $local = Join-Path $SeedDir $FileName
  if (-not (Test-Path $local)) {
    throw "seed file missing: $local"
  }
  $idx = $RemoteFile.LastIndexOf('/')
  if ($idx -gt 0) {
    Ensure-RemoteDir $RemoteFile.Substring(0, $idx)
  }
  # Requires MANAGE_EXTERNAL_STORAGE (granted above) or paste cannot move/delete these.
  Invoke-AdbOk -AdbArgs @("push", $local, $RemoteFile) | Out-Null
}

function Apply-SnapshotRels([string]$Root, [object]$Rels) {
  if ($null -eq $Rels) { return }
  foreach ($raw in $Rels) {
    $rel = ([string]$raw).Replace('\', '/').Trim().Trim('/')
    if ([string]::IsNullOrWhiteSpace($rel)) { continue }
    $remote = "$Root/$rel"
    $leaf = Split-Path -Leaf $rel
    if ($leaf -match '\.') {
      if ($leaf -eq "simple.glb" -or $leaf -eq "DemonA.blend") {
        Push-Seed $leaf $remote
      } else {
        $idx = $remote.LastIndexOf('/')
        if ($idx -gt 0) { Ensure-RemoteDir $remote.Substring(0, $idx) }
        $empty = Join-Path $tmp "empty.bin"
        if (-not (Test-Path $empty)) { Set-Content -Path $empty -Value "" -NoNewline }
        Invoke-AdbOk -AdbArgs @("push", $empty, $remote) | Out-Null
      }
    } else {
      Ensure-RemoteDir $remote
    }
  }
}

Write-Host "Seeding $demo1 from snapshot..."
Apply-SnapshotRels $d1 $snap.demo1
Write-Host "Seeding $demo2 from snapshot..."
Apply-SnapshotRels $d2 $snap.demo2

Write-Host "==== Demo1 ===="
Invoke-ShellOk -CmdArgs @("ls", "-la", $d1)
Write-Host "==== Demo2 ===="
Invoke-ShellOk -CmdArgs @("ls", $d2)
Write-Host "==== DemonProject ===="
Invoke-ShellOk -CmdArgs @("ls", "-la", $demonRoot)
Write-Host "Quest demo Documents reset to snapshot baseline."
