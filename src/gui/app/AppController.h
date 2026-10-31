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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/* The facade the QML window binds to (docs/ui-redesign.md §4.3).
 *
 * Milestone M0: a stub. Role and run state are kept in memory and the
 * start/stop transitions are simulated with timers, so the shell, the tray
 * and the bindings can be exercised before CoreProcess & friends exist.
 * M1 replaces the internals with the real application layer; the surface
 * (properties, enums, invokables) is what the QML keeps binding to.
 */
class AppController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(Role role READ role WRITE setRole NOTIFY roleChanged)
    Q_PROPERTY(RunState runState READ runState NOTIFY stateChanged)
    Q_PROPERTY(bool canSwitchRole READ canSwitchRole NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString lastEvent READ lastEvent NOTIFY stateChanged)
    Q_PROPERTY(QString screenName READ screenName CONSTANT)
    Q_PROPERTY(QStringList addresses READ addresses CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString monoFontFamily READ monoFontFamily CONSTANT)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString pendingJoin READ pendingJoin NOTIFY stateChanged)
    Q_PROPERTY(QVariantList computers READ computers NOTIFY stateChanged)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY stateChanged)
    Q_PROPERTY(QStringList logLines READ logLines NOTIFY logLinesChanged)

public:
    enum Role { Server, Client };
    Q_ENUM(Role)

    enum RunState { Stopped, Starting, Running, Stopping, Failed };
    Q_ENUM(RunState)

    // One instance for the whole process: created by main() before the QML
    // engine, handed to QML as the `App` singleton, shared with the tray.
    static AppController* instance();
    static AppController* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    Role role() const { return m_role; }
    RunState runState() const { return m_runState; }
    bool canSwitchRole() const;
    QString statusText() const;
    QString lastEvent() const { return m_lastEvent; }
    QString screenName() const { return m_screenName; }
    QStringList addresses() const { return m_addresses; }
    QString version() const { return m_version; }
    QString monoFontFamily() const { return m_monoFontFamily; }
    bool updateAvailable() const { return m_updateAvailable; }
    QString pendingJoin() const { return m_pendingJoin; }
    QVariantList computers() const { return m_computers; }
    QVariantList processes() const { return m_processes; }
    QStringList logLines() const { return m_logLines; }

public slots:
    void setRole(Role role);
    void start();
    void stop();
    void restart();
    void appendLog(const QString& line);
    void clearLog();
    void ignorePendingJoin();

signals:
    void roleChanged();
    void stateChanged();
    void logLinesChanged();

private:
    explicit AppController(QObject* parent = nullptr);
    void setRunState(RunState state);
    void refreshDemoData();

    Role m_role = Server;
    RunState m_runState = Stopped;
    QString m_lastEvent;
    QString m_screenName;
    QStringList m_addresses;
    QString m_version;
    QString m_monoFontFamily;
    bool m_updateAvailable = false;
    QString m_pendingJoin;
    QVariantList m_computers;
    QVariantList m_processes;
    QStringList m_logLines;
};
