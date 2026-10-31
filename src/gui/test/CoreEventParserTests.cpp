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

#include <gtest/gtest.h>

using Type = CoreEvent::Type;

TEST(CoreEventParser, splitsThePrefix)
{
    const auto line = CoreEventParser::splitLine(QStringLiteral("[2026-10-31T09:41:07] NOTE: client \"macbook\" has connected"));
    EXPECT_EQ(line.timestamp, QStringLiteral("2026-10-31T09:41:07"));
    EXPECT_EQ(line.level, QStringLiteral("NOTE"));
    EXPECT_EQ(line.message, QStringLiteral("client \"macbook\" has connected"));
}

TEST(CoreEventParser, keepsLinesWithoutPrefix)
{
    const auto line = CoreEventParser::splitLine(QStringLiteral("started server (IPv4), waiting for clients"));
    EXPECT_TRUE(line.timestamp.isEmpty());
    EXPECT_TRUE(line.level.isEmpty());
    EXPECT_EQ(line.message, QStringLiteral("started server (IPv4), waiting for clients"));
}

TEST(CoreEventParser, dropsTheDebugLocationSuffix)
{
    const auto line = CoreEventParser::splitLine(QStringLiteral("[2026-10-31T09:41:07] DEBUG: something\n\tServer.cpp,415"));
    EXPECT_EQ(line.message, QStringLiteral("something"));
}

TEST(CoreEventParser, serverEvents)
{
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("started server (IPv4), waiting for clients")).type, Type::ServerStarted);
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("server status: active")).type, Type::ServerStarted);
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("stopped server")).type, Type::ServerStopped);

    const auto connected = CoreEventParser::parse(QStringLiteral("client \"mac-studio\" has connected"));
    EXPECT_EQ(connected.type, Type::ClientConnected);
    EXPECT_EQ(connected.name, QStringLiteral("mac-studio"));

    const auto gone = CoreEventParser::parse(QStringLiteral("client \"mac-studio\" has disconnected"));
    EXPECT_EQ(gone.type, Type::ClientDisconnected);
    EXPECT_EQ(gone.name, QStringLiteral("mac-studio"));

    const auto refused = CoreEventParser::parse(QStringLiteral("unrecognised client name \"ghost\", check server config"));
    EXPECT_EQ(refused.type, Type::ClientRefused);
    EXPECT_EQ(refused.name, QStringLiteral("ghost"));

    const auto sw = CoreEventParser::parse(QStringLiteral("switch from \"desk-win\" to \"mac-studio\" at 1919,540"));
    EXPECT_EQ(sw.type, Type::SwitchedScreen);
    EXPECT_EQ(sw.name, QStringLiteral("mac-studio"));
    EXPECT_EQ(sw.detail, QStringLiteral("desk-win"));
}

TEST(CoreEventParser, clientEvents)
{
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("connected to server")).type, Type::ConnectedToServer);
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("disconnected from server")).type, Type::DisconnectedFromServer);

    const auto closed = CoreEventParser::parse(QStringLiteral("disconnected: server closed the connection"));
    EXPECT_EQ(closed.type, Type::DisconnectedFromServer);
    EXPECT_EQ(closed.detail, QStringLiteral("server closed the connection"));

    const auto failed = CoreEventParser::parse(QStringLiteral("failed to connect to server: connection refused"));
    EXPECT_EQ(failed.type, Type::ConnectFailed);
    EXPECT_EQ(failed.detail, QStringLiteral("connection refused"));

    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("clipboard was updated")).type, Type::ClipboardUpdated);
}

TEST(CoreEventParser, cursorLockAndForkMarkers)
{
    auto locked = CoreEventParser::parse(QStringLiteral("cursor locked to current screen"));
    EXPECT_EQ(locked.type, Type::CursorLockChanged);
    EXPECT_TRUE(locked.flag);
    auto unlocked = CoreEventParser::parse(QStringLiteral("cursor unlocked from current screen"));
    EXPECT_EQ(unlocked.type, Type::CursorLockChanged);
    EXPECT_FALSE(unlocked.flag);
    auto elsewhere = CoreEventParser::parse(QStringLiteral("fork: cursor locked to another screen"));
    EXPECT_EQ(elsewhere.type, Type::CursorLockChanged);
    EXPECT_TRUE(elsewhere.flag);
    EXPECT_EQ(elsewhere.detail, QStringLiteral("elsewhere"));
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("fork: screen dimmed")).type, Type::ScreenDimmed);
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("fork: screen restored")).type, Type::ScreenRestored);
}

TEST(CoreEventParser, peerFingerprint)
{
    const auto fp = CoreEventParser::parseLine(QStringLiteral(
        "[2026-10-31T09:41:07] NOTE: peer fingerprint (SHA1): a4:17:bc:02 (SHA256): 3f:9a:71:20"));
    EXPECT_EQ(fp.type, Type::PeerFingerprint);
    EXPECT_EQ(fp.sha1, QStringLiteral("A4:17:BC:02"));
    EXPECT_EQ(fp.sha256, QStringLiteral("3F:9A:71:20"));
}

TEST(CoreEventParser, fatalAndNoise)
{
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("cannot read configuration \"x.conf\": error")).type, Type::Fatal);
    EXPECT_EQ(CoreEventParser::parse(QStringLiteral("accepted client connection")).type, Type::None);
    EXPECT_EQ(CoreEventParser::parse(QString()).type, Type::None);
}

TEST(CoreEventParser, levels)
{
    EXPECT_EQ(CoreEventParser::levelIndex(QStringLiteral("ERROR")), 0);
    EXPECT_EQ(CoreEventParser::levelIndex(QStringLiteral("FATAL")), 0);
    EXPECT_EQ(CoreEventParser::levelIndex(QStringLiteral("INFO")), 3);
    EXPECT_EQ(CoreEventParser::levelIndex(QStringLiteral("DEBUG2")), 6);
    EXPECT_EQ(CoreEventParser::levelIndex(QStringLiteral("nope")), -1);
    EXPECT_EQ(CoreEventParser::levelName(3), QStringLiteral("INFO"));
}
