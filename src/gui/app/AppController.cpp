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
#include "../../fork/gui/NotificationRouter.h"
#include "../../fork/gui/UpdateChecker.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHostInfo>
#include <QJSEngine>
#include <QNetworkInterface>
#include <QQmlEngine>
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
    m_logModel = new LogModel(this);
    m_log = new LogFilterModel(m_logModel, this);
    m_updates = new inputleap::fork_gui::UpdateChecker(nullptr);
    m_updates->setParent(this);
    m_restartTimer = new QTimer(this);
    m_restartTimer->setSingleShot(true);

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
        emit stateChanged();
    });
    connect(m_layout, &LayoutModel::layoutChanged, this, &AppController::stateChanged);
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

    log(tr("Input Leap %1 started").arg(m_version));
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
    switch (m_runState) {
    case Stopped:
        return m_pausedForTrust ? tr("Paused · waiting for you to trust the server") : tr("Not running");
    case Starting:
        return server ? tr("Starting the server…") : tr("Connecting to %1…").arg(serverAddress());
    case Running: {
        if (server) {
            const int total = m_layout->screenCount() - (m_layout->hasScreen(screenName()) ? 1 : 0);
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
    return m_discovery->servers().value(0);
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
        const QStringList names = m_layout->screenNames();
        if (!names.contains(self)) {
            list << entry(self, tr("not placed"), tr("this computer · server"), QStringLiteral("warning"), m_addresses.value(0), QDateTime());
        }
        for (const QString& name : names) {
            const int cell = m_layout->cellOf(name);
            if (name == self) {
                const bool active = m_activeScreen.isEmpty() || m_activeScreen == self;
                list << entry(name, m_layout->positionText(cell),
                              running() && active ? tr("this computer · cursor here") : tr("this computer · server"),
                              running() ? QStringLiteral("success") : QStringLiteral("neutral"), m_addresses.value(0), m_since);
                continue;
            }
            const bool on = m_connectedClients.contains(name);
            QString state = on ? tr("Connected") : (running() ? tr("Not connected") : tr("Offline"));
            if (on && m_activeScreen == name) state = tr("Connected · cursor here");
            list << entry(name, m_layout->positionText(cell), state,
                          on ? QStringLiteral("success") : QStringLiteral("neutral"), QString(), on ? m_since : QDateTime());
        }
    } else {
        list << entry(self, tr("client"), m_connectedToServer ? tr("Connected") : (running() ? tr("Connecting…") : tr("Not running")),
                      m_connectedToServer ? QStringLiteral("success") : QStringLiteral("neutral"), m_addresses.value(0), m_since);
        const QString server = serverAddress();
        if (!server.isEmpty()) {
            list << entry(server, tr("server"), m_connectedToServer ? tr("Connected") : tr("Server"),
                          m_connectedToServer ? QStringLiteral("success") : QStringLiteral("neutral"), server, m_since);
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
                     m_core->serviceConnected() ? tr("Connected over IPC") : tr("Not reachable over IPC"),
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
    return list;
}

// ---- lifecycle -----------------------------------------------------------------

void AppController::startup()
{
    // INPUTLEAP_UI_DRY_RUN=1 (implied by INPUTLEAP_UI_GRAB): render only, no
    // core, no discovery, no update check; development runs must not touch
    // the fleet.
    const bool dryRun = qEnvironmentVariableIsSet("INPUTLEAP_UI_DRY_RUN") || qEnvironmentVariableIsSet("INPUTLEAP_UI_GRAB");
    refreshAccessibility();
    if (!Platform::installedInApplicationsFolder()) {
        QVariantMap fields;
        fields.insert(QStringLiteral("text"), tr("Input Leap runs from a disk image. Drag it to the Applications folder and open it from there, or macOS will keep asking for the Accessibility permission."));
        addRequest(QStringLiteral("not-installed"), QStringLiteral("error"), fields);
    }
    if (m_settings->cryptoEnabled()) {
        m_identity->ensureCertificate();
    }
    if (dryRun) {
        log(tr("dry run: the core, discovery and update checks stay off"), QStringLiteral("WARNING"));
        emit stateChanged();
        return;
    }
    if (m_settings->discovery() && m_discovery->available()) {
        m_discovery->start(role() == Server, screenName());
    }
    if (m_settings->autoCheckUpdates()) {
        m_updates->maybeCheckOnStartup();
    }
    if (m_settings->startedBefore() && m_settings->startCoreOnLaunch()) {
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
    if (m_settings->discovery() && m_discovery->available()) {
        m_discovery->start(wanted == Server, screenName());
    }
    emit roleChanged();
    emit stateChanged();
}

bool AppController::buildLaunch(QString& program, QStringList& arguments, QString& error)
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
    arguments << QStringLiteral("--profile-dir") << QDir::toNativeSeparators(dataFolder());
#endif
    if (m_settings->logToFile()) {
        QDir().mkpath(QFileInfo(m_settings->logFilename()).path());
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
            m_layout->setServerName(screenName());
            m_layout->ensureServerPlaced();
            QDir().mkpath(dataFolder());
            configPath = generatedConfigPath();
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
        arguments << QStringLiteral("[%1]:%2").arg(host).arg(m_settings->port());
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
    if (!buildLaunch(program, arguments, error)) {
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
    m_lastStart = QDateTime::currentDateTime();
    log(tr("starting %1").arg(role() == Server ? tr("the server") : tr("the client")));
    m_logModel->append(LogModel::App, QStringLiteral("DEBUG"), tr("command: %1 %2").arg(program, arguments.join(QLatin1Char(' '))));
    m_core->start(program, arguments, m_settings->elevateMode());

    if (!m_settings->startedBefore()) {
        m_settings->setStartedBefore(true);
    }
    if (m_settings->discovery() && m_discovery->available() && !m_discovery->active()) {
        m_discovery->start(role() == Server, screenName());
    }
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

void AppController::applyLayout()
{
    if (running() && role() == Server) {
        log(tr("layout changed: restarting the server"));
        restart();
    }
}

void AppController::onCoreStateChanged()
{
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
        if (!event.name.isEmpty() && !m_layout->hasScreen(event.name)) {
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
    if (role() == Server && !m_layout->hasScreen(name) && !m_layout->ignoreAutoConfigClient()) {
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
        log(tr("no free cell for %1 in the layout").arg(name), QStringLiteral("WARNING"));
        return;
    }
    log(tr("placed %1 %2").arg(name, m_layout->positionText(m_layout->cellOf(name))));
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
    const int index = m_layout->addHotkey(keys->sequence());
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
    if (!m_layout->addAction(hotkeyIndex, action)) {
        return false;
    }
    applyLayout();
    return true;
}

void AppController::log(const QString& text, const QString& level)
{
    m_logModel->append(LogModel::App, level, text);
    m_lastEvent = text;
    emit stateChanged();
}
