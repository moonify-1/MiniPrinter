param(
    [string]$Base = "http://192.168.0.168",
    [int]$RepeatLines = 16,
    [int]$Density = 25,
    [int]$Heat = 25,
    [int]$MotorSteps = 4,
    [double]$Timeout = 8.0,
    [switch]$KeepFile,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

# $PSCommandPath 是 PowerShell 提供的“当前脚本文件路径”。
# 某些旧宿主里它可能为空，所以再用 MyInvocation 做兜底。
$scriptFile = $PSCommandPath
if ([string]::IsNullOrWhiteSpace($scriptFile)) {
    $scriptFile = $MyInvocation.MyCommand.Definition
}

# 先取脚本所在目录，再拼出 Python 脚本路径，可以让你在项目任意目录下启动本脚本。
$scriptDir = Split-Path -Parent $scriptFile
if ([string]::IsNullOrWhiteSpace($scriptDir)) {
    $scriptDir = Join-Path (Get-Location).Path "tools"
}
$scriptPath = Join-Path $scriptDir "real_simple_print_test.py"

$argsList = @(
    $scriptPath,
    "--base", $Base,
    "--repeat-lines", $RepeatLines,
    "--density", $Density,
    "--heat", $Heat,
    "--motor-steps", $MotorSteps,
    "--timeout", $Timeout
)

if ($KeepFile) {
    $argsList += "--keep-file"
}

if ($DryRun) {
    $argsList += "--dry-run"
}

python @argsList
exit $LASTEXITCODE
