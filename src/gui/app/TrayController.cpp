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

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QQuickWindow>
#include <QSystemTrayIcon>

namespace {

QIcon trayIcon(AppController::RunState state)
{
#if defined(Q_OS_MAC)
    const char* file = state == AppController::Running
        ? ":/tray/icons/128x128/input-leap-connected-mask.png"
        : ":/tray/icons/128x128/input-leap-disconnected-mask.png";
    QIcon icon(QString::fromLatin1(file));
    icon.setIsMask(true);
    return icon;
#else
    const char* name = state == AppController::Running ? "input-leap-connected" : "input-leap-disconnected";
    const QString file = QStringLiteral(":/tray/icons/128x128/%1.png").arg(QLatin1String(name));
    return QIcon::fromTheme(QLatin1String(name), QIcon(file));
#endif
}

} // namespace

TrayController::TrayController(QQuickWindow* window, QObject* parent) :
    QObject(parent),
    m_window(window)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    AppController* app = AppController::instance();

    m_menu = new QMenu();
    m_menu->addAction(tr("Show Input Leap"), this, &TrayController::showWindow);
    m_startStopAction = m_menu->addAction(QString(), this, [app]() {
        const auto state = app->runState();
        if (state == AppController::Stopped || state == AppController::Failed) {
            app->start();
        } else {
            app->stop();
        }
    });
    m_menu->addAction(tr("Restart"), app, &AppController::restart);
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit Input Leap"), qApp, &QCoreApplication::quit);

    m_tray = new QSystemTrayIcon(this);
    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip(QStringLiteral("Input Leap"));
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            toggleWindow();
        }
    });
    connect(app, &AppController::stateChanged, this, &TrayController::sync);

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
    const auto state = app->runState();
    m_tray->setIcon(trayIcon(state));
    m_tray->setToolTip(QStringLiteral("Input Leap · ") + app->statusText());
    const bool stopped = state == AppController::Stopped || state == AppController::Failed;
    m_startStopAction->setText(stopped ? tr("Start") : tr("Stop"));
}
