# Fail before a Quest APK build if embedded lib3os_kernel.so is older than kernel C/C++ sources.
param(
    [string]$RepoRoot = "F:\3OS",
    [switch]$AllowStale,
    # Exit 2 when stale (no rebuild hint spam); 0 fresh; 1 missing SO.
    [switch]$QueryOnly
)

$ErrorActionPreference = "Stop"
$repo = $RepoRoot

$soPath = Join-Path $repo "3OS_Unity_Sample_Project\Assets\Plugins\Android\lib3os_kernel.so"
if (-not (Test-Path $soPath)) {
    Write-Host "MISSING: $soPath - run tools/sync_kernel_to_unity.ps1 after building Android." -ForegroundColor Red
    if (-not $AllowStale) { exit 1 }
    exit 0
}

$soTime = (Get-Item $soPath).LastWriteTime
$srcDirs = @(
    (Join-Path $repo "src")
    (Join-Path $repo "include")
)
$newest = Get-ChildItem -Path $srcDirs -Recurse -Include *.cpp,*.c,*.h,*.hpp -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $newest) {
    Write-Host "WARNING: no kernel sources found under src/ or include/." -ForegroundColor Yellow
    exit 0
}

if ($newest.LastWriteTime -gt $soTime) {
    if (-not $QueryOnly) {
        Write-Host "STALE lib3os_kernel.so for Quest APK" -ForegroundColor Red
        Write-Host "  Embedded SO : $soPath ($soTime)"
        Write-Host "  Newer source: $($newest.FullName) ($($newest.LastWriteTime))"
        Write-Host "Fix: run .\tools\build_final_demo.ps1 (auto-rebuilds) or .\tools\build_android_kernel.ps1"
        Write-Host "Or pass -AllowStaleKernel to build_final_demo.ps1 (not recommended for gate tests)."
    }
    if ($QueryOnly) { exit 2 }
    if (-not $AllowStale) { exit 1 }
}

Write-Host "Kernel SO OK for APK. SO=$soTime newest_source=$($newest.LastWriteTime)"
exit 0
