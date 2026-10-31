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

class QAction;
class QMenu;
class QQuickWindow;
class QSystemTrayIcon;
class QTimer;

/* The system tray icon and its menu. QtWidgets-based on purpose: the tray
 * and native menus are the one place the widgets module still earns its
 * place next to the Qt Quick window. When the tray is not there yet (a
 * launch at login before the shell's tray exists) it keeps trying for a
 * while, and the window stays visible until it appears.
 */
class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QQuickWindow* window, QObject* parent = nullptr);
    ~TrayController() override;

    bool available() const { return m_tray != nullptr; }

public slots:
    void showWindow();
    void toggleWindow();

signals:
    void availableChanged();

private:
    void createTray();
    void sync();

    QQuickWindow* m_window;
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_startStopAction = nullptr;
    QTimer* m_retry = nullptr;
    int m_attempts = 0;
};
