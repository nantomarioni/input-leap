# Input Leap Build Scripts

This directory contains automated build scripts to simplify the development process and avoid environment setup issues.

## Scripts

### `build_env.ps1` (PowerShell Script)
Main build script that handles environment setup and building.

**Usage:**
```powershell
.\build_env.ps1 [action] [config]
```

**Actions:**
- `setup` - Only set up environment variables and verify dependencies
- `build` - Build the project (default action)
- `clean` - Remove the build directory
- `rebuild` - Clean and build from scratch

**Config:**
- `Release` - Release build (default)
- `Debug` - Debug build

**Examples:**
```powershell
.\build_env.ps1                    # Build in Release mode
.\build_env.ps1 build Debug        # Build in Debug mode
.\build_env.ps1 rebuild             # Clean and rebuild
.\build_env.ps1 setup               # Just verify environment
```

### `build_env.bat` (Batch File)
Windows batch wrapper for the PowerShell script. Useful if you prefer batch files or need to call from other scripts.

**Usage:**
```cmd
build_env.bat [action] [config]
```

### `run.ps1` (PowerShell Script)
Script to run the built Input Leap applications.

**Usage:**
```powershell
.\run.ps1 [options]
```

**Options:**
- No options: Run the GUI application (`input-leap.exe`)
- `-Server`: Run the server (`input-leaps.exe`)
- `-Client`: Run the client (`input-leapc.exe`)
- `-Daemon`: Run the daemon (`input-leapd.exe`)
- `-Config Release|Debug`: Specify build configuration (default: Release)

**Examples:**
```powershell
.\run.ps1                          # Run GUI
.\run.ps1 -Server                  # Run server
.\run.ps1 -Client -Config Debug    # Run client (debug build)
```

## Prerequisites

The scripts expect the following directory structure:
- Qt 6.6.3 installed at: `C:\Users\nicoc\6.6.3\msvc2019_64`
- Bonjour SDK at: `.\deps\bonjour-sdk`

## Quick Start

1. Open PowerShell in the Input Leap root directory
2. Run: `.\build_env.ps1`
3. Run: `.\run.ps1`

## Troubleshooting

If you encounter issues:
1. Run `.\build_env.ps1 setup` to verify all dependencies
2. Run `.\build_env.ps1 rebuild` to do a clean build
3. Check that Qt and Bonjour SDK paths are correct in `build_env.ps1`

## Environment Variables

The scripts automatically set:
- `QT_ROOT`: Path to Qt installation
- `BONJOUR_SDK_HOME`: Path to Bonjour SDK

These are set only for the duration of the script execution and don't pollute your global environment.
