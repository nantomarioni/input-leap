#include "ModernMainWindow.h"
#include "ui_ModernMainWindow.h"

#include "../../gui/src/AppConfig.h"
#include "../../gui/src/ServerConfig.h"
#include "../../gui/src/BaseConfig.h"
#include "DaemonManager.h"
#include "../../lib/inputleap/AppRole.h"

#include <QtCore>
#include <QtGui>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>

ModernMainWindow::ModernMainWindow(QWidget* parent, AppConfig& config, ServerConfig& serverConfig, const QString& defaultScreen)
    : QMainWindow(parent)
    , ui(new Ui::ModernMainWindow)
    , m_appConfig(config)
    , m_serverConfig(serverConfig)
    , m_daemonManager(new DaemonManager(config, serverConfig, this))
    , m_serverConfigChanged(false)
    , m_isServerMode(true)
    , m_currentDaemonStatus(DaemonStatus::Stopped)
{
    ui->setupUi(this);
    setupConnections();
    setupServerTab();
    loadServerConfig();
    updateTabVisibility();
    
    // Initialize daemon status using DaemonManager
    syncDaemonStatus();
    
    // Set a proper window title and icon
    setWindowTitle(tr("Input Leap Configuration"));
    setWindowIcon(QIcon(":/res/icons/256x256/input-leap.png"));
    
    // Center on parent if provided
    if (parent) {
        move(parent->frameGeometry().topLeft() + 
             parent->rect().center() - rect().center());
    }
}

ModernMainWindow::~ModernMainWindow()
{
    delete ui;
}

void ModernMainWindow::setupConnections()
{
    // Mode selection connections
    connect(ui->radioButtonServerMode, &QRadioButton::toggled, this, &ModernMainWindow::onServerModeSelected);
    connect(ui->radioButtonClientMode, &QRadioButton::toggled, this, &ModernMainWindow::onClientModeSelected);
    
    // Server status control connections
    connect(ui->pushButtonStart, &QPushButton::clicked, this, &ModernMainWindow::onStartDaemonClicked);
    connect(ui->pushButtonStop, &QPushButton::clicked, this, &ModernMainWindow::onStopDaemonClicked);
    connect(ui->pushButtonReload, &QPushButton::clicked, this, &ModernMainWindow::onReloadDaemonClicked);
    
    // Connect to modern DaemonManager for real-time daemon status updates
    connect(m_daemonManager, &DaemonManager::connectionStateChanged, this, &ModernMainWindow::updateDaemonButtons);
    connect(m_daemonManager, &DaemonManager::daemonStarted, this, [this](AppRole role) {
        updateDaemonStatus("Running", "green");
    });
    connect(m_daemonManager, &DaemonManager::daemonStopped, this, [this]() {
        updateDaemonStatus("Stopped", "red");
    });
    connect(m_daemonManager, &DaemonManager::daemonErrorOccurred, this, [this](const QString& error) {
        updateDaemonStatus("Error: " + error, "red");
    });
    
    // Dialog buttons
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* button) {
        if (ui->buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
            onApplyClicked();
        }
    });
    
    // Client configuration  
    connect(ui->pushButtonClientConfigure, &QPushButton::clicked, this, &ModernMainWindow::onClientConfigureClicked);
    
    // Layout configuration
    connect(ui->pushButtonLayoutConfigure, &QPushButton::clicked, this, &ModernMainWindow::onLayoutConfigureClicked);
    
    // Multi-monitor configuration
    connect(ui->pushButtonMultiMonitorConfigure, &QPushButton::clicked, this, &ModernMainWindow::onMultiMonitorConfigureClicked);
    
    // Advanced configuration
    connect(ui->pushButtonAdvancedConfigure, &QPushButton::clicked, this, &ModernMainWindow::onAdvancedConfigureClicked);
    
    // Server tab controls
    connect(ui->lineEditServerName, &QLineEdit::textChanged, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxHeartbeat, &QCheckBox::toggled, this, &ModernMainWindow::onHeartbeatToggled);
    connect(ui->spinBoxHeartbeat, QOverload<int>::of(&QSpinBox::valueChanged), this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxRelativeMouseMoves, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxScreenSaverSync, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxWin32KeepForeground, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxSwitchDelay, &QCheckBox::toggled, this, &ModernMainWindow::onSwitchDelayToggled);
    connect(ui->spinBoxSwitchDelay, QOverload<int>::of(&QSpinBox::valueChanged), this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxSwitchDoubleTap, &QCheckBox::toggled, this, &ModernMainWindow::onSwitchDoubleTapToggled);
    connect(ui->spinBoxSwitchDoubleTap, QOverload<int>::of(&QSpinBox::valueChanged), this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->spinBoxSwitchCornerSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxCornerTopLeft, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxCornerTopRight, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxCornerBottomLeft, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxCornerBottomRight, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxClipboardSharing, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxDragAndDrop, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxIgnoreAutoConfig, &QCheckBox::toggled, this, &ModernMainWindow::onServerConfigChanged);
    connect(ui->checkBoxScreenDimming, &QCheckBox::toggled, this, &ModernMainWindow::onScreenDimmingToggled);
    connect(ui->sliderDimmingPercentage, &QSlider::valueChanged, this, &ModernMainWindow::onDimmingPercentageChanged);
}

void ModernMainWindow::onServerConfigureClicked()
{
    // This method is no longer needed as server configuration is now inline
    // The server tab contains all the controls directly
}

void ModernMainWindow::setupServerTab()
{
    // Set up platform-specific controls
#ifndef Q_OS_WIN
    ui->checkBoxWin32KeepForeground->setVisible(false);
#endif

    // Set up initial Apply button state
    updateApplyButton();
}

void ModernMainWindow::loadServerConfig()
{
    // TODO: Fix ServerConfig access issues - temporarily disable to test daemon status
    /*
    // Store original config for comparison
    m_originalServerConfig.serverName = QString(); // TODO: Get server name from config
    m_originalServerConfig.hasHeartbeat = m_serverConfig.hasHeartbeat();
    m_originalServerConfig.heartbeat = m_serverConfig.heartbeat();
    m_originalServerConfig.relativeMouseMoves = m_serverConfig.relativeMouseMoves();
    m_originalServerConfig.screenSaverSync = m_serverConfig.screenSaverSync();
    m_originalServerConfig.win32KeepForeground = m_serverConfig.win32KeepForeground();
    m_originalServerConfig.hasSwitchDelay = m_serverConfig.hasSwitchDelay();
    m_originalServerConfig.switchDelay = m_serverConfig.switchDelay();
    m_originalServerConfig.hasSwitchDoubleTap = m_serverConfig.hasSwitchDoubleTap();
    m_originalServerConfig.switchDoubleTap = m_serverConfig.switchDoubleTap();
    m_originalServerConfig.switchCornerSize = m_serverConfig.switchCornerSize();
    
    // Load switch corners
    using SC = BaseConfig::SwitchCorner;
    m_originalServerConfig.switchCorners[0] = m_serverConfig.switchCorner(SC::TopLeft);
    m_originalServerConfig.switchCorners[1] = m_serverConfig.switchCorner(SC::TopRight);
    m_originalServerConfig.switchCorners[2] = m_serverConfig.switchCorner(SC::BottomLeft);
    m_originalServerConfig.switchCorners[3] = m_serverConfig.switchCorner(SC::BottomRight);
    
    m_originalServerConfig.ignoreAutoConfigClient = m_serverConfig.ignoreAutoConfigClient();
    m_originalServerConfig.enableDragAndDrop = m_serverConfig.enableDragAndDrop();
    m_originalServerConfig.clipboardSharing = m_serverConfig.clipboardSharing();
    m_originalServerConfig.screenDimmingEnabled = m_serverConfig.screenDimmingEnabled();
    m_originalServerConfig.screenDimmingPercentage = m_serverConfig.screenDimmingPercentage();

    // Load values into UI controls
    ui->lineEditServerName->setText(m_originalServerConfig.serverName);
    ui->checkBoxHeartbeat->setChecked(m_originalServerConfig.hasHeartbeat);
    ui->spinBoxHeartbeat->setValue(m_originalServerConfig.heartbeat);
    ui->spinBoxHeartbeat->setEnabled(m_originalServerConfig.hasHeartbeat);
    
    ui->checkBoxRelativeMouseMoves->setChecked(m_originalServerConfig.relativeMouseMoves);
    ui->checkBoxScreenSaverSync->setChecked(m_originalServerConfig.screenSaverSync);
    ui->checkBoxWin32KeepForeground->setChecked(m_originalServerConfig.win32KeepForeground);
    
    ui->checkBoxSwitchDelay->setChecked(m_originalServerConfig.hasSwitchDelay);
    ui->spinBoxSwitchDelay->setValue(m_originalServerConfig.switchDelay);
    ui->spinBoxSwitchDelay->setEnabled(m_originalServerConfig.hasSwitchDelay);
    
    ui->checkBoxSwitchDoubleTap->setChecked(m_originalServerConfig.hasSwitchDoubleTap);
    ui->spinBoxSwitchDoubleTap->setValue(m_originalServerConfig.switchDoubleTap);
    ui->spinBoxSwitchDoubleTap->setEnabled(m_originalServerConfig.hasSwitchDoubleTap);
    
    ui->spinBoxSwitchCornerSize->setValue(m_originalServerConfig.switchCornerSize);
    
    ui->checkBoxCornerTopLeft->setChecked(m_originalServerConfig.switchCorners[0]);
    ui->checkBoxCornerTopRight->setChecked(m_originalServerConfig.switchCorners[1]);
    ui->checkBoxCornerBottomLeft->setChecked(m_originalServerConfig.switchCorners[2]);
    ui->checkBoxCornerBottomRight->setChecked(m_originalServerConfig.switchCorners[3]);
    
    ui->checkBoxClipboardSharing->setChecked(m_originalServerConfig.clipboardSharing);
    ui->checkBoxDragAndDrop->setChecked(m_originalServerConfig.enableDragAndDrop);
    ui->checkBoxIgnoreAutoConfig->setChecked(m_originalServerConfig.ignoreAutoConfigClient);
    
    ui->checkBoxScreenDimming->setChecked(m_originalServerConfig.screenDimmingEnabled);
    ui->sliderDimmingPercentage->setValue(m_originalServerConfig.screenDimmingPercentage);
    ui->sliderDimmingPercentage->setEnabled(m_originalServerConfig.screenDimmingEnabled);
    ui->labelDimmingValue->setText(QString("%1%").arg(m_originalServerConfig.screenDimmingPercentage));
    */
    
    // Reset change tracking
    m_serverConfigChanged = false;
}

void ModernMainWindow::saveServerConfig()
{
    if (!validateServerConfig()) {
        return;
    }

    // Save all settings to ServerConfig
    // TODO: Fix friend class access issue - these methods should be accessible but compilation fails
    /*
    m_serverConfig.haveHeartbeat(ui->checkBoxHeartbeat->isChecked());
    m_serverConfig.setHeartbeat(ui->spinBoxHeartbeat->value());
    m_serverConfig.setRelativeMouseMoves(ui->checkBoxRelativeMouseMoves->isChecked());
    m_serverConfig.setScreenSaverSync(ui->checkBoxScreenSaverSync->isChecked());
    m_serverConfig.setWin32KeepForeground(ui->checkBoxWin32KeepForeground->isChecked());
    
    m_serverConfig.haveSwitchDelay(ui->checkBoxSwitchDelay->isChecked());
    m_serverConfig.setSwitchDelay(ui->spinBoxSwitchDelay->value());
    m_serverConfig.haveSwitchDoubleTap(ui->checkBoxSwitchDoubleTap->isChecked());
    m_serverConfig.setSwitchDoubleTap(ui->spinBoxSwitchDoubleTap->value());
    m_serverConfig.setSwitchCornerSize(ui->spinBoxSwitchCornerSize->value());
    
    // Save switch corners
    using SC = BaseConfig::SwitchCorner;
    m_serverConfig.setSwitchCorner(SC::TopLeft, ui->checkBoxCornerTopLeft->isChecked());
    m_serverConfig.setSwitchCorner(SC::TopRight, ui->checkBoxCornerTopRight->isChecked());
    m_serverConfig.setSwitchCorner(SC::BottomLeft, ui->checkBoxCornerBottomLeft->isChecked());
    m_serverConfig.setSwitchCorner(SC::BottomRight, ui->checkBoxCornerBottomRight->isChecked());
    
    m_serverConfig.setIgnoreAutoConfigClient(ui->checkBoxIgnoreAutoConfig->isChecked());
    m_serverConfig.setEnableDragAndDrop(ui->checkBoxDragAndDrop->isChecked());
    m_serverConfig.setClipboardSharing(ui->checkBoxClipboardSharing->isChecked());
    m_serverConfig.setScreenDimmingEnabled(ui->checkBoxScreenDimming->isChecked());
    m_serverConfig.setScreenDimmingPercentage(ui->sliderDimmingPercentage->value());
    
    // Save to persistent storage
    m_serverConfig.saveSettings();
    */
    
    // Update original config and reset change tracking
    loadServerConfig();
    
    emit serverConfigChanged();
}

bool ModernMainWindow::hasUnsavedChanges() const
{
    return m_serverConfigChanged;
}

void ModernMainWindow::updateApplyButton()
{
    QPushButton* applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);
    if (applyButton) {
        applyButton->setEnabled(hasUnsavedChanges());
    }
}

bool ModernMainWindow::validateServerConfig()
{
    // Validate server name (if implemented)
    QString serverName = ui->lineEditServerName->text().trimmed();
    if (serverName.isEmpty()) {
        // For now, allow empty server name, but could be changed based on requirements
    }
    
    // Validate heartbeat range
    if (ui->checkBoxHeartbeat->isChecked()) {
        int heartbeat = ui->spinBoxHeartbeat->value();
        if (heartbeat < 1000 || heartbeat > 30000) {
            QMessageBox::warning(this, tr("Validation Error"), 
                                tr("Heartbeat must be between 1000 and 30000 milliseconds."));
            return false;
        }
    }
    
    // Validate switch delay range
    if (ui->checkBoxSwitchDelay->isChecked()) {
        int switchDelay = ui->spinBoxSwitchDelay->value();
        if (switchDelay < 0 || switchDelay > 5000) {
            QMessageBox::warning(this, tr("Validation Error"), 
                                tr("Switch delay must be between 0 and 5000 milliseconds."));
            return false;
        }
    }
    
    // Validate switch double-tap range
    if (ui->checkBoxSwitchDoubleTap->isChecked()) {
        int doubleTap = ui->spinBoxSwitchDoubleTap->value();
        if (doubleTap < 100 || doubleTap > 2000) {
            QMessageBox::warning(this, tr("Validation Error"), 
                                tr("Switch double-tap must be between 100 and 2000 milliseconds."));
            return false;
        }
    }
    
    // Validate switch corner size
    int cornerSize = ui->spinBoxSwitchCornerSize->value();
    if (cornerSize < 0 || cornerSize > 100) {
        QMessageBox::warning(this, tr("Validation Error"), 
                            tr("Switch corner size must be between 0 and 100 pixels."));
        return false;
    }
    
    // Validate dimming percentage
    if (ui->checkBoxScreenDimming->isChecked()) {
        int dimmingPercentage = ui->sliderDimmingPercentage->value();
        if (dimmingPercentage < 10 || dimmingPercentage > 90) {
            QMessageBox::warning(this, tr("Validation Error"), 
                                tr("Screen dimming percentage must be between 10% and 90%."));
            return false;
        }
    }
    
    return true;
}

void ModernMainWindow::resetServerConfigToOriginal()
{
    // Reset all UI controls to original values
    ui->lineEditServerName->setText(m_originalServerConfig.serverName);
    ui->checkBoxHeartbeat->setChecked(m_originalServerConfig.hasHeartbeat);
    ui->spinBoxHeartbeat->setValue(m_originalServerConfig.heartbeat);
    ui->checkBoxRelativeMouseMoves->setChecked(m_originalServerConfig.relativeMouseMoves);
    ui->checkBoxScreenSaverSync->setChecked(m_originalServerConfig.screenSaverSync);
    ui->checkBoxWin32KeepForeground->setChecked(m_originalServerConfig.win32KeepForeground);
    ui->checkBoxSwitchDelay->setChecked(m_originalServerConfig.hasSwitchDelay);
    ui->spinBoxSwitchDelay->setValue(m_originalServerConfig.switchDelay);
    ui->checkBoxSwitchDoubleTap->setChecked(m_originalServerConfig.hasSwitchDoubleTap);
    ui->spinBoxSwitchDoubleTap->setValue(m_originalServerConfig.switchDoubleTap);
    ui->spinBoxSwitchCornerSize->setValue(m_originalServerConfig.switchCornerSize);
    ui->checkBoxCornerTopLeft->setChecked(m_originalServerConfig.switchCorners[0]);
    ui->checkBoxCornerTopRight->setChecked(m_originalServerConfig.switchCorners[1]);
    ui->checkBoxCornerBottomLeft->setChecked(m_originalServerConfig.switchCorners[2]);
    ui->checkBoxCornerBottomRight->setChecked(m_originalServerConfig.switchCorners[3]);
    ui->checkBoxClipboardSharing->setChecked(m_originalServerConfig.clipboardSharing);
    ui->checkBoxDragAndDrop->setChecked(m_originalServerConfig.enableDragAndDrop);
    ui->checkBoxIgnoreAutoConfig->setChecked(m_originalServerConfig.ignoreAutoConfigClient);
    ui->checkBoxScreenDimming->setChecked(m_originalServerConfig.screenDimmingEnabled);
    ui->sliderDimmingPercentage->setValue(m_originalServerConfig.screenDimmingPercentage);
    
    // Reset change tracking
    m_serverConfigChanged = false;
    updateApplyButton();
}

void ModernMainWindow::markServerConfigChanged()
{
    m_serverConfigChanged = true;
    updateApplyButton();
}

void ModernMainWindow::onHeartbeatToggled(bool enabled)
{
    ui->spinBoxHeartbeat->setEnabled(enabled);
    onServerConfigChanged();
}

void ModernMainWindow::onSwitchDelayToggled(bool enabled)
{
    ui->spinBoxSwitchDelay->setEnabled(enabled);
    onServerConfigChanged();
}

void ModernMainWindow::onSwitchDoubleTapToggled(bool enabled)
{
    ui->spinBoxSwitchDoubleTap->setEnabled(enabled);
    onServerConfigChanged();
}

void ModernMainWindow::onScreenDimmingToggled(bool enabled)
{
    ui->sliderDimmingPercentage->setEnabled(enabled);
    onServerConfigChanged();
}

void ModernMainWindow::onDimmingPercentageChanged(int value)
{
    ui->labelDimmingValue->setText(QString("%1%").arg(value));
    onServerConfigChanged();
}

void ModernMainWindow::onServerConfigChanged()
{
    markServerConfigChanged();
}

void ModernMainWindow::onApplyClicked()
{
    saveServerConfig();
}

void ModernMainWindow::onClientConfigureClicked()
{
    QMessageBox::information(this, tr("Client Configuration"), 
                           tr("Client configuration will be implemented here.\n\n"
                              "This will include:\n"
                              "• Server connection settings\n"
                              "• Auto-configuration options\n"
                              "• Client-specific preferences"));
}

void ModernMainWindow::onLayoutConfigureClicked()
{
    QMessageBox::information(this, tr("Screen Layout Configuration"), 
                           tr("Screen layout configuration will be implemented here.\n\n"
                              "This will include:\n"
                              "• Drag-and-drop screen arrangement\n"
                              "• Screen positioning and sizing\n"
                              "• Hotkey assignments"));
}

void ModernMainWindow::onMultiMonitorConfigureClicked()
{
    QMessageBox::information(this, tr("Multi-Monitor Configuration"), 
                           tr("Multi-monitor configuration will be implemented here.\n\n"
                              "This will include:\n"
                              "• Automatic monitor detection\n"
                              "• Multi-monitor screen layouts\n"
                              "• Monitor-specific settings"));
}

void ModernMainWindow::onAdvancedConfigureClicked()
{
    QMessageBox::information(this, tr("Advanced Settings"), 
                           tr("Advanced settings will be implemented here.\n\n"
                              "This will include:\n"
                              "• Logging configuration\n"
                              "• Performance settings\n"
                              "• Import/Export configuration"));
}

// Mode selection slots
void ModernMainWindow::onServerModeSelected()
{
    if (ui->radioButtonServerMode->isChecked()) {
        m_isServerMode = true;
        updateTabVisibility();
        // Daemon status is always visible regardless of mode
    }
}

void ModernMainWindow::onClientModeSelected()
{
    if (ui->radioButtonClientMode->isChecked()) {
        m_isServerMode = false;
        updateTabVisibility();
        // Daemon status is always visible regardless of mode
    }
}

// Daemon status control slots
void ModernMainWindow::onStartDaemonClicked()
{
    // Use modern DaemonManager for daemon control
    AppRole role = m_isServerMode ? AppRole::Server : AppRole::Client;
    
    // Set up client connection details if in client mode
    if (!m_isServerMode) {
        // Get server hostname from UI (you may need to adjust this based on your UI)
        // For now, using a default - this should be configurable in the UI
        m_daemonManager->setServerHostname("localhost");
        m_daemonManager->setServerPort(24800);
    }
    
    updateDaemonStatus("Starting...", "orange");
    ui->pushButtonStart->setEnabled(false);
    ui->pushButtonStop->setEnabled(true);
    ui->pushButtonReload->setEnabled(false);
    
    m_daemonManager->startDaemon(role);
    m_currentDaemonStatus = DaemonStatus::Initializing;
}

void ModernMainWindow::onStopDaemonClicked()
{
    updateDaemonStatus("Stopping...", "orange");
    ui->pushButtonStart->setEnabled(true);
    ui->pushButtonStop->setEnabled(false);
    ui->pushButtonReload->setEnabled(false);
    
    m_daemonManager->stopDaemon();
    m_currentDaemonStatus = DaemonStatus::Stopped;
}

void ModernMainWindow::onReloadDaemonClicked()
{
    updateDaemonStatus("Restarting...", "orange");
    ui->pushButtonStart->setEnabled(false);
    ui->pushButtonStop->setEnabled(false);
    ui->pushButtonReload->setEnabled(false);
    
    m_daemonManager->restartDaemon();
    m_currentDaemonStatus = DaemonStatus::Initializing;
}

// Mode and status management functions
void ModernMainWindow::updateTabVisibility()
{
    QTabWidget* tabWidget = ui->tabWidget;
    
    if (m_isServerMode) {
        // Server mode: show all tabs except Client
        for (int i = 0; i < tabWidget->count(); ++i) {
            QWidget* tab = tabWidget->widget(i);
            if (tab == ui->tabClient) {
                tabWidget->setTabVisible(i, false);
            } else {
                tabWidget->setTabVisible(i, true);
            }
        }
        // Switch to Server tab if currently on Client tab
        if (tabWidget->currentWidget() == ui->tabClient) {
            tabWidget->setCurrentWidget(ui->tabServer);
        }
    } else {
        // Client mode: show only Client tab
        for (int i = 0; i < tabWidget->count(); ++i) {
            QWidget* tab = tabWidget->widget(i);
            if (tab == ui->tabClient) {
                tabWidget->setTabVisible(i, true);
                tabWidget->setCurrentWidget(ui->tabClient);
            } else {
                tabWidget->setTabVisible(i, false);
            }
        }
    }
}

void ModernMainWindow::updateDaemonStatus(const QString& status, const QString& color)
{
    ui->labelStatusText->setText(status);
    ui->labelStatusIcon->setStyleSheet(QString("color: %1; font-size: 14px;").arg(color));
}

void ModernMainWindow::updateDaemonButtons(AppConnectionState state)
{
    bool isConnecting = (state == AppConnectionState::CONNECTING);
    bool isConnected = (state == AppConnectionState::CONNECTED || state == AppConnectionState::TRANSFERRING);
    bool isDisconnected = (state == AppConnectionState::DISCONNECTED);
    
    // Update button states to match MainWindow behavior
    ui->pushButtonStart->setEnabled(isDisconnected);
    ui->pushButtonStop->setEnabled(isConnected || isConnecting);
    ui->pushButtonReload->setEnabled(isConnected);
    
    // Update visual status
    switch (state) {
        case AppConnectionState::DISCONNECTED:
            updateDaemonStatus("Stopped", "red");
            m_currentDaemonStatus = DaemonStatus::Stopped;
            break;
        case AppConnectionState::CONNECTING:
            updateDaemonStatus("Initializing", "orange");
            m_currentDaemonStatus = DaemonStatus::Initializing;
            break;
        case AppConnectionState::CONNECTED:
        case AppConnectionState::TRANSFERRING:
            updateDaemonStatus("Running", "green");
            m_currentDaemonStatus = DaemonStatus::Running;
            break;
    }
}

void ModernMainWindow::syncDaemonStatus()
{
    // Get current daemon status from DaemonManager
    bool isRunning = m_daemonManager->isDaemonRunning();
    
    if (isRunning) {
        updateDaemonStatus("Running", "green");
        ui->pushButtonStart->setEnabled(false);
        ui->pushButtonStop->setEnabled(true);
        ui->pushButtonReload->setEnabled(true);
        m_currentDaemonStatus = DaemonStatus::Running;
    } else {
        updateDaemonStatus("Stopped", "red");
        ui->pushButtonStart->setEnabled(true);
        ui->pushButtonStop->setEnabled(false);
        ui->pushButtonReload->setEnabled(false);
        m_currentDaemonStatus = DaemonStatus::Stopped;
    }
}

void ModernMainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QMainWindow::changeEvent(event);
}
