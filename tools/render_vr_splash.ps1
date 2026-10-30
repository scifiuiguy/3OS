# Regenerate Assets/StreamingAssets/vr_splash.png (2048×1024 stereo) via Blender + Myriad Pro.
# Requires: Blender 5.x on PATH or default install path; Myriad Pro (see assets/fonts/FONT.txt).
param(
    [switch]$SwapStereo,
    [string]$FontPath = $env:MYRIAD_PRO_FONT
)
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$blender = "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"
if (-not (Test-Path $blender)) {
    throw "Blender not found at $blender"
}
$outPng = Join-Path $repo "3OS_Unity_Sample_Project\Assets\StreamingAssets\vr_splash.png"
$outGlb = Join-Path $repo "assets\splash\3OS_glyph.glb"
New-Item -ItemType Directory -Force -Path (Split-Path $outGlb) | Out-Null
$args = @($outPng, $outGlb)
if ($FontPath) { $args += $FontPath }
if ($SwapStereo) { $args += "swap" }
& $blender -b -P (Join-Path $repo "tools\make_splash_glyph.py") -- @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
python (Join-Path $repo "tools\sanitize_vr_splash_png.py") $outPng
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Wrote $outPng"
