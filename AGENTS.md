# Agent guide — input-leap (fork)

Tool-agnostic guide for coding agents (Cursor, Claude Code, Copilot, Codex, …).
Read this first; it orients you to the toolchain **and** to the fact that this
is a *fork* with a deliberate "extension" discipline. Loading is path-based and
nearest-first; this root file is repo-wide orientation.

There are **no nested `AGENTS.md` files** in this repo — this single root file is
the whole guide. (The `src/fork/` tree and `.github/copilot-instructions.md`
carry the fork's own conventions, but neither is an `AGENTS.md`.)

## What this is

Input Leap is a cross-platform KVM-over-network utility: it shares one
keyboard/mouse (and the clipboard) across multiple computers over TCP. A
**client/server** model — the *server* owns the physical keyboard/mouse, each
*client* receives injected input. It's a C++ codebase descended from
Synergy → Barrier → Input Leap, with a Qt GUI.

- **Language/stack:** C++ (C++17 with Qt6, C++14 with Qt5), some Objective-C++
  (`.mm`) on macOS. Build system is **CMake** (≥ 3.21). GUI is **Qt** (6.2+
  preferred, 5.9+ supported). Tests use **GoogleTest/GoogleMock**.
- **Key deps:** Qt (GUI), OpenSSL ≥ 1.1.1 (TLS), Bonjour/Avahi mDNS (service
  discovery), X11 / libei (Linux input), Cocoa/Carbon (macOS), Win32 (Windows).
  Two git submodules in `ext/`: `gtest` (Google Test, repo `googletest`) and
  `gulrak-filesystem`.
- **Platforms:** Windows 10/11, macOS 10.12+, Linux (X11 and Wayland-via-libei),
  FreeBSD, OpenBSD.

### This is a FORK — fork discipline is the #1 rule

This repo (`nantomarioni/input-leap`) is a fork of upstream
`input-leap/input-leap`. The default branch is **`fork`**.

- **`master`** tracks upstream as a clean mirror (no local changes). The
  `upstream` remote points at `input-leap/input-leap`; local `master` tracks
  `upstream/master`.
- **`fork`** is `master` + a small linear stack of commits: the base
  `feat: Add dimming to inactive screens` commit (the entire `src/fork/` tree +
  the screen-dimming feature) and one commit per addition since (AGENTS.md,
  future features). To pick up upstream: `git fetch upstream`, fast-forward
  `master`, then `git rebase master fork` and force-push. `rerere.enabled` is
  set in this clone so recurring hook conflicts auto-resolve on later rebases.
  Last rebased onto upstream: Dec 2025 tip (`34a34fb2`), conflict-free.

The fork is designed to stay **rebase-friendly against upstream**: almost all
new code lives in a self-contained `src/fork/` tree, and edits to upstream files
under `src/lib/` are kept to a tiny, mechanical minimum (see "The fork extension
pattern" below). **Preserve this discipline.** Don't scatter feature code across
`src/lib/`; follow upstream conventions; keep the upstream diff small so the next
rebase is painless.

## How to build / run / test

The real toolchain is plain CMake. Two convenience wrappers exist:
`clean_build.sh` (POSIX) and `build.ps1` (Windows). The `.github/` copilot file
references a `build_env.ps1` that **does not exist in the repo** — use `build.ps1`
or raw CMake.

### POSIX (Linux / macOS) — the canonical path

```bash
git submodule update --init --recursive    # ext/gtest, ext/gulrak-filesystem

# One-shot clean build (Debug, prefers Ninja, handles macOS sysroot):
./clean_build.sh

# …or configure + build by hand (mirrors CI):
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build
cmake --build build --parallel
```

`clean_build.sh` honours env overrides: `B_BUILD_TYPE` (default `Debug`),
`B_BUILD_DIR` (default `build`), `B_CMAKE_FLAGS`, `B_CMAKE`.

> macOS gotchas: if linking fails with `ld: library 'c++' not found`, the
> sysroot didn't get picked up — reconfigure with
> `-DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"`. And with
> `-DINPUTLEAP_BUILD_GUI=OFF`, the default (bundle) target fails because
> `InputLeap_MacOS` needs the GUI binary — build explicit targets instead:
> `cmake --build build --target input-leaps input-leapc unittests integtests`.

Outputs land in `build/bin/` (executables) and `build/lib/`. The four binaries:

| Binary | What | Built when |
|---|---|---|
| `input-leaps` | server daemon (`src/server`) | always |
| `input-leapc` | client daemon (`src/client`) | always |
| `input-leap` | Qt GUI (`src/gui`) | `INPUTLEAP_BUILD_GUI=ON` (default) |
| `input-leapd` | Windows service wrapper (`src/daemon`) | Windows only |

Run a daemon directly, e.g. `./build/bin/input-leaps -f` (foreground) or
`input-leapc -f <server-ip>`. The GUI launches/configures the daemons for you.
Config file examples are in `doc/input-leap.conf.example*`; man pages in
`doc/input-leapc.1` / `input-leaps.1`.

### Windows

```powershell
.\build.ps1 build          # configure + build (Debug)
.\build.ps1 rebuild        # fullclean + build
.\build.ps1 build -Config Release
.\run.ps1                  # launch the built app (see run.ps1 for -Server/-Client)
```

`build.ps1` expects **`QT_ROOT`** (Qt MSVC kit) and **`BONJOUR_SDK_HOME`** set —
the script hard-codes machine-local defaults near the top; **override them for
your machine, don't commit your paths.**

### Tests (GoogleTest)

Tests build by default (`INPUTLEAP_BUILD_TESTS=ON`) into two binaries:
`unittests` and `integtests` (sources in `src/test/unittests`,
`src/test/integtests`, mocks in `src/test/mock`). The GUI has its own
`guiunittests`. Run via CTest (what CI does) or directly:

```bash
ctest --test-dir build --verbose          # runs the registered test targets
# or:
./build/bin/unittests
./build/bin/integtests
```

> Note: `integtests` exercises real platform/network/IPC paths and can be flaky
> in headless CI; upstream runs them under `xvfb` on Linux. Prefer `unittests`
> for fast local iteration.

### Useful CMake options (defaults in parens)

`INPUTLEAP_BUILD_GUI` (ON) · `INPUTLEAP_BUILD_TESTS` (ON) ·
`INPUTLEAP_BUILD_X11` (ON) · `INPUTLEAP_BUILD_LIBEI` (OFF, Wayland) ·
`INPUTLEAP_USE_EXTERNAL_GTEST` (OFF) ·
`INPUTLEAP_BUILD_GULRAK_FILESYSTEM` (OFF) · `QT_DEFAULT_MAJOR_VERSION` (6).
On Linux you must have **at least one** of X11 or libei enabled.

### Fork releases & updates

`.github/workflows/release.yml` builds macOS **arm64 only** (no Intel — the
fleet is Windows host + Apple Silicon satellites) and a Windows Inno installer.
Two channels:

- **Dev (default):** every push to `fork` rebuilds and force-updates the
  rolling `latest-build` prerelease with fresh installers. Versioning is
  commit-based (`INPUTLEAP_VERSION_DESC=git` → `3.0.3-git-<date>-<hash>`), so
  the version string in the app/dmg identifies the exact commit.
- **Stable (optional):** pushing a `fork-v*` tag creates a permanent Release —
  used only to bless known-good builds.

The macOS bundle is **codesigned with a stable self-signed identity**
("InputLeap Fork", secrets `MACOS_CERT_P12` / `MACOS_CERT_P12_PASSWORD`) so TCC
permissions (Accessibility / Input Monitoring) survive updates; the dmg is
repackaged from the signed app in CI. If the secrets are missing the pipeline
warns and ships unsigned. Cert material lives outside the repo
(`~/.inputleap-fork-signing/` on the authoring machine) — never commit it.

## Quality gate (before declaring a change done)

1. **Builds clean** on the platform(s) your change touches — and ideally still
   builds on the others (this is cross-platform code; a `#if`-guarded edit can
   break a platform you didn't test). CI builds Linux (gcc + clang, Ubuntu
   20.04→24.10, Debian, libei variants), macOS, and Windows.
2. **Tests pass:** `ctest --test-dir build` (CI runs this on non-Release builds).
3. **CI builds with `-Werror`** on Debug (`-DCMAKE_CXX_FLAGS_DEBUG="-g -Werror"`,
   plus `-Wall -Wextra`). Treat warnings as errors locally too.
4. **No new upstream-file churn** beyond the minimal extension hooks (see below).
5. **Release note** for user-visible changes: add a `doc/newsfragments/*.feature`
   (or `.bugfix`, etc.) towncrier fragment — see `doc/newsfragments/README.md`.

There is **no clang-format or clang-tidy config** in the repo. Style is enforced
by convention + `.editorconfig` only.

## Non-negotiables / conventions

1. **Fork discipline (the prime directive).** New feature code goes in
   `src/fork/`. Edits to `src/lib/**`, root `CMakeLists.txt`, and the per-target
   `CMakeLists.txt` are limited to the mechanical hook described below. Keep the
   diff against upstream small and rebase-friendly.
2. **Code style** (from `.editorconfig` + the existing code): 4-space indent, no
   tabs, LF line endings, UTF-8, trim trailing whitespace, final newline.
   Note the existing `src/lib/` code is mixed-era and some files use tabs — match
   the file you're editing; new fork files use the `.editorconfig` (4 spaces).
   Prefer RAII / smart pointers. C++17 features are fine under Qt6.
3. **Platform abstraction.** OS-specific code is selected by preprocessor macros
   and file-name prefix — never `#ifdef _WIN32` ad hoc:
   - Macros: `WINAPI_MSWINDOWS`, `WINAPI_XWINDOWS`, `WINAPI_LIBEI`,
     `WINAPI_CARBON`; `SYSAPI_WIN32` / `SYSAPI_UNIX` (set in root `CMakeLists.txt`).
   - File prefixes: `MSWindows*` (Win), `OSX*` (mac), `XWindows*` (X11),
     `Ei*` (libei/Wayland). The cross-platform interface lives alongside (e.g.
     `IPlatformScreen.h`, `PlatformScreen`); each OS provides a concrete impl.
4. **Client/server architecture.** `src/lib/server` (server side, `Server`,
   `ClientProxy*`, `Config`), `src/lib/client` (`Client`, `ServerProxy`),
   `src/lib/inputleap` (shared domain: `Screen`, protocol/option types),
   `src/lib/net` (TCP + TLS), `src/lib/platform` (input injection/capture). The
   wire protocol is versioned (`ClientProxy1_x`); don't break it casually.
5. **No reformatting unrelated code.** A drive-by reformat balloons the upstream
   diff — exactly what the fork is structured to avoid.

## Where things live

```
.
├── CMakeLists.txt              # top-level: options, platform macros, deps; adds src/fork then src
├── clean_build.sh              # POSIX build wrapper (clean_build.ps1 is the Windows twin)
├── build.ps1 / run.ps1         # Windows: build / launch the built app
├── towncrier.toml              # towncrier config (release notes assembled from doc/newsfragments)
├── ext/                        # submodules: gtest (Google Test), gulrak-filesystem
├── cmake/                      # Version.cmake, Package.cmake, gtest.cmake, uninstall
├── dist/                       # packaging: debian, rpm, flatpak, snap, wix, inno, macos
├── doc/                        # man pages, conf examples, newsfragments (towncrier), release notes
├── res/                        # icons, desktop/appdata, platform resources
└── src/
    ├── client/                 # input-leapc executable (thin main)
    ├── server/                 # input-leaps executable (thin main)
    ├── daemon/                 # input-leapd Windows service (Win only)
    ├── gui/                    # Qt GUI app `input-leap` (src/, res/, test/)
    ├── lib/                    # UPSTREAM core libraries (edit minimally)
    │   ├── arch/{unix,win32}   # arch abstraction
    │   ├── base/               # logging, utilities
    │   ├── client/ server/     # client- and server-side logic + ClientProxy versions
    │   ├── inputleap/          # shared domain: Screen, protocol_types, option_types
    │   ├── net/                # TCP + SSL/TLS
    │   ├── platform/           # MSWindows*/OSX*/XWindows*/Ei* screen + clipboard impls
    │   ├── io/ ipc/ mt/ common/
    ├── fork/                   # ALL fork-only code lives here (see below)
    │   ├── CMakeLists.txt       # builds the inputleap_fork_server static lib
    │   └── lib/{base,client,inputleap,platform,server}/  # *Extension.{h,cpp,mm}
    └── test/{unittests,integtests,mock,global}
```

## The fork extension pattern (how local changes are isolated)

The fork keeps upstream files nearly untouched by putting behavior in
`src/fork/lib/<component>/<Name>Extension.{h,cpp}` and attaching it to the
upstream class. The actual wiring used in this repo (study it before adding
more):

- **Inheritance + friend.** The upstream class inherits its fork extension and
  friends it. E.g. `src/lib/server/Server.h`:
  ```cpp
  #include "../../fork/lib/server/ServerExtension.h"
  class Server : public INode, public EventTarget, public ServerExtension {
      friend class ServerExtension;   // grants access to internals
      ...
  ```
  Same shape for `Screen` (`ScreenExtension`), `Client`, `ServerProxy`,
  `BaseClientProxy`, `PrimaryClient`, `Config`, the platform `*Screen` classes,
  and `IPlatformScreen` / `PlatformScreenLoggingWrapper`.
- **Free-function hooks** where inheritance doesn't fit. E.g. `Config.cpp` calls
  `fork_readSectionOptions(...)`, `fork_getOptionName(...)`,
  `fork_getOptionValue(...)` implemented in the fork `ConfigExtension` /
  `option_types_extension`. Same shape for quit tracing: `EventQueue.cpp`'s
  signal handler calls `fork::noteSignalQuit()` and `ClientApp::mainLoop`
  installs `fork::installExitTracer(...)` / marks `fork::noteCleanTeardown()`
  (implemented in `src/fork/lib/base/QuitReason.cpp`) so every process-exit
  path leaves a trace in the fork log (`inputleap_fork_debug.log` in the
  temp dir; a start line with no matching exit line = killed by
  SIGKILL/default signal action/crash).
- Fork-only logging uses the `FORK_LOG(...)` macro
  (`src/fork/lib/base/LogExtension.h`) — timestamped + PID-prefixed lines in
  the fork log, independent of the upstream `Log` verbosity. Fork code may
  also use the upstream `LOG_*` macros (include `base/Log.h`) when a message
  belongs in the main log (e.g. the reconnect-flap warning in
  `ServerExtension`).
- The relative include is always `#include "../../fork/lib/<component>/<Hdr>.h"`
  — this keeps fork details out of the public include path while letting the
  upstream class inherit/call the extension.
- All fork sources are listed in `src/fork/CMakeLists.txt` (target
  `inputleap_fork_server`, platform-gated via `if(WIN32/APPLE/LINUX)`), and the
  per-target `CMakeLists.txt` under `src/{client,server,daemon}` and
  `src/lib/platform` + the test CMakeLists link/expose it.
- **GUI fork code is the exception**: Qt sources live in `src/fork/gui/`
  (e.g. `UpdateChecker.{h,cpp}`) and are compiled *into the gui target*
  (listed directly in `src/gui/CMakeLists.txt` so AUTOMOC picks them up),
  with a one-line hook in `MainWindow.cpp`. The update checker polls the
  `latest-build` GitHub Release and compares its target commit against the
  hash inside `INPUTLEAP_VERSION` (menu: Help → "Check for Updates...", plus
  a rate-limited daily startup check).

**Invariants the pattern relies on (enforced only by convention):**

- **Each `*Extension` class has exactly one inheritor** — its upstream host
  class. The extensions recover their host via
  `static_cast<Host*>(const_cast<XxxExtension*>(this))` (see
  `ServerExtension::host()`), which is undefined behavior if any other class
  ever inherits the extension. Never reuse an extension class.
- **Fork wire messages assume fork builds on both ends.** `kMsgCDimScreen` /
  `kMsgDUndimRequest` are sent unconditionally (no capability negotiation);
  a stock upstream client receiving one will error out. Acceptable for this
  fleet (all machines run the fork) — but any new fork message inherits the
  same constraint, and mixed-fleet support would require a handshake guard in
  `BaseClientProxyExtension::fork_dimScreen` and friends.

> There's a much longer, prescriptive version of this pattern (with a strict
> "no code outside `src/fork/` except the hook" rule and a step-by-step recipe)
> in `.github/copilot-instructions.md`. It is the fork author's intended
> workflow — follow it for new extension work. Ignore its references to
> `build_env.ps1` (use `build.ps1`) and `MasterConfigDialog` (not present).

## Making a change — walkthrough

**Adding/altering fork behavior (the common case):**
1. Decide the upstream class to extend (e.g. `Server`, `Screen`, a platform
   `*Screen`). Find or create `src/fork/lib/<component>/<Name>Extension.{h,cpp}`.
2. Put the logic in the extension. Need private access? `friend class
   <Name>Extension;` already exists on the upstream class (add it if not).
3. Need a new fork source? List it in `src/fork/CMakeLists.txt` (in the right
   platform `if()` block for `*Windows*`/`OSX*`/`XWindows*` files).
4. For platform behavior, implement per-OS (`MSWindowsScreenExtension.cpp`,
   `OSXScreenExtension.mm`, `XWindowsScreenExtension.cpp`) — mirror the existing
   screen-dimming feature, which is implemented across all three.
5. Build the relevant target, then `ctest`. Keep the `src/lib/**` diff to the
   minimal hook.

**A genuinely upstream-shaped change** (bug fix you'd send upstream): edit the
real `src/lib/**` file in upstream style, add a `doc/newsfragments/*.bugfix`,
and ideally base it on `master` so it can be PR'd upstream and cleanly rebased.

## When stuck

- **Upstream project / wiki / config syntax:** the upstream repo
  `https://github.com/input-leap/input-leap` and its wiki (README links to it);
  config examples in `doc/`.
- **Exact build/test invocations per platform:** `.github/workflows/builds.yml`
  is the source of truth (Linux/macOS/Windows + Flatpak); upstream release flow
  in `RELEASING.md`; the fork's own release pipeline in
  `.github/workflows/release.yml` (see "Fork releases & updates" above).
- **The fork pattern in detail:** `.github/copilot-instructions.md` (caveats
  above) and the existing `src/fork/lib/**` files.
- **Release notes:** `doc/newsfragments/README.md` (towncrier).
```
