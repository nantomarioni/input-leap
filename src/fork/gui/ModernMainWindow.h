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

#include <QMainWindow>
#include "../../gui/src/AppConnectionState.h"

class AppConfig;
class ServerConfig;
class QEvent;
class DaemonManager;

namespace Ui {
    class ModernMainWindow;
}

/**
 * @brief Modern main window for InputLeap
 * 
 * This is the modern replacement for the legacy MainWindow, providing
 * a clean interface with independent daemon management through DaemonManager.
 * Unlike the legacy MainWindow, this doesn't have tight coupling with process
 * management and provides a more maintainable architecture.
 */
class ModernMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    ModernMainWindow(QWidget* parent, AppConfig& config, ServerConfig& serverConfig, const QString& defaultScreen = "");
    ~ModernMainWindow();

signals:
    void serverConfigChanged();
    void clientConfigChanged();
    void screenLayoutChanged();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onServerConfigureClicked();
    void onClientConfigureClicked();
    void onLayoutConfigureClicked();
    void onMultiMonitorConfigureClicked();
    void onAdvancedConfigureClicked();
    
    // Mode selection slots
    void onServerModeSelected();
    void onClientModeSelected();
    
    // Daemon status control slots
    void onStartDaemonClicked();
    void onStopDaemonClicked();
    void onReloadDaemonClicked();
    
    // Server configuration slots
    void onHeartbeatToggled(bool enabled);
    void onSwitchDelayToggled(bool enabled);
    void onSwitchDoubleTapToggled(bool enabled);
    void onScreenDimmingToggled(bool enabled);
    void onDimmingPercentageChanged(int value);
    void onServerConfigChanged();
    void onApplyClicked();

private:
    void setupConnections();
    void setupServerTab();
    void loadServerConfig();
    void saveServerConfig();
    bool hasUnsavedChanges() const;
    void updateApplyButton();
    bool validateServerConfig();
    void resetServerConfigToOriginal();
    void markServerConfigChanged();
    
    // Mode and status management
    void updateTabVisibility();
    void updateDaemonStatus(const QString& status, const QString& color);
    void updateDaemonButtons(AppConnectionState state);
    void syncDaemonStatus();
    
    enum class DaemonStatus {
        Stopped,
        Initializing,
        Running
    };
    
    Ui::ModernMainWindow* ui;
    AppConfig& m_appConfig;
    ServerConfig& m_serverConfig;
    DaemonManager* m_daemonManager;  // Modern daemon management
    
    // Server configuration state management
    struct ServerConfigState {
        QString serverName;
        bool hasHeartbeat;
        int heartbeat;
        bool relativeMouseMoves;
        bool screenSaverSync;
        bool win32KeepForeground;
        bool hasSwitchDelay;
        int switchDelay;
        bool hasSwitchDoubleTap;
        int switchDoubleTap;
        int switchCornerSize;
        bool switchCorners[4]; // TopLeft, TopRight, BottomLeft, BottomRight
        bool ignoreAutoConfigClient;
        bool enableDragAndDrop;
        bool clipboardSharing;
        bool screenDimmingEnabled;
        int screenDimmingPercentage;
    };
    
    ServerConfigState m_originalServerConfig;
    bool m_serverConfigChanged;
    
    // Mode and status state
    bool m_isServerMode;
    DaemonStatus m_currentDaemonStatus;
};
