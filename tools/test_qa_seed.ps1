# Dry-run QA carry-forward (no Unity build). Usage: .\tools\test_qa_seed.ps1 365
param([string]$BuildNumber = "365")
$ErrorActionPreference = "Stop"
$repo = "F:\3OS"
$Milestone = "0.8.1"
. (Join-Path $PSScriptRoot "_qa_seed_lib.ps1")
$qaDir = Join-Path $repo "QA"
$carry = Get-QaCarryForwardBlock -Milestone $Milestone -QaDir $qaDir -BeforeBuildNumber $BuildNumber
Write-Host "--- carry-forward for build $BuildNumber ---"
Write-Host $carry
