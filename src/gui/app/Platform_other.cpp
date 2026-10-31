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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace {
QString autostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/autostart/io.github.input_leap.input-leap.desktop");
}
} // namespace

namespace Platform {

bool accessibilityGranted() { return true; }
void requestAccessibility() {}
bool installedInApplicationsFolder() { return true; }
void setDockIconVisible(bool) {}

bool launchAtLoginEnabled()
{
    return QFile::exists(autostartPath());
}

bool setLaunchAtLogin(bool enabled)
{
    const QString path = autostartPath();
    if (!enabled) {
        return !QFile::exists(path) || QFile::remove(path);
    }
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    out << "[Desktop Entry]\nType=Application\nName=Input Leap\nExec="
        << QCoreApplication::applicationFilePath() << "\nX-GNOME-Autostart-enabled=true\n";
    return true;
}

bool serviceRunning(const QString&) { return false; }

QString name()
{
    return QStringLiteral("Linux");
}

} // namespace Platform
