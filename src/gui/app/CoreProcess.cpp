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

#include "CoreProcess.h"
#include "IpcClient.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#if defined(Q_OS_MACOS)
#include <libproc.h>
#endif
#if defined(Q_OS_UNIX)
#include <signal.h>
#endif

#if defined(Q_OS_UNIX)
#include <csignal>
#endif

CoreProcess::CoreProcess(QObject* parent) :
    QObject(parent)
{
#if defined(Q_OS_WIN)
    // The service is always there to talk to on Windows, even to stop a
    // core that an earlier app instance started.
    m_ipc = new IpcClient();
    m_ipc->setParent(this);
    connect(m_ipc, &IpcClient::readLogLine, this, &CoreProcess::lineReceived);
    connect(m_ipc, &IpcClient::infoMessage, this, [this](const QString& text) {
        m_serviceConnected = text.contains(QLatin1String("established"));
        emit serviceMessage(text, false);
        emit stateChanged();
    });
    connect(m_ipc, &IpcClient::errorMessage, this, [this](const QString& text) {
        m_serviceConnected = false;
        emit serviceMessage(text, true);
        emit stateChanged();
    });
    m_ipc->connectToHost();
#endif
}

CoreProcess::~CoreProcess()
{
    if (m_process != nullptr && m_state != Stopped) {
        m_stopRequested = true;
        stop();
    }
}

bool CoreProcess::serviceMode() const
{
    return m_ipc != nullptr;
}

qint64 CoreProcess::pid() const
{
    return m_process != nullptr ? m_process->processId() : 0;
}

QString CoreProcess::commandLine() const
{
    if (m_program.isEmpty()) {
        return QString();
    }
    QStringList parts{m_program};
    for (const QString& arg : m_arguments) {
        parts << (arg.contains(QLatin1Char(' ')) ? QStringLiteral("\"%1\"").arg(arg) : arg);
    }
    return parts.join(QLatin1Char(' '));
}

void CoreProcess::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

void CoreProcess::start(const QString& program, const QStringList& arguments, int elevateMode)
{
    if (m_state == Starting || m_state == Running) {
        return;
    }
    m_program = program;
    m_arguments = arguments;
    m_lastError.clear();
    m_stopRequested = false;
    m_pending.clear();
    m_startedAt = QDateTime::currentDateTime();

    if (serviceMode()) {
        // The service wants one string; quote the program path (spaces in
        // "Program Files") the way the legacy GUI did.
        QStringList parts{QStringLiteral("\"%1\"").arg(program)};
        for (const QString& arg : arguments) {
            parts << (arg.contains(QLatin1Char(' ')) ? QStringLiteral("\"%1\"").arg(arg) : arg);
        }
        m_ipc->sendCommand(parts.join(QLatin1Char(' ')), static_cast<ElevateMode>(elevateMode));
        setState(Running);
        return;
    }

    setState(Starting);
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        readLines(m_process->readAllStandardOutput());
    });
    connect(m_process, &QProcess::started, this, [this]() { setState(Running); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_lastError = tr("could not start %1").arg(m_program);
            setState(Failed);
        }
    });
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (!m_pending.isEmpty()) {
            readLines(QByteArray("\n"));
        }
        const bool unexpected = !m_stopRequested;
        if (status == QProcess::CrashExit && unexpected) {
            m_lastError = tr("the core crashed");
        } else if (exitCode != 0 && unexpected) {
            m_lastError = tr("the core exited with code %1").arg(exitCode);
        }
        m_process->deleteLater();
        m_process = nullptr;
        setState(Stopped);
        emit exited(exitCode, unexpected);
    });
    m_process->start(program, arguments);
}

void CoreProcess::stop()
{
    m_stopRequested = true;
    if (serviceMode()) {
        if (m_state != Stopped) {
            m_ipc->sendCommand(QString(), ElevateNever);
            setState(Stopped);
        }
        return;
    }
    if (m_process == nullptr || m_state == Stopped) {
        setState(Stopped);
        return;
    }
    setState(Stopping);
#if defined(Q_OS_UNIX)
    ::kill(static_cast<pid_t>(m_process->processId()), SIGTERM);
#else
    m_process->terminate();
#endif
    if (!m_process->waitForFinished(5000)) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

bool CoreProcess::terminateStaleCore(qint64 pid, const QString& binDir)
{
#if defined(Q_OS_UNIX)
    if (pid <= 1 || ::kill(static_cast<pid_t>(pid), 0) != 0) {
        return false;   // gone already
    }
    QString executable;
#if defined(Q_OS_MACOS)
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(static_cast<int>(pid), path, sizeof(path)) > 0) {
        executable = QString::fromUtf8(path);
    }
#else
    executable = QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid));
#endif
    const QString name = QFileInfo(executable).fileName();
    if (!executable.startsWith(QDir(binDir).absolutePath()) ||
        (name != QLatin1String("input-leapc") && name != QLatin1String("input-leaps"))) {
        return false;   // the pid was reused by something else
    }
    return ::kill(static_cast<pid_t>(pid), SIGTERM) == 0;
#else
    Q_UNUSED(pid);
    Q_UNUSED(binDir);
    return false;       // the service owns the core on Windows
#endif
}

void CoreProcess::reconnectService()
{
    if (m_ipc != nullptr) {
        m_ipc->disconnectFromHost();
        m_ipc->connectToHost();
    }
}

void CoreProcess::readLines(const QByteArray& chunk)
{
    m_pending += chunk;
    int newline;
    while ((newline = m_pending.indexOf('\n')) >= 0) {
        QByteArray line = m_pending.left(newline);
        m_pending.remove(0, newline + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (!line.trimmed().isEmpty()) {
            emit lineReceived(QString::fromUtf8(line));
        }
    }
}
