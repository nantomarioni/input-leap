# Input Leap - AI Development Guide

## Architecture Overview

Input Leap is a cross-platform KVM software solution with a client-server architecture for sharing mouse/keyboard between computers. The codebase is organized into discrete components:

**Core Applications:**
- `input-leap` - Qt6-based GUI for configuration and management
- `input-leaps` - Server daemon that shares mouse/keyboard 
- `input-leapc` - Client daemon that receives input from server
- `input-leapd` - Windows service wrapper

**Library Structure (`src/lib/`):**
- `platform/` - OS-specific implementations (Windows/macOS/Linux/libei)
- `server/` & `client/` - Core networking and input handling logic
- `net/` - Network communication layer
- `arch/` - Architecture abstraction layer
- `base/` - Fundamental utilities and logging
- `gui/` components in `src/gui/src/` for Qt-based configuration

## Build System & Dependencies

**Environment Setup:**
Use the provided build scripts to avoid environment issues:
```bash
# Windows
.\build_env.ps1 build
.\run.ps1

# Build from scratch
.\build_env.ps1 rebuild
```

**Critical Dependencies:**
- **Qt6** (6.2+): GUI framework, set via `QT_ROOT` environment variable
- **Bonjour SDK** (Windows): Required for service discovery, set via `BONJOUR_SDK_HOME`
- **OpenSSL**: For encrypted connections
- **Platform-specific**: X11 libraries (Linux), Cocoa (macOS), Win32 APIs

**CMake Configuration:**
- Multi-config generator with Release/Debug builds
- Option flags: `INPUTLEAP_BUILD_GUI`, `INPUTLEAP_BUILD_TESTS`, `INPUTLEAP_BUILD_X11`, `INPUTLEAP_BUILD_LIBEI`
- Output: `build/bin/{Release|Debug}/` for executables

## Platform-Specific Patterns

**Conditional Compilation:**
```cpp
#if WINAPI_MSWINDOWS
    // Windows-specific code
#elif WINAPI_XWINDOWS  
    // X11 Linux code
#elif WINAPI_CARBON
    // macOS code
#elif WINAPI_LIBEI
    // Wayland/libei code
#endif
```

**File Naming Convention:**
- `MSWindows*` - Windows implementations
- `OSX*` - macOS implementations  
- `XWindows*` - X11/Linux implementations
- `Ei*` - libei/Wayland implementations

## Qt GUI Architecture

**Main Components:**
- `MainWindow` - Primary application window with system tray integration
- `ServerConfigDialog` - Screen layout configuration
- `SettingsDialog` - Application preferences
- `ModernMainWindow` - Modern UI interface (fork enhancement)
- `ZeroconfService` - Network service discovery using Bonjour/mDNS

**Configuration Management:**
- `ServerConfig` - Screen layout and server settings
- `AppConfig` - Application-wide preferences
- Settings persist via QSettings

**Key Patterns:**
- Qt's signal/slot mechanism for component communication
- QProcess for launching server/client processes
- QSystemTrayIcon for background operation
- IPC communication between GUI and daemons

## Testing Infrastructure

**Test Organization:**
- `src/test/unittests/` - Component unit tests using Google Test
- `src/test/integtests/` - Integration tests
- `src/test/mock/` - Mock objects for testing

**Running Tests:**
```bash
.\build_env.ps1 build
.\build\bin\Release\unittests.exe
.\build\bin\Release\integtests.exe
```

## Network & Communication

**Protocol Stack:**
- TCP-based custom protocol for input events
- SSL/TLS encryption support
- Service discovery via Bonjour/mDNS (`ZeroconfService`)
- Configuration exchange during handshake

**Key Classes:**
- `inputleap::Server` & `inputleap::Client` - Core networking
- `IpcClient` - Inter-process communication with daemons
- `ZeroconfBrowser` & `ZeroconfRegister` - Service discovery

## Development Workflow

**Code Style:**
- C++17 standard (Qt6), C++14 (Qt5)
- Platform abstraction via preprocessor conditionals
- RAII patterns, prefer smart pointers
- Qt naming conventions for GUI code

**Common Tasks:**
```bash
# Full rebuild after dependency changes
.\build_env.ps1 rebuild

# Quick development iteration
.\build_env.ps1 build
.\run.ps1

# Run specific component  
.\run.ps1 -Server  # Run server daemon
.\run.ps1 -Client  # Run client daemon
```

**Key Files to Understand:**
- `src/gui/src/MainWindow.{h,cpp}` - GUI application entry point
- `src/lib/inputleap/` - Core application logic
- `CMakeLists.txt` - Build configuration and dependencies
- `src/lib/platform/` - OS-specific implementations

**Adding Features:**
1. Identify if feature is GUI, core logic, or platform-specific
2. For GUI: Add to appropriate dialog in `src/gui/src/`
3. For core: Extend classes in `src/lib/`  
4. For platform: Add to relevant `platform/` implementation files
5. Update CMakeLists.txt if adding new source files

## External Integration Points

**Service Discovery:** Bonjour/mDNS for automatic server detection
**SSL/TLS:** OpenSSL for encrypted connections
**Platform Input:** Direct OS input injection (Windows hooks, X11 events, etc.)
**Clipboard:** Platform-specific clipboard integration for content sharing
