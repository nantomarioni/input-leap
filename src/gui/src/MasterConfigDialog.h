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

#include <QDialog>

class AppConfig;
class ServerConfig;
class QEvent;

namespace Ui {
    class MasterConfigDialog;
}

class MasterConfigDialog : public QDialog
{
    Q_OBJECT

public:
    MasterConfigDialog(QWidget* parent, AppConfig& config, ServerConfig& serverConfig, const QString& defaultScreen);
    ~MasterConfigDialog();

    void accept() override;
    void reject() override;

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

private:
    void setupConnections();
    
    Ui::MasterConfigDialog* ui;
    AppConfig& m_appConfig;
    ServerConfig& m_serverConfig;
};
