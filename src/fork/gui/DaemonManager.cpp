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

#include "DaemonManager.h"
#include "../../gui/src/AppConfig.h"
#include "../../gui/src/ServerConfig.h"
#include "../../lib/inputleap/AppRole.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QMessageBox>
#include <QHostInfo>
#include <QTemporaryFile>
#include <QDebug>

DaemonManager::DaemonManager(AppConfig& appConfig, ServerConfig& serverConfig, QObject* parent)
    : QObject(parent)
    , m_appConfig(appConfig)
    , m_serverConfig(serverConfig)
    , m_daemonProcess(nullptr)
    , m_connectionState(AppConnectionState::DISCONNECTED)
    , m_currentRole(AppRole::Server)
    , m_serverHostname("localhost")
    , m_serverPort(24800)
    , m_reconnectTimer(new QTimer(this))
    , m_isStarting(false)
    , m_shouldRestart(false)
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_shouldRestart) {
            startDaemon(m_currentRole);
        }
    });
}

DaemonManager::~DaemonManager()
{
    cleanupProcess();
}

void DaemonManager::startDaemon(AppRole role)
{
    if (m_isStarting) {
        emit logMessage("Daemon is already starting...");
        return;
    }
    
    if (m_daemonProcess && m_daemonProcess->state() != QProcess::NotRunning) {
        emit logMessage("Stopping existing daemon before starting new one...");
        stopDaemon();
        // Schedule restart after stop completes
        m_shouldRestart = true;
        m_currentRole = role;
        m_reconnectTimer->start(1000);
        return;
    }
    
    m_currentRole = role;
    m_isStarting = true;
    m_shouldRestart = false;
    
    setConnectionState(AppConnectionState::CONNECTING);
    
    QString app;
    QStringList args;
    
    // Base arguments for both server and client
    args << "-f" << "--no-tray" << "--debug" << m_appConfig.logLevelText();
    args << "--name" << getScreenName();
    
    // Add IPC if in service mode
    if (m_appConfig.processMode() != Desktop) {
        args << "--ipc";
    }
    
    // Role-specific arguments
    bool success = false;
    if (role == AppRole::Client) {
        success = buildClientArgs(args, app);
        emit logMessage("Starting InputLeap client daemon...");
    } else {
        success = buildServerArgs(args, app);
        emit logMessage("Starting InputLeap server daemon...");
    }
    
    if (!success) {
        setConnectionState(AppConnectionState::DISCONNECTED);
        m_isStarting = false;
        return;
    }
    
    // Create and configure process
    m_daemonProcess = new QProcess(this);
    
    connect(m_daemonProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &DaemonManager::onDaemonFinished);
    connect(m_daemonProcess, &QProcess::errorOccurred,
            this, &DaemonManager::onDaemonErrorOccurred);
    connect(m_daemonProcess, &QProcess::stateChanged,
            this, &DaemonManager::onDaemonStateChanged);
    connect(m_daemonProcess, &QProcess::readyReadStandardOutput,
            this, &DaemonManager::onReadyReadStandardOutput);
    connect(m_daemonProcess, &QProcess::readyReadStandardError,
            this, &DaemonManager::onReadyReadStandardError);
    
    // Start the process
    emit logMessage(QString("Executing: %1 %2").arg(app, args.join(" ")));
    
    m_daemonProcess->start(app, args);
    
    if (!m_daemonProcess->waitForStarted(5000)) {
        emit daemonErrorOccurred(QString("Failed to start daemon: %1").arg(m_daemonProcess->errorString()));
        setConnectionState(AppConnectionState::DISCONNECTED);
        cleanupProcess();
        m_isStarting = false;
        return;
    }
    
    emit daemonStarted(role);
    m_isStarting = false;
}

void DaemonManager::stopDaemon()
{
    if (!m_daemonProcess) {
        setConnectionState(AppConnectionState::DISCONNECTED);
        return;
    }
    
    emit logMessage("Stopping InputLeap daemon...");
    setConnectionState(AppConnectionState::DISCONNECTED);
    
    if (m_daemonProcess->state() != QProcess::NotRunning) {
        m_daemonProcess->terminate();
        
        if (!m_daemonProcess->waitForFinished(5000)) {
            emit logMessage("Force killing daemon process...");
            m_daemonProcess->kill();
            m_daemonProcess->waitForFinished(2000);
        }
    }
    
    cleanupProcess();
    emit daemonStopped();
}

void DaemonManager::restartDaemon()
{
    emit logMessage("Restarting InputLeap daemon...");
    stopDaemon();
    
    // Schedule restart
    m_shouldRestart = true;
    m_reconnectTimer->start(1000);
}

AppConnectionState DaemonManager::connectionState() const
{
    return m_connectionState;
}

AppRole DaemonManager::currentRole() const
{
    return m_currentRole;
}

bool DaemonManager::isDaemonRunning() const
{
    return m_daemonProcess && m_daemonProcess->state() == QProcess::Running;
}

void DaemonManager::setRole(AppRole role)
{
    if (m_currentRole != role) {
        m_currentRole = role;
        if (isDaemonRunning()) {
            restartDaemon();
        }
    }
}

void DaemonManager::setServerHostname(const QString& hostname)
{
    m_serverHostname = hostname;
}

void DaemonManager::setServerPort(int port)
{
    m_serverPort = port;
}

bool DaemonManager::buildClientArgs(QStringList& args, QString& app)
{
    app = appPath(m_appConfig.client_name());
    
    if (!QFileInfo::exists(app)) {
        emit daemonErrorOccurred("InputLeap client executable not found: " + app);
        return false;
    }
    
    // Add logging
    if (m_appConfig.logToFile()) {
        args << "--log" << m_appConfig.logFilenameCmd();
    }
    
    // Add server address
    if (m_serverHostname.isEmpty()) {
        emit daemonErrorOccurred("No server hostname specified for client mode");
        return false;
    }
    
    QString serverAddress = QString("[%1]:%2").arg(m_serverHostname).arg(m_serverPort);
    args << serverAddress;
    
    return true;
}

bool DaemonManager::buildServerArgs(QStringList& args, QString& app)
{
    app = appPath(m_appConfig.server_name());
    
    if (!QFileInfo::exists(app)) {
        emit daemonErrorOccurred("InputLeap server executable not found: " + app);
        return false;
    }
    
    // Add logging
    if (m_appConfig.logToFile()) {
        args << "--log" << m_appConfig.logFilenameCmd();
    }
    
    // Add configuration file
    // For now, create a temporary config file from ServerConfig
    QTemporaryFile* tempConfigFile = new QTemporaryFile(this);
    if (!tempConfigFile->open()) {
        emit daemonErrorOccurred("Cannot create temporary configuration file");
        return false;
    }
    
    m_serverConfig.save(*tempConfigFile);
    QString configFile = tempConfigFile->fileName();
    tempConfigFile->close();
    
    if (configFile.isEmpty() || !QFileInfo::exists(configFile)) {
        emit daemonErrorOccurred("No valid configuration file found for server mode");
        return false;
    }
    
    args << "--config" << QString("\"%1\"").arg(configFile);
    
    // Add optional features
    if (!m_appConfig.getRequireClientCertificate()) {
        args << "--disable-client-cert-checking";
    }
    
    if (m_serverConfig.enableDragAndDrop()) {
        args << "--enable-drag-drop";
    }
    
    if (!m_appConfig.getCryptoEnabled()) {
        args << "--disable-crypto";
    }
    
    return true;
}

QString DaemonManager::appPath(const QString& name) const
{
    QString appDir = QApplication::applicationDirPath();
    return QDir(appDir).absoluteFilePath(name);
}

QString DaemonManager::getScreenName() const
{
    QString name = m_appConfig.screenName();
    if (name.isEmpty()) {
        name = QHostInfo::localHostName();
    }
    return name;
}

void DaemonManager::setConnectionState(AppConnectionState newState)
{
    if (m_connectionState != newState) {
        m_connectionState = newState;
        emit connectionStateChanged(newState);
    }
}

void DaemonManager::cleanupProcess()
{
    if (m_daemonProcess) {
        m_daemonProcess->disconnect();
        if (m_daemonProcess->state() != QProcess::NotRunning) {
            m_daemonProcess->kill();
        }
        m_daemonProcess->deleteLater();
        m_daemonProcess = nullptr;
    }
}

void DaemonManager::onDaemonFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString roleStr = (m_currentRole == AppRole::Server) ? "server" : "client";
    
    if (exitStatus == QProcess::CrashExit) {
        emit logMessage(QString("InputLeap %1 daemon crashed (exit code: %2)").arg(roleStr).arg(exitCode));
        emit daemonErrorOccurred(QString("Daemon crashed with exit code %1").arg(exitCode));
    } else {
        emit logMessage(QString("InputLeap %1 daemon finished (exit code: %2)").arg(roleStr).arg(exitCode));
    }
    
    setConnectionState(AppConnectionState::DISCONNECTED);
    cleanupProcess();
    emit daemonStopped();
}

void DaemonManager::onDaemonErrorOccurred(QProcess::ProcessError error)
{
    QString errorStr;
    switch (error) {
        case QProcess::FailedToStart:
            errorStr = "Failed to start daemon process";
            break;
        case QProcess::Crashed:
            errorStr = "Daemon process crashed";
            break;
        case QProcess::Timedout:
            errorStr = "Daemon process timed out";
            break;
        case QProcess::WriteError:
            errorStr = "Write error to daemon process";
            break;
        case QProcess::ReadError:
            errorStr = "Read error from daemon process";
            break;
        default:
            errorStr = "Unknown daemon process error";
            break;
    }
    
    emit logMessage("Daemon error: " + errorStr);
    emit daemonErrorOccurred(errorStr);
    setConnectionState(AppConnectionState::DISCONNECTED);
}

void DaemonManager::onDaemonStateChanged(QProcess::ProcessState newState)
{
    switch (newState) {
        case QProcess::Starting:
            emit logMessage("Daemon is starting...");
            setConnectionState(AppConnectionState::CONNECTING);
            break;
        case QProcess::Running:
            emit logMessage("Daemon is running");
            setConnectionState(AppConnectionState::CONNECTED);
            break;
        case QProcess::NotRunning:
            emit logMessage("Daemon is not running");
            setConnectionState(AppConnectionState::DISCONNECTED);
            break;
    }
}

void DaemonManager::onReadyReadStandardOutput()
{
    if (m_daemonProcess) {
        QByteArray data = m_daemonProcess->readAllStandardOutput();
        emit logMessage(QString::fromUtf8(data).trimmed());
    }
}

void DaemonManager::onReadyReadStandardError()
{
    if (m_daemonProcess) {
        QByteArray data = m_daemonProcess->readAllStandardError();
        QString errorMsg = QString::fromUtf8(data).trimmed();
        if (!errorMsg.isEmpty()) {
            emit logMessage("Daemon error: " + errorMsg);
        }
    }
}
