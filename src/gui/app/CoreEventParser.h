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

#include <QString>

/* The only place that knows what the core prints (docs/ui-redesign.md §4.3).
 *
 * The core writes "[2026-10-31T09:41:07] NOTE: client "macbook" has
 * connected". splitLine() takes the prefix apart; parse() turns the message
 * into a typed event the controller acts on. Everything else stays a plain
 * log line.
 */
struct CoreLogLine
{
    QString timestamp;   // "2026-10-31T09:41:07", empty when the line has no prefix
    QString level;       // "NOTE", "INFO", "ERROR", ... empty when none
    QString message;     // the rest, trimmed
};

struct CoreEvent
{
    enum class Type {
        None,
        ServerStarted,          // server listens
        ServerStopped,
        ClientConnected,        // name
        ClientDisconnected,     // name
        ClientRefused,          // name (duplicate name, bad config)
        ConnectedToServer,      // client is in
        DisconnectedFromServer, // detail = reason when known
        ConnectFailed,          // detail = reason
        SwitchedScreen,         // name = new screen, detail = previous one
        CursorLockChanged,      // flag = locked; detail = "elsewhere" on a client that lost the cursor
        ScreenDimmed,
        ScreenRestored,
        ClipboardUpdated,
        PeerFingerprint,        // sha1 / sha256 (hex with colons)
        Fatal                   // detail = message (config unreadable, cannot listen, ...)
    };

    Type type = Type::None;
    QString name;
    QString detail;
    bool flag = false;
    QString sha1;
    QString sha256;
};

class CoreEventParser
{
public:
    static CoreLogLine splitLine(const QString& rawLine);
    static CoreEvent parse(const QString& message);

    // Convenience: split + parse a raw line.
    static CoreEvent parseLine(const QString& rawLine) { return parse(splitLine(rawLine).message); }

    // Numeric severity for a core level name (ERROR=0 ... DEBUG2=6, as the
    // settings store it); -1 when unknown.
    static int levelIndex(const QString& level);
    static QString levelName(int index);
};
