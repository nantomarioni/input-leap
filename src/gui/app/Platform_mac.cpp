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

#include <ApplicationServices/ApplicationServices.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace {

const char kAgentLabel[] = "io.github.input_leap.input-leap";

QString agentPath()
{
    return QDir::homePath() + QStringLiteral("/Library/LaunchAgents/") + QLatin1String(kAgentLabel) + QStringLiteral(".plist");
}

QString bundlePath()
{
    // .../InputLeap.app/Contents/MacOS/input-leap -> .../InputLeap.app
    const QDir dir(QCoreApplication::applicationDirPath());
    const QString app = QDir(dir.filePath(QStringLiteral("../.."))).canonicalPath();
    return app.endsWith(QLatin1String(".app")) ? app : QString();
}

} // namespace

namespace Platform {

bool accessibilityGranted()
{
    return AXIsProcessTrusted();
}

void requestAccessibility()
{
    const void* keys[] = {kAXTrustedCheckOptionPrompt};
    const void* values[] = {kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(nullptr, keys, values, 1, nullptr, nullptr);
    AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
}

bool installedInApplicationsFolder()
{
    return !QCoreApplication::applicationDirPath().startsWith(QLatin1String("/Volumes/"));
}

void setDockIconVisible(bool visible)
{
    ProcessSerialNumber psn = {0, kCurrentProcess};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    TransformProcessType(&psn, visible ? kProcessTransformToForegroundApplication
                                       : kProcessTransformToBackgroundApplication);
#pragma GCC diagnostic pop
}

bool launchAtLoginEnabled()
{
    return QFile::exists(agentPath());
}

bool setLaunchAtLogin(bool enabled)
{
    const QString path = agentPath();
    if (!enabled) {
        return !QFile::exists(path) || QFile::remove(path);
    }
    const QString app = bundlePath();
    if (app.isEmpty()) {
        return false;   // not running from a bundle: nothing sensible to register
    }
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
           "<plist version=\"1.0\">\n<dict>\n"
           "\t<key>Label</key>\n\t<string>" << kAgentLabel << "</string>\n"
           "\t<key>ProgramArguments</key>\n\t<array>\n"
           "\t\t<string>/usr/bin/open</string>\n\t\t<string>-g</string>\n\t\t<string>-a</string>\n"
           "\t\t<string>" << app << "</string>\n"
           "\t\t<string>--args</string>\n\t\t<string>--hidden</string>\n\t</array>\n"
           "\t<key>RunAtLoad</key>\n\t<true/>\n"
           "</dict>\n</plist>\n";
    return true;
}

bool serviceRunning(const QString&)
{
    return false;
}

QString name()
{
    return QStringLiteral("macOS");
}

QVector<QRect> displays()
{
    QVector<QRect> list;
    CGDirectDisplayID ids[16];
    uint32_t count = 0;
    if (CGGetActiveDisplayList(16, ids, &count) != kCGErrorSuccess) {
        return list;
    }
    for (uint32_t i = 0; i < count; ++i) {
        // a mirror has the bounds of the display it copies: one tile is enough
        if (CGDisplayIsInMirrorSet(ids[i]) && CGDisplayMirrorsDisplay(ids[i]) != kCGNullDirectDisplay) {
            continue;
        }
        const CGRect r = CGDisplayBounds(ids[i]);
        list << QRect(int(r.origin.x), int(r.origin.y), int(r.size.width), int(r.size.height));
    }
    return list;
}

} // namespace Platform
