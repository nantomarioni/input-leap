/*  InputLeap (nantomarioni fork) -- mouse and keyboard sharing utility
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

#include "NotificationRouter.h"
#include "OverlayNotification.h"

#include <QObject>

namespace inputleap {
namespace fork_gui {

namespace {
QString tr(const char* s) { return QObject::tr(s); }
} // namespace

void routeLogLineToOverlay(const QString& line)
{
    auto* overlay = OverlayNotification::instance();

    // ---- connection ------------------------------------------------------
    if (line.contains(QLatin1String("connected to server")) ||
        line.contains(QLatin1String("started server"))) {
        overlay->dismissKey(QStringLiteral("disconnected"));
        overlay->showTransient(tr("InputLeap connected"),
                               OverlayNotification::Tone::Success);
        return;
    }
    if (line.contains(QLatin1String("disconnected: server closed the connection")) ||
        line.contains(QLatin1String("failed to connect to server"))) {
        overlay->showTransient(tr("InputLeap disconnected"),
                               OverlayNotification::Tone::Warning, 3200);
        return;
    }

    // ---- clipboard -------------------------------------------------------
    if (line.contains(QLatin1String("clipboard was updated"))) {
        overlay->showTransient(tr("Clipboard synced"),
                               OverlayNotification::Tone::Info, 1800);
        return;
    }

    // ---- screen dimming (fork markers from the client daemon) -------------
    if (line.contains(QLatin1String("fork: screen dimmed"))) {
        overlay->showTransient(tr("Screen dimmed \u2014 touch input to wake"),
                               OverlayNotification::Tone::Info, 2200);
        return;
    }
    if (line.contains(QLatin1String("fork: screen restored"))) {
        // restoring is self-evident (the screen brightens); keep it quiet,
        // but clear a stale lock warning if one is showing.
        overlay->dismissKey(QStringLiteral("wake-ignored"));
        return;
    }
    if (line.contains(QLatin1String("fork: undim requests not honored"))) {
        overlay->showPersistent(QStringLiteral("wake-ignored"),
                                tr("Can't wake this screen \u2014 cursor may be locked elsewhere"),
                                OverlayNotification::Tone::Warning);
        return;
    }

    // ---- cursor lock (server side, upstream NOTE lines) --------------------
    if (line.contains(QLatin1String("cursor locked to current screen"))) {
        overlay->showPersistent(QStringLiteral("cursor-lock"),
                                tr("Cursor locked to this screen (Scroll Lock)"),
                                OverlayNotification::Tone::Warning);
        return;
    }
    if (line.contains(QLatin1String("cursor unlocked from current screen"))) {
        overlay->dismissKey(QStringLiteral("cursor-lock"));
        overlay->showTransient(tr("Cursor unlocked"),
                               OverlayNotification::Tone::Success, 1800);
        return;
    }
}

} // namespace fork_gui
} // namespace inputleap
