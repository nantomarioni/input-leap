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

#include <QCoreApplication>
#include <QDateTime>
#include <QFontDatabase>
#include <QHostInfo>
#include <QJSEngine>
#include <QNetworkInterface>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

namespace {
AppController* s_instance = nullptr;
const int kMaxLogLines = 10000;
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
    m_screenName = QHostInfo::localHostName();
    const auto all = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : all) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            // the usual LAN range first, the way the old window hinted it
            if (address.toString().startsWith(QLatin1String("192.168"))) {
                m_addresses.prepend(address.toString());
            } else {
                m_addresses.append(address.toString());
            }
        }
    }
    m_monoFontFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    m_version = QStringLiteral(INPUTLEAP_VERSION);

    if (qEnvironmentVariableIsSet("INPUTLEAP_UI_DEMO")) {
        m_updateAvailable = true;
        m_pendingJoin = QStringLiteral("mac-mini");
    }

    refreshDemoData();
    appendLog(tr("Input Leap %1 started (new window, milestone M0)").arg(m_version));
}

bool AppController::canSwitchRole() const
{
    return m_runState == Stopped || m_runState == Failed;
}

QString AppController::statusText() const
{
    const bool server = m_role == Server;
    switch (m_runState) {
    case Stopped:  return tr("Not running");
    case Starting: return server ? tr("Starting the server…") : tr("Connecting…");
    case Running:  return server ? tr("Running as server · no clients yet") : tr("Connected");
    case Stopping: return tr("Stopping…");
    case Failed:   return tr("Failed to start");
    }
    return QString();
}

void AppController::setRole(Role role)
{
    if (role == m_role) {
        return;
    }
    if (!canSwitchRole()) {
        appendLog(tr("stop Input Leap before changing the role"));
        return;
    }
    m_role = role;
    appendLog(tr("role is now %1").arg(role == Server ? tr("server") : tr("client")));
    refreshDemoData();
    emit roleChanged();
    emit stateChanged();
}

void AppController::start()
{
    if (m_runState != Stopped && m_runState != Failed) {
        return;
    }
    setRunState(Starting);
    appendLog(tr("starting %1 (M0: simulated, no core process yet)")
                  .arg(m_role == Server ? tr("server") : tr("client")));
    QTimer::singleShot(800, this, [this]() {
        if (m_runState == Starting) {
            setRunState(Running);
            appendLog(m_role == Server ? tr("started server, waiting for clients")
                                       : tr("connected to server"));
        }
    });
}

void AppController::stop()
{
    if (m_runState == Stopped || m_runState == Stopping) {
        return;
    }
    setRunState(Stopping);
    appendLog(tr("stopping"));
    QTimer::singleShot(300, this, [this]() {
        if (m_runState == Stopping) {
            setRunState(Stopped);
            appendLog(tr("stopped"));
        }
    });
}

void AppController::restart()
{
    if (m_runState != Running) {
        return;
    }
    stop();
    QTimer::singleShot(400, this, [this]() { start(); });
}

void AppController::appendLog(const QString& line)
{
    const QString stamped = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))
                            + QStringLiteral("  ") + line;
    m_logLines.append(stamped);
    if (m_logLines.size() > kMaxLogLines) {
        m_logLines.removeFirst();
    }
    m_lastEvent = line;
    emit logLinesChanged();
    emit stateChanged();
}

void AppController::clearLog()
{
    m_logLines.clear();
    emit logLinesChanged();
}

void AppController::ignorePendingJoin()
{
    if (m_pendingJoin.isEmpty()) {
        return;
    }
    appendLog(tr("ignored join request from %1").arg(m_pendingJoin));
    m_pendingJoin.clear();
    emit stateChanged();
}

void AppController::setRunState(RunState state)
{
    if (state == m_runState) {
        return;
    }
    m_runState = state;
    refreshDemoData();
    emit stateChanged();
}

// M0 only: the rows the Overview shows. M1 derives them from the layout and
// the core's events.
void AppController::refreshDemoData()
{
    const bool running = m_runState == Running;

    QVariantMap self;
    self.insert(QStringLiteral("name"), m_screenName);
    self.insert(QStringLiteral("position"), tr("centre"));
    self.insert(QStringLiteral("state"), m_role == Server ? tr("this computer · server") : tr("this computer · client"));
    self.insert(QStringLiteral("stateTone"), running ? QStringLiteral("success") : QStringLiteral("neutral"));
    self.insert(QStringLiteral("address"), m_addresses.value(0));
    self.insert(QStringLiteral("session"), QString());
    m_computers = QVariantList{self};

    auto process = [](const QString& name, const QString& detail, bool ok) {
        QVariantMap map;
        map.insert(QStringLiteral("name"), name);
        map.insert(QStringLiteral("detail"), detail);
        map.insert(QStringLiteral("ok"), ok);
        return map;
    };
    const QString coreName = m_role == Server ? QStringLiteral("input-leaps") : QStringLiteral("input-leapc");
    m_processes = QVariantList{
        process(tr("App"), tr("This window and the tray icon"), true),
        process(tr("Core · %1").arg(coreName), running ? tr("Running (M0: simulated)") : tr("Not running"), running),
#if defined(Q_OS_WIN)
        process(tr("Service · input-leapd"), tr("Windows service"), false),
#else
        process(tr("Service"), tr("Not used on this platform: the app launches the core itself"), true),
#endif
        process(tr("Discovery · Bonjour"), tr("Not started (M1)"), false),
    };
}
