# Agent guide — input-leap (fork)

Primary agent guide for this repo — a *fork* with a deliberate "extension"
discipline. No nested `AGENTS.md`. On-demand docs: `docs/fork-extension-pattern.md`
(the concrete wiring to copy), `docs/releases.md` (dev/stable channels, signing).

## What this is

Input Leap shares one keyboard/mouse (and clipboard) across computers over TCP:
the *server* owns the physical input, each *client* receives injected input.
C++ descended from Synergy → Barrier → Input Leap, with a Qt GUI.

- **Stack:** C++17 (Qt6) / C++14 (Qt5), Objective-C++ on macOS; CMake ≥ 3.21;
  GoogleTest/GoogleMock. Deps: Qt, OpenSSL ≥ 1.1.1, Bonjour/Avahi mDNS, X11 /
  libei, Cocoa/Carbon, Win32. Submodules in `ext/`: `gtest`, `gulrak-filesystem`.
- **Platforms:** Windows 10/11, macOS 10.12+, Linux (X11, Wayland via libei),
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
  `master`, then `git rebase master fork` and force-push (`rerere.enabled` is
  set in this clone so recurring hook conflicts auto-resolve).

The fork stays **rebase-friendly against upstream**: new code lives in
`src/fork/`, edits to `src/lib/` are a tiny mechanical hook (convention 1).
**Preserve this discipline** so the next rebase is painless.

## How to build / run / test

The real toolchain is plain CMake. Two convenience wrappers exist:
`clean_build.sh` (POSIX) and `build.ps1` (Windows).

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

> macOS: `ld: library 'c++' not found` → reconfigure with
> `-DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"`; with `-DINPUTLEAP_BUILD_GUI=OFF`
> build explicit targets (`--target input-leaps input-leapc unittests integtests`)
> because the default bundle target needs the GUI binary.

Outputs land in `build/bin/` (executables) and `build/lib/`. The four binaries:

| Binary | What | Built when |
|---|---|---|
| `input-leaps` | server daemon (`src/server`) | always |
| `input-leapc` | client daemon (`src/client`) | always |
| `input-leap` | Qt GUI (`src/gui`) | `INPUTLEAP_BUILD_GUI=ON` (default) |
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
(`src/test/{unittests,integtests,mock}`) and the GUI's `guiunittests`.

```bash
ctest --test-dir build --verbose   # what CI runs; or ./build/bin/unittests, ./build/bin/integtests
```

`integtests` touches real platform/network/IPC paths and can be flaky headless
(upstream runs it under `xvfb`); prefer `unittests` for fast iteration.

### Useful CMake options (defaults in parens)

`INPUTLEAP_BUILD_GUI` (ON) · `INPUTLEAP_BUILD_TESTS` (ON) · `INPUTLEAP_BUILD_X11`
(ON) · `INPUTLEAP_BUILD_LIBEI` (OFF) · `QT_DEFAULT_MAJOR_VERSION` (6). Linux
needs at least one of X11 / libei.

## Quality gate

1. **Builds clean** on the platform(s) you touched — and ideally the others (a
   `#if`-guarded edit can break a platform you didn't test). CI builds Linux
   (gcc + clang, several Ubuntu/Debian, libei variants), macOS, Windows.
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
   `src/fork/lib/<component>/<Name>Extension.{h,cpp,mm}`; the upstream class
   inherits its extension and `friend`s it (always via a relative include into
   `src/fork/lib/<component>/`, never the public include path), or calls a `fork_*` free-function
   hook where inheritance doesn't fit. Edits to `src/lib/**` and the
   `CMakeLists.txt` files are limited to that mechanical hook. Fork sources are
   listed in `src/fork/CMakeLists.txt` (target `inputleap_fork_server`,
   platform-gated); GUI fork code is the exception — `src/fork/gui/` is compiled
   into the gui target for AUTOMOC. Fork-only logging is `FORK_LOG(...)`
   (`src/fork/lib/base/LogExtension.h` → `inputleap_fork_debug.log` in the temp
   dir). Wiring examples: `docs/fork-extension-pattern.md`.
2. **Each `*Extension` class has exactly one inheritor** — its host recovers
   itself via `static_cast<Host*>(this)` (UB with any other inheritor). Never
   reuse an extension class.
3. **Fork wire messages assume fork builds on both ends** (`kMsgCDimScreen`,
   `kMsgDUndimRequest`, `kMsgCLockState` are sent without capability
   negotiation; a stock client errors out). Fine for this fleet; a new fork
   message inherits the constraint.
4. **Code style** (`.editorconfig`): 4-space indent, LF, UTF-8, trimmed
   whitespace, final newline. Some `src/lib/` files use tabs — match the file
   you're editing. Prefer RAII / smart pointers; C++17 is fine under Qt6.
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
7. **No reformatting unrelated code.** A drive-by reformat balloons the upstream
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
├── docs/                       # agent knowledge: fork-extension-pattern.md, releases.md
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
    │   ├── lib/{base,client,inputleap,platform,server}/  # *Extension.{h,cpp,mm}, QuitReason, LogExtension
    │   └── gui/                 # UpdateChecker, NotificationRouter, OverlayNotification (built into the gui target)
    └── test/{unittests,integtests,mock,global}
```

## Ripple awareness

None — this repo shares no contract with a sibling repo (the only "contract" is
the fork's own wire messages, intra-repo, see convention 3).

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

**An upstream-shaped change** (a fix you'd send upstream): edit `src/lib/**` in
upstream style, add a `doc/newsfragments/*.bugfix`, base it on `master`.

## When stuck

- **Upstream project / wiki / config syntax:** github.com/input-leap/input-leap
  and its wiki; config examples in `doc/`.
- **Exact build/test invocations per platform:** `.github/workflows/builds.yml`
  is the source of truth (Linux/macOS/Windows + Flatpak); upstream release flow
  in `RELEASING.md`; the fork's own release pipeline in
  `.github/workflows/release.yml` + `docs/releases.md`.
- **The fork pattern in detail:** `docs/fork-extension-pattern.md` and the
  existing `src/fork/lib/**` files.
- **Release notes:** `doc/newsfragments/README.md`.
```
