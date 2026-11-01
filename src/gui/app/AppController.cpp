/*  InputLeap -- mouse and keyboard sharing utility
    Copyright (C) - Nicolas Antomarioni (nantomarioni@gmail.com)

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AppController.h"

#include "AppSettings.h"
#include "CoreEventParser.h"
#include "CoreProcess.h"
#include "Discovery.h"
#include "KeyCaptureItem.h"
#include "LayoutModel.h"
#include "LogModel.h"
#include "Platform.h"
#include "TlsIdentity.h"
#include "common/DataDirectories.h"
#include "../../fork/gui/NotificationRouter.h"
#include "../../fork/gui/UpdateChecker.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHostInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>

namespace {
AppController* s_instance = nullptr;
const int kMaxUnexpectedExits = 5;
const int kRestartDelayMs = 1000;
} // namespace

AppController* AppController::instance()
{
    if (s_instance == nullptr) {
        s_instance = new AppController(QCoreApplication::instance());
    }
    return s_instance;
}

AppController* AppController::create(QQmlEngine*, QJSEngine*)
{
    AppController* controller = instance();
    QJSEngine::setObjectOwnership(controller, QJSEngine::CppOwnership);
    return controller;
}

AppController::AppController(QObject* parent) :
    QObject(parent)
{
    m_settings = new AppSettings(this);
    m_layout = new LayoutModel(&m_settings->store(), this);
    m_identity = new TlsIdentity(m_settings, this);
    m_discovery = new Discovery(this);
    m_core = new CoreProcess(this);
    m_sync = new SyncService(this);
    m_mirror = new LayoutModel(nullptr, this);
    m_external = new LayoutModel(nullptr, this);
    m_externalWatcher = new QFileSystemWatcher(this);
    m_logModel = new LogModel(this);
    m_log = new LogFilterModel(m_logModel, this);
    m_updates = new inputleap::fork_gui::UpdateChecker(nullptr);
    m_updates->setParent(this);
    m_restartTimer = new QTimer(this);
    m_restartTimer->setSingleShot(true);
    m_displaysTimer = new QTimer(this);
    m_displaysTimer->setSingleShot(true);
    m_displaysTimer->setInterval(1000);
    m_applyTimer = new QTimer(this);
    m_applyTimer->setSingleShot(true);
    m_applyTimer->setInterval(1500);
    m_configSendTimer = new QTimer(this);
    m_configSendTimer->setSingleShot(true);
    m_configSendTimer->setInterval(300);
    m_external->setReadOnly(true);
    m_mirror->setReadOnly(true);

    const auto all = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : all) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            if (address.toString().startsWith(QLatin1String("192.168"))) {
                m_addresses.prepend(address.toString());
            } else {
                m_addresses.append(address.toString());
            }
        }
    }
    m_monoFontFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    m_version = QStringLiteral(INPUTLEAP_VERSION);
    m_accessibilityGranted = Platform::accessibilityGranted();

    m_layout->setServerName(m_settings->screenName());

    connect(m_settings, &AppSettings::changed, this, [this]() {
        m_layout->setServerName(m_settings->screenName());
        onDisplaysReported(screenName(), m_localDisplays);   // a new name or role: the server's own desktop
        if (m_settings->useExternalConfig() != m_externalActive || m_settings->configFile() != m_externalPath) {
            reloadExternalLayout();
        }
        if (m_settings->serverHostname() != m_syncServerHost) {
            dialServerAddress();
        }
        // the channel follows its switch; a new name is announced to the
        // peers and re-registered, both deferred so a change a peer asked
        // for is answered first
        if (!m_dryRun) {
            if (m_settings->syncEnabled() != m_sync->listening()) {
                QTimer::singleShot(0, this, [this]() { startSync(); startDiscovery(); });
            } else if (m_sync->listening() && m_syncName != screenName()) {
                m_syncName = screenName();
                m_sync->setLocalInfo(screenName(), role() == Server ? QStringLiteral("server") : QStringLiteral("client"), m_version);
                QTimer::singleShot(0, this, [this]() { startDiscovery(); });
            }
        }
        emit stateChanged();
    });
    connect(m_layout, &LayoutModel::layoutChanged, this, &AppController::stateChanged);
    // this computer's monitors, sent to the server (docs/ui-redesign.md §4.4)
    connect(m_displaysTimer, &QTimer::timeout, this, &AppController::publishDisplays);
    // every change takes effect at once: the core restarts when what it was
    // started with no longer matches (settings, the generated configuration)
    connect(m_applyTimer, &QTimer::timeout, this, &AppController::applyIfChanged);
    connect(m_settings, &AppSettings::changed, m_applyTimer, qOverload<>(&QTimer::start));
    connect(m_layout, &LayoutModel::edited, m_applyTimer, qOverload<>(&QTimer::start));
    // a client's edits of the server's configuration go to the server
    connect(m_mirror, &LayoutModel::edited, m_configSendTimer, qOverload<>(&QTimer::start));
    connect(m_configSendTimer, &QTimer::timeout, this, &AppController::sendServerConfig);
    if (auto* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        auto watch = [this](QScreen* screen) {
            connect(screen, &QScreen::geometryChanged, m_displaysTimer, qOverload<>(&QTimer::start));
        };
        const auto screens = QGuiApplication::screens();
        for (QScreen* screen : screens) {
            watch(screen);
        }
        connect(gui, &QGuiApplication::screenAdded, this, [this, watch](QScreen* screen) {
            watch(screen);
            m_displaysTimer->start();
        });
        connect(gui, &QGuiApplication::screenRemoved, m_displaysTimer, qOverload<>(&QTimer::start));
        connect(gui, &QGuiApplication::primaryScreenChanged, m_displaysTimer, qOverload<>(&QTimer::start));
    }
    connect(m_sync, &SyncService::peerDocumentChanged, this, [this](const QString& name, const QString& kind) {
        if (kind == QLatin1String("displays")) {
            const QJsonObject doc = m_sync->peerDocument(name, kind);
            onDisplaysReported(name, LayoutModel::displaysFromJson(doc.value(QStringLiteral("displays")).toArray()));
        }
    });
    connect(m_layout, &LayoutModel::optionsChanged, this, &AppController::publishSync);
    connect(m_external, &LayoutModel::layoutChanged, this, &AppController::stateChanged);
    connect(m_externalWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        QTimer::singleShot(300, this, &AppController::reloadExternalLayout);   // editors save in several steps
    });
    connect(m_layout, &LayoutModel::hotkeysChanged, this, &AppController::publishSync);
    connect(this, &AppController::stateChanged, this, &AppController::publishSync);
    connect(m_discovery, &Discovery::peerResolved, this, &AppController::onPeerResolved);
    connect(m_discovery, &Discovery::peerLost, this, [this](const QString& name) { m_sync->forget(name); });
    connect(m_sync, &SyncService::message, this, [this](const QString& text, bool isError) {
        log(text, isError ? QStringLiteral("WARNING") : QStringLiteral("INFO"));
    });
    connect(m_sync, &SyncService::changed, this, &AppController::stateChanged);
    connect(m_sync, &SyncService::serverConfigChanged, this, &AppController::reloadMirror);
    connect(m_sync, &SyncService::changed, this, &AppController::updateMirrorAccess);
    connect(m_core, &CoreProcess::lineReceived, this, &AppController::handleCoreLine);
    connect(m_core, &CoreProcess::stateChanged, this, &AppController::onCoreStateChanged);
    connect(m_core, &CoreProcess::exited, this, &AppController::onCoreExited);
    connect(m_core, &CoreProcess::serviceMessage, this, [this](const QString& text, bool isError) {
        m_logModel->append(LogModel::Service, isError ? QStringLiteral("ERROR") : QStringLiteral("INFO"), text);
    });
    connect(m_discovery, &Discovery::clientFound, this, &AppController::onClientFound);
    connect(m_discovery, &Discovery::serverFound, this, &AppController::onServerFound);
    connect(m_discovery, &Discovery::changed, this, &AppController::stateChanged);
    connect(m_identity, &TlsIdentity::message, this, [this](const QString& text, bool isError) {
        log(text, isError ? QStringLiteral("ERROR") : QStringLiteral("INFO"));
    });
    connect(m_identity, &TlsIdentity::changed, this, &AppController::stateChanged);
    connect(m_identity, &TlsIdentity::changed, this, [this]() {
        m_sync->retryNow();
        m_sync->reevaluateAccess();
    });
    connect(m_restartTimer, &QTimer::timeout, this, [this]() {
        if (m_wantRunning && m_core->state() == CoreProcess::Stopped) {
            start();
        }
    });
    connect(m_updates, &inputleap::fork_gui::UpdateChecker::updateAvailable, this,
            [this](const QString& name, const QString& assetUrl, const QString& assetName) {
                m_updateName = name;
                m_updateAssetUrl = assetUrl;
                m_updateAssetName = assetName;
                m_updateStatus = tr("Newer build available: %1").arg(name);
                log(m_updateStatus);
                emit stateChanged();
            });
    connect(m_updates, &inputleap::fork_gui::UpdateChecker::upToDate, this, [this]() {
        m_updateStatus = tr("Up to date (checked %1)").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")));
        emit stateChanged();
    });
    connect(m_updates, &inputleap::fork_gui::UpdateChecker::checkFailed, this, [this](const QString& error) {
        m_updateStatus = tr("Update check failed: %1").arg(error);
        emit stateChanged();
    });

    reloadExternalLayout();
    publishDisplays();
    log(tr("Input Leap %1 started").arg(m_version));
}

// The configuration file, when the server runs one, read into a model of
// its own so the pages show what the core actually loads.
void AppController::reloadExternalLayout()
{
    const bool want = m_settings->useExternalConfig();
    const QString path = want ? m_settings->configFile() : QString();
    if (want) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_external->loadConfigText(QString::fromUtf8(file.readAll()), screenName());
        } else {
            m_external->loadJson(QJsonObject());
            log(tr("cannot read the configuration file %1").arg(path), QStringLiteral("WARNING"));
        }
    }
    const QStringList watched = m_externalWatcher->files();
    if (!watched.isEmpty()) {
        m_externalWatcher->removePaths(watched);
    }
    if (want && QFile::exists(path)) {
        m_externalWatcher->addPath(path);
    }
    const bool changed = want != m_externalActive || path != m_externalPath;
    m_externalActive = want;
    m_externalPath = path;
    if (changed) {
        emit layoutSourceChanged();
    }
    emit stateChanged();
}

QString AppController::layoutSource() const
{
    if (role() == Client) {
        return tr("held by %1").arg(m_sync->serverConnected() ? m_sync->serverName() : tr("the server"));
    }
    return m_externalActive ? tr("the file %1").arg(m_externalPath) : tr("generated from the layout");
}

// ---- the server's configuration, edited from any window ---------------------------

bool AppController::serverSettingsEditable() const
{
    if (role() == Server) {
        return true;
    }
    if (!m_sync->serverConnected()) {
        return false;
    }
    const QVariantMap server = m_sync->peer(m_sync->serverName());
    return server.value(QStringLiteral("accessThere")).toInt() >= SyncService::Write
           && server.value(QStringLiteral("protocol")).toInt() >= 3;
}

bool AppController::layoutEditable() const
{
    if (role() == Server) {
        return !m_externalActive;
    }
    return m_mirrorAvailable && serverSettingsEditable()
           && !m_sync->serverSettings().value(QStringLiteral("useExternalConfig")).toBool();
}

QString AppController::layoutReadOnlyReason() const
{
    if (layoutEditable()) {
        return QString();
    }
    if (role() == Server) {
        return tr("As read from %1. Edit that file (it reloads here when saved), or choose \"Generate from the layout\" on the Server page to arrange it here.").arg(m_externalPath);
    }
    if (!m_mirrorAvailable) {
        return tr("Waiting for the server to share its configuration.");
    }
    const QString server = m_sync->serverName();
    if (m_sync->serverSettings().value(QStringLiteral("useExternalConfig")).toBool()) {
        return tr("%1 runs a configuration file: edit it there.").arg(server);
    }
    if (m_sync->peer(server).value(QStringLiteral("protocol")).toInt() < 3) {
        return tr("%1 runs an older build: update it to edit its configuration from here.").arg(server);
    }
    return tr("Read-only: %1 does not trust this computer's certificate. Trust it on %1 (it asks when this computer connects) to edit from here.").arg(server);
}

void AppController::updateMirrorAccess()
{
    m_mirror->setReadOnly(!layoutEditable());
}

// The server shared its configuration: the copy here follows, unless edits
// made here are still on their way (the answer to them brings it back).
void AppController::reloadMirror()
{
    if (m_configInFlight > 0 || m_configSendTimer->isActive()) {
        m_mirrorStale = true;
        return;
    }
    m_mirrorStale = false;
    const QJsonObject config = m_sync->serverDocument(QStringLiteral("server-config"));
    const bool available = !config.isEmpty();
    if (available) {
        m_mirror->loadJson(config.value(QStringLiteral("layout")).toObject());
    } else if (m_mirrorAvailable) {
        m_mirror->loadJson(QJsonObject());
    }
    m_mirrorAvailable = available;
    updateMirrorAccess();
    emit layoutSourceChanged();
    emit stateChanged();
}

// A client edited its copy of the server's configuration: the whole of it
// goes to the server, which checks it, saves it and shares it with everyone.
void AppController::sendServerConfig()
{
    if (role() != Client || !m_sync->serverConnected()) {
        return;
    }
    QJsonObject body;
    body.insert(QStringLiteral("layout"), m_mirror->toJson());
    ++m_configInFlight;
    m_sync->request(m_sync->serverName(), QStringLiteral("set-config"), body, [this](const QJsonObject& ack) {
        m_configInFlight = qMax(0, m_configInFlight - 1);
        const bool ok = ack.value(QStringLiteral("ok")).toBool();
        log(tr("layout · %1").arg(ack.value(QStringLiteral("text")).toString()), ok ? QStringLiteral("INFO") : QStringLiteral("WARNING"));
        if (!ok) {
            m_mirrorStale = true;   // show what the server really holds
            emit notify(tr("Layout not changed"), ack.value(QStringLiteral("text")).toString());
        }
        if (m_configInFlight == 0 && m_mirrorStale) {
            reloadMirror();
        }
    });
}

void AppController::setServerSetting(const QString& key, const QVariant& value)
{
    if (role() == Server) {
        const QJsonObject ack = applySetting(QString(), key, QJsonValue::fromVariant(value));
        if (!ack.value(QStringLiteral("ok")).toBool()) {
            log(ack.value(QStringLiteral("text")).toString(), QStringLiteral("WARNING"));
        }
        return;
    }
    setRemote(m_sync->serverName(), key, value);
}

// ---- derived state -----------------------------------------------------------

AppController::Role AppController::role() const
{
    return m_settings->role() == AppSettings::Client ? Client : Server;
}

bool AppController::connected() const
{
    return role() == Server ? !m_connectedClients.isEmpty() : m_connectedToServer;
}

QString AppController::statusText() const
{
    const bool server = role() == Server;
    // Windows: the service launches the core; without it nothing runs,
    // whatever was asked
    if (m_core->serviceMode() && !m_core->serviceConnected() && (m_runState == Starting || m_runState == Running)) {
        return tr("Waiting for the InputLeap service · if this stays, start it in Windows Services");
    }
    switch (m_runState) {
    case Stopped:
        return m_pausedForTrust ? tr("Paused · waiting for you to trust the server") : tr("Not running");
    case Starting:
        return server ? tr("Starting the server…") : tr("Connecting to %1…").arg(serverAddress());
    case Running: {
        if (server) {
            const int total = layout()->screenCount() - (layout()->hasScreen(screenName()) ? 1 : 0);
            if (!m_listening) return tr("Server starting…");
            if (m_connectedClients.isEmpty()) return tr("Running as server · waiting for clients");
            return tr("Running as server · %1 of %2 clients connected").arg(m_connectedClients.size()).arg(qMax(total, int(m_connectedClients.size())));
        }
        return m_connectedToServer ? tr("Connected to %1").arg(serverAddress()) : tr("Connecting to %1…").arg(serverAddress());
    }
    case Stopping:
        return tr("Stopping…");
    case Failed:
        return m_lastError.isEmpty() ? tr("Failed to start") : tr("Failed: %1").arg(m_lastError);
    }
    return QString();
}

QString AppController::serverAddress() const
{
    const QString manual = m_settings->serverHostname();
    if (!manual.isEmpty()) {
        return manual;
    }
    const QString name = m_discovery->servers().value(0);
    Discovery::Peer peer;
    if (m_discovery->resolvedPeer(name, peer) && !peer.host.isEmpty()) {
        return peer.host;
    }
    return name;
}

// The core port to connect to: the one the server advertises when
// discovery found it, this computer's own setting otherwise.
int AppController::serverPort() const
{
    if (m_settings->serverHostname().isEmpty()) {
        Discovery::Peer peer;
        if (m_discovery->resolvedPeer(m_discovery->servers().value(0), peer) && peer.corePort() > 0) {
            return peer.corePort();
        }
    }
    return m_settings->port();
}

QString AppController::screenName() const
{
    return m_settings->screenName();
}

bool AppController::firstRun() const
{
    return !m_settings->startedBefore();
}

QString AppController::generatedConfigPath() const
{
    return dataFolder() + QStringLiteral("/generated.conf");
}

QString AppController::dataFolder() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString AppController::platformName() const
{
    return Platform::name();
}

void AppController::setTrayAvailable(bool available)
{
    if (m_trayAvailable == available) {
        return;
    }
    m_trayAvailable = available;
    log(available ? tr("tray icon shown") : tr("no tray icon: this window stays reachable from the Start menu or Dock"),
        available ? QStringLiteral("INFO") : QStringLiteral("WARNING"));
    emit stateChanged();
}

bool AppController::isWindows() const
{
#if defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

bool AppController::isMac() const
{
#if defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

QVariantList AppController::computers() const
{
    QVariantList list;
    const QString self = screenName();
    auto entry = [&](const QString& name, const QString& position, const QString& state, const QString& tone, const QString& address, const QDateTime& since) {
        QVariantMap map;
        map.insert(QStringLiteral("name"), name);
        map.insert(QStringLiteral("isSelf"), name == self);
        map.insert(QStringLiteral("position"), position);
        map.insert(QStringLiteral("state"), state);
        map.insert(QStringLiteral("stateTone"), tone);
        map.insert(QStringLiteral("address"), address);
        map.insert(QStringLiteral("since"), since.isValid() ? since.toString(QStringLiteral("HH:mm")) : QString());
        return map;
    };

    if (role() == Server) {
        const QStringList names = layout()->screenNames();
        if (!layout()->hasScreen(self)) {
            list << entry(self, tr("not placed"), tr("this computer · server"), QStringLiteral("warning"), m_addresses.value(0), QDateTime());
        }
        for (const QString& name : names) {
            const int cell = layout()->indexOf(name);
            if (name.compare(self, Qt::CaseInsensitive) == 0) {
                const bool active = m_activeScreen.isEmpty() || m_activeScreen == self;
                list << entry(name, layout()->positionText(cell),
                              running() && active ? tr("this computer · cursor here") : tr("this computer · server"),
                              running() ? QStringLiteral("success") : QStringLiteral("neutral"), m_addresses.value(0), m_since);
                continue;
            }
            // the core only tells us about clients that connected after this
            // window attached; a client's own window says so too, over the channel
            const QVariantMap peer = m_sync->peer(name);
            const bool peerConnected = peer.value(QStringLiteral("connected")).toBool()
                                       && peer.value(QStringLiteral("state")).toMap().value(QStringLiteral("connected")).toBool();
            bool loggedOn = false;
            for (const QString& client : std::as_const(m_connectedClients)) {
                if (client.compare(name, Qt::CaseInsensitive) == 0) loggedOn = true;
            }
            const bool on = loggedOn || (running() && peerConnected);
            QString state = on ? tr("Connected") : (running() ? tr("Not connected") : tr("Offline"));
            if (on && m_activeScreen.compare(name, Qt::CaseInsensitive) == 0) state = tr("Connected · cursor here");
            const QString address = peer.value(QStringLiteral("settings")).toMap().value(QStringLiteral("addresses")).toList().value(0).toString();
            list << entry(name, layout()->positionText(cell), state,
                          on ? QStringLiteral("success") : QStringLiteral("neutral"), address, on ? m_since : QDateTime());
        }
    } else {
        list << entry(self, tr("client"), m_connectedToServer ? tr("Connected") : (running() ? tr("Connecting…") : tr("Not running")),
                      m_connectedToServer ? QStringLiteral("success") : QStringLiteral("neutral"), m_addresses.value(0), m_since);
        // the server by its screen name when its window shares it, else by address
        const QString address = serverAddress();
        QString server = m_sync->serverConnected() ? m_sync->serverName() : m_mirror->serverName();
        if (server.isEmpty()) {
            server = address;
        }
        if (!server.isEmpty()) {
            const QString position = m_mirrorAvailable ? m_mirror->positionText(m_mirror->indexOf(self)) : QString();
            list << entry(server, position.isEmpty() ? tr("server") : tr("server · this computer is %1").arg(position),
                          m_connectedToServer ? tr("Connected") : tr("Server"),
                          m_connectedToServer ? QStringLiteral("success") : QStringLiteral("neutral"), address, m_since);
        }
        // the other clients: those of the server's layout, and any the server
        // talks to that are not placed yet
        QStringList others = m_mirrorAvailable ? m_mirror->screenNames() : QStringList();
        const QJsonArray fleet = m_sync->serverDocument(QStringLiteral("fleet")).value(QStringLiteral("computers")).toArray();
        for (const QJsonValue& value : fleet) {
            const QString name = value.toObject().value(QStringLiteral("name")).toString();
            if (!others.contains(name)) others << name;
        }
        for (const QString& name : std::as_const(others)) {
            if (name == self || name == server) continue;
            const QVariantMap peer = peerInfo(name);
            const bool sharing = peer.value(QStringLiteral("connected")).toBool();
            const bool on = peer.value(QStringLiteral("state")).toMap().value(QStringLiteral("connected")).toBool();
            const int cell = m_mirror->indexOf(name);
            list << entry(name, cell >= 0 ? m_mirror->positionText(cell) : tr("not placed"),
                          on ? tr("Connected") : sharing ? tr("Sharing") : tr("Not shared"),
                          on ? QStringLiteral("success") : QStringLiteral("neutral"),
                          peer.value(QStringLiteral("settings")).toMap().value(QStringLiteral("addresses")).toList().value(0).toString(), QDateTime());
        }
    }
    return list;
}

QVariantList AppController::processes() const
{
    QVariantList list;
    auto item = [](const QString& id, const QString& name, const QString& detail, bool ok, bool relevant) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), id);
        map.insert(QStringLiteral("name"), name);
        map.insert(QStringLiteral("detail"), detail);
        map.insert(QStringLiteral("ok"), ok);
        map.insert(QStringLiteral("relevant"), relevant);
        return map;
    };
    list << item(QStringLiteral("app"), tr("App"), tr("This window and the tray icon"), true, true);

    const QString coreName = role() == Server ? QStringLiteral("input-leaps") : QStringLiteral("input-leapc");
    QString coreDetail;
    switch (m_core->state()) {
    case CoreProcess::Stopped: coreDetail = tr("Not running"); break;
    case CoreProcess::Starting: coreDetail = tr("Starting"); break;
    case CoreProcess::Running:
        coreDetail = m_core->pid() > 0 ? tr("Running · PID %1").arg(m_core->pid()) : tr("Running (via the service)");
        break;
    case CoreProcess::Stopping: coreDetail = tr("Stopping"); break;
    case CoreProcess::Failed: coreDetail = m_core->lastError(); break;
    }
    list << item(QStringLiteral("core"), tr("Core · %1").arg(coreName), coreDetail, m_core->state() == CoreProcess::Running, true);

    if (m_core->serviceMode()) {
        list << item(QStringLiteral("service"), tr("Service · input-leapd"),
                     m_core->serviceConnected() ? tr("Connected over IPC") : tr("Not reachable: the InputLeap service is not running"),
                     m_core->serviceConnected(), true);
    } else {
        list << item(QStringLiteral("service"), tr("Service"), tr("Not used on %1: the app launches the core itself").arg(Platform::name()), true, false);
    }

    QString discoveryDetail;
    if (!m_discovery->available()) {
        discoveryDetail = m_discovery->unavailableReason();
    } else if (!m_settings->discovery()) {
        discoveryDetail = tr("Turned off in the settings");
    } else if (!m_discovery->active()) {
        discoveryDetail = m_discovery->error().isEmpty() ? tr("Not started") : m_discovery->error();
    } else {
        const int found = role() == Server ? m_discovery->clients().size() : m_discovery->servers().size();
        discoveryDetail = found == 0 ? tr("Advertising %1 · nothing found yet").arg(screenName())
                                     : tr("Advertising %1 · %2 found").arg(screenName()).arg(found);
    }
    list << item(QStringLiteral("discovery"), tr("Discovery · Bonjour"), discoveryDetail,
                 m_discovery->active() && m_discovery->error().isEmpty(), true);

    QString syncDetail;
    if (!m_settings->syncEnabled()) {
        syncDetail = tr("Turned off in the settings");
    } else if (!m_sync->listening()) {
        syncDetail = m_sync->error().isEmpty() ? tr("Not started") : m_sync->error();
    } else if (role() == Server) {
        syncDetail = m_sync->connectedCount() == 0 ? tr("Port %1 · no client sharing yet").arg(m_sync->port())
                                                   : tr("Port %1 · %2 sharing").arg(m_sync->port()).arg(m_sync->connectedCount());
    } else {
        syncDetail = m_sync->serverConnected() ? tr("Port %1 · mirroring %2").arg(m_sync->port()).arg(m_sync->serverName())
                                               : tr("Port %1 · server not reached yet").arg(m_sync->port());
    }
    list << item(QStringLiteral("sync"), tr("Settings channel"), syncDetail,
                 m_sync->listening() && (role() == Server || m_sync->serverConnected()), m_settings->syncEnabled());
    return list;
}

// ---- lifecycle -----------------------------------------------------------------

void AppController::startup()
{
    // INPUTLEAP_UI_DRY_RUN=1 (implied by INPUTLEAP_UI_GRAB): render only, no
    // core, no discovery, no update check; development runs must not touch
    // the fleet.
    const bool dryRun = qEnvironmentVariableIsSet("INPUTLEAP_UI_DRY_RUN") || qEnvironmentVariableIsSet("INPUTLEAP_UI_GRAB");
    // INPUTLEAP_UI_NO_CORE=1: everything but the core (a second window next
    // to the installed one, to look at the pages with live data)
    const bool noCore = qEnvironmentVariableIsSet("INPUTLEAP_UI_NO_CORE");
    refreshAccessibility();
    // a window that was killed leaves its core behind; a second client with
    // the same name confuses the server
    if (!dryRun && !noCore) {
        const qint64 stale = m_settings->corePid();
        if (stale > 0 && CoreProcess::terminateStaleCore(stale, QCoreApplication::applicationDirPath())) {
            log(tr("stopped a core left behind by an earlier window (pid %1)").arg(stale), QStringLiteral("WARNING"));
        }
        m_settings->setCorePid(0);
    }
    if (!Platform::installedInApplicationsFolder()) {
        QVariantMap fields;
        fields.insert(QStringLiteral("text"), tr("Input Leap runs from a disk image. Drag it to the Applications folder and open it from there, or macOS will keep asking for the Accessibility permission."));
        addRequest(QStringLiteral("not-installed"), QStringLiteral("error"), fields);
    }
    // the certificate serves the core's TLS and the settings channel alike
    m_identity->ensureCertificate();
    m_dryRun = dryRun;
    if (dryRun) {
        log(tr("dry run: the core, discovery and update checks stay off"), QStringLiteral("WARNING"));
        emit stateChanged();
        return;
    }
    startSync();
    startDiscovery();
    if (m_settings->launchAtLogin()) {
        Platform::setLaunchAtLogin(true);   // keeps the entry current (path, --hidden)
    }
    if (m_settings->autoCheckUpdates()) {
        m_updates->maybeCheckOnStartup();
    }
    if (noCore) {
        log(tr("development run: the core is not started"), QStringLiteral("WARNING"));
    } else if (m_settings->startedBefore() && m_settings->startCoreOnLaunch()) {
        start();
    }
    emit stateChanged();
}

void AppController::setRunState(RunState state)
{
    if (m_runState == state) {
        return;
    }
    m_runState = state;
    emit stateChanged();
}

void AppController::setRole(int role)
{
    const Role wanted = role == Client ? Client : Server;
    if (wanted == this->role()) {
        return;
    }
    if (!canSwitchRole()) {
        log(tr("stop Input Leap before changing the role"), QStringLiteral("WARNING"));
        return;
    }
    m_settings->setRole(wanted == Client ? AppSettings::Client : AppSettings::Server);
    m_pausedForTrust = false;
    resetLiveState();
    log(tr("role is now %1").arg(wanted == Server ? tr("server") : tr("client")));
    emit layoutSourceChanged();   // layout() is the server's copy on a client
    if (!m_dryRun) {
        startSync();
        startDiscovery();
    }
    emit roleChanged();
    emit stateChanged();
}

// prepare = false only builds the command line (to compare it with the
// running core's): no folder made, no configuration written.
bool AppController::buildLaunch(QString& program, QStringList& arguments, QString& error, bool prepare)
{
    const bool server = role() == Server;
    program = QCoreApplication::applicationDirPath() + QLatin1Char('/')
              + (server ? QStringLiteral("input-leaps") : QStringLiteral("input-leapc"));
#if defined(Q_OS_WIN)
    program += QStringLiteral(".exe");
#endif
    if (!QFile::exists(program)) {
        error = tr("The core program is missing: %1").arg(program);
        return false;
    }

    arguments << QStringLiteral("-f") << QStringLiteral("--no-tray")
              << QStringLiteral("--debug") << m_settings->logLevelName()
              << QStringLiteral("--name") << screenName();
    if (m_core->serviceMode()) {
        arguments << QStringLiteral("--ipc");
#if defined(Q_OS_WIN)
        if (m_settings->elevateMode() == AppSettings::ElevateAsNeeded) {
            arguments << QStringLiteral("--stop-on-desk-switch");
        }
#endif
    }
#if !defined(Q_OS_LINUX)
    if (m_layout->enableDragAndDrop()) {
        arguments << QStringLiteral("--enable-drag-drop");
    }
#endif
    if (!m_settings->cryptoEnabled()) {
        arguments << QStringLiteral("--disable-crypto");
    }
#if defined(Q_OS_WIN)
    // The core's own profile (certificate, trusted fingerprints), the same
    // folder TlsIdentity reads: not the app's data folder. On Windows this
    // also keeps one profile however the core gets launched (elevated or not).
    arguments << QStringLiteral("--profile-dir")
              << QDir::toNativeSeparators(QString::fromStdString(inputleap::DataDirectories::profile().u8string()));
#endif
    if (m_settings->logToFile()) {
        if (prepare) QDir().mkpath(QFileInfo(m_settings->logFilename()).path());
        arguments << QStringLiteral("--log") << m_settings->logFilename();
    }

    if (server) {
        if (!m_settings->requireClientCertificate()) {
            arguments << QStringLiteral("--disable-client-cert-checking");
        }
        QString configPath;
        if (m_settings->useExternalConfig()) {
            configPath = m_settings->configFile();
            if (!QFile::exists(configPath)) {
                error = tr("The configuration file does not exist: %1").arg(configPath);
                return false;
            }
        } else {
            configPath = generatedConfigPath();
        }
        if (prepare && !m_settings->useExternalConfig()) {
            m_layout->setServerName(screenName());
            m_layout->ensureServerPlaced();
            QDir().mkpath(dataFolder());
            if (!m_layout->writeConfig(configPath)) {
                error = tr("Could not write the generated configuration to %1").arg(configPath);
                return false;
            }
        }
        QString address = m_settings->networkInterface();
        if (!address.isEmpty()) {
            address = QStringLiteral("[%1]").arg(address);
        }
        arguments << QStringLiteral("-c") << configPath
                  << QStringLiteral("--address") << address + QStringLiteral(":") + QString::number(m_settings->port());
    } else {
        const QString host = serverAddress();
        if (host.isEmpty()) {
            error = tr("No server to connect to: enter its address or wait for discovery to find one");
            return false;
        }
        if (m_settings->scrollStep() != 120) {
            arguments << QStringLiteral("--yscroll") << QString::number(m_settings->scrollStep());
        }
        arguments << QStringLiteral("[%1]:%2").arg(host).arg(serverPort());
    }
    return true;
}

void AppController::start()
{
    if (m_runState == Starting || m_runState == Running || m_runState == Stopping) {
        return;
    }
    m_wantRunning = true;
    m_pausedForTrust = false;
    m_lastError.clear();
    removeRequest(QStringLiteral("start-error"));
    resetLiveState();

    if (m_settings->cryptoEnabled()) {
        QString message;
        if (!m_identity->ensureCertificate(&message)) {
            log(message, QStringLiteral("ERROR"));
        }
    }
    refreshAccessibility();
    if (!m_accessibilityGranted) {
        Platform::requestAccessibility();
    }

    QString program, error;
    QStringList arguments;
    if (!buildLaunch(program, arguments, error, true)) {
        m_wantRunning = false;
        m_lastError = error;
        log(error, QStringLiteral("ERROR"));
        QVariantMap fields;
        fields.insert(QStringLiteral("text"), error);
        addRequest(QStringLiteral("start-error"), QStringLiteral("error"), fields);
        setRunState(Failed);
        return;
    }

    setRunState(Starting);
    m_launchSignature = launchSignature();
    m_lastStart = QDateTime::currentDateTime();
    log(tr("starting %1").arg(role() == Server ? tr("the server") : tr("the client")));
    m_logModel->append(LogModel::App, QStringLiteral("DEBUG"), tr("command: %1 %2").arg(program, arguments.join(QLatin1Char(' '))));
    m_core->start(program, arguments, m_settings->elevateMode());

    if (!m_settings->startedBefore()) {
        m_settings->setStartedBefore(true);
    }
    if (!m_discovery->active() && !m_dryRun) {
        startDiscovery();
    }
}

void AppController::quit()
{
    if (m_runState != Stopped && m_runState != Failed) {
        stop();
    }
    QCoreApplication::quit();
}

void AppController::stop()
{
    m_wantRunning = false;
    m_restartTimer->stop();
    if (m_core->state() == CoreProcess::Stopped) {
        setRunState(Stopped);
        resetLiveState();
        return;
    }
    setRunState(Stopping);
    log(tr("stopping"));
    m_core->stop();
}

void AppController::restart()
{
    if (m_runState == Stopped || m_runState == Failed) {
        start();
        return;
    }
    const bool wanted = true;
    stop();
    m_wantRunning = wanted;
    m_restartTimer->start(300);
}

// What the core is started with: its command line and, on a server, the
// configuration it reads.
QString AppController::launchSignature()
{
    QString program, error;
    QStringList arguments;
    if (!buildLaunch(program, arguments, error, false)) {
        return QString();   // cannot tell: leave the core alone
    }
    if (role() == Client && !arguments.isEmpty()) {
        // the server's address as set, not as discovery resolved it this time
        arguments.removeLast();
        arguments << m_settings->serverHostname() << QString::number(m_settings->port());
    }
    QString signature = program + QLatin1Char('\n') + arguments.join(QLatin1Char('\n'));
    if (role() == Server && !m_settings->useExternalConfig()) {
        signature += QLatin1Char('\n') + m_layout->configText();
    }
    return signature;
}

// A setting or the layout changed (here or from another window): a running
// core that was started with something else restarts, so the change takes
// effect now.
void AppController::applyIfChanged()
{
    if (m_runState != Running || m_dryRun) {
        return;
    }
    const QString now = launchSignature();
    if (now.isEmpty() || m_launchSignature.isEmpty() || now == m_launchSignature) {
        return;
    }
    log(tr("settings changed: restarting the %1").arg(role() == Server ? tr("server") : tr("client")));
    restart();
}

void AppController::applyLayout()
{
    if (running() && role() == Server) {
        log(tr("layout changed: restarting the server"));
        restart();
    }
}

void AppController::onCoreStateChanged()
{
    if (!m_core->serviceMode()) {
        m_settings->setCorePid(m_core->state() == CoreProcess::Running ? m_core->pid() : 0);
    }
    switch (m_core->state()) {
    case CoreProcess::Starting: setRunState(Starting); break;
    case CoreProcess::Running:
        setRunState(Running);
        m_since = QDateTime();
        break;
    case CoreProcess::Stopping: setRunState(Stopping); break;
    case CoreProcess::Failed:
        m_wantRunning = false;
        m_lastError = m_core->lastError();
        log(m_lastError, QStringLiteral("ERROR"));
        setRunState(Failed);
        break;
    case CoreProcess::Stopped:
        if (!m_wantRunning) {
            setRunState(m_runState == Failed ? Failed : Stopped);
            resetLiveState();
        }
        break;
    }
    emit stateChanged();
}

void AppController::onCoreExited(int exitCode, bool unexpected)
{
    if (!unexpected || !m_wantRunning) {
        if (unexpected && m_pausedForTrust) {
            setRunState(Stopped);
        }
        return;
    }
    // ran for a while: it was a one-off, start counting again
    if (m_lastStart.isValid() && m_lastStart.secsTo(QDateTime::currentDateTime()) > 60) {
        m_unexpectedExits = 0;
    }
    ++m_unexpectedExits;
    if (m_unexpectedExits > kMaxUnexpectedExits) {
        m_wantRunning = false;
        m_lastError = m_core->lastError().isEmpty() ? tr("the core keeps exiting (code %1)").arg(exitCode) : m_core->lastError();
        log(tr("giving up after %1 restarts: %2").arg(kMaxUnexpectedExits).arg(m_lastError), QStringLiteral("ERROR"));
        setRunState(Failed);
        resetLiveState();
        return;
    }
    log(tr("the core exited (code %1), restarting in a second").arg(exitCode), QStringLiteral("WARNING"));
    resetLiveState();
    setRunState(Starting);
    m_restartTimer->start(kRestartDelayMs);
}

void AppController::resetLiveState()
{
    m_connectedClients.clear();
    m_activeScreen.clear();
    m_cursorLocked = false;
    m_cursorLockedElsewhere = false;
    m_dimmed = false;
    m_listening = false;
    m_connectedToServer = false;
    m_connectedServer.clear();
    m_since = QDateTime();
    m_layout->clearLiveStates();
    emit stateChanged();
}

// ---- the core's output ----------------------------------------------------------

void AppController::handleCoreLine(const QString& line)
{
    const CoreLogLine parsed = CoreEventParser::splitLine(line);
    m_logModel->append(LogModel::Core, parsed.level, parsed.message);
    m_lastEvent = parsed.message;
    handleEvent(CoreEventParser::parse(parsed.message));
    if (m_settings->overlayHints()) {
        inputleap::fork_gui::routeLogLineToOverlay(line);
    }
    emit stateChanged();
}

void AppController::handleEvent(const CoreEvent& event)
{
    using Type = CoreEvent::Type;
    switch (event.type) {
    case Type::ServerStarted:
        m_listening = true;
        m_since = QDateTime::currentDateTime();
        m_unexpectedExits = 0;
        break;
    case Type::ServerStopped:
        m_listening = false;
        break;
    case Type::ClientConnected:
        m_connectedClients.insert(event.name);
        m_layout->setLiveState(event.name, QStringLiteral("connected"));
        emit notify(tr("Client connected"), tr("%1 is now sharing your keyboard and mouse").arg(event.name));
        break;
    case Type::ClientDisconnected:
        m_connectedClients.remove(event.name);
        m_layout->setLiveState(event.name, QStringLiteral("offline"));
        if (m_activeScreen == event.name) m_activeScreen.clear();
        break;
    case Type::ClientRefused:
        if (!event.name.isEmpty() && !layout()->hasScreen(event.name)) {
            requestJoin(event.name);
        }
        break;
    case Type::SwitchedScreen:
        m_activeScreen = event.name;
        break;
    case Type::CursorLockChanged:
        if (event.detail == QLatin1String("elsewhere")) {
            m_cursorLockedElsewhere = event.flag;
        } else {
            m_cursorLocked = event.flag;
            if (!event.flag) m_cursorLockedElsewhere = false;
        }
        break;
    case Type::ScreenDimmed: m_dimmed = true; break;
    case Type::ScreenRestored: m_dimmed = false; break;
    case Type::ConnectedToServer:
        m_connectedToServer = true;
        m_connectedServer = serverAddress();
        m_since = QDateTime::currentDateTime();
        m_unexpectedExits = 0;
        break;
    case Type::DisconnectedFromServer:
        m_connectedToServer = false;
        m_since = QDateTime();
        break;
    case Type::ConnectFailed:
        m_connectedToServer = false;
        m_lastError = event.detail;
        break;
    case Type::PeerFingerprint: {
        const bool server = role() == Server;
        if (m_identity->isTrusted(server, event.sha256) || m_knownFingerprints.contains(event.sha256)) {
            break;
        }
        m_knownFingerprints.insert(event.sha256);
        const QString id = QStringLiteral("trust:") + event.sha256;
        QVariantMap fields;
        fields.insert(QStringLiteral("sha1"), event.sha1);
        fields.insert(QStringLiteral("sha256"), event.sha256);
        fields.insert(QStringLiteral("columns"), TlsIdentity::columnsFor(event.sha256));
        fields.insert(QStringLiteral("randomart"), TlsIdentity::randomartFor(event.sha256));
        fields.insert(QStringLiteral("peerRole"), server ? QStringLiteral("client") : QStringLiteral("server"));
        fields.insert(QStringLiteral("name"), server ? QString() : serverAddress());
        addRequest(id, QStringLiteral("trust"), fields);
        emit notify(tr("New %1 fingerprint").arg(server ? tr("client") : tr("server")), tr("Open Input Leap to check and trust it"));
        if (!server) {
            // the client would retry every second and never get in; wait for the decision
            m_pausedForTrust = true;
            stop();
        }
        break;
    }
    case Type::Fatal:
        m_lastError = event.detail;
        break;
    case Type::ClipboardUpdated:
    case Type::None:
        break;
    }
}

// ---- discovery --------------------------------------------------------------------

void AppController::onClientFound(const QString& name)
{
    log(tr("discovery found client %1").arg(name));
    if (role() == Server && !layout()->hasScreen(name) && !layout()->ignoreAutoConfigClient()) {
        requestJoin(name);
    }
    emit stateChanged();
}

void AppController::onServerFound(const QString& name)
{
    log(tr("discovery found server %1").arg(name));
    if (role() == Client && m_settings->serverHostname().isEmpty() && running() && !m_connectedToServer) {
        restart();
    }
    emit stateChanged();
}

void AppController::onPeerResolved(const QString& name, const QString& host, quint16 port, const QMap<QString, QString>& txt)
{
    log(tr("%1 is %2 · channel port %3%4").arg(name, host).arg(port)
            .arg(txt.value(QStringLiteral("sync")) == QLatin1String("1") ? QString() : tr(" · no settings channel")));
    // the record's display list is a first guess; the settings channel, once
    // the computer is in, has the last word
    if (role() == Server && txt.contains(QStringLiteral("d")) && m_sync->peerDocument(name, QStringLiteral("displays")).isEmpty()) {
        onDisplaysReported(name, LayoutModel::displaysFromText(txt.value(QStringLiteral("d"))));
    }
    if (role() == Client && txt.value(QStringLiteral("sync")) == QLatin1String("1") && m_sync->listening()) {
        m_sync->connectTo(name, host, port);
    }
    emit stateChanged();
}

void AppController::startDiscovery()
{
    if (!m_settings->discovery() || !m_discovery->available()) {
        return;
    }
    QMap<QString, QString> txt;
    txt.insert(QStringLiteral("sync"), m_sync->listening() ? QStringLiteral("1") : QStringLiteral("0"));
    txt.insert(QStringLiteral("port"), QString::number(m_settings->port()));
    txt.insert(QStringLiteral("v"), m_version);
    m_advertisedDisplays = LayoutModel::displaysToText(m_localDisplays);
    if (m_advertisedDisplays.size() > 250) {   // one TXT entry holds 255 bytes: the sizes go first
        m_advertisedDisplays = LayoutModel::displaysToText(m_localDisplays, false);
    }
    if (!m_advertisedDisplays.isEmpty() && m_advertisedDisplays.size() <= 250) {
        txt.insert(QStringLiteral("d"), m_advertisedDisplays);
    }
    m_discovery->start(role() == Server, screenName(), quint16(m_sync->port()), txt);
}

// ---- the settings channel (docs/ui-redesign.md §4.7) --------------------------

void AppController::startSync()
{
    m_sync->stop();
    m_syncName = screenName();
    if (!m_settings->syncEnabled()) {
        return;
    }
    QString error;
    if (!m_sync->setIdentity(m_identity->certificatePath(), &error)) {
        log(tr("settings channel off: %1").arg(error), QStringLiteral("WARNING"));
        return;
    }
    m_sync->setLocalInfo(screenName(), role() == Server ? QStringLiteral("server") : QStringLiteral("client"), m_version);
    m_sync->setTrustCheck([this](const QString& sha256, const QString& name, bool incoming) {
        return syncTrusted(sha256, name, incoming);
    });
    m_sync->setRequestHandler([this](const QString& peer, const QString& kind, const QJsonObject& body, int access) {
        return handleSyncRequest(peer, kind, body, access);
    });
    if (!m_sync->start()) {
        log(tr("settings channel off: %1").arg(m_sync->error()), QStringLiteral("WARNING"));
        return;
    }
    log(tr("settings channel listening on port %1 (%2)").arg(m_sync->port()).arg(m_sync->backend()));
    publishSync();
    if (role() == Client) {
        const QStringList servers = m_discovery->servers();
        for (const QString& name : servers) {
            Discovery::Peer peer;
            if (m_discovery->resolvedPeer(name, peer) && peer.sync()) {
                m_sync->connectTo(name, peer.host, peer.port);
            }
        }
        dialServerAddress();
    }
}

// A server address typed by hand (discovery off, or a different network):
// its window listens on the well-known channel port.
void AppController::dialServerAddress()
{
    const QString host = m_settings->serverHostname();
    if (!m_syncServerHost.isEmpty() && m_syncServerHost != host) {
        m_sync->forget(m_syncServerHost + QLatin1Char(':') + QString::number(SyncService::kDefaultPort));
    }
    m_syncServerHost = host;
    if (role() == Client && m_sync->listening() && !host.isEmpty()) {
        m_sync->connectTo(QString(), host, SyncService::kDefaultPort);
    }
}

void AppController::restartSync()
{
    if (m_dryRun) {
        return;
    }
    startSync();
    startDiscovery();
}

// Who may see and change this computer's settings: the same posture as the
// core. A peer whose certificate is in the trusted database may change
// them. A server also lets any computer of its layout read them when it
// does not check client certificates; a client lets the server it found
// read them when TLS is off altogether. Nobody else gets in.
int AppController::syncTrusted(const QString& sha256, const QString& name, bool incoming)
{
    if (role() == Server) {
        if (!incoming) {
            return SyncService::None;
        }
        if (m_identity->isTrusted(true, sha256)) {
            return SyncService::Write;
        }
        // a computer of the layout, or one asking to join: offer to trust it,
        // which lets it change settings from its own window
        if (!name.isEmpty() && !sha256.isEmpty() && (layout()->hasScreen(name) || m_discovery->clients().contains(name))) {
            const QString id = QStringLiteral("trust:") + sha256;
            QTimer::singleShot(0, this, [this, id, sha256, name]() {
                if (hasRequest(id) || m_rejectedSyncPeers.contains(sha256) || m_identity->isTrusted(true, sha256)) return;
                QVariantMap fields;
                fields.insert(QStringLiteral("sha256"), sha256);
                fields.insert(QStringLiteral("columns"), TlsIdentity::columnsFor(sha256));
                fields.insert(QStringLiteral("randomart"), TlsIdentity::randomartFor(sha256));
                fields.insert(QStringLiteral("peerRole"), QStringLiteral("client"));
                fields.insert(QStringLiteral("name"), name);
                fields.insert(QStringLiteral("reason"), QStringLiteral("settings"));
                addRequest(id, QStringLiteral("trust"), fields);
                emit notify(tr("%1 wants to change settings").arg(name), tr("Open Input Leap to check its fingerprint and trust it"));
            });
        }
        return !m_settings->requireClientCertificate() && layout()->hasScreen(name) ? SyncService::Read : SyncService::None;
    }
    if (incoming) {
        return SyncService::None;
    }
    if (m_identity->isTrusted(false, sha256)) {
        return SyncService::Write;
    }
    const bool found = m_discovery->servers().contains(name) || serverAddress() == name;
    return !m_settings->cryptoEnabled() && found ? SyncService::Read : SyncService::None;
}

// A peer changes one of this computer's own settings, or asks it to act.
// The ack says where and when it lands ("on mac-studio, after its client
// restarts"), which the other window shows next to the control.
QJsonObject AppController::handleSyncRequest(const QString& peer, const QString& kind, const QJsonObject& body, int access)
{
    auto answer = [](bool ok, const QString& text) {
        QJsonObject ack;
        ack.insert(QStringLiteral("ok"), ok);
        ack.insert(QStringLiteral("text"), text);
        return ack;
    };
    const QString me = screenName();
    if (access < SyncService::Write) {
        return answer(false, tr("%1 may read %2's settings but not change them: its certificate is not trusted here").arg(peer, me));
    }
    // a request the server relayed names who asked
    const QString relayedBy = body.value(QStringLiteral("by")).toString();
    const QString by = relayedBy.isEmpty() || relayedBy == peer ? peer : tr("%1 (via %2)").arg(relayedBy, peer);
    const QString coreWord = role() == Server ? tr("server") : tr("client");

    if (kind == QLatin1String("relay")) {
        return relayRequest(peer, body);
    }
    if (kind == QLatin1String("set-config")) {
        if (role() != Server) return answer(false, tr("%1 is not the server").arg(me));
        if (m_externalActive) return answer(false, tr("%1 runs the configuration file %2: edit it there").arg(me, m_externalPath));
        QString error;
        if (!m_layout->replaceWith(body.value(QStringLiteral("layout")).toObject(), &error)) {
            return answer(false, tr("%1 refused the layout: %2").arg(me, error));
        }
        log(tr("%1 changed the layout").arg(by));
        return answer(true, running() ? tr("applied on %1; the server restarts").arg(me) : tr("saved on %1").arg(me));
    }
    if (kind == QLatin1String("action")) {
        const QString action = body.value(QStringLiteral("action")).toString();
        log(tr("%1 asked for %2").arg(by, action));
        if (action == QLatin1String("restart")) {
            if (!running()) return answer(false, tr("%1 is not running its %2").arg(me, coreWord));
            restart();
            return answer(true, tr("%1 is restarting its %2").arg(me, coreWord));
        }
        if (action == QLatin1String("restartFleet")) {
            if (role() != Server) return answer(false, tr("%1 is not the server").arg(me));
            const int clients = restartClients(relayedBy.isEmpty() ? peer : relayedBy);
            restart();
            return answer(true, tr("%1 is restarting with %n client(s)", "", clients).arg(me));
        }
        if (action == QLatin1String("start")) {
            if (running()) return answer(true, tr("%1 is already running").arg(me));
            start();
            return answer(true, tr("%1 is starting its %2").arg(me, coreWord));
        }
        if (action == QLatin1String("stop")) {
            stop();
            return answer(true, tr("%1 stopped its %2").arg(me, coreWord));
        }
        if (action == QLatin1String("restartDiscovery")) {
            startDiscovery();
            return answer(true, tr("%1 restarted discovery").arg(me));
        }
        if (action == QLatin1String("checkForUpdates")) {
            checkForUpdates();
            return answer(true, tr("%1 is checking for updates").arg(me));
        }
        return answer(false, tr("%1 does not know the action %2").arg(me, action));
    }
    if (kind != QLatin1String("set")) {
        return answer(false, tr("%1 does not understand %2").arg(me, kind));
    }
    return applySetting(by, body.value(QStringLiteral("key")).toString(), body.value(QStringLiteral("value")));
}

// One of this computer's settings, from another window (by) or, on the
// server, from its own Server page (by empty). Changes that the core reads
// take effect when it restarts, which happens by itself (applyIfChanged).
QJsonObject AppController::applySetting(const QString& by, const QString& key, const QJsonValue& value)
{
    auto answer = [](bool ok, const QString& text) {
        QJsonObject ack;
        ack.insert(QStringLiteral("ok"), ok);
        ack.insert(QStringLiteral("text"), text);
        return ack;
    };
    const QString me = screenName();
    const QString coreWord = role() == Server ? tr("server") : tr("client");
    const bool server = role() == Server;
    enum When { Now, CoreRestart, NextLaunch } when = Now;
    static const QRegularExpression namePattern(QStringLiteral("^[A-Za-z0-9._-]{1,255}$"));
    if (key == QLatin1String("screenName")) {
        const QString name = value.toString().trimmed();
        if (!namePattern.match(name).hasMatch()) return answer(false, tr("%1 is not a valid screen name").arg(name));
        m_settings->setScreenName(name);
        when = CoreRestart;
    } else if (key == QLatin1String("serverHostname")) {
        m_settings->setServerHostname(value.toString());
        when = CoreRestart;
    } else if (key == QLatin1String("port")) {
        const int port = value.toInt();
        if (port < 1 || port > 65535) return answer(false, tr("%1 is not a valid port").arg(port));
        m_settings->setPort(port);
        when = CoreRestart;
    } else if (key == QLatin1String("discovery")) {
        m_settings->setDiscovery(value.toBool());
        if (value.toBool()) startDiscovery(); else m_discovery->stop();
    } else if (key == QLatin1String("scrollStep")) {
        const int step = value.toInt();
        if (step < 1 || step > 10000) return answer(false, tr("%1 is not a valid scroll step").arg(step));
        m_settings->setScrollStep(step);
        when = CoreRestart;
    } else if (key == QLatin1String("overlayHints")) {
        m_settings->setOverlayHints(value.toBool());
    } else if (key == QLatin1String("logLevel")) {
        const int level = value.toInt(-1);
        if (level < 0 || level > 6) return answer(false, tr("%1 is not a log level").arg(level));
        m_settings->setLogLevel(level);
        when = CoreRestart;
    } else if (key == QLatin1String("launchAtLogin")) {
        setLaunchAtLogin(value.toBool());
    } else if (key == QLatin1String("startCoreOnLaunch")) {
        m_settings->setStartCoreOnLaunch(value.toBool());
        when = NextLaunch;
    } else if (key == QLatin1String("openHidden")) {
        m_settings->setOpenHidden(value.toBool());
        when = NextLaunch;
    } else if (key == QLatin1String("cryptoEnabled")) {
        m_settings->setCryptoEnabled(value.toBool());
        when = CoreRestart;
    } else if (server && key == QLatin1String("networkInterface")) {
        const QString address = value.toString().trimmed();
        if (!address.isEmpty() && QHostAddress(address).isNull()) return answer(false, tr("%1 is not an address").arg(address));
        m_settings->setNetworkInterface(address);
        when = CoreRestart;
    } else if (server && key == QLatin1String("requireClientCertificate")) {
        m_settings->setRequireClientCertificate(value.toBool());
        when = CoreRestart;
    } else if (server && key == QLatin1String("useExternalConfig")) {
        m_settings->setUseExternalConfig(value.toBool());
        when = CoreRestart;
    } else if (server && key == QLatin1String("configFile")) {
        m_settings->setConfigFile(value.toString());
        when = CoreRestart;
    } else {
        return answer(false, tr("%1 cannot be changed from another computer").arg(key));
    }

    const QString shown = value.isBool() ? (value.toBool() ? tr("on") : tr("off")) : value.toVariant().toString();
    if (!by.isEmpty()) {
        log(tr("%1 set %2 to %3").arg(by, key, shown));
        emit notify(tr("%1 changed a setting").arg(by), tr("%1 is now %2").arg(key, shown));
    }
    QString text;
    switch (when) {
    case Now: text = tr("applied on %1").arg(me); break;
    case CoreRestart: text = running() ? tr("applied on %1; its %2 restarts").arg(me, coreWord) : tr("on %1, at its next start").arg(me); break;
    case NextLaunch: text = tr("on %1, at its next launch").arg(me); break;
    }
    QJsonObject ack = answer(true, text);
    ack.insert(QStringLiteral("applied"), when == Now ? QStringLiteral("now") : when == CoreRestart ? QStringLiteral("restart") : QStringLiteral("launch"));
    return ack;
}

// Server: a client's request for another client (or this server). It goes on
// as this server's request, so it lands only where both let it: the asker may
// change things here, and the target lets this server change things there.
QJsonObject AppController::relayRequest(const QString& peer, const QJsonObject& body)
{
    auto answer = [](bool ok, const QString& text) {
        QJsonObject ack;
        ack.insert(QStringLiteral("ok"), ok);
        ack.insert(QStringLiteral("text"), text);
        return ack;
    };
    if (role() != Server) {
        return answer(false, tr("%1 is not the server: it relays nothing").arg(screenName()));
    }
    const QString target = body.value(QStringLiteral("target")).toString();
    const QString kind = body.value(QStringLiteral("kind")).toString();
    QJsonObject inner = body.value(QStringLiteral("body")).toObject();
    inner.remove(QStringLiteral("id"));
    inner.insert(QStringLiteral("by"), peer);
    if (kind != QLatin1String("set") && kind != QLatin1String("action")) {
        return answer(false, tr("%1 does not relay %2").arg(screenName(), kind));
    }
    if (target == screenName()) {
        return handleSyncRequest(peer, kind, inner, SyncService::Write);
    }
    const QVariantMap there = m_sync->peer(target);
    if (!there.value(QStringLiteral("connected")).toBool()) {
        return answer(false, tr("%1 is not connected to %2").arg(target, screenName()));
    }
    if (there.value(QStringLiteral("accessThere")).toInt() < SyncService::Write) {
        return answer(false, tr("%1 does not let %2 change its settings: it does not trust the server's certificate").arg(target, screenName()));
    }
    const QJsonValue id = body.value(QStringLiteral("id"));
    m_sync->request(target, kind, inner, [this, peer, id](const QJsonObject& ack) {
        m_sync->answer(peer, id, ack);
    }, 7000);
    QJsonObject deferred;
    deferred.insert(QStringLiteral("deferred"), true);
    return deferred;
}

void AppController::setRemote(const QString& computer, const QString& key, const QVariant& value)
{
    QJsonObject body;
    body.insert(QStringLiteral("key"), key);
    body.insert(QStringLiteral("value"), QJsonValue::fromVariant(value));
    QVariantMap pending;
    pending.insert(QStringLiteral("pending"), true);
    pending.insert(QStringLiteral("text"), tr("sending to %1…").arg(computer));
    m_remoteResults[computer][key] = pending;
    emit stateChanged();
    requestTo(computer, QStringLiteral("set"), body, [this, computer, key](const QJsonObject& ack) {
        QVariantMap result;
        result.insert(QStringLiteral("pending"), false);
        result.insert(QStringLiteral("ok"), ack.value(QStringLiteral("ok")).toBool());
        result.insert(QStringLiteral("text"), ack.value(QStringLiteral("text")).toString());
        m_remoteResults[computer][key] = result;
        log(tr("%1 · %2: %3").arg(computer, key, ack.value(QStringLiteral("text")).toString()),
            ack.value(QStringLiteral("ok")).toBool() ? QStringLiteral("INFO") : QStringLiteral("WARNING"));
        emit stateChanged();
    });
}

void AppController::remoteAction(const QString& computer, const QString& action)
{
    QJsonObject body;
    body.insert(QStringLiteral("action"), action);
    const QString key = QStringLiteral("action");
    QVariantMap pending;
    pending.insert(QStringLiteral("pending"), true);
    pending.insert(QStringLiteral("text"), tr("asking %1…").arg(computer));
    m_remoteResults[computer][key] = pending;
    emit stateChanged();
    requestTo(computer, QStringLiteral("action"), body, [this, computer, key](const QJsonObject& ack) {
        QVariantMap result;
        result.insert(QStringLiteral("pending"), false);
        result.insert(QStringLiteral("ok"), ack.value(QStringLiteral("ok")).toBool());
        result.insert(QStringLiteral("text"), ack.value(QStringLiteral("text")).toString());
        m_remoteResults[computer][key] = result;
        log(tr("%1: %2").arg(computer, ack.value(QStringLiteral("text")).toString()),
            ack.value(QStringLiteral("ok")).toBool() ? QStringLiteral("INFO") : QStringLiteral("WARNING"));
        emit stateChanged();
    });
}

QVariantMap AppController::remoteResult(const QString& computer, const QString& key) const
{
    return m_remoteResults.value(computer).value(key);
}

// To a computer this window talks to, or, for another client, through the
// server, which forwards it when both let it.
void AppController::requestTo(const QString& computer, const QString& kind, const QJsonObject& body, SyncService::AckHandler handler)
{
    if (m_sync->peer(computer).value(QStringLiteral("connected")).toBool() || role() == Server || !m_sync->serverConnected()) {
        m_sync->request(computer, kind, body, std::move(handler));
        return;
    }
    QJsonObject relay;
    relay.insert(QStringLiteral("target"), computer);
    relay.insert(QStringLiteral("kind"), kind);
    relay.insert(QStringLiteral("body"), body);
    m_sync->request(m_sync->serverName(), QStringLiteral("relay"), relay, std::move(handler));
}

void AppController::restartFleet()
{
    if (role() == Server) {
        const int clients = restartClients(screenName());
        log(tr("restarting the fleet: %n client(s) and this server", "", clients));
        restart();
        return;
    }
    remoteAction(m_sync->serverName(), QStringLiteral("restartFleet"));
}

// Server: asks every client that lets it to restart its core.
int AppController::restartClients(const QString& by)
{
    int count = 0;
    const QVariantList peers = m_sync->peers();
    for (const QVariant& p : peers) {
        const QVariantMap peer = p.toMap();
        if (!peer.value(QStringLiteral("connected")).toBool() || peer.value(QStringLiteral("accessThere")).toInt() < SyncService::Write) {
            continue;
        }
        const QString name = peer.value(QStringLiteral("name")).toString();
        QJsonObject body;
        body.insert(QStringLiteral("action"), QStringLiteral("restart"));
        body.insert(QStringLiteral("by"), by);
        m_sync->request(name, QStringLiteral("action"), body, [this, name](const QJsonObject& ack) {
            log(tr("%1: %2").arg(name, ack.value(QStringLiteral("text")).toString()),
                ack.value(QStringLiteral("ok")).toBool() ? QStringLiteral("INFO") : QStringLiteral("WARNING"));
        });
        ++count;
    }
    return count;
}

QVariantMap AppController::peerInfo(const QString& name) const
{
    QVariantMap direct = m_sync->peer(name);
    if (direct.value(QStringLiteral("connected")).toBool() || role() == Server) {
        return direct;
    }
    // another client: as the server shares it
    const QJsonArray fleet = m_sync->serverDocument(QStringLiteral("fleet")).value(QStringLiteral("computers")).toArray();
    for (const QJsonValue& value : fleet) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("name")).toString() != name) continue;
        const QJsonObject state = entry.value(QStringLiteral("state")).toObject();
        QVariantMap map;
        map.insert(QStringLiteral("name"), name);
        map.insert(QStringLiteral("role"), entry.value(QStringLiteral("role")).toString());
        map.insert(QStringLiteral("version"), entry.value(QStringLiteral("version")).toString());
        map.insert(QStringLiteral("connected"), true);
        map.insert(QStringLiteral("relayed"), true);
        map.insert(QStringLiteral("settings"), entry.value(QStringLiteral("settings")).toObject().toVariantMap());
        map.insert(QStringLiteral("state"), state.toVariantMap());
        map.insert(QStringLiteral("processes"), state.value(QStringLiteral("processes")).toArray().toVariantList());
        // changes reach it when the server lets this computer and it lets the server
        const bool writable = serverSettingsEditable() && entry.value(QStringLiteral("accessThere")).toInt() >= SyncService::Write;
        map.insert(QStringLiteral("accessThere"), writable ? int(SyncService::Write) : int(SyncService::Read));
        return map;
    }
    return direct;
}

// This computer's monitors: to the server over the settings channel, in the
// zeroconf record for a server that has not let it in yet, and into the
// layout directly when this computer is the server.
void AppController::publishDisplays()
{
    QVector<DisplayConfig> displays;
    const QVector<Platform::Display> found = Platform::displays();
    for (const Platform::Display& p : found) {
        DisplayConfig d;
        d.x = p.rect.x();
        d.y = p.rect.y();
        d.width = p.rect.width();
        d.height = p.rect.height();
        d.mmWidth = p.mm.width();
        d.mmHeight = p.mm.height();
        displays << d;
    }
    displays = LayoutModel::cleanDisplays(displays);
    if (displays.isEmpty() || displays == m_localDisplays) {
        return;
    }
    m_localDisplays = displays;
    log(tr("this computer's displays: %1").arg(LayoutModel::displaysToText(displays)));
    QJsonObject doc;
    doc.insert(QStringLiteral("displays"), LayoutModel::displaysToJson(displays));
    m_sync->setDocument(QStringLiteral("displays"), doc);
    onDisplaysReported(screenName(), displays);
    if (m_discovery->active() && LayoutModel::displaysToText(displays) != m_advertisedDisplays) {
        startDiscovery();
    }
}

void AppController::onDisplaysReported(const QString& name, const QVector<DisplayConfig>& displays)
{
    if (role() != Server || displays.isEmpty()) {
        return;
    }
    if (m_layout->reportDisplays(name, displays)) {
        log(tr("%1 reports its displays: %2").arg(name, LayoutModel::displaysToText(displays)));
    }
}

void AppController::publishSync()
{
    if (!m_sync->listening()) {
        return;
    }
    QJsonObject settings;
    settings.insert(QStringLiteral("screenName"), screenName());
    settings.insert(QStringLiteral("role"), role() == Server ? QStringLiteral("server") : QStringLiteral("client"));
    settings.insert(QStringLiteral("serverHostname"), m_settings->serverHostname());
    settings.insert(QStringLiteral("discovery"), m_settings->discovery());
    settings.insert(QStringLiteral("scrollStep"), m_settings->scrollStep());
    settings.insert(QStringLiteral("overlayHints"), m_settings->overlayHints());
    settings.insert(QStringLiteral("logLevel"), m_settings->logLevel());
    settings.insert(QStringLiteral("logLevelName"), m_settings->logLevelName());
    settings.insert(QStringLiteral("launchAtLogin"), m_settings->launchAtLogin());
    settings.insert(QStringLiteral("startCoreOnLaunch"), m_settings->startCoreOnLaunch());
    settings.insert(QStringLiteral("port"), m_settings->port());
    settings.insert(QStringLiteral("networkInterface"), m_settings->networkInterface());
    settings.insert(QStringLiteral("cryptoEnabled"), m_settings->cryptoEnabled());
    settings.insert(QStringLiteral("requireClientCertificate"), m_settings->requireClientCertificate());
    settings.insert(QStringLiteral("version"), m_version);
    settings.insert(QStringLiteral("platform"), Platform::name());
    settings.insert(QStringLiteral("addresses"), QJsonArray::fromStringList(m_addresses));
    settings.insert(QStringLiteral("fingerprint"), m_identity->sha256());
    m_sync->setDocument(QStringLiteral("settings"), settings);

    QJsonObject state;
    state.insert(QStringLiteral("runState"), int(m_runState));
    state.insert(QStringLiteral("statusText"), statusText());
    state.insert(QStringLiteral("connected"), connected());
    state.insert(QStringLiteral("activeScreen"), m_activeScreen);
    state.insert(QStringLiteral("cursorLocked"), m_cursorLocked);
    state.insert(QStringLiteral("dimmed"), m_dimmed);
    state.insert(QStringLiteral("connectedClients"), QJsonArray::fromStringList(connectedClients()));
    state.insert(QStringLiteral("since"), m_since.isValid() ? m_since.toString(Qt::ISODate) : QString());
    state.insert(QStringLiteral("lastEvent"), m_lastEvent);
    state.insert(QStringLiteral("processes"), QJsonArray::fromVariantList(processes()));
    m_sync->setDocument(QStringLiteral("state"), state);

    if (role() == Server) {
        QJsonObject config;
        QJsonObject server;
        server.insert(QStringLiteral("port"), m_settings->port());
        server.insert(QStringLiteral("networkInterface"), m_settings->networkInterface());
        server.insert(QStringLiteral("cryptoEnabled"), m_settings->cryptoEnabled());
        server.insert(QStringLiteral("requireClientCertificate"), m_settings->requireClientCertificate());
        server.insert(QStringLiteral("useExternalConfig"), m_settings->useExternalConfig());
        server.insert(QStringLiteral("configFile"), m_settings->configFile());
        server.insert(QStringLiteral("generatedConfigPath"), generatedConfigPath());
        config.insert(QStringLiteral("settings"), server);
        config.insert(QStringLiteral("layout"), layout()->toJson());
        config.insert(QStringLiteral("conf"), layout()->configText());
        m_sync->setDocument(QStringLiteral("server-config"), config);

        // every client this server talks to, so each client sees the others
        QJsonArray computers;
        const QVariantList peers = m_sync->peers();
        for (const QVariant& p : peers) {
            const QVariantMap peer = p.toMap();
            if (!peer.value(QStringLiteral("connected")).toBool()) continue;
            QJsonObject entry;
            entry.insert(QStringLiteral("name"), peer.value(QStringLiteral("name")).toString());
            entry.insert(QStringLiteral("role"), peer.value(QStringLiteral("role")).toString());
            entry.insert(QStringLiteral("version"), peer.value(QStringLiteral("version")).toString());
            entry.insert(QStringLiteral("accessThere"), peer.value(QStringLiteral("accessThere")).toInt());
            entry.insert(QStringLiteral("settings"), QJsonObject::fromVariantMap(peer.value(QStringLiteral("settings")).toMap()));
            entry.insert(QStringLiteral("state"), QJsonObject::fromVariantMap(peer.value(QStringLiteral("state")).toMap()));
            computers << entry;
        }
        QJsonObject fleet;
        fleet.insert(QStringLiteral("computers"), computers);
        m_sync->setDocument(QStringLiteral("fleet"), fleet);
    }
}

void AppController::requestJoin(const QString& name)
{
    if (m_ignoredJoins.contains(name) || hasRequest(QStringLiteral("join:") + name)) {
        return;
    }
    QVariantMap fields;
    fields.insert(QStringLiteral("name"), name);
    addRequest(QStringLiteral("join:") + name, QStringLiteral("join"), fields);
    emit notify(tr("%1 wants to join").arg(name), tr("Open Input Leap to place it on the layout"));
}

void AppController::placeJoin(const QString& name, int direction)
{
    if (m_externalActive) {
        removeRequest(QStringLiteral("join:") + name);
        log(tr("%1 asked to join, but the layout comes from %2: add it to that file").arg(name, m_externalPath), QStringLiteral("WARNING"));
        return;
    }
    bool placed = false;
    m_layout->setServerName(screenName());
    m_layout->ensureServerPlaced();
    if (direction >= 0) {
        placed = m_layout->placeNextTo(name, screenName(), direction);
    }
    if (!placed) {
        placed = m_layout->addScreenAnywhere(name);
    }
    removeRequest(QStringLiteral("join:") + name);
    if (!placed) {
        log(tr("no room for %1 in the layout").arg(name), QStringLiteral("WARNING"));
        return;
    }
    log(tr("placed %1 %2").arg(name, m_layout->positionText(m_layout->indexOf(name))));
    applyLayout();
}

void AppController::ignoreJoin(const QString& name)
{
    m_ignoredJoins.insert(name);
    removeRequest(QStringLiteral("join:") + name);
    log(tr("ignored %1 for this session").arg(name));
}

// ---- requests ---------------------------------------------------------------------

void AppController::addRequest(const QString& id, const QString& kind, const QVariantMap& fields)
{
    removeRequest(id);
    QVariantMap request = fields;
    request.insert(QStringLiteral("id"), id);
    request.insert(QStringLiteral("kind"), kind);
    m_requests.prepend(request);
    emit requestsChanged();
}

bool AppController::hasRequest(const QString& id) const
{
    for (const QVariant& r : m_requests) {
        if (r.toMap().value(QStringLiteral("id")).toString() == id) {
            return true;
        }
    }
    return false;
}

void AppController::removeRequest(const QString& id)
{
    for (int i = 0; i < m_requests.size(); ++i) {
        if (m_requests[i].toMap().value(QStringLiteral("id")).toString() == id) {
            m_requests.removeAt(i);
            emit requestsChanged();
            return;
        }
    }
}

void AppController::acceptTrust(const QString& id)
{
    for (const QVariant& r : m_requests) {
        const QVariantMap map = r.toMap();
        if (map.value(QStringLiteral("id")).toString() != id) continue;
        const bool server = role() == Server;
        m_identity->trust(server, map.value(QStringLiteral("sha1")).toString(),
                          map.value(QStringLiteral("sha256")).toString(), map.value(QStringLiteral("name")).toString());
        log(tr("trusted %1 fingerprint %2").arg(map.value(QStringLiteral("peerRole")).toString(),
                                                 map.value(QStringLiteral("sha256")).toString().left(11)));
        removeRequest(id);
        if (m_pausedForTrust) {
            m_pausedForTrust = false;
            start();
        }
        return;
    }
}

void AppController::rejectTrust(const QString& id)
{
    for (const QVariant& r : std::as_const(m_requests)) {
        const QVariantMap map = r.toMap();
        if (map.value(QStringLiteral("id")).toString() == id && map.value(QStringLiteral("reason")).toString() == QLatin1String("settings")) {
            m_rejectedSyncPeers.insert(map.value(QStringLiteral("sha256")).toString());   // its window reconnects often: ask once a session
        }
    }
    removeRequest(id);
    m_pausedForTrust = false;
    log(tr("rejected a fingerprint; it will be asked again on the next connection"));
    emit stateChanged();
}

void AppController::dismissRequest(const QString& id)
{
    removeRequest(id);
}

// ---- misc actions -----------------------------------------------------------------

void AppController::checkForUpdates()
{
    m_updateStatus = tr("Checking…");
    emit stateChanged();
    m_updates->check(false);
}

void AppController::installUpdate()
{
    if (!m_updateAssetUrl.isEmpty()) {
        m_updates->startSelfUpdate(m_updateAssetUrl, m_updateAssetName);
    }
}

void AppController::refreshAccessibility()
{
    const bool granted = Platform::accessibilityGranted();
    if (granted != m_accessibilityGranted) {
        m_accessibilityGranted = granted;
        emit stateChanged();
    }
    if (!granted) {
        QVariantMap fields;
        fields.insert(QStringLiteral("text"), tr("macOS has not granted Input Leap the Accessibility permission, which it needs to see and inject keyboard and mouse input."));
        if (!hasRequest(QStringLiteral("accessibility"))) {
            addRequest(QStringLiteral("accessibility"), QStringLiteral("permission"), fields);
        }
    } else {
        removeRequest(QStringLiteral("accessibility"));
    }
}

void AppController::requestAccessibility()
{
    Platform::requestAccessibility();
    QTimer::singleShot(3000, this, &AppController::refreshAccessibility);
}

void AppController::openLogsFolder()
{
    const QString path = m_settings->logToFile() ? QFileInfo(m_settings->logFilename()).path() : dataFolder();
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void AppController::openDataFolder()
{
    QDir().mkpath(dataFolder());
    QDesktopServices::openUrl(QUrl::fromLocalFile(dataFolder()));
}

void AppController::copyLogToClipboard()
{
    QGuiApplication::clipboard()->setText(m_logModel->allText());
}

void AppController::appendLog(const QString& line)
{
    log(line);
}

void AppController::setLaunchAtLogin(bool enabled)
{
    if (Platform::setLaunchAtLogin(enabled)) {
        m_settings->setLaunchAtLogin(enabled);
        log(enabled ? tr("Input Leap will launch at login") : tr("Input Leap will not launch at login"));
    } else {
        log(tr("could not change the launch at login setting"), QStringLiteral("WARNING"));
    }
    emit stateChanged();
}

void AppController::copyText(const QString& text)
{
    QGuiApplication::clipboard()->setText(text);
}

QString AppController::pickFile(const QString& title, const QString& start, bool save)
{
    return save ? QFileDialog::getSaveFileName(nullptr, title, start)
                : QFileDialog::getOpenFileName(nullptr, title, start);
}

int AppController::addHotkey(QObject* capture)
{
    auto* keys = qobject_cast<KeyCaptureItem*>(capture);
    if (keys == nullptr || !keys->valid()) {
        return -1;
    }
    const int index = layout()->addHotkey(keys->sequence());
    if (index < 0) {
        log(tr("that hotkey already exists"), QStringLiteral("WARNING"));
    }
    return index;
}

bool AppController::addHotkeyAction(int hotkeyIndex, const QVariantMap& map, QObject* capture)
{
    static const QHash<QString, int> types{
        {QStringLiteral("keyDown"), Action::keyDown}, {QStringLiteral("keyUp"), Action::keyUp},
        {QStringLiteral("keystroke"), Action::keystroke}, {QStringLiteral("switchToScreen"), Action::switchToScreen},
        {QStringLiteral("toggleScreen"), Action::toggleScreen}, {QStringLiteral("switchInDirection"), Action::switchInDirection},
        {QStringLiteral("lockCursorToScreen"), Action::lockCursorToScreen}};
    const QString typeName = map.value(QStringLiteral("type")).toString();
    if (!types.contains(typeName)) {
        return false;
    }
    Action action;
    action.setType(types.value(typeName));
    if (action.type() == Action::keyDown || action.type() == Action::keyUp || action.type() == Action::keystroke) {
        auto* keys = qobject_cast<KeyCaptureItem*>(capture);
        if (keys == nullptr || !keys->valid()) {
            log(tr("record the keys the action should press first"), QStringLiteral("WARNING"));
            return false;
        }
        action.setKeySequence(keys->sequence());
        const QStringList screens = map.value(QStringLiteral("screens")).toStringList();
        action.setHaveScreens(!screens.isEmpty());
        for (const QString& screen : screens) {
            action.appendTypeScreenName(screen);
        }
    }
    action.setSwitchScreenName(map.value(QStringLiteral("switchScreen")).toString());
    action.setSwitchDirection(map.value(QStringLiteral("direction"), Action::switchLeft).toInt());
    action.setLockCursorMode(map.value(QStringLiteral("lockMode"), Action::lockCursorToggle).toInt());
    action.setActiveOnRelease(map.value(QStringLiteral("onRelease"), false).toBool());
    if (!layout()->addAction(hotkeyIndex, action)) {
        return false;
    }
    applyLayout();
    return true;
}

void AppController::log(const QString& text, const QString& level)
{
    m_logModel->append(LogModel::App, level, text);
    m_lastEvent = text;
    // gui.log (and stderr) get the app's own lines too, so a machine can be
    // diagnosed from its files alone
    if (level == QLatin1String("ERROR") || level == QLatin1String("FATAL")) {
        qCritical("app: %s", qPrintable(text));
    } else if (level == QLatin1String("WARNING")) {
        qWarning("app: %s", qPrintable(text));
    } else {
        qInfo("app: %s", qPrintable(text));
    }
    emit stateChanged();
}
