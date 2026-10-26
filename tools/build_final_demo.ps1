# Builds the Final Demo APK through the live Unity Editor (must be open on the sample project).
# The build step bumps the version code itself and runs reset_quest_demo_docs.ps1.
$ErrorActionPreference = "Stop"
$project = "F:\3OS\3OS_Unity_Sample_Project"
$unity = "$env:LOCALAPPDATA\Unity\bin\unity.exe"
Set-Location $project

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
Write-Host "Newest APK: $($apk.Name) ($($apk.LastWriteTime))"
