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

#include "TrayController.h"
#include "AppController.h"
#include "AppSettings.h"
#include "Platform.h"

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QQuickWindow>
#include <QSystemTrayIcon>

namespace {

QIcon trayIcon(bool connected)
{
#if defined(Q_OS_MAC)
    const char* file = connected ? ":/tray/icons/128x128/input-leap-connected-mask.png"
                                 : ":/tray/icons/128x128/input-leap-disconnected-mask.png";
    QIcon icon(QString::fromLatin1(file));
    icon.setIsMask(true);
    return icon;
#else
    const char* name = connected ? "input-leap-connected" : "input-leap-disconnected";
    const QString file = QStringLiteral(":/tray/icons/128x128/%1.png").arg(QLatin1String(name));
    return QIcon::fromTheme(QLatin1String(name), QIcon(file));
#endif
}

} // namespace

TrayController::TrayController(QQuickWindow* window, QObject* parent) :
    QObject(parent),
    m_window(window)
{
    AppController* app = AppController::instance();

    if (m_window != nullptr) {
        connect(m_window, &QWindow::visibleChanged, this, [](bool visible) {
            Platform::setDockIconVisible(visible);
        });
        Platform::setDockIconVisible(m_window->isVisible());
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_menu = new QMenu();
    m_menu->addAction(tr("Show Input Leap"), this, &TrayController::showWindow);
    m_startStopAction = m_menu->addAction(QString(), this, [app]() {
        if (app->canSwitchRole()) {
            app->start();
        } else {
            app->stop();
        }
    });
    m_menu->addAction(tr("Restart"), app, &AppController::restart);
    m_menu->addSeparator();
    m_menu->addAction(tr("Check for Updates…"), app, &AppController::checkForUpdates);
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit Input Leap"), qApp, &QCoreApplication::quit);

    m_tray = new QSystemTrayIcon(this);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            toggleWindow();
        }
    });
    connect(app, &AppController::stateChanged, this, &TrayController::sync);
    connect(app, &AppController::notify, this, [this, app](const QString& title, const QString& text) {
        if (m_tray != nullptr && app->settings()->systemNotifications() && (m_window == nullptr || !m_window->isVisible())) {
            m_tray->showMessage(title, text, QSystemTrayIcon::Information, 5000);
        }
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &TrayController::showWindow);

    sync();
    m_tray->show();
}

TrayController::~TrayController()
{
    delete m_menu;
}

void TrayController::showWindow()
{
    if (m_window == nullptr) {
        return;
    }
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void TrayController::toggleWindow()
{
    if (m_window == nullptr) {
        return;
    }
    if (m_window->isVisible()) {
        m_window->hide();
    } else {
        showWindow();
    }
}

void TrayController::sync()
{
    AppController* app = AppController::instance();
    m_tray->setIcon(trayIcon(app->connected()));
    m_tray->setToolTip(QStringLiteral("Input Leap · ") + app->statusText());
    m_startStopAction->setText(app->canSwitchRole() ? tr("Start") : tr("Stop"));
}
