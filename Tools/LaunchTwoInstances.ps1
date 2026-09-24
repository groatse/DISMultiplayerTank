# Launches two windowed game instances side by side for local DIS sync testing.
param(
    [string]$EnginePath = "D:\UE4\UE_5.7",
    [string]$ProjectPath = "D:\UE5\DISMultiplayerTank\DISMultiplayerTank.uproject"
)

$EditorExe = Join-Path $EnginePath "Engine\Binaries\Win64\UnrealEditor.exe"

& $EditorExe $ProjectPath -game -windowed -resx=960 -resy=540 -WinX=0 -WinY=100 -log
& $EditorExe $ProjectPath -game -windowed -resx=960 -resy=540 -WinX=970 -WinY=100 -log
