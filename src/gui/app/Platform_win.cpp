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

#include "Platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace {
const char kRunKey[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char kRunValue[] = "InputLeap";

BOOL CALLBACK addMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        const RECT& r = info.rcMonitor;
        reinterpret_cast<QVector<QRect>*>(data)->append(QRect(r.left, r.top, r.right - r.left, r.bottom - r.top));
    }
    return TRUE;
}
} // namespace

namespace Platform {

bool accessibilityGranted() { return true; }
void requestAccessibility() {}
bool installedInApplicationsFolder() { return true; }
void setDockIconVisible(bool) {}

bool launchAtLoginEnabled()
{
    QSettings run(QLatin1String(kRunKey), QSettings::NativeFormat);
    return run.contains(QLatin1String(kRunValue));
}

bool setLaunchAtLogin(bool enabled)
{
    QSettings run(QLatin1String(kRunKey), QSettings::NativeFormat);
    if (enabled) {
        run.setValue(QLatin1String(kRunValue),
                     QStringLiteral("\"%1\" --hidden").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    } else {
        run.remove(QLatin1String(kRunValue));
    }
    run.sync();
    return run.status() == QSettings::NoError;
}

bool serviceRunning(const QString& name)
{
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (manager == nullptr) {
        return false;
    }
    bool running = false;
    SC_HANDLE service = OpenServiceW(manager, reinterpret_cast<LPCWSTR>(name.utf16()), SERVICE_QUERY_STATUS);
    if (service != nullptr) {
        SERVICE_STATUS status;
        if (QueryServiceStatus(service, &status)) {
            running = status.dwCurrentState == SERVICE_RUNNING;
        }
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return running;
}

QString name()
{
    return QStringLiteral("Windows");
}

// Qt makes the window per-monitor DPI aware, so the rectangles are in
// physical pixels, the same space as the core's virtual desktop.
QVector<QRect> displays()
{
    QVector<QRect> list;
    EnumDisplayMonitors(nullptr, nullptr, addMonitor, reinterpret_cast<LPARAM>(&list));
    return list;
}

} // namespace Platform
