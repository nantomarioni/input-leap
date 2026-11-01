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
#include <setupapi.h>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace {
const char kRunKey[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char kRunValue[] = "InputLeap";

// GUID_DEVINTERFACE_MONITOR, without the DDK header that declares it
const GUID kMonitorInterface = {0xe6f07b5f, 0xee97, 0x4a90, {0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7}};

// A monitor's size from its EDID: the first detailed timing descriptor in
// millimetres, else the basic block's centimetres. Empty when unknown.
QSize edidSize(const QByteArray& edid)
{
    if (edid.size() < 128) {
        return QSize();
    }
    const auto* b = reinterpret_cast<const unsigned char*>(edid.constData());
    if ((b[54] | b[55]) != 0) {   // a pixel clock: the block is a timing, not a text
        const int w = b[66] | ((b[68] & 0xF0) << 4);
        const int h = b[67] | ((b[68] & 0x0F) << 8);
        if (w > 0 && h > 0) {
            return QSize(w, h);
        }
    }
    return b[21] > 0 && b[22] > 0 ? QSize(b[21] * 10, b[22] * 10) : QSize();
}

// The EDID of the monitor on one adapter output ("\\.\DISPLAY1"): its
// device interface, then that device's registry key.
QByteArray monitorEdid(const wchar_t* adapter)
{
    DISPLAY_DEVICEW device;
    device.cb = sizeof(device);
    if (!EnumDisplayDevicesW(adapter, 0, &device, EDD_GET_DEVICE_INTERFACE_NAME)) {
        return QByteArray();
    }
    HDEVINFO set = SetupDiGetClassDevsW(&kMonitorInterface, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) {
        return QByteArray();
    }
    QByteArray edid;
    SP_DEVICE_INTERFACE_DATA iface;
    iface.cbSize = sizeof(iface);
    if (SetupDiOpenDeviceInterfaceW(set, device.DeviceID, 0, &iface)) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &needed, nullptr);
        QByteArray buffer(int(qMax<DWORD>(needed, sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))), 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA info;
        info.cbSize = sizeof(info);
        if (SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, DWORD(buffer.size()), nullptr, &info)) {
            HKEY key = SetupDiOpenDevRegKey(set, &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
            if (key != INVALID_HANDLE_VALUE) {
                BYTE data[1024];
                DWORD size = sizeof(data);
                if (RegQueryValueExW(key, L"EDID", nullptr, nullptr, data, &size) == ERROR_SUCCESS) {
                    edid = QByteArray(reinterpret_cast<const char*>(data), int(size));
                }
                RegCloseKey(key);
            }
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return edid;
}

BOOL CALLBACK addMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    MONITORINFOEXW info;
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        const RECT& r = info.rcMonitor;
        Platform::Display d;
        d.rect = QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
        d.mm = Platform::matchOrientation(edidSize(monitorEdid(info.szDevice)), d.rect);
        reinterpret_cast<QVector<Platform::Display>*>(data)->append(d);
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
QVector<Display> displays()
{
    QVector<Display> list;
    EnumDisplayMonitors(nullptr, nullptr, addMonitor, reinterpret_cast<LPARAM>(&list));
    return list;
}

} // namespace Platform
