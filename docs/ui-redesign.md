# UI redesign — one window for Input Leap (draft for review)

Status: **proposal, nothing implemented.** Mocks of every screen:
https://claude.ai/artifact/CbVAg6Sr3CyLvoRa8xDvs8 (private canvas; press Play
and use the left rail to move between screens). This file is the written half
of the plan: libraries, code layout, information architecture, milestones and
the decisions still open. Once agreed it becomes the knowledge file the
implementation follows (and gets listed in `AGENTS.md`'s on-demand docs).

## 1. Goals

- **One window** that shows everything: state, who is connected, the layout,
  settings for both roles, security, the processes behind the service, the log.
  No chain of modal dialogs; requests that need a decision (a client wants to
  join, an unknown fingerprint) appear inline and in the tray.
- **The moving parts are visible.** The service is the app plus a core process
  (`input-leaps` or `input-leapc`), plus on Windows the `input-leapd` service
  and Bonjour. Each one gets status and its own actions.
- **Pages show the real network, not ghost configurations.** The Server page
  is the one server's configuration; the Computers page has a tab per machine,
  this one included; a client sees the server it is connected to, read-only,
  instead of a configuration it would use in a role it does not have.
- **Screen layout** editable in the window, drag based, ready for free
  placement later.
- **Modern look**: light/dark following the system, resizable window, keyboard
  navigable, consistent on Windows (the server) and macOS (the clients).
- **The fleet keeps working** through the self-updater: existing settings
  migrate, the generated `.conf` is unchanged, the core binaries are untouched.

Non-goals: changing the core, the daemon or the wire protocol; Linux packaging
(it must still build); new core features. Translations: see §7.

## 2. What we are replacing

Qt Widgets, eleven `.ui` forms, ~13 k lines under `src/gui/`. `MainWindow.cpp`
(1 465 lines) owns the process launch, the Windows IPC, zeroconf, the tray,
fingerprint checks, the Bonjour installer, settings persistence *and* the UI.

| Window today | What it holds |
|---|---|
| Setup wizard | language, server-or-client |
| Main window (fixed size) | two exclusive group boxes (server / client), IPs, config source, server host, auto-config, fingerprint, Start/Stop/Reload |
| Settings dialog | screen name, language, elevate (Win), tray options, address/port, TLS, require client cert, logging |
| Server Configuration dialog | tab Screens (5×3 drag grid), tab Hotkeys/Actions, tab Advanced (switch, options, dead corners) |
| Screen Settings dialog | aliases, modifier remap, dead corners, fixes |
| Hotkey / Action dialogs | key capture, action editor |
| Add Client dialog | "a client wants to connect": left/up/right/down/advanced |
| Fingerprint dialog | trust SHA256 (+SHA1) |
| Log window | plain text, clear/hide |

Pain points this plan removes: the role toggle hides the other role's options;
configuration is spread over four dialogs; connection state comes from string
matching on log lines in `MainWindow::checkConnected`; the window is fixed
size; the processes are invisible; our own options (screen dimming) are not
exposed at all; the "Show Tray Icon upon App Start" checkbox actually means
"start the core automatically"; every request interrupts with a modal.

## 3. What comparable apps do

| App | Toolkit | Layout | Worth borrowing |
|---|---|---|---|
| Deskflow 1.27 (live Synergy descendant) | Qt Widgets 6.7+ | one small main window (name, two radios, host, status bar, dockable log); server config, screen settings, settings still dialogs; fixed 5×3 grid; GUI↔core over IPC | dockable log, status bar, GUI/core IPC |
| Lan Mouse | GTK4 / libadwaita | one scrolling window: General (host, port, fingerprint), Connections (one row per client: toggle, host, port, position), Incoming connections (authorized fingerprints) | inline authorization, everything in one scroll |
| PowerToys Mouse Without Borders | WinUI 3 settings page | key + machine name to connect; Device layout by dragging machine icons (row or 2×2); flat behaviour settings; colour coded connection states | drag layout, flat settings page |
| Synergy 3 | Electron | free placement and resizing of screens on a canvas | the layout canvas direction |

None of them shows the processes behind the service; that page is ours.

## 4. Decisions

### 4.1 Toolkit: Qt 6 Quick (QML) front-end over a C++ application layer

Recommended. The whole value of the rewrite is in the front-end; the C++ that
talks to processes, sockets, Bonjour and OpenSSL stays C++ and stays in this
repo.

- **Qt Quick Controls 2**, style **FluentWinUI3** on every platform (Qt ≥ 6.8;
  one look on the Windows server and the macOS clients, light and dark from
  the system). Our own components for the layout canvas, status pills, process
  cards and the log view. Fallback if FluentWinUI3 misbehaves: the `Basic`
  style with the same theme tokens.
- **`QApplication` stays**, QtWidgets stays linked: the tray icon and menu
  (`QSystemTrayIcon`), native file pickers, and the existing `OverlayNotification`
  (a QWidget HUD) keep working unchanged.
- **Qt version**: CI moves from 6.6 to **6.9.x** (install-qt-action; add
  `qtdeclarative` explicitly to `modules`). Local Homebrew Qt is 6.9.1. The
  Qt 5 code paths (`QT_DEFAULT_MAJOR_VERSION 5`, gulrak filesystem) go away.
- **Deployment**: `qt_add_qml_module` for the UI, `qt_generate_deploy_qml_app_script`
  (or `macdeployqt -qmldir` in `dist/macos/bundle/build_dist.sh.in` and
  `windeployqt --qmldir` in `src/gui/CMakeLists.txt`). The Inno script copies
  the whole install prefix, so the QML plugins ship automatically.

Alternatives considered:

| Option | Why not |
|---|---|
| Rebuild in Qt Widgets with a custom style | same deployment, lowest risk, but "modern" would be a stylesheet over 2008-era controls (Deskflow's path, and it still looks like the screenshot); drag canvas and animations are uphill |
| Slint (C++ API) | nice declarative UI, but GPLv3 or a royalty-free licence that forbids selling; this repo is GPLv2 + OpenSSL exception, so GPLv3 is incompatible; also a new toolchain in CI |
| Tauri / Electron (web UI) | the GUI's real work (spawn + watch processes, Windows service IPC, dns_sd, cert generation via the core's `net` lib, fingerprint DB) would have to be redone in Rust/JS or wrapped in a helper; Electron is 150 MB for a tray app |
| Flutter, Avalonia, MAUI | same problem, different language; no reuse of the C++ |

Gate: milestone M0 is a two-day spike that proves QML + tray + overlay +
deployment on both CI targets before any page is built.

### 4.2 Information architecture

Window: header, left rail, page, footer, log drawer (see the mocks).

- **Header**: logo · role switch (`Server · shares this keyboard & mouse` /
  `Client`) · state pill · Restart · Start/Stop.
- **Rail**: Overview · Layout · Server · Computers · Security · Processes · Settings.
  Version and the "update available" chip live at the bottom of the rail.
- **Footer**: one line of state (`Running as server · 2 of 3 clients
  connected`, last event), the log toggle, the "keeps running in the tray" hint.
- **Log drawer**: bottom of every page, filter by source (App / Core / Service),
  minimum level, search, copy, clear, pop out. Replaces the Log window.

| Page | Contents |
|---|---|
| Overview | inline requests (join / trust / errors); status card (role, uptime, screen name, listening addresses, TLS, config source, cursor); mini layout; Computers table (position, state, address, session, "Send cursor"); "What is running" strip. First run: a "Get started" state here (pick role, confirm name, Start) replaces the wizard |
| Layout | canvas with one tile per display, grouped by computer, drag to arrange, snap to grid; selected display's size and role plus the computer's settings in a side panel (name, aliases, dead corners, modifiers, fixes); detected-but-unplaced clients as draggable chips with "place right of…" shortcuts; Apply & restart |
| Server | the one server's configuration: source (generated vs file); Listening; Switching; Behaviour; Inactive screens (dimming, brightness, overlay hints); Hotkeys & actions with an inline editor. On the server: always available and editable, running or not. On a client: a read-only mirror of the connected server (phase C, mock 8); before that, and while disconnected, an empty state saying "connect to a server to see its configuration" |
| Computers | one tab per computer in the layout, this one included (the server too), with role, live state, address, displays, build, position and trust. Two groups per tab: **held by the server** (aliases, modifiers, dead corners, fixes, dimming), editable on the server; **on that computer** (screen name, server to connect to with the discovered list, discovery toggle, scroll step, overlay hints, log level, launch at login, its core process), editable on the machine itself, mirrored read-only elsewhere (phase C), writable remotely later (phase D). Tabs show only the name and a state dot (the selected tab's header spells out role and reason), sit on one row and scroll horizontally when they do not fit. Never unavailable: offline it shows this computer's own tab |
| Security | pending trust request inline (randomart + SHA256 + compare instructions); this computer's identity (TLS toggle, fingerprint, copy, regenerate); policy (require client certs); trusted peers table with Forget |
| Processes | chain diagram App → Service → Core → clients, Discovery; one card each: App, Core (PID, uptime, restarts, level, command line, Restart/Stop/Copy), Service (Windows: state, IPC, elevation), Discovery (advertising/browsing); log drawer open by default here |
| Settings | General (screen name, appearance, language); Startup & window; Notifications (overlay, system notifications); Logging; Updates (channel, daily check, install); Windows service (elevation); Reset |

Tray menu: Start/Stop, Restart, Show window, Show log, Check for updates, Quit.
System notifications (tray balloon / macOS notification) for join and trust
requests when the window is hidden; clicking opens the right page.

Role semantics: `role` is a plain setting. The role switch in the header is
enabled only while the core is stopped: stop, switch, start. While running it
is disabled with a hint saying so; it never restarts the core by itself. A
client's own settings (server to connect to, discovery, scroll step…) live on
its tab of the Computers page, so switching to client never reveals a page
that was hidden before. Discovery registers the right zeroconf record for the
role.

### 4.3 Code layout

```
src/gui/
├── CMakeLists.txt            # qt_add_qml_module(InputLeapUi) + the app layer; deploy script
├── main.cpp                  # QApplication, single instance, tray, QQmlApplicationEngine
├── app/                      # C++ application layer: QObject + QtCore/QtNetwork only (no widgets)
│   ├── AppController         # facade exposed to QML: role, runState, connection, screens, requests, actions
│   ├── AppSettings           # typed QSettings wrapper; one place for every key; legacy-key migration
│   ├── ServerLayout          # screens, links, per-screen options; writes the .conf (today's ServerConfig/Screen)
│   ├── ServerOptions, Hotkeys # the "section: options" values and hotkey/action model
│   ├── CoreProcess           # launches/stops the core; desktop mode (QProcess) and service mode (IPC) behind one interface
│   ├── CoreEventParser       # log line -> typed events (Started, Connected(name), Disconnected, Fingerprint(sha1, sha256), Dimmed(name), CursorLocked…); the ONLY string matching
│   ├── DaemonService         # Windows: input-leapd status, IPC client (today's IpcClient + isServiceRunning)
│   ├── Discovery             # zeroconf browse/register (today's Zeroconf*)
│   ├── TlsIdentity           # cert generation, fingerprints, trusted-peer DB (SslCertificate + FingerprintDatabase glue)
│   ├── LogModel              # QAbstractListModel of lines with source/level; file sink
│   ├── NetworkInfo           # local addresses, suggested one
│   ├── ProcessInfo           # PID/uptime/command line for the cards
│   ├── ComputersModel        # one entry per computer: layout data + live state + mirrored settings
│   └── ConfigChannel         # phase C: GUI-to-GUI mirror of settings over TLS (§4.7), read-only first
├── qml/
│   ├── Main.qml              # ApplicationWindow: header, rail, StackView of pages, footer, log drawer
│   ├── pages/                # Overview, Layout, Server, Computers, Security, Processes, Settings
│   ├── components/           # StatusPill, ProcessCard, ScreenTile, LayoutCanvas, FingerprintView, SettingRow, InlineRequest, LogView
│   └── Theme.qml             # singleton: colours (light/dark), spacing, type scale, radii
├── platform/                 # OSX*/MSWindows* only bits: dock hiding, TCC check, service control, launch at login
└── test/                     # gtest for app/ (parser tables, layout -> conf, settings migration) + Qt Quick Test smoke
```

Rules: QML never touches `QProcess`, sockets or `QSettings`; `app/` never
includes a widget; `CoreEventParser` is the only place that knows what the
core prints; every user-visible string is in QML (`qsTr`).

The GUI drops the `src/fork/gui/` split: the fork is the product now (a
rename may follow), so the overlay (`OverlayNotification`, unchanged), the
notification router (now fed by `CoreEventParser` events instead of raw
lines) and the update checker (exposed through `AppController` for the
Settings page and the rail chip) move into `src/gui/app/` with everything
else, and the UI never labels an option as "fork" or "ours". The extension
pattern for core code is a separate decision for `AGENTS.md`.

`AppController` sketch (what QML binds to):

```
role: Server|Client            runState: Stopped|Starting|Running|Restarting|Failed
connection: { connectedCount, total, serverName, since }
computers: ListModel { name, role, state, address, displays, build, position, serverHeld, local, mirroredAt }
remoteServer: { name, config, mirroredAt }               // phase C, read-only on clients
requests: ListModel { kind: Join|Trust|Error, payload }    // inline cards
processes: { app, core, service, discovery } each { state, detail, actions }
start() stop() restart() setRole()   // setRole refused unless runState == Stopped
applyLayout() accept(request) reject(request)
```

Status source: still the core's output (desktop mode: stdout; service mode:
IPC log lines), but parsed in one tested table. The dimming code already
emits structured markers (`fork: screen dimmed`, to be renamed with the rest);
a later core-side change can add `status:` lines for connection state and
make the parser trivial. Not required
for this plan.

Settings: keep every existing key readable and keep the `internalConfig`
group and its keys for the layout, so the old GUI and the new one can run
against the same settings during the transition and a rollback never loses the
layout. New keys get a `ui/` prefix. Migration (`groupServerChecked` → `role`,
`autoStart` → `startCoreOnLaunch`, …) is a unit-tested function.

### 4.4 Screen layout model

Target (planned, not built): the canvas arranges **displays**, not computers.
A computer with two monitors shows two tiles grouped under its name, so a
portrait monitor beside a laptop reads the way the desk looks (the Layout
mock shows `desk-win · 1` / `desk-win · 2` and a portrait `mac-studio · 2`).

- **Phase A (M4)**: keep the 5×3 grid model under the hood, one tile per
  computer. The canvas draws cells as tiles, dragging snaps to cells, links
  are computed from neighbours exactly as `ServerConfig::operator<<` does
  today. Same `.conf` output.
- **Phase B (M5, done)**: per-display free placement. Each display is a
  rectangle with a size and a position inside its computer
  (`LayoutModel::DisplayConfig`), reported by that computer (below) or
  typed by hand. The config
  compiler collapses each computer's displays into its desktop bounding box
  and turns touching edges into links with ranges, `left(a,b) = name(c,d)`,
  which the config format already supports; the core switches on
  whole-desktop edges, so arrangement needs no protocol change. A grid-era
  store migrates on load: every cell becomes a 1920×1080 desktop at the
  cell's place (`internalConfig/layoutVersion` = 2 once saved). Dragging
  snaps to nearby edges; a drop that would overlap is refused.
- **Reported displays (built 2026-11-01)**: every window reads its own
  monitors in the core's units (`Platform::displays()`: `CGDisplayBounds`
  points on macOS, physical pixels from `EnumDisplayMonitors` on Windows,
  where the core is per-monitor DPI aware; Qt geometry times the device
  pixel ratio elsewhere) and publishes them as a `displays` snapshot on the
  settings channel and as TXT `d=x,y,WxH;…` in its zeroconf record. The
  record covers a computer the server has not let in yet (the channel only
  grants Read to computers in the layout), so it is placed at its real size;
  the channel's snapshot wins once it arrives. The server's own displays go
  straight into its layout. `LayoutModel::reportDisplays` applies a report
  to a computer still on the placeholder display or whose displays came
  from a report (`displaysReported`, persisted); one typed by hand keeps
  its values and the panel offers "Use them". A desktop that changes size
  keeps its origin and moves the desktops wholly right of / below it by the
  change, so neighbours keep touching; then the server restarts once
  (debounced). No core or wire change. Still open: overlay hints naming the
  display the cursor left from.

### 4.5 The process model, as shown in the UI

| Piece | Windows | macOS / Linux | Card shows | Actions |
|---|---|---|---|---|
| App (`input-leap`) | this window + tray | same | version, launched at, tray, settings location, update state | install update, open logs folder |
| Core (`input-leaps` / `input-leapc`) | started by the service (elevation as configured), log over IPC | `QProcess` child, stdout/stderr | state, PID, uptime, restarts, log level, full command line | restart, stop, copy command |
| Service (`input-leapd`) | Windows service, IPC `127.0.0.1:24801` | absent (card says so) | service state, IPC state, elevation mode | restart service, reconnect IPC |
| Discovery | Bonjour service (`mDNSResponder`) | mDNSResponder / Avahi compat | advertising record, browsing, found peers | refresh, turn off |

### 4.6 Look and feel (the mocks)

Light surface `#F3F5F7` / cards white, text `#1B2430`, accent `#236F93`
(the logo blue, darkened to pass 4.5:1), success `#4C7A12` (the logo green,
darkened), amber for pending things. 12 px card radius, 8 px grid, 13 px UI
text, monospace for names, addresses, fingerprints and commands. Dark theme
derives from the same tokens. Window default 1040×760, minimum 900×600,
resizable. The mocks use Inter; the app should use the system UI font
(Segoe UI / SF Pro) unless we decide to bundle one (§7).

### 4.7 Where settings live, and the sync channel

| Setting | Lives on | Edited where |
|---|---|---|
| layout and links; per-computer screen options (aliases, modifiers, dead corners, fixes); switching; behaviour; dimming; hotkeys; listening address and port; TLS policy | the server, in the generated `.conf` and its settings | the server's Server, Layout and Computers pages; mirrored read-only to clients in phase C |
| screen name; server to connect to; discovery; scroll step; overlay hints; log level; launch at login; appearance | each machine | that machine's own Computers tab and Settings; mirrored read-only to the others in phase C; written remotely in phase D |

Phase C adds a **GUI-to-GUI channel**, independent of the core
(`src/gui/app/SyncService.{h,cpp}`, built in M6/M7): each app listens on a
TLS socket using the certificate it already has, advertises the port in the
zeroconf record it already publishes (TXT `sync=1`, `port=<core port>`,
`v=<build>`; records are resolved with `ZeroconfResolver`, which also gives
the client core a proper `.local` address), and accepts peers the trust
check allows. Clients dial the server's GUI; the server accepts. Trust
mirrors the core's posture: a certificate in the trusted database grants
**Write** (the peer may change this computer's own settings); a server
without client-certificate checks grants **Read** to the computers of its
layout, a client with TLS off grants Read to the server it found; anybody
else is dropped before anything is shared. Each side tells the other what
it granted (`access`). A window listens on port 24802 when it is free (any
port otherwise); a client whose server address was typed in, with
discovery off, dials that port on the server directly, and the Windows
installer's firewall rule covers it.

Messages are one JSON document per line, each with a `type`: snapshots
kept per peer and resent on change (`hello` name/role/build/protocol,
`settings` the machine's own settings, `state` run state/cursor/pieces,
`server-config` the server's settings + the layout as JSON + the generated
`.conf`), and requests answered by an `ack` with the same `id`: `set`
(one key, validated and applied through the normal setters) and `action`
(start, stop, restart, restartDiscovery, checkForUpdates). The ack says
where and when the change lands ("on mac-studio, after its client
restarts"), shown next to the control on the other window (phase D). The
core and the wire protocol stay untouched. Alternative considered: new
messages on the core connection; rejected because the GUI has no command
channel to its own core beyond the Windows IPC, so every message would need
core plumbing on both ends.

### 4.8 One fleet, edited from any window (built 2026-11-01)

The server is the hub. Every window shows the same pages, and anything can
be changed from any of them; a change takes effect at once everywhere.

- **The server's configuration** (layout, options, hotkeys): on a client,
  `App.layout` is its copy of the server's (`m_mirror`, loaded from the
  `server-config` snapshot), editable when the server grants Write. Every
  edit (`LayoutModel::edited`, emitted from `save()`, never from a load)
  sends the whole configuration as `set-config`, batched over 300 ms; the
  server checks it (`LayoutModel::replaceWith`: names, counts), saves it and
  shares it again. Hotkeys travel whole (`hotkeyData`, `Hotkey::toJson`).
  While edits are in flight the copy ignores the server's snapshots and
  reloads after the answer; a refused edit reloads what the server holds.
- **The server's own settings** (port, address, TLS, client certificates,
  configuration source): `set` requests to the server
  (`App.setServerSetting`), the same path as any other computer's settings.
- **Other clients**: the server publishes a `fleet` snapshot (each client's
  settings, state and whether it lets the server write); a client lists
  those computers and reads them from it (`App.peerInfo`). Its requests for
  them go to the server as `relay`; the server forwards them as its own
  request (a deferred ack, `SyncService::answer`), so a change lands only
  where the server trusts the asker and the target trusts the server.
- **Taking effect**: a running core restarts by itself 1.5 s after the
  last change when what it was started with differs (`applyIfChanged`: the
  command line, with the client's server address as set rather than as
  discovery resolved it, and the generated configuration). No more "Apply
  and restart". **Restart all** asks every client that lets the server do
  it, then restarts the server.
- **Trust**: unchanged rules (Write needs the peer's certificate in the
  trusted database). New: a computer of the layout whose window connects
  untrusted raises an inline "wants to change settings" request on the
  server; trusting it upgrades the open connection
  (`SyncService::reevaluateAccess`), no reconnect. Refused once, it is not
  asked again that session.
- Protocol 3 (`relay`, `set-config`); a protocol-2 peer still shares and
  takes `set`/`action`, and the pages say it needs an update to edit the
  fleet. The core still restarts on Windows to pick up a configuration
  (no SIGHUP there); a core-side reload would keep clients connected.

## 5. Milestones

| # | Scope | Done when |
|---|---|---|
| M0 | Spike: `qt_add_qml_module`, FluentWinUI3 window with the shell (header, rail, footer), `QSystemTrayIcon`, the existing overlay, CI on Qt 6.9 for macOS arm64 + Windows, installers produced | both installers launch the spike window, tray and overlay work, dark mode follows the system |
| M1 | App layer: `CoreProcess`, `CoreEventParser`, `AppSettings` (+migration), `ServerLayout` (+conf writer), `TlsIdentity`, `Discovery`, `LogModel`, with gtest coverage | old GUI still ships; tests green on both platforms; parser table covers every line the old `MainWindow` matched |
| M2 | New window behind CMake option `INPUTLEAP_NEW_GUI` (default OFF): Overview, Processes, Settings, log drawer, start/stop/restart, tray, update checker | a day's use as server (Windows) and client (macOS) with the new window |
| M3 | Server, Computers (local data only: this computer's settings, the server-held options of every computer, live state) and Security pages; inline join/trust requests; first-run state; wizard and dialogs no longer reachable from the new window | every setting of the old dialogs has a home; settings round-trip old ⇄ new |
| M4 | Layout page (phase A); `INPUTLEAP_NEW_GUI` defaults ON; old `src/gui/src/*` removed; AGENTS.md updated | `latest-build` ships the new UI; the fleet updates without reconfiguring |
| M5 | Polish: keyboard navigation, accessibility names, platform details (dock hiding, TCC prompts, launch at login), layout phase B | done 2026-10-31 |
| M6 | Phase C, the sync channel read-only: the Server page on clients mirrors the connected server; remote tabs on Computers show each machine's own settings and process | built 2026-10-31 (`SyncService`, loopback-tested); not yet exercised on the fleet |
| M7 | Phase D, remote writes with per-machine apply feedback | built 2026-10-31; loopback-tested, not yet on the fleet |

The fleet runs `latest-build`, so the old GUI keeps shipping until M4 flips
the default; the option lets us test the new window on the real machines
without blocking fixes to the core.

## 6. Risks

| Risk | Mitigation |
|---|---|
| FluentWinUI3 gaps (some controls fall back to Fusion) | M0 checks the controls we use; `Basic` style + tokens as fallback |
| QML deployment on macOS/Windows | M0 proves the installers; `qt_generate_deploy_qml_app_script` is the supported path |
| Settings migration breaks a client in the fleet | tested migration; legacy keys stay readable; `internalConfig` keys unchanged |
| Status still comes from log parsing | one tested parser; optional core `status:` lines later |
| Windows service / elevation behaviour | the IPC code path is wrapped, not rewritten |
| Bigger install (Qt Quick libs, ~15–20 MB) | acceptable for this fleet |

## 7. Decisions needed before starting

1. **QML front-end** (recommended) or a Widgets rebuild with the same C++ layer?
2. **Translations**: the 47 `.ts` files become obsolete with a rewrite. Ship
   English only (recommended; the fleet is one person) and keep `qsTr` so
   languages can return later?
3. **External config file mode**: keep it (cheap, mocked in the Server page) or
   drop it?
4. **Transition**: the rewrite lived behind a CMake option (`INPUTLEAP_NEW_GUI`, default OFF) until M4 made it the only GUI
   (recommended), or a runtime flag?
5. ~~Where the new GUI lives~~ Resolved: everything under `src/gui/`, the
   overlay, router and updater included; no fork/upstream distinction in the
   UI or the GUI tree (the app may be renamed later).
6. **Font**: system UI font (recommended) or bundle Inter as in the mocks?
7. **Drop Qt 5 and the gulrak filesystem option** with the Qt 6.8 floor?
8. **Sync channel transport** (phase C): GUI-to-GUI TLS with the existing
   certificates and trusted fingerprints (recommended), or new messages on the
   core connection?

## 8. Working on it locally (M0 onwards)

The rewrite was done on a branch and merged into `main` once M7 was built
(2026-10-31). CI runs only on pushes to `main` and on `v*` tags, and each run
builds the macOS and Windows installers (the macOS runner is the expensive
one), so iterate locally and push a few times per milestone at most. Since
M4 the Qt Quick GUI is the only GUI and CI builds it on Qt 6.9.

Build tree `build/ui` (ignored by git):

```bash
cmake -S . -B build/ui -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt;/opt/homebrew/opt/openssl@3" \
      -DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"
cmake --build build/ui --target input-leap guiunittests --parallel   # the window and its tests
./build/ui/bin/guiunittests
./build/ui/bin/input-leap                                            # runs against Homebrew Qt
cmake --build build/ui --target InputLeap_MacOS --parallel           # core + deployed bundle in build/ui/bundle
open build/ui/bundle/InputLeap.app                                   # the deployed app, self-contained
```

Development hooks in `src/gui/app/main.cpp`:

- `INPUTLEAP_UI_GRAB=<file.png>` renders the window to a file 1.5 s after start
  and exits. With `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` it runs
  headless; the offscreen platform has no real palette, so standard buttons
  look faint there. Judge looks on the real platform (the window shows for
  under two seconds).
- `INPUTLEAP_UI_SETTINGS_FILE=<ini>` uses that file as the settings store, so
  a development run never touches the real one; `INPUTLEAP_UI_DRY_RUN=1`
  (implied by `INPUTLEAP_UI_GRAB`) never starts the core, discovery or the
  update check, so a development run never touches the fleet.
  `INPUTLEAP_UI_PAGE=<0-6>` opens that page, `INPUTLEAP_UI_COMPUTER=<name>`
  that tab of the Computers page. `INPUTLEAP_UI_DEMO_PEER=<name>` pretends
  that client shares its settings (the remote tab); `=<name>:server`
  pretends a server does (a client's mirrored Server and Layout pages).
  `INPUTLEAP_UI_TLS_CHECK=1` prints the TLS backend the channel would use.
  `INPUTLEAP_UI_NO_CORE=1` runs a second window beside the installed one with
  the real settings, discovery and the settings channel on but no core: the
  way to look at a branch's pages with live fleet data without replacing the
  installed, signed app (an unsigned local bundle would lose the Accessibility
  grant). The window remembers the core it launched (`ui/corePid`) and, at
  the next start, terminates one a killed window left behind.
  `INPUTLEAP_UI_DEMO_OVERLAY=1` fires one HUD overlay.
- The app runs one window per user (`SingleInstance`, a local socket): a
  second launch tells the first to show itself and exits. The autostart
  entries launch with `--hidden`; only such a launch honours "Open hidden in
  the tray". Qt's warnings and QML errors are appended to `gui.log` in the
  data folder (not during development runs with a settings file). The tray
  is retried for five minutes when it is not there at launch.
- Windows: windeployqt puts Qt's QML modules under `qml/` next to the
  executable; the installer ships a `qt.conf` naming that folder and the app
  adds it as an import path, and the install step copies that tree whole:
  an earlier `*.*` filter dropped every extension-less `qmldir`, so the
  engine fell back to the resource copies of Qt's modules and looked for
  their plugins next to the executable (`module "QtQuick.Controls" plugin
  "qtquickcontrols2plugin" not found`). A failed window shows a dialog with
  the errors; `gui.log` in `%LOCALAPPDATA%\InputLeap\InputLeap` has them
  too. Check a Windows artifact with `find . -name qmldir` before trusting it.
- `INPUTLEAP_UI_COLOR_SCHEME=dark|light` forces the scheme where the platform
  theme honours it (macOS yes, offscreen no).

Local quirks handled in the build:

- The macOS 26 SDK has no AGL framework but Qt before 6.9.2 still links it;
  the root `CMakeLists.txt` substitutes the OpenGL framework. This is also
  what made the legacy GUI buildable on this machine.
- `dist/macos/bundle/build_dist.sh.in` grew post-deploy steps that
  macdeployqt does not do: `prune_qml`/`prune_plugins` drop the QML modules,
  image formats and the `PlugIns/quick` copies the app never imports
  (macdeployqt copies whole module trees for `import QtQuick`);
  `fix_rpaths` strips the build tree's Qt rpath from the executables (with it
  in place, plugins loaded a second Qt from the build machine);
  `homebrew_fixup` (Homebrew Qt only) copies the @rpath helper dylibs and
  frameworks macdeployqt leaves unresolved and rewrites Cellar paths;
  `prune_frameworks` removes every framework nothing references;
  `adhoc_sign` re-signs the bundle, which Apple Silicon requires after
  install-name edits (CI still signs Release with the real identity). The
  script also returns its own exit status instead of `tput`'s.
- QtSvg is linked only so the deploy tools ship the SVG image/icon plugins.

Status, 2026-10-31: M0 to M5 are done on this Mac. The Qt Quick GUI is the
only GUI (the Widgets sources and the translations are gone; the reused
UI-free classes moved into `src/gui/app/`), all seven pages exist, the layout
places desktops freely with per-display sizes (phase B), join and trust
requests are inline, the window is keyboard-navigable with accessible
names, and the unit tests cover the parser, the settings store and the
layout model. M6 and M7 are built: the settings channel (§4.7) feeds the
client's Server and Layout pages with the server's configuration and the
Computers tabs with each peer's own settings and pieces, editable there
when that computer trusts this one's certificate, every change answered
with where it lands; the loopback tests cover the exchange, access levels,
a request and its ack, and the rejection of an untrusted peer. On this Mac
the channel runs on Qt's Secure Transport backend (the deployed bundle
carries that plugin; no OpenSSL library is needed). Not yet verified: a
Windows build (the next push to `main` makes one) and the channel between
two real machines.

