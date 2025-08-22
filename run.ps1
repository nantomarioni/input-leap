# Input Leap Run Script
# This script runs the built Input Leap application

param(
    [string]$Config = "Release", # Options: Release, Debug
    [switch]$Server,             # Run input-leaps (server)
    [switch]$Client,             # Run input-leapc (client)
    [switch]$Daemon              # Run input-leapd (daemon)
)

function Write-Info { param($Message) Write-Host $Message -ForegroundColor Cyan }
function Write-Error { param($Message) Write-Host $Message -ForegroundColor Red }
function Write-Success { param($Message) Write-Host $Message -ForegroundColor Green }

$BuildDir = "build\bin\$Config"

if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir"
    Write-Error "Please build the project first using: .\build_env.ps1 build"
    exit 1
}

# Determine which executable to run
$ExeName = "input-leap.exe"  # Default to GUI
if ($Server) { $ExeName = "input-leaps.exe" }
elseif ($Client) { $ExeName = "input-leapc.exe" }
elseif ($Daemon) { $ExeName = "input-leapd.exe" }

$ExePath = "$BuildDir\$ExeName"

if (-not (Test-Path $ExePath)) {
    Write-Error "Executable not found: $ExePath"
    Write-Error "Available executables:"
    Get-ChildItem "$BuildDir\*.exe" | ForEach-Object { Write-Error "  $($_.Name)" }
    exit 1
}

Write-Info "Starting Input Leap: $ExeName"
Write-Success "Path: $ExePath"

# Set Qt deployment directory for the GUI app
if ($ExeName -eq "input-leap.exe") {
    $env:QT_PLUGIN_PATH = (Resolve-Path "build\qtDeploy").Path
    Write-Info "Qt plugins path: $env:QT_PLUGIN_PATH"
}

# Run the application
& $ExePath
