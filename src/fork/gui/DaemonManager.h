/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include "../../gui/src/AppConnectionState.h"
#include "inputleap/AppRole.h"

class AppConfig;
class ServerConfig;

/**
 * @brief Modern daemon management class for InputLeap
 * 
 * This class provides independent daemon process management without
 * requiring MainWindow dependencies. It handles starting, stopping,
 * and monitoring both server and client daemons.
 */
class DaemonManager : public QObject
{
    Q_OBJECT

public:
    explicit DaemonManager(AppConfig& appConfig, ServerConfig& serverConfig, QObject* parent = nullptr);
    ~DaemonManager();

    // Daemon control
    void startDaemon(AppRole role);
    void stopDaemon();
    void restartDaemon();
    
    // Status queries
    AppConnectionState connectionState() const;
    AppRole currentRole() const;
    bool isDaemonRunning() const;
    
    // Configuration
    void setRole(AppRole role);
    void setServerHostname(const QString& hostname);
    void setServerPort(int port);
    
signals:
    void connectionStateChanged(AppConnectionState newState);
    void daemonStarted(AppRole role);
    void daemonStopped();
    void daemonErrorOccurred(const QString& error);
    void logMessage(const QString& message);

private slots:
    void onDaemonFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onDaemonErrorOccurred(QProcess::ProcessError error);
    void onDaemonStateChanged(QProcess::ProcessState newState);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    // Core daemon management
    bool buildClientArgs(QStringList& args, QString& app);
    bool buildServerArgs(QStringList& args, QString& app);
    QString appPath(const QString& name) const;
    QString getScreenName() const;
    void setConnectionState(AppConnectionState newState);
    void cleanupProcess();
    
    // Member variables
    AppConfig& m_appConfig;
    ServerConfig& m_serverConfig;
    QProcess* m_daemonProcess;
    AppConnectionState m_connectionState;
    AppRole m_currentRole;
    QString m_serverHostname;
    int m_serverPort;
    QTimer* m_reconnectTimer;
    
    // State tracking
    bool m_isStarting;
    bool m_shouldRestart;
};
