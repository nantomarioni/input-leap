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

#include <QRect>
#include <QString>
#include <QVector>

/* The few OS-specific things the window needs, one implementation per
 * platform file (Platform_mac.cpp, Platform_win.cpp, Platform_other.cpp).
 */
namespace Platform {

// macOS: the Accessibility permission the core needs to see and inject input.
// Elsewhere: always granted.
bool accessibilityGranted();
// macOS: asks the system to prompt for it (opens the privacy settings).
void requestAccessibility();

// macOS: the app must live in /Applications for the permission to stick.
bool installedInApplicationsFolder();

// macOS: hide the Dock icon while the window is hidden (tray app behaviour).
void setDockIconVisible(bool visible);

// Launch at login: a LaunchAgent on macOS, the Run registry key on Windows,
// an autostart .desktop file on Linux. Returns false when unsupported or on
// failure.
bool launchAtLoginEnabled();
bool setLaunchAtLogin(bool enabled);

// Windows: whether a service is running ("Bonjour Service", the daemon).
bool serviceRunning(const QString& name);

// This computer's displays in the pixels its core sees: points on macOS
// (CGDisplayBounds, like OSXScreen), physical pixels on Windows (the core is
// per-monitor DPI aware), device pixels elsewhere. Empty when unknown.
QVector<QRect> displays();

// Human readable name of the platform's service flavour ("Windows service",
// "LaunchAgent", ...).
QString name();

} // namespace Platform
