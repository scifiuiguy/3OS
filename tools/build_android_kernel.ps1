# Configure (if needed) and build lib3os_kernel.so for Quest, then sync into Unity.
param(
    [string]$RepoRoot = "F:\3OS",
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$repo = $RepoRoot
$buildDir = Join-Path $repo "build-android-arm64"

function Find-UnityAndroidCmake {
    $hub = "C:\Program Files\Unity\Hub\Editor"
    if (-not (Test-Path $hub)) { return $null }
    $cmakes = Get-ChildItem -Path $hub -Recurse -Filter "cmake.exe" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "AndroidPlayer\\SDK\\cmake" } |
        Sort-Object { $_.Directory.Parent.Parent.Parent.Parent.Name } -Descending
    return $cmakes | Select-Object -First 1
}

function Find-UnityNdk {
    param([string]$CmakeExe)
    $editorRoot = $CmakeExe
    for ($i = 0; $i -lt 8; $i++) {
        $editorRoot = Split-Path $editorRoot -Parent
    }
    $ndk = Join-Path $editorRoot "Data\PlaybackEngines\AndroidPlayer\NDK"
    if (Test-Path $ndk) { return $ndk }
    return $null
}

$cmakeItem = Find-UnityAndroidCmake
if (-not $cmakeItem) {
    $cmakeExe = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeExe) {
        $cmakePath = $cmakeExe.Source
        $ninjaPath = (Get-Command ninja -ErrorAction SilentlyContinue).Source
        $ndk = $env:ANDROID_NDK_HOME
        if (-not $ndk) { $ndk = $env:NDK_ROOT }
    } else {
        Write-Host "No cmake found (Unity Android SDK cmake or cmake on PATH)." -ForegroundColor Red
        exit 1
    }
} else {
    $cmakePath = $cmakeItem.FullName
    $ninjaPath = Join-Path $cmakeItem.Directory.FullName "ninja.exe"
    $ndk = Find-UnityNdk -CmakeExe $cmakePath
}

if (-not (Test-Path $cmakePath)) {
    Write-Host "cmake missing: $cmakePath" -ForegroundColor Red
    exit 1
}

$needConfigure = -not (Test-Path (Join-Path $buildDir "build.ninja"))
if ($needConfigure) {
    if (-not $ndk -or -not (Test-Path $ndk)) {
        Write-Host "Android NDK not found. Set ANDROID_NDK_HOME or install Unity Android build support." -ForegroundColor Red
        exit 1
    }
    if (-not (Test-Path $ninjaPath)) {
        Write-Host "ninja not found next to Unity cmake: $ninjaPath" -ForegroundColor Red
        exit 1
    }
    if (-not $Quiet) { Write-Host "Configuring $buildDir ..." }
    $toolchain = Join-Path $ndk "build\cmake\android.toolchain.cmake"
    & $cmakePath -S $repo -B $buildDir -G Ninja `
        -DCMAKE_TOOLCHAIN_FILE=$toolchain `
        -DANDROID_ABI=arm64-v8a `
        -DANDROID_PLATFORM=android-29 `
        -DANDROID_STL=c++_shared `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_MAKE_PROGRAM=$ninjaPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not $Quiet) { Write-Host "Building 3os_kernel (Android arm64) ..." }
& $cmakePath --build $buildDir --target 3os_kernel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $repo "tools\sync_kernel_to_unity.ps1") -RepoRoot $repo -Quiet:$Quiet
exit $LASTEXITCODE
