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

#include "CoreEventParser.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

const QStringList& levelNames()
{
    static const QStringList names{
        QStringLiteral("ERROR"), QStringLiteral("WARNING"), QStringLiteral("NOTE"),
        QStringLiteral("INFO"), QStringLiteral("DEBUG"), QStringLiteral("DEBUG1"),
        QStringLiteral("DEBUG2")};
    return names;
}

QString quoted(const QString& text, int index)
{
    // the index-th double-quoted token
    static const QRegularExpression re(QStringLiteral("\"([^\"]*)\""));
    auto it = re.globalMatch(text);
    int i = 0;
    while (it.hasNext()) {
        const auto match = it.next();
        if (i++ == index) {
            return match.captured(1);
        }
    }
    return QString();
}

} // namespace

CoreLogLine CoreEventParser::splitLine(const QString& rawLine)
{
    CoreLogLine line;
    QString text = rawLine;
    // debug builds append "\n\tfile,line"; keep the message only
    const int nl = text.indexOf(QLatin1Char('\n'));
    if (nl >= 0) {
        text.truncate(nl);
    }
    text = text.trimmed();

    static const QRegularExpression prefix(
        QStringLiteral("^\\[([0-9T:\\-]+)\\]\\s+([A-Z0-9]+):\\s*(.*)$"));
    const auto match = prefix.match(text);
    if (match.hasMatch()) {
        line.timestamp = match.captured(1);
        line.level = match.captured(2);
        line.message = match.captured(3).trimmed();
    } else {
        static const QRegularExpression bare(QStringLiteral("^([A-Z0-9]+):\\s+(.*)$"));
        const auto bareMatch = bare.match(text);
        if (bareMatch.hasMatch() && levelIndex(bareMatch.captured(1)) >= 0) {
            line.level = bareMatch.captured(1);
            line.message = bareMatch.captured(2).trimmed();
        } else {
            line.message = text;
        }
    }
    return line;
}

CoreEvent CoreEventParser::parse(const QString& message)
{
    CoreEvent event;
    const QString m = message.trimmed();
    if (m.isEmpty()) {
        return event;
    }

    // ---- server side -------------------------------------------------------
    if (m.startsWith(QLatin1String("started server")) ||
        m == QLatin1String("server status: active")) {
        event.type = CoreEvent::Type::ServerStarted;
        return event;
    }
    if (m == QLatin1String("stopped server")) {
        event.type = CoreEvent::Type::ServerStopped;
        return event;
    }
    if (m.startsWith(QLatin1String("client \"")) && m.endsWith(QLatin1String("\" has connected"))) {
        event.type = CoreEvent::Type::ClientConnected;
        event.name = quoted(m, 0);
        return event;
    }
    if (m.startsWith(QLatin1String("client \"")) && m.endsWith(QLatin1String("\" has disconnected"))) {
        event.type = CoreEvent::Type::ClientDisconnected;
        event.name = quoted(m, 0);
        return event;
    }
    if (m.startsWith(QLatin1String("unrecognised client name \"")) ||
        m.startsWith(QLatin1String("a client with name \"")) ||
        (m.startsWith(QLatin1String("disconnecting client \"")) && m.contains(QLatin1String("already")))) {
        event.type = CoreEvent::Type::ClientRefused;
        event.name = quoted(m, 0);
        event.detail = m;
        return event;
    }
    if (m.startsWith(QLatin1String("switch from \""))) {
        event.type = CoreEvent::Type::SwitchedScreen;
        event.detail = quoted(m, 0);
        event.name = quoted(m, 1);
        return event;
    }
    if (m == QLatin1String("cursor locked to current screen") ||
        m == QLatin1String("Scroll Lock is on, locking cursor to screen")) {
        event.type = CoreEvent::Type::CursorLockChanged;
        event.flag = true;
        return event;
    }
    if (m == QLatin1String("cursor unlocked from current screen")) {
        event.type = CoreEvent::Type::CursorLockChanged;
        event.flag = false;
        return event;
    }
    if (m.startsWith(QLatin1String("cannot read configuration")) ||
        m.startsWith(QLatin1String("cannot listen for clients")) ||
        m.startsWith(QLatin1String("primary screen unavailable")) ||
        m.startsWith(QLatin1String("secondary screen unavailable"))) {
        event.type = CoreEvent::Type::Fatal;
        event.detail = m;
        return event;
    }

    // ---- client side -------------------------------------------------------
    if (m == QLatin1String("connected to server")) {
        event.type = CoreEvent::Type::ConnectedToServer;
        return event;
    }
    if (m == QLatin1String("disconnected from server") ||
        m.startsWith(QLatin1String("disconnected:"))) {
        event.type = CoreEvent::Type::DisconnectedFromServer;
        event.detail = m.startsWith(QLatin1String("disconnected:")) ? m.mid(13).trimmed() : QString();
        return event;
    }
    if (m.startsWith(QLatin1String("failed to connect to server"))) {
        event.type = CoreEvent::Type::ConnectFailed;
        const int colon = m.indexOf(QLatin1Char(':'));
        event.detail = colon >= 0 ? m.mid(colon + 1).trimmed() : QString();
        return event;
    }
    if (m.startsWith(QLatin1String("server refused client with name \""))) {
        event.type = CoreEvent::Type::ClientRefused;
        event.name = quoted(m, 0);
        event.detail = m;
        return event;
    }
    if (m == QLatin1String("clipboard was updated")) {
        event.type = CoreEvent::Type::ClipboardUpdated;
        return event;
    }

    // ---- fork markers (both sides) ------------------------------------------
    if (m.startsWith(QLatin1String("fork: screen dimmed"))) {
        event.type = CoreEvent::Type::ScreenDimmed;
        return event;
    }
    if (m.startsWith(QLatin1String("fork: screen restored"))) {
        event.type = CoreEvent::Type::ScreenRestored;
        return event;
    }
    if (m.startsWith(QLatin1String("fork: cursor locked to another screen"))) {
        event.type = CoreEvent::Type::CursorLockChanged;
        event.flag = true;
        event.detail = QStringLiteral("elsewhere");
        return event;
    }
    if (m.startsWith(QLatin1String("fork: cursor lock released"))) {
        event.type = CoreEvent::Type::CursorLockChanged;
        event.flag = false;
        return event;
    }

    // ---- TLS ----------------------------------------------------------------
    static const QRegularExpression fingerprint(
        QStringLiteral("peer fingerprint \\(SHA1\\): ([A-Fa-f0-9:]+) \\(SHA256\\): ([A-Fa-f0-9:]+)"));
    const auto fp = fingerprint.match(m);
    if (fp.hasMatch()) {
        event.type = CoreEvent::Type::PeerFingerprint;
        event.sha1 = fp.captured(1).toUpper();
        event.sha256 = fp.captured(2).toUpper();
        return event;
    }

    return event;
}

int CoreEventParser::levelIndex(const QString& level)
{
    if (level == QLatin1String("FATAL")) {
        return 0;
    }
    return levelNames().indexOf(level.toUpper());
}

QString CoreEventParser::levelName(int index)
{
    return levelNames().value(qBound(0, index, int(levelNames().size()) - 1));
}
