# Builds the Final Demo APK through the live Unity Editor (must be open on the sample project).
# The build step bumps the version code itself and runs reset_quest_demo_docs.ps1.
# After a successful build, seeds QA/<milestone>_build<N>.md for device-test notes.
# By default: if kernel C++ is newer than lib3os_kernel.so, rebuild Android + sync, then Unity APK.
# -SkipKernelRebuild: fail instead of auto-rebuild (old guard-only behavior).
# -AllowStaleKernel: skip rebuild and allow a stale SO into the APK (not for gate tests).
# -ForceKernelRebuild: always rebuild/sync even when SO looks fresh.
param(
    [switch]$AllowStaleKernel,
    [switch]$SkipKernelRebuild,
    [switch]$ForceKernelRebuild
)
$ErrorActionPreference = "Stop"
$repo = "F:\3OS"
$tools = Join-Path $repo "tools"
$project = "$repo\3OS_Unity_Sample_Project"
$unity = "$env:LOCALAPPDATA\Unity\bin\unity.exe"
# Bump this when the active Quest gate milestone changes (matches IMPLEMENTATION_PLAN).
$qaMilestone = "0.8.1"
function New-QaNotesFile {
    param(
        [Parameter(Mandatory = $true)][string]$Milestone,
        [Parameter(Mandatory = $true)][string]$BuildNumber,
        [string]$ApkName = ""
    )
    $qaDir = Join-Path $repo "QA"
    New-Item -ItemType Directory -Force $qaDir | Out-Null
    $path = Join-Path $qaDir ("{0}_build{1}.md" -f $Milestone, $BuildNumber)
    if (Test-Path $path) {
        Write-Host "QA notes already exist: $path"
        return $path
    }
    $apkLine = if ($ApkName) { $ApkName } else { "3OS_Final_Demo_$BuildNumber.apk" }
    $body = @"
# QA — Phase $Milestone / build $BuildNumber

> Seeded by ``tools/build_final_demo.ps1`` after ``$apkLine``.
> Drop device-test notes below; mark items fixed as they land.

## Solo Drop

-

## Multi-Select

-

## Other

-

"@
    Set-Content -Path $path -Value $body -Encoding utf8
    Write-Host "Created QA notes: $path"
    return $path
}

if (-not $AllowStaleKernel) {
    if ($ForceKernelRebuild) {
        Write-Host "ForceKernelRebuild: rebuilding lib3os_kernel.so ..."
        & (Join-Path $tools "build_android_kernel.ps1") -RepoRoot $repo
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } else {
        & (Join-Path $tools "assert_kernel_fresh_for_demo.ps1") -RepoRoot $repo -QueryOnly
        $stale = ($LASTEXITCODE -eq 2)
        if ($LASTEXITCODE -eq 1) { exit 1 }
        if ($stale) {
            if ($SkipKernelRebuild) {
                & (Join-Path $tools "assert_kernel_fresh_for_demo.ps1") -RepoRoot $repo
                exit $LASTEXITCODE
            }
            Write-Host "Kernel stale; rebuilding Android lib3os_kernel.so before Unity APK ..."
            & (Join-Path $tools "build_android_kernel.ps1") -RepoRoot $repo
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
    }
    & (Join-Path $tools "assert_kernel_fresh_for_demo.ps1") -RepoRoot $repo
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Unity CLI wants the sample project cwd; always return to tools afterward
# (including on failure — `exit` would skip a finally block).
Push-Location $project
$exitCode = 0
try {
    $before = Get-ChildItem "$project\Builds\Android\3OS_Final_Demo_*.apk" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime | Select-Object -Last 1
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $out = & $unity command invoke_menu --path "3OS/Build/Final Demo APK" --detach --json 2>&1 | Out-String
    $m = [regex]::Match($out, '"(?:jobId|job_id|id)"\s*:\s*"([^"]+)"')
    if (-not $m.Success) { $m = [regex]::Match($out, '[0-9a-fA-F]{8}-[0-9a-fA-F-]{27}|[A-Za-z0-9_-]{16,}') }
    if (-not $m.Success) { Write-Host $out; throw "Could not find a job id in the CLI output above." }
    $jobId = if ($m.Groups.Count -gt 1 -and $m.Groups[1].Value) { $m.Groups[1].Value } else { $m.Value }
    Write-Host "Build job: $jobId"

    & $unity job wait $jobId --timeout 0
    Write-Host ("Build finished in {0:mm\:ss}" -f $sw.Elapsed)

    $apk = Get-ChildItem "$project\Builds\Android\3OS_Final_Demo_*.apk" | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $apk -or ($before -and $apk.FullName -eq $before.FullName -and $apk.LastWriteTime -eq $before.LastWriteTime)) {
        Write-Host "BUILD FAILED: no new APK was produced (newest is still $($before.Name))." -ForegroundColor Red
        Write-Host "See the Editor log: $env:LOCALAPPDATA\Unity\Editor\Editor.log (search for 'Error building Player')."
        $exitCode = 1
    } else {
        Write-Host "Newest APK: $($apk.Name) ($($apk.LastWriteTime))"

        $buildNum = [regex]::Match($apk.Name, '_(\d+)\.apk$').Groups[1].Value
        if (-not $buildNum) {
            Write-Host "WARNING: could not parse build number from $($apk.Name); skipping QA notes seed." -ForegroundColor Yellow
        } else {
            New-QaNotesFile -Milestone $qaMilestone -BuildNumber $buildNum -ApkName $apk.Name | Out-Null
        }
    }
} catch {
    Write-Host $_ -ForegroundColor Red
    $exitCode = 1
} finally {
    Set-Location $tools
    # Windows system chime so you can look away during long Quest APK builds.
    try {
        if ($exitCode -eq 0) {
            [System.Media.SystemSounds]::Asterisk.Play()
        } else {
            [System.Media.SystemSounds]::Hand.Play()
        }
    } catch {
        [Console]::Beep(800, 200)
    }
}
exit $exitCode
