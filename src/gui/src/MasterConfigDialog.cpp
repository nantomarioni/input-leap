#include "MasterConfigDialog.h"
#include "ui_MasterConfigDialog.h"

#include "AppConfig.h"
#include "ServerConfig.h"

#include <QtCore>
#include <QtGui>
#include <QMessageBox>

MasterConfigDialog::MasterConfigDialog(QWidget* parent, AppConfig& config, ServerConfig& serverConfig, const QString& defaultScreen)
    : QDialog(nullptr, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
    , ui(new Ui::MasterConfigDialog)
    , m_appConfig(config)
    , m_serverConfig(serverConfig)
{
    ui->setupUi(this);
    setupConnections();
    
    // Set a proper window title and icon
    setWindowTitle(tr("Input Leap Configuration"));
    setWindowIcon(QIcon(":/res/icons/256x256/input-leap.png"));
    
    // Center on parent if provided
    if (parent) {
        move(parent->frameGeometry().topLeft() + 
             parent->rect().center() - rect().center());
    }
}

MasterConfigDialog::~MasterConfigDialog()
{
    delete ui;
}

void MasterConfigDialog::setupConnections()
{
    // Server configuration
    connect(ui->pushButtonServerConfigure, &QPushButton::clicked, this, &MasterConfigDialog::onServerConfigureClicked);
    
    // Client configuration  
    connect(ui->pushButtonClientConfigure, &QPushButton::clicked, this, &MasterConfigDialog::onClientConfigureClicked);
    
    // Layout configuration
    connect(ui->pushButtonLayoutConfigure, &QPushButton::clicked, this, &MasterConfigDialog::onLayoutConfigureClicked);
    
    // Multi-monitor configuration
    connect(ui->pushButtonMultiMonitorConfigure, &QPushButton::clicked, this, &MasterConfigDialog::onMultiMonitorConfigureClicked);
    
    // Advanced configuration
    connect(ui->pushButtonAdvancedConfigure, &QPushButton::clicked, this, &MasterConfigDialog::onAdvancedConfigureClicked);
}

void MasterConfigDialog::onServerConfigureClicked()
{
    QMessageBox::information(this, tr("Server Configuration"), 
                           tr("Server configuration will be implemented here.\n\n"
                              "This will include:\n"
                              "• Server screen name and settings\n"
                              "• Network configuration\n"
                              "• Security settings"));
}

void MasterConfigDialog::onClientConfigureClicked()
{
    QMessageBox::information(this, tr("Client Configuration"), 
                           tr("Client configuration will be implemented here.\n\n"
                              "This will include:\n"
                              "• Server connection settings\n"
                              "• Auto-configuration options\n"
                              "• Client-specific preferences"));
}

void MasterConfigDialog::onLayoutConfigureClicked()
{
    QMessageBox::information(this, tr("Screen Layout Configuration"), 
                           tr("Screen layout configuration will be implemented here.\n\n"
                              "This will include:\n"
                              "• Drag-and-drop screen arrangement\n"
                              "• Screen positioning and sizing\n"
                              "• Hotkey assignments"));
}

void MasterConfigDialog::onMultiMonitorConfigureClicked()
{
    QMessageBox::information(this, tr("Multi-Monitor Configuration"), 
                           tr("Multi-monitor configuration will be implemented here.\n\n"
                              "This will include:\n"
                              "• Automatic monitor detection\n"
                              "• Multi-monitor screen layouts\n"
                              "• Monitor-specific settings"));
}

void MasterConfigDialog::onAdvancedConfigureClicked()
{
    QMessageBox::information(this, tr("Advanced Settings"), 
                           tr("Advanced settings will be implemented here.\n\n"
                              "This will include:\n"
                              "• Logging configuration\n"
                              "• Performance settings\n"
                              "• Import/Export configuration"));
}

void MasterConfigDialog::accept()
{
    // TODO: Save any configuration changes when implemented
    // For now, just close the dialog
    QDialog::accept();
}

void MasterConfigDialog::reject()
{
    QDialog::reject();
}

void MasterConfigDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QDialog::changeEvent(event);
}