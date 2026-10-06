# Portable entry point for the existing distance-11 batch runner.
# Resolve repository inputs relative to this script, regardless of the working directory.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$RipesPath,
    [string]$StatesPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'distance11_states.txt'),
    [string]$SourcePath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Ripes code\minirubik_solver.s'),
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'test_results\t5_distance11'),
    [ValidateRange(0, 2644)]
    [int]$MaxCases = 0,
    [ValidateRange(1000, 3600000)]
    [int]$TimeoutMs = 120000,
    [switch]$Resume
)

$ErrorActionPreference = 'Stop'
$taskRunnerPath = Join-Path $PSScriptRoot 't5_distance11_runner.ps1'
$taskParameters = @{
    RipesPath       = (Resolve-Path -LiteralPath $RipesPath).Path
    StatesPath      = (Resolve-Path -LiteralPath $StatesPath).Path
    SourcePath      = (Resolve-Path -LiteralPath $SourcePath).Path
    OutputDirectory = $OutputDirectory
    MaxCases        = $MaxCases
    TimeoutMs       = $TimeoutMs
    Resume          = $Resume.IsPresent
}

# Keep the runner byte-for-byte identical so existing manifests remain resumable.
& $taskRunnerPath @taskParameters

# Publish the latest summary beside the test scripts after a successful batch run.
$taskSummarySource = Join-Path (Resolve-Path -LiteralPath $OutputDirectory).Path 'summary.md'
$taskSummaryTarget = Join-Path $PSScriptRoot 'summary.md'
if ($taskSummarySource -ne $taskSummaryTarget) {
    Copy-Item -LiteralPath $taskSummarySource -Destination $taskSummaryTarget -Force
}
Write-Output ('Summary available at: ' + $taskSummaryTarget)
