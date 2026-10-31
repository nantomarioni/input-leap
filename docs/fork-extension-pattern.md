# The fork extension pattern — how fork features are isolated

Knowledge file (load on demand). The rule itself is in `AGENTS.md`
("Conventions"); this is the concrete wiring to copy.

Fork *features* live in `src/fork/lib/<component>/<Name>Extension.{h,cpp}` and
attach to the core (upstream-derived) class. The pattern started as a way to
keep rebases on upstream painless; upstream is now archived, so fixes to core
behaviour are edited in place, but features still use this pattern so they
stay in one place and portable (e.g. to Deskflow). The actual wiring used in
this repo (study it before adding more):

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
- **The GUI is not an extension**: `src/gui/` is the fork's own Qt Quick
  app (docs/ui-redesign.md). `src/fork/gui/` keeps the pieces that predate it
  (`UpdateChecker`, `NotificationRouter`, `OverlayNotification`), compiled
  into the gui target and driven from `src/gui/app/AppController.cpp`. The
  update checker polls the `latest-build` GitHub Release and compares its
  target commit against the hash inside `INPUTLEAP_VERSION` (Settings →
  Updates, the tray menu, plus a rate-limited daily startup check).

**Invariants the pattern relies on (enforced only by convention):**

- **Each `*Extension` class has exactly one inheritor** — its upstream host
  class. The extensions recover their host via
  `static_cast<Host*>(const_cast<XxxExtension*>(this))` (see
  `ServerExtension::host()`), which is undefined behavior if any other class
  ever inherits the extension. Never reuse an extension class.
- **Fork wire messages assume fork builds on both ends.** `kMsgCDimScreen` /
  `kMsgDUndimRequest` / `kMsgCLockState` are sent unconditionally (no
  capability negotiation);
  a stock upstream client receiving one will error out. Acceptable for this
  fleet (all machines run the fork) — but any new fork message inherits the
  same constraint, and mixed-fleet support would require a handshake guard in
  `BaseClientProxyExtension::fork_dimScreen` and friends.
