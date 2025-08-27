# Input Leap Build Environment Setup Script
# This script sets up the environment variables and builds the project
#
# Available Actions:
# - setup:      Set up environment variables only
# - build:      Build the project (configures CMake if needed)
# - clean:      Clean build outputs (preserves CMake configuration)
# - fullclean:  Remove entire build directory
# - reconfigure: Reconfigure CMake (preserves build outputs)
# - rebuild:    Full rebuild (fullclean + build)

param(
    [string]$Action = "build",  # Options: setup, build, clean, fullclean, reconfigure, rebuild
    [string]$Config = "Debug"   # Options: Release, Debug (case-insensitive)
)

# Color output functions
function Write-Success { param($Message) Write-Host $Message -ForegroundColor Green }
function Write-Info { param($Message) Write-Host $Message -ForegroundColor Cyan }
function Write-Warning { param($Message) Write-Host $Message -ForegroundColor Yellow }
function Write-Error { param($Message) Write-Host $Message -ForegroundColor Red }

# Print multi-line command output in a readable way. Passing an array directly
# to Write-Error/Write-Host can cause PowerShell to coerce it into a single
# space-joined string. Use this helper to emit each captured line separately.
function Write-ErrorMulti {
    param([Parameter(Mandatory=$true)][object]$Lines)
    if ($null -eq $Lines) { return }
    if ($Lines -is [System.Array]) {
        foreach ($line in $Lines) {
            # Use the local Write-Error function for consistent coloring
            Write-Error $line
        }
    } else {
        Write-Error $Lines
    }
}

Write-Info "=== Input Leap Build Environment Setup ==="

# Normalize configuration casing early
switch ($Config.ToLower()) {
    'debug'   { $Config = 'Debug' }
    'release' { $Config = 'Release' }
    default   { Write-Error "Unsupported configuration '$Config'. Use Release or Debug."; exit 1 }
}

# Set environment variables
$env:QT_ROOT = "C:\Users\nicoc\6.6.3\msvc2019_64"
$env:BONJOUR_SDK_HOME = (Resolve-Path ".\deps\bonjour-sdk").Path

Write-Info "Environment Variables Set:"
Write-Info "  QT_ROOT: $env:QT_ROOT"
Write-Info "  BONJOUR_SDK_HOME: $env:BONJOUR_SDK_HOME"

# Verify Qt installation
if (-not (Test-Path $env:QT_ROOT)) {
    Write-Error "Qt installation not found at $env:QT_ROOT"
    Write-Error "Please install Qt 6.6.3 or update the QT_ROOT path in this script"
    exit 1
}

# Verify Bonjour SDK
if (-not (Test-Path $env:BONJOUR_SDK_HOME)) {
    Write-Error "Bonjour SDK not found at $env:BONJOUR_SDK_HOME"
    Write-Error "Please ensure Bonjour SDK is downloaded to .\deps\bonjour-sdk"
    exit 1
}

# Verify required files
$qtConfigFile = "$env:QT_ROOT\lib\cmake\Qt6\Qt6Config.cmake"
$bonjourHeader = "$env:BONJOUR_SDK_HOME\Include\dns_sd.h"
$bonjourLib = "$env:BONJOUR_SDK_HOME\Lib\x64\dnssd.lib"

if (-not (Test-Path $qtConfigFile)) {
    Write-Error "Qt CMake config not found: $qtConfigFile"
    exit 1
}

if (-not (Test-Path $bonjourHeader)) {
    Write-Error "Bonjour header not found: $bonjourHeader"
    exit 1
}

if (-not (Test-Path $bonjourLib)) {
    Write-Error "Bonjour library not found: $bonjourLib"
    exit 1
}

Write-Success "All dependencies verified successfully!"

function Ensure-QtLibraries {
    param([string]$Config)
    $binDir = "build/bin/$Config"
    $exe    = Join-Path $binDir 'input-leap.exe'
    if (-not (Test-Path $exe)) { Write-Warning "Qt check: executable missing ($exe)"; return }
    $coreDllPattern = if ($Config -eq 'Debug') { 'Qt6Cored.dll' } else { 'Qt6Core.dll' }
    $corePresent = Get-ChildItem -Path $binDir -Filter $coreDllPattern -ErrorAction SilentlyContinue
    $platformsDir = Join-Path $binDir 'platforms'
    $needDeploy = $false
    if (-not $corePresent) { $needDeploy = $true }
    elseif (-not (Test-Path $platformsDir)) { $needDeploy = $true }
    if (-not $needDeploy) {
        Write-Info "Qt libraries already deployed (core + plugins)."
        return
    }
    $deployTool = Join-Path $env:QT_ROOT 'bin/windeployqt.exe'
    if (-not (Test-Path $deployTool)) { Write-Warning "Cannot deploy Qt: windeployqt not found at $deployTool"; return }
    Write-Info "Qt components missing; running windeployqt..."
    $qtDeployDir = 'build/qtDeploy'
    if (Test-Path $qtDeployDir) { Remove-Item $qtDeployDir -Recurse -Force -ErrorAction SilentlyContinue }
    $args = @('--no-compiler-runtime','--no-system-d3d-compiler','--dir', $qtDeployDir)
    if ($Config -eq 'Debug') { $args += '--debug' }
    $args += $exe
    & $deployTool @args | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Warning "windeployqt exited with code $LASTEXITCODE"; return }
    if (-not (Test-Path $qtDeployDir)) { Write-Warning "windeployqt did not create $qtDeployDir"; return }
    # Copy core DLLs
    Get-ChildItem $qtDeployDir -Filter *.dll -File | ForEach-Object { Copy-Item $_.FullName $binDir -Force }
    # Copy plugin / resource directories
    $pluginDirs = @('platforms','imageformats','iconengines','generic','styles','tls','translations','networkinformation')
    foreach ($d in $pluginDirs) {
        $src = Join-Path $qtDeployDir $d
        if (Test-Path $src) {
            $dest = Join-Path $binDir $d
            Copy-Item $src $dest -Recurse -Force
        }
    }
    Write-Success "Qt libraries + plugins deployed to $binDir"
}

# Handle different actions
switch ($Action.ToLower()) {
    "setup" {
        Write-Info "Environment setup complete. Use './build_env.ps1 build' to build the project."
    }
    
    "clean" {
        Write-Info "Cleaning build outputs (preserving CMake configuration)..."
        if (Test-Path "build") {
            # Clean build outputs but preserve CMake configuration
            # Note: Qt DLLs are automatically redeployed during next build
            $itemsToClean = @(
                "build\bin",
                "build\src", 
                "build\qtDeploy",
                "build\*.vcxproj*",
                "build\*.sln",
                "build\x64",
                "build\installer-*"
            )
            
            foreach ($item in $itemsToClean) {
                if (Test-Path $item) {
                    Remove-Item $item -Recurse -Force -ErrorAction SilentlyContinue
                    Write-Info "  Cleaned: $item"
                }
            }
            Write-Success "Build outputs cleaned (CMake configuration preserved)."
        } else {
            Write-Warning "Build directory doesn't exist."
        }
    }
    
    "fullclean" {
        Write-Info "Performing full clean (removes entire build directory)..."
        if (Test-Path "build") {
            Remove-Item "build" -Recurse -Force
            Write-Success "Build directory completely removed."
        } else {
            Write-Warning "Build directory doesn't exist."
        }
    }
    
    "reconfigure" {
        Write-Info "Reconfiguring CMake (preserving build outputs)..."
        
        # Remove CMake cache and configuration files
        $configFilesToRemove = @(
            "build\CMakeCache.txt",
            "build\cmake_install.cmake",
            "build\CMakeFiles"
        )
        
        foreach ($file in $configFilesToRemove) {
            if (Test-Path $file) {
                Remove-Item $file -Recurse -Force -ErrorAction SilentlyContinue
                Write-Info "  Removed: $file"
            }
        }
        
        Write-Info "Configuring CMake with Qt6 detection..."
        $configResult = & cmake -B build -A x64 -DCMAKE_PREFIX_PATH="$env:QT_ROOT" -DQt6_DIR="$env:QT_ROOT\lib\cmake\Qt6" -DCMAKE_BUILD_TYPE=$Config 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake configuration failed!"
            Write-ErrorMulti $configResult
            exit 1
        }
        Write-Success "CMake reconfiguration successful!"
    }
    
    "rebuild" {
        Write-Info "Rebuilding project (clean + build)..."
        
        # Stop any running Input Leap processes to avoid file locking issues
        Write-Info "Stopping any running Input Leap processes..."
        $processes = Get-Process | Where-Object {$_.ProcessName -like "*input-leap*"}
        if ($processes) {
            $processes | Stop-Process -Force -ErrorAction SilentlyContinue
            Write-Info "Stopped $($processes.Count) Input Leap process(es)"
            Start-Sleep -Seconds 1  # Brief pause to ensure files are unlocked
        } else {
            Write-Info "No running Input Leap processes found"
        }
        
        if (Test-Path "build") {
            Remove-Item "build" -Recurse -Force
            Write-Info "Build directory cleaned."
        }
        
        Write-Info "Configuring CMake with Qt6 detection..."
        $configResult = & cmake -B build -A x64 -DCMAKE_PREFIX_PATH="$env:QT_ROOT" -DQt6_DIR="$env:QT_ROOT\lib\cmake\Qt6" -DCMAKE_BUILD_TYPE=$Config 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake configuration failed!"
            Write-ErrorMulti $configResult
            exit 1
        }
        Write-Success "CMake configuration successful!"
        
        Write-Info "Building project ($Config configuration)..."
        $buildResult = & cmake --build build --config $Config 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed!"
            Write-ErrorMulti $buildResult
            exit 1
        }
    Write-Success "Build completed successfully!"
    Ensure-QtLibraries -Config $Config
    }
    
    "build" {
        # Stop any running Input Leap processes to avoid file locking issues
        Write-Info "Stopping any running Input Leap processes..."
        $processes = Get-Process | Where-Object {$_.ProcessName -like "*input-leap*"}
        if ($processes) {
            $processes | Stop-Process -Force -ErrorAction SilentlyContinue
            Write-Info "Stopped $($processes.Count) Input Leap process(es)"
            Start-Sleep -Seconds 1  # Brief pause to ensure files are unlocked
        } else {
            Write-Info "No running Input Leap processes found"
        }
        
        # Check if build directory exists and is configured
        if (-not (Test-Path "build\CMakeCache.txt")) {
            Write-Info "Build not configured. Running initial CMake configuration..."
            $configResult = & cmake -B build -A x64 -DCMAKE_PREFIX_PATH="$env:QT_ROOT" -DQt6_DIR="$env:QT_ROOT\lib\cmake\Qt6" -DCMAKE_BUILD_TYPE=$Config 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Error "CMake configuration failed!"
                Write-ErrorMulti $configResult
                exit 1
            }
            Write-Success "CMake configuration successful!"
        }
        
        Write-Info "Building project ($Config configuration)..."
        # Capture stdout/stderr and preserve line breaks for readable errors
        $buildResult = & cmake --build build --config $Config 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed!"
            Write-ErrorMulti $buildResult
            exit 1
        }
    Write-Success "Build completed successfully!"
    Ensure-QtLibraries -Config $Config
        
        # Show build outputs
        Write-Info "Build outputs:"
        Get-ChildItem "build\bin\$Config\*.exe" | ForEach-Object {
            Write-Info "  $($_.Name)"
        }
    Write-Info "Qt libraries present beside executable:";
    Get-ChildItem "build\bin\$Config" -Filter Qt*.dll -ErrorAction SilentlyContinue | ForEach-Object { Write-Info "  $($_.Name)" }
    }
    
    default {
        Write-Error "Unknown action: $Action"
        Write-Info "Available actions:"
        Write-Info "  setup      - Set up environment variables only"
        Write-Info "  build      - Build the project (configures if needed)"
        Write-Info "  clean      - Clean build outputs (preserves CMake config)"
        Write-Info "  fullclean  - Remove entire build directory"
        Write-Info "  reconfigure- Reconfigure CMake (preserves build outputs)"
        Write-Info "  rebuild    - Full rebuild (fullclean + build)"
        Write-Info ""
        Write-Info "Usage: .\build_env.ps1 [action] [Release|Debug]"
        Write-Info "Examples:"
        Write-Info "  .\build_env.ps1 build"
        Write-Info "  .\build_env.ps1 clean"
        Write-Info "  .\build_env.ps1 reconfigure Debug"
        exit 1
    }
}

Write-Success "=== Script completed successfully! ==="
