# Semi-automated network integration test: two autopilot instances fight under a lag profile,
# then both logs are parsed and cross-checked. Exit code 0 = pass.
param(
    [ValidateSet("Baseline", "Latency", "Loss", "Nasty")]
    [string]$Profile = "Baseline",
    [int]$DurationSec = 120,
    [string]$EnginePath = "D:\UE4\UE_5.7",
    [string]$ProjectPath = "D:\UE5\DISMultiplayerTank\DISMultiplayerTank.uproject"
)

$SimArgs = switch ($Profile) {
    "Baseline" { "" }
    "Latency"  { "-NetSimLatencyMs=150 -NetSimJitterMs=50" }
    "Loss"     { "-NetSimLossPct=20" }
    "Nasty"    { "-NetSimLatencyMs=250 -NetSimJitterMs=100 -NetSimLossPct=30 -NetSimDupPct=5" }
}

$EditorExe = Join-Path $EnginePath "Engine\Binaries\Win64\UnrealEditor.exe"
$LogDir = Join-Path (Split-Path $ProjectPath) "Saved\Logs"
Remove-Item (Join-Path $LogDir "DISMultiplayerTank*.log") -Force -ErrorAction SilentlyContinue

Write-Host "=== NetTest profile=$Profile duration=${DurationSec}s sim='$SimArgs' ==="
$CommonArgs = "`"$ProjectPath`" -game -windowed -resx=960 -resy=540 -AutoPilot -log $SimArgs"
$ProcA = Start-Process $EditorExe -ArgumentList "$CommonArgs -WinX=0 -WinY=100" -PassThru
$ProcB = Start-Process $EditorExe -ArgumentList "$CommonArgs -WinX=970 -WinY=100" -PassThru

Start-Sleep -Seconds $DurationSec
Stop-Process -Id $ProcA.Id, $ProcB.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 3

$LogA = Join-Path $LogDir "DISMultiplayerTank.log"
$LogB = Join-Path $LogDir "DISMultiplayerTank_2.log"
$Failures = @()
$Notes = @()

foreach ($Log in @($LogA, $LogB)) {
    if (-not (Test-Path $Log)) { $Failures += "missing log: $Log"; continue }
    if (-not (Select-String -Path $Log -Pattern "MatchStarted" -Quiet)) { $Failures += "no MatchStarted in $(Split-Path $Log -Leaf)" }
    if (Select-String -Path $Log -Pattern "Fatal error" -Quiet) { $Failures += "fatal error in $(Split-Path $Log -Leaf)" }
}

if ($Failures.Count -eq 0) {
    # Score convergence: the final score view of each instance must appear in the other's recent views.
    $ViewsA = @(Select-String -Path $LogA -Pattern "ScoreView (Round=\d+ Scores=\S+)" | ForEach-Object { $_.Matches[0].Groups[1].Value })
    $ViewsB = @(Select-String -Path $LogB -Pattern "ScoreView (Round=\d+ Scores=\S+)" | ForEach-Object { $_.Matches[0].Groups[1].Value })
    $KillsA = @(Select-String -Path $LogA -Pattern "KillConfirmed").Count
    $KillsB = @(Select-String -Path $LogB -Pattern "KillConfirmed").Count
    $Notes += "kills adjudicated: A=$KillsA B=$KillsB, score views: A=$($ViewsA.Count) B=$($ViewsB.Count)"

    if ($ViewsA.Count -eq 0 -and $ViewsB.Count -eq 0) {
        $Notes += "no kills happened; score convergence not exercised"
    }
    else {
        $RecentA = $ViewsA | Select-Object -Last 4
        $RecentB = $ViewsB | Select-Object -Last 4
        if (($ViewsA.Count -gt 0 -and $RecentB -notcontains $ViewsA[-1]) -and ($ViewsB.Count -gt 0 -and $RecentA -notcontains $ViewsB[-1])) {
            $Failures += "score views diverged: A='$($ViewsA[-1])' B='$($ViewsB[-1])'"
        }
    }

    # Dead-reckoning error: mean must stay sane for the profile.
    foreach ($Pair in @(@($LogA, "A"), @($LogB, "B"))) {
        $HealthLines = @(Select-String -Path $Pair[0] -Pattern "RouterHealth .*DRSamples=(\d+) DRMeanCm=(\d+) DRMaxCm=(\d+)")
        if ($HealthLines.Count -gt 0) {
            $Means = $HealthLines | ForEach-Object { [int]$_.Matches[0].Groups[2].Value }
            $Maxes = $HealthLines | ForEach-Object { [int]$_.Matches[0].Groups[3].Value }
            $WorstMean = ($Means | Measure-Object -Maximum).Maximum
            $WorstMax = ($Maxes | Measure-Object -Maximum).Maximum
            $Notes += "DR error $($Pair[1]): worst mean=${WorstMean}cm, worst max=${WorstMax}cm"
            if ($WorstMean -gt 400) { $Failures += "DR mean error too high on $($Pair[1]): ${WorstMean}cm" }
        }
    }
}

Write-Host ""
foreach ($Note in $Notes) { Write-Host "  note: $Note" }
if ($Failures.Count -eq 0) {
    Write-Host "RESULT: PASS ($Profile)" -ForegroundColor Green
    exit 0
}
foreach ($Failure in $Failures) { Write-Host "  FAIL: $Failure" -ForegroundColor Red }
Write-Host "RESULT: FAIL ($Profile)" -ForegroundColor Red
exit 1
