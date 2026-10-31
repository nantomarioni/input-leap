# Agent guide — input-leap (fork)

Primary agent guide for this repo — a fork of the archived upstream Input Leap.
No nested `AGENTS.md`. On-demand docs: `docs/fork-extension-pattern.md` (how
fork features attach to core classes), `docs/releases.md` (dev/stable channels,
signing), `docs/ui-redesign.md` (the Qt Quick GUI rewrite: plan, mocks, local
workflow).

## What this is

Input Leap shares one keyboard/mouse (and clipboard) across computers over TCP:
the *server* owns the physical input, each *client* receives injected input.
C++ descended from Synergy → Barrier → Input Leap, with a Qt GUI.

- **Stack:** C++17, Objective-C++ on macOS; CMake ≥ 3.21; GoogleTest/GoogleMock.
  Deps: Qt ≥ 6.8 (Qt Quick, FluentWinUI3 style) for the GUI, OpenSSL ≥ 1.1.1,
  Bonjour/Avahi mDNS, X11 / libei, Cocoa/Carbon, Win32. Submodules in `ext/`: `gtest`, `gulrak-filesystem`.
- **Platforms:** Windows 10/11, macOS 10.12+, Linux (X11, Wayland via libei),
  FreeBSD, OpenBSD. This fleet: a Windows server, Apple Silicon clients.

### Fork of an archived upstream

`nantomarioni/input-leap` (default branch **`fork`**) forks
`input-leap/input-leap`, which is **archived (read-only; last push Dec 2025)**.
`master` is frozen at upstream's final commit (`34a34fb2`); `fork` is that plus
this fork's linear commits. There is nothing left to rebase on, so fix core code
in place.

The live Synergy descendant is **Deskflow** (github.com/deskflow/deskflow); it
shares this core and protocol, so its security advisories and Windows/macOS
fixes often apply here — check and port them.

## How to build / run / test

The real toolchain is plain CMake. Two convenience wrappers exist:
`clean_build.sh` (POSIX) and `build.ps1` (Windows).

### POSIX (Linux / macOS) — the canonical path

```bash
git submodule update --init --recursive    # ext/gtest, ext/gulrak-filesystem

# One-shot clean build (Debug, prefers Ninja, handles macOS sysroot):
./clean_build.sh

# …or configure + build by hand (add -DCMAKE_UNITY_BUILD=1 to match CI):
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build
cmake --build build --parallel
```

`clean_build.sh` honours env overrides: `B_BUILD_TYPE` (default `Debug`),
`B_BUILD_DIR` (default `build`), `B_CMAKE_FLAGS`, `B_CMAKE`.

> macOS: `ld: library 'c++' not found` → reconfigure with
> `-DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"`; Homebrew deps need
> `-DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt6;/opt/homebrew/opt/openssl@3"`. With
> `-DINPUTLEAP_BUILD_GUI=OFF` build explicit targets (`--target input-leaps
> input-leapc unittests integtests`); the default bundle target needs the GUI.

Outputs land in `build/bin/` (executables) and `build/lib/`. The four binaries:

| Binary | What | Built when |
|---|---|---|
| `input-leaps` | server daemon (`src/server`) | always |
| `input-leapc` | client daemon (`src/client`) | always |
| `input-leap` | Qt Quick GUI (`src/gui`: `app/` C++ layer, `qml/` window) | `INPUTLEAP_BUILD_GUI=ON` (default) |
| `input-leapd` | Windows service wrapper (`src/daemon`) | Windows only |

Run a daemon directly: `./build/bin/input-leaps -f` / `input-leapc -f <server-ip>`;
the GUI launches/configures them. Config examples: `doc/input-leap.conf.example*`.

### Windows

```powershell
.\build.ps1 build          # configure + build (Debug)
.\build.ps1 rebuild        # fullclean + build
.\build.ps1 build -Config Release
.\run.ps1                  # launch the built app (see run.ps1 for -Server/-Client)
```

`build.ps1` expects `QT_ROOT` (Qt MSVC kit) and `BONJOUR_SDK_HOME`; it
hard-codes machine-local defaults near the top — **override, don't commit yours.**

### Tests (GoogleTest)

Built by default (`INPUTLEAP_BUILD_TESTS=ON`): `unittests`, `integtests`
(`src/test/{unittests,integtests,mock}`) and the GUI's `guiunittests`. Test
sources are globbed: re-run `cmake` after adding a test file.

```bash
ctest --test-dir build --verbose   # or ./build/bin/unittests, ./build/bin/integtests
```

`integtests` uses the real machine: on macOS it writes the system clipboard and
injects keystrokes, so don't run it while using the Mac. Prefer `unittests`.
`guiunittests` opens two TLS sockets on localhost for the settings channel's
loopback tests; it needs no network and no certificate of yours.

### Useful CMake options (defaults in parens)

`INPUTLEAP_BUILD_GUI` (ON; needs Qt ≥ 6.8, no Qt 5) · `INPUTLEAP_BUILD_TESTS`
(ON) · `INPUTLEAP_BUILD_X11` (ON) · `INPUTLEAP_BUILD_LIBEI` (OFF). Linux needs
at least one of X11 / libei. GUI workflow and dev hooks: `docs/ui-redesign.md` §8.

## Quality gate

The fork's only CI is `.github/workflows/release.yml` (every push to `fork`):
Release, `CMAKE_UNITY_BUILD=1` builds for macOS arm64 and Windows. It runs **no
tests**, builds **no Linux**, and doesn't use `-Werror` (`builds.yml` only
triggers on `master`). So before pushing:

1. **Build with `-DCMAKE_UNITY_BUILD=1`** on the platform you touched. A `.mm`
   that imports Foundation/AppKit needs `SKIP_UNITY_BUILD_INCLUSION TRUE`, or it
   lands in a C++-mode unity batch. Windows-only code is first compiled by CI.
2. **No new warnings** in the files you touched. `-Werror` can't be used on
   macOS (upstream `OSX*` files use deprecated AppKit/IOKit APIs).
3. **Tests pass locally:** `ctest --test-dir build`. Known macOS failures, not
   regressions: `SecureUtilsTest` ×2 (libstdc++-specific expectations),
   `OSXClipboardTests.add_*` ×2 (test bug), `OSXKeyStateTests.fakeAndPoll_*` ×3
   (need Accessibility + Input Monitoring).
4. **Release note** for user-visible changes: a `doc/newsfragments/*.feature`
   (or `.bugfix`, `.security`, …) fragment — see `doc/newsfragments/README.md`.

## Non-negotiables / conventions

1. **Where code goes.** Fixes to existing behaviour (bugs, security, Deskflow
   ports) are edited in place in `src/lib/**`. New **fork features** go in
   `src/fork/lib/<component>/<Name>Extension.{h,cpp,mm}`, attached to the core
   class by inheritance + `friend` (via a relative include into
   `src/fork/lib/<component>/`) or a `fork_*` free-function hook; this keeps
   them in one place and portable (e.g. to Deskflow). Fork sources are listed in
   `src/fork/CMakeLists.txt` (target `inputleap_fork_server`, platform-gated).
   The GUI (`src/gui/`) is the fork's own code, not an extension; `src/fork/gui/`
   holds the overlay, notification router and update checker it compiles in. Fork-only logging is `FORK_LOG(...)`
   (`src/fork/lib/base/LogExtension.h` → `inputleap_fork_debug.log` in the temp
   dir). Wiring examples: `docs/fork-extension-pattern.md`.
2. **Each `*Extension` class has exactly one inheritor** — its host recovers
   itself via `static_cast<Host*>(this)` (UB with any other inheritor). Never
   reuse an extension class.
3. **Fork wire messages assume fork builds on both ends** (`kMsgCDimScreen`,
   `kMsgDUndimRequest`, `kMsgCLockState` are sent without capability
   negotiation; a stock client errors out). Fine for this fleet; a new fork
   message inherits the constraint.
4. **Code style** (`.editorconfig` only; no clang-format/clang-tidy): 4-space
   indent, LF, UTF-8, trimmed whitespace, final newline. Some `src/lib/` files
   use tabs — match the file you're editing. Prefer RAII / smart pointers.
5. **Platform abstraction.** OS-specific code is selected by preprocessor macros
   and file-name prefix — never `#ifdef _WIN32` ad hoc:
   - Macros: `WINAPI_MSWINDOWS`, `WINAPI_XWINDOWS`, `WINAPI_LIBEI`,
     `WINAPI_CARBON`; `SYSAPI_WIN32` / `SYSAPI_UNIX` (set in root `CMakeLists.txt`).
   - File prefixes: `MSWindows*` (Win), `OSX*` (mac), `XWindows*` (X11),
     `Ei*` (libei/Wayland). The cross-platform interface lives alongside (e.g.
     `IPlatformScreen.h`, `PlatformScreen`); each OS provides a concrete impl.
6. **Client/server architecture.** `src/lib/server` (server side, `Server`,
   `ClientProxy*`, `Config`), `src/lib/client` (`Client`, `ServerProxy`),
   `src/lib/inputleap` (shared domain: `Screen`, protocol/option types),
   `src/lib/net` (TCP + TLS), `src/lib/platform` (input injection/capture). The
   wire protocol is versioned (`ClientProxy1_x`); don't break it casually.
   Bounds-check all wire input.
7. **No reformatting unrelated code.** It hides the real change and makes
   porting fixes to or from Deskflow harder.

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
├── docs/                       # agent knowledge: fork-extension-pattern.md, releases.md
├── res/                        # icons, desktop/appdata, platform resources
└── src/
    ├── client/ server/         # input-leapc / input-leaps executables (thin mains)
    ├── daemon/                 # input-leapd Windows service (Win only)
    ├── gui/                    # Qt Quick GUI `input-leap`: app/ (C++ layer), qml/ (window), res/ (icons), test/
    ├── lib/                    # core libraries (from upstream; fix in place)
    │   ├── arch/{unix,win32}   # arch abstraction
    │   ├── base/               # logging, utilities
    │   ├── client/ server/     # client- and server-side logic + ClientProxy versions
    │   ├── inputleap/          # shared domain: Screen, protocol_types, option_types
    │   ├── net/                # TCP + SSL/TLS
    │   ├── platform/           # MSWindows*/OSX*/XWindows*/Ei* screen + clipboard impls
    │   ├── io/ ipc/ mt/ common/
    ├── fork/                   # fork features (extension pattern)
    │   ├── CMakeLists.txt       # builds the inputleap_fork_server static lib
    │   ├── lib/{base,client,inputleap,platform,server}/  # *Extension.{h,cpp,mm}, QuitReason, LogExtension
    │   └── gui/                 # UpdateChecker, NotificationRouter, OverlayNotification (compiled into the gui)
    └── test/{unittests,integtests,mock,global}
```

## Ripple awareness

None — this repo shares no contract with a sibling repo (the only "contract" is
the fork's own wire messages, intra-repo, see convention 3).

## Making a change — walkthrough

**A fork feature (new behaviour):**
1. Pick the core class to extend (e.g. `Server`, `Screen`, a platform
   `*Screen`). Find or create `src/fork/lib/<component>/<Name>Extension.{h,cpp}`.
2. Put the logic in the extension. Need private access? `friend class
   <Name>Extension;` already exists on the core class (add it if not).
3. List new sources in `src/fork/CMakeLists.txt` (in the right platform `if()`
   block for `*Windows*`/`OSX*`/`XWindows*` files).
4. For platform behaviour, implement per OS — mirror screen dimming
   (`MSWindowsScreenExtension.cpp`, `OSXScreenExtension.mm`,
   `XWindowsScreenExtension.cpp`).

**A fix (bug, security, Deskflow port):** edit `src/lib/**` in that file's
style, add a test under `src/test/unittests/` where feasible, add a
`.bugfix`/`.security` fragment, and credit the Deskflow commit or CVE in the
commit message. Example: `f5c54fb1`..`689cbd3d`.

Then run the quality gate and push to `fork` (CI publishes `latest-build`).

## When stuck

- **Upstream project / wiki / config syntax:** github.com/input-leap/input-leap
  (archived, still readable) and its wiki; config examples in `doc/`.
- **Fixes and advisories to port:** github.com/deskflow/deskflow — its Security
  tab and release notes.
- **The GUI's design, pages, dev hooks and local workflow:** `docs/ui-redesign.md`.
- **Release pipeline:** `.github/workflows/release.yml` + `docs/releases.md`.
  `builds.yml` / `RELEASING.md` are upstream's (Linux recipes too), reference only.
- **The fork pattern in detail:** `docs/fork-extension-pattern.md` and the
  existing `src/fork/lib/**` files.
- **Release notes:** `doc/newsfragments/README.md`.
