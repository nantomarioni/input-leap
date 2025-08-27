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
- `MasterConfigDialog` - Unified configuration interface (user enhancement)
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
1. Follow fork-style extension patter (described below)
2. Adding code to any file outside of `src/fork/` is forbidden with the following exceptions:
   - Making a class implement a ExtensionHost so it can be extended
   - Adding a Extension as a friend class so it can access private members
   - Modify CMakeList files to expose code from fork to the other pakcages
   - Adding a single line that calls the ExtensionHost method handleCommand to execute logic in the middle of existing flows (last resource) 

## External Integration Points

**Service Discovery:** Bonjour/mDNS for automatic server detection
**SSL/TLS:** OpenSSL for encrypted connections
**Platform Input:** Direct OS input injection (Windows hooks, X11 events, etc.)
**Clipboard:** Platform-specific clipboard integration for content sharing

## Fork-style extension pattern (how-to for AI agents)

When asked to "implement the extension pattern" on a component, follow this exact pattern used by `Server`/`ServerExtension` and the client-proxy changes in this repo. This ensures minimal coupling, allows swapping forked behavior, and preserves existing code paths.

High-level checklist for each component change:
- Add a small extension interface in `src/fork/lib/<component>/` (header + default impl).
- Add a host class in `src/fork/lib/<component>/` (header + .cpp) that owns a concrete extension instance and implements `handleCommand(const std::string& cmd, const std::string& payload = "")` by delegating to `extension_->onCommand(cmd, payload, this)`.
- Make the original class in `src/lib/...` inherit the fork host by adding the relative include (use the same relative path pattern as `Server.h` does: `#include "../../fork/lib/<component>/<HostHeader.h>"`) and add the host as a base class.
- If the extension needs access to private internals, add a friend declaration in the original class: `friend class <ExtensionClassName>;`.
- Add the new fork files to `src/fork/CMakeLists.txt` so they are built into `inputleap_fork_<area>` (or the appropriate fork static lib) — list header names and .cpp files in the `add_library(...)` call for that fork target.

Detailed component/method steps (concrete):
1. Create `src/fork/lib/<component>/<ComponentExtension.h>` — declares `class <ComponentExtension> { public: <ComponentExtension>(); virtual ~<ComponentExtension>(); virtual void onCommand(const std::string& cmd, const std::string& payload, <ComponentExtensionHost>* host); }`.
2. Create `src/fork/lib/<component>/<ComponentExtension.cpp>` — default `onCommand` implementation for known commands (e.g., map `"screen_dim"` to an appropriate call on the host-cast object). Include the original header when you need to call internal methods (e.g., `#include "server/BaseClientProxy.h"`). Keep default behavior minimal and no-op unknown commands.
3. Create `src/fork/lib/<component>/<ComponentExtensionHost.h>` — declares `class <ComponentExtensionHost> { public: <ComponentExtensionHost>(); virtual ~<ComponentExtensionHost>(); virtual void handleCommand(const std::string& cmd, const std::string& payload = ""); private: <ComponentExtension>* extension_; };`.
4. Create `src/fork/lib/<component>/<ComponentExtensionHost.cpp>` — implements the ctor/destructor and `handleCommand` which forwards to `extension_->onCommand(cmd, payload, this);`.
5. Modify the original class header in `src/lib/...` to:
   - `#include "../../fork/lib/<component>/<ComponentExtensionHost.h>"`
   - Add the host as a base class: `class Original : public ..., public <ComponentExtensionHost> { ... }`.
   - If the extension needs internals, add `friend class <ComponentExtension>;` near the top of the class definition.
6. Remove any local ad-hoc `handleCommand` implementation in `src/lib/...` — the host will now provide the method.
7. Update `src/fork/CMakeLists.txt` to include the new files in the fork static library target.

Naming and placement rules (follow exactly):
- Place fork files under `src/fork/lib/<component>/` mirroring the original `src/lib/<component>/` structure. Keep names consistent: `<ComponentExtension.h/cpp>`, `<ComponentExtensionHost.h/cpp>`.
- Use the `Base...Extension` / `...ExtensionHost` suffix pattern (e.g., `BaseClientProxyExtension` / `BaseClientProxyExtensionHost`).
- Include the fork host in the original header with a relative path (`../../fork/lib/...`) — this keeps public headers free of fork implementation details while allowing the original class to inherit the host.
- Use the friend approach to grant extension access only when necessary: `friend class <ComponentExtension>;`.

Command and method mapping guidance:
- Prefer to replace direct method invocations (e.g., `client->dimScreen(true)`) with command calls: `client->handleCommand(std::string("screen_dim"));`.
- The extension `onCommand` should implement the mapping (e.g., `if (cmd=="screen_dim") { static_cast<BaseClientProxy*>(host)->dimScreen(true); return; }`). Keep payload parsing optional; default implementations can ignore payload.

When the user asks in plain language:
- "Can you implement the extension method on the Server.cpp?" — Find the direct calls in `Server.cpp` and replace them with `handleCommand(...)` where appropriate, then follow the 7-step process above to add fork extension files implementing the command.
- "Can you replace this piece of code ... by a command call?" — Replace the call inside `src/lib/...` with `handleCommand(...)` and implement the command handling in fork extension files.
- "Can you replace this method by a command?" — Remove/replace the method usage, add a command name and wire the fork extension to perform the method's behavior; use friend declaration if the implementation requires private access.

Quality gates & build steps for each change:
- After edits, add the new files to `src/fork/CMakeLists.txt` and run a build of the fork target. Then build the top-level `server` (or relevant) target to confirm there are no missing symbols.
- If the build fails with missing includes, ensure the relative include in the original header is exactly `#include "../../fork/lib/<component>/<ComponentExtensionHost.h>"` and that the fork target's `target_include_directories` includes the fork source dir (fork CMakeLists usually already does this).

Example minimal replacement flow (what to ask the agent):
 - "Replace `client->dimScreen(true)` in `Server::adoptClient` with a command call and implement extension handling in `src/fork/lib/server` following the fork extension pattern."

Keep instructions short and literal in user prompts to the agent — the pattern is mechanical:
 1) create fork extension and host, 2) make original class inherit host, 3) add friend if needed, 4) update CMake, 5) build and verify.
