# Copy a freshly built Android lib3os_kernel.so into the UPM package and sample override.
# Default source: F:\3OS\build-android-arm64\lib3os_kernel.so (see docs/NATIVE_BINARIES.md).
param(
    [string]$RepoRoot = "F:\3OS",
    [string]$SourceSo = "",
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$repo = $RepoRoot

$candidates = @(
    if ($SourceSo) { $SourceSo }
    (Join-Path $repo "build-android-arm64\lib3os_kernel.so")
    (Join-Path $repo "build\android\arm64-v8a\lib3os_kernel.so")
) | Where-Object { $_ -and (Test-Path $_) }

$src = $candidates | Select-Object -First 1
if (-not $src) {
    Write-Host "No lib3os_kernel.so found. Build Android first, e.g.:" -ForegroundColor Red
    Write-Host "  cmake --build build-android-arm64 --target 3os_kernel"
    exit 1
}

$dests = @(
    (Join-Path $repo "3OS_Unity\Plugins\Android\lib3os_kernel.so")
    (Join-Path $repo "3OS_Unity_Sample_Project\Assets\Plugins\Android\lib3os_kernel.so")
)

foreach ($dest in $dests) {
    $dir = Split-Path $dest -Parent
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    Copy-Item -Path $src -Destination $dest -Force
    if (-not $Quiet) {
        Write-Host ("Synced {0} -> {1}" -f (Get-Item $src).LastWriteTime, $dest)
    }
}

if (-not $Quiet) {
    Write-Host ("Source: $src ({0})" -f (Get-Item $src).LastWriteTime)
}
exit 0
