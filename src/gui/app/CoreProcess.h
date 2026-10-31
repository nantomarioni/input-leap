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

#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QProcess;
class IpcClient;

/* Runs the core (input-leaps / input-leapc) and relays what it prints.
 *
 * Desktop mode (macOS, Linux): the core is a child QProcess and its stdout
 * and stderr are the log. Service mode (Windows): the command line goes to
 * the input-leapd service over the local IPC socket, which launches the core
 * (elevated or not) and streams its log back. The state here is the
 * process's; whether it is connected is the controller's business, read from
 * the log through CoreEventParser.
 */
class CoreProcess : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.core")

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool serviceMode READ serviceMode CONSTANT)
    Q_PROPERTY(qint64 pid READ pid NOTIFY stateChanged)
    Q_PROPERTY(QString program READ program NOTIFY stateChanged)
    Q_PROPERTY(QString commandLine READ commandLine NOTIFY stateChanged)
    Q_PROPERTY(QDateTime startedAt READ startedAt NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(bool serviceConnected READ serviceConnected NOTIFY stateChanged)

public:
    enum State { Stopped, Starting, Running, Stopping, Failed };
    Q_ENUM(State)

    explicit CoreProcess(QObject* parent = nullptr);
    ~CoreProcess() override;

    State state() const { return m_state; }
    bool serviceMode() const;
    qint64 pid() const;
    QString program() const { return m_program; }
    QString commandLine() const;
    QDateTime startedAt() const { return m_startedAt; }
    QString lastError() const { return m_lastError; }
    bool serviceConnected() const { return m_serviceConnected; }

    // elevateMode: AppSettings::ElevateMode, used by the Windows service only.
    void start(const QString& program, const QStringList& arguments, int elevateMode);
    void stop();

    // Windows: reconnect the IPC socket to the service.
    Q_INVOKABLE void reconnectService();

signals:
    void lineReceived(const QString& line);                     // one raw core line
    void serviceMessage(const QString& text, bool isError);     // IPC chatter
    void stateChanged();
    void exited(int exitCode, bool unexpected);                 // desktop mode

private:
    void setState(State state);
    void readLines(const QByteArray& chunk);

    State m_state = Stopped;
    QString m_program;
    QStringList m_arguments;
    QDateTime m_startedAt;
    QString m_lastError;
    bool m_stopRequested = false;
    bool m_serviceConnected = false;
    QByteArray m_pending;

    QProcess* m_process = nullptr;
    IpcClient* m_ipc = nullptr;
};
