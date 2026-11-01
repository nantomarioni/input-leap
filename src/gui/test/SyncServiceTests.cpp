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

#include "SyncService.h"

#include "net/SecureUtils.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QSslCertificate>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <gtest/gtest.h>

#include <cstdio>
#include <functional>

namespace {

QString makeCertificate(const QTemporaryDir& dir, const QString& name)
{
    const QString path = dir.filePath(name + QStringLiteral(".pem"));
    inputleap::generate_pem_self_signed_cert(path.toStdString());
    return path;
}

QString fingerprintOfFile(const QString& pem)
{
    const QList<QSslCertificate> certs = QSslCertificate::fromPath(pem, QSsl::Pem);
    return certs.isEmpty() ? QString() : SyncService::fingerprintOf(certs.first());
}

bool waitFor(const std::function<bool()>& condition, int timeoutMs = 8000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        QTest::qWait(20);
    }
    return true;
}

} // namespace

TEST(SyncService, exchangesDocumentsBetweenTrustedPeers)
{
    if (!QSslSocket::supportsSsl()) {
        GTEST_SKIP() << "no TLS backend in this Qt";
    }
    QTemporaryDir dir;
    const QString serverPem = makeCertificate(dir, QStringLiteral("server"));
    const QString clientPem = makeCertificate(dir, QStringLiteral("client"));
    const QString serverFp = fingerprintOfFile(serverPem);
    const QString clientFp = fingerprintOfFile(clientPem);
    ASSERT_FALSE(serverFp.isEmpty());
    ASSERT_NE(serverFp, clientFp);

    QString error;
    SyncService server;
    std::fprintf(stderr, "[ tls      ] backend %s, %s\n", qPrintable(QSslSocket::activeBackend()), qPrintable(QSslSocket::sslLibraryVersionString()));
    ASSERT_TRUE(server.setIdentity(serverPem, &error)) << error.toStdString();
    server.setLocalInfo(QStringLiteral("desk-win"), QStringLiteral("server"), QStringLiteral("test"));
    server.setTrustCheck([&](const QString& sha, const QString& name, bool incoming) {
        return incoming && sha == clientFp && (name == QLatin1String("macbook") || name == QLatin1String("mini"))
               ? SyncService::Read : SyncService::None;
    });
    QJsonObject lastRequest;
    server.setRequestHandler([&](const QString& peer, const QString& kind, const QJsonObject& body, int access) {
        lastRequest = body;
        lastRequest.insert(QStringLiteral("peer"), peer);
        lastRequest.insert(QStringLiteral("kind"), kind);
        QJsonObject ack;
        ack.insert(QStringLiteral("ok"), access >= SyncService::Write);
        ack.insert(QStringLiteral("text"), access >= SyncService::Write ? QStringLiteral("applied") : QStringLiteral("read-only here"));
        return ack;
    });
    ASSERT_TRUE(server.start()) << server.error().toStdString();
    ASSERT_GT(server.port(), 0);
    QJsonObject config;
    config.insert(QStringLiteral("conf"), QStringLiteral("section: screens\nend\n"));
    server.setDocument(QStringLiteral("server-config"), config);

    SyncService client;
    ASSERT_TRUE(client.setIdentity(clientPem, &error)) << error.toStdString();
    client.setLocalInfo(QStringLiteral("macbook"), QStringLiteral("client"), QStringLiteral("test"));
    client.setTrustCheck([&](const QString& sha, const QString&, bool incoming) {
        return !incoming && sha == serverFp ? SyncService::Write : SyncService::None;
    });
    client.setRequestHandler([&](const QString&, const QString& kind, const QJsonObject& body, int access) {
        QJsonObject ack;
        ack.insert(QStringLiteral("ok"), access >= SyncService::Write && kind == QLatin1String("set"));
        ack.insert(QStringLiteral("text"), QStringLiteral("on macbook, after its client restarts"));
        ack.insert(QStringLiteral("echo"), body.value(QStringLiteral("value")));
        return ack;
    });
    ASSERT_TRUE(client.start()) << client.error().toStdString();
    QJsonObject settings;
    settings.insert(QStringLiteral("scrollStep"), 120);
    client.setDocument(QStringLiteral("settings"), settings);
    client.connectTo(QStringLiteral("desk-win"), QStringLiteral("127.0.0.1"), quint16(server.port()));

    ASSERT_TRUE(waitFor([&]() { return client.serverConnected() && !client.serverDocument(QStringLiteral("server-config")).isEmpty(); }));
    EXPECT_EQ(client.serverName(), QStringLiteral("desk-win"));
    EXPECT_EQ(client.serverDocument(QStringLiteral("server-config")).value(QStringLiteral("conf")).toString(),
              QStringLiteral("section: screens\nend\n"));
    ASSERT_TRUE(waitFor([&]() { return server.peerDocument(QStringLiteral("macbook"), QStringLiteral("settings")).value(QStringLiteral("scrollStep")).toInt() == 120; }));
    EXPECT_EQ(server.connectedCount(), 1);
    EXPECT_EQ(server.peer(QStringLiteral("macbook")).value(QStringLiteral("role")).toString(), QStringLiteral("client"));

    // a later change travels too
    settings.insert(QStringLiteral("scrollStep"), 240);
    client.setDocument(QStringLiteral("settings"), settings);
    ASSERT_TRUE(waitFor([&]() { return server.peerDocument(QStringLiteral("macbook"), QStringLiteral("settings")).value(QStringLiteral("scrollStep")).toInt() == 240; }));

    // each side learns what it may do on the other
    ASSERT_TRUE(waitFor([&]() { return server.accessAt(QStringLiteral("macbook")) == SyncService::Write && client.accessAt(QStringLiteral("desk-win")) == SyncService::Read; }));

    // the server may change the client's settings: the ack says where it lands
    QJsonObject body;
    body.insert(QStringLiteral("key"), QStringLiteral("logLevel"));
    body.insert(QStringLiteral("value"), 4);
    QJsonObject gotAck;
    server.request(QStringLiteral("macbook"), QStringLiteral("set"), body, [&](const QJsonObject& ack) { gotAck = ack; });
    ASSERT_TRUE(waitFor([&]() { return !gotAck.isEmpty(); }));
    EXPECT_TRUE(gotAck.value(QStringLiteral("ok")).toBool());
    EXPECT_EQ(gotAck.value(QStringLiteral("text")).toString(), QStringLiteral("on macbook, after its client restarts"));
    EXPECT_EQ(gotAck.value(QStringLiteral("echo")).toInt(), 4);

    // the client is read-only on the server: refused, not applied
    gotAck = QJsonObject();
    client.request(QStringLiteral("desk-win"), QStringLiteral("set"), body, [&](const QJsonObject& ack) { gotAck = ack; });
    ASSERT_TRUE(waitFor([&]() { return !gotAck.isEmpty(); }));
    EXPECT_FALSE(gotAck.value(QStringLiteral("ok")).toBool());
    EXPECT_EQ(lastRequest.value(QStringLiteral("peer")).toString(), QStringLiteral("macbook"));
    EXPECT_EQ(lastRequest.value(QStringLiteral("key")).toString(), QStringLiteral("logLevel"));

    // a typed-in address, no name known: whatever server answers is taken
    SyncService mini;
    ASSERT_TRUE(mini.setIdentity(clientPem, &error)) << error.toStdString();
    mini.setLocalInfo(QStringLiteral("mini"), QStringLiteral("client"), QStringLiteral("test"));
    mini.setTrustCheck([&](const QString& sha, const QString&, bool incoming) {
        return !incoming && sha == serverFp ? SyncService::Write : SyncService::None;
    });
    ASSERT_TRUE(mini.start());
    mini.connectTo(QString(), QStringLiteral("127.0.0.1"), quint16(server.port()));
    ASSERT_TRUE(waitFor([&]() { return mini.serverConnected(); }));
    EXPECT_EQ(mini.serverName(), QStringLiteral("desk-win"));
    mini.forget(QStringLiteral("127.0.0.1:") + QString::number(server.port()));
    ASSERT_TRUE(waitFor([&]() { return !mini.serverConnected(); }));

    // a request to nobody fails at once
    gotAck = QJsonObject();
    server.request(QStringLiteral("nobody"), QStringLiteral("action"), QJsonObject(), [&](const QJsonObject& ack) { gotAck = ack; });
    EXPECT_FALSE(gotAck.isEmpty());
    EXPECT_FALSE(gotAck.value(QStringLiteral("ok")).toBool());

    // the server going away empties the mirror
    server.stop();
    ASSERT_TRUE(waitFor([&]() { return !client.serverConnected(); }));
    EXPECT_TRUE(client.serverDocument(QStringLiteral("server-config")).isEmpty());
}

TEST(SyncService, dropsPeersItDoesNotTrust)
{
    if (!QSslSocket::supportsSsl()) {
        GTEST_SKIP() << "no TLS backend in this Qt";
    }
    QTemporaryDir dir;
    const QString serverPem = makeCertificate(dir, QStringLiteral("server"));
    const QString roguePem = makeCertificate(dir, QStringLiteral("rogue"));
    const QString serverFp = fingerprintOfFile(serverPem);

    SyncService server;
    ASSERT_TRUE(server.setIdentity(serverPem));
    server.setLocalInfo(QStringLiteral("desk-win"), QStringLiteral("server"), QStringLiteral("test"));
    server.setTrustCheck([](const QString&, const QString&, bool) { return SyncService::None; });
    ASSERT_TRUE(server.start());
    QJsonObject secret;
    secret.insert(QStringLiteral("conf"), QStringLiteral("private"));
    server.setDocument(QStringLiteral("server-config"), secret);

    SyncService rogue;
    ASSERT_TRUE(rogue.setIdentity(roguePem));
    rogue.setLocalInfo(QStringLiteral("intruder"), QStringLiteral("client"), QStringLiteral("test"));
    rogue.setTrustCheck([&](const QString& sha, const QString&, bool) { return sha == serverFp ? SyncService::Write : SyncService::None; });
    ASSERT_TRUE(rogue.start());
    rogue.connectTo(QStringLiteral("desk-win"), QStringLiteral("127.0.0.1"), quint16(server.port()));

    QTest::qWait(1500);
    EXPECT_EQ(server.connectedCount(), 0);
    EXPECT_FALSE(rogue.serverConnected());
    EXPECT_TRUE(rogue.serverDocument(QStringLiteral("server-config")).isEmpty());
}

// The server relays a client's request to another client (a deferred ack),
// and a change of trust reaches a connected peer without reconnecting.
TEST(SyncService, relaysRequestsBetweenClientsThroughTheServer)
{
    if (!QSslSocket::supportsSsl()) {
        GTEST_SKIP() << "no TLS backend in this Qt";
    }
    QTemporaryDir dir;
    const QString serverPem = makeCertificate(dir, QStringLiteral("server"));
    const QString clientPem = makeCertificate(dir, QStringLiteral("client"));
    const QString serverFp = fingerprintOfFile(serverPem);
    const QString clientFp = fingerprintOfFile(clientPem);

    QString error;
    SyncService server;
    ASSERT_TRUE(server.setIdentity(serverPem, &error)) << error.toStdString();
    server.setLocalInfo(QStringLiteral("desk-win"), QStringLiteral("server"), QStringLiteral("test"));
    int clientAccess = SyncService::Read;
    server.setTrustCheck([&](const QString& sha, const QString&, bool incoming) {
        return incoming && sha == clientFp ? clientAccess : int(SyncService::None);
    });
    server.setRequestHandler([&](const QString& peer, const QString& kind, const QJsonObject& body, int access) {
        if (kind != QLatin1String("relay") || access < SyncService::Write) {
            QJsonObject ack;
            ack.insert(QStringLiteral("ok"), false);
            ack.insert(QStringLiteral("text"), QStringLiteral("refused"));
            return ack;
        }
        const QJsonValue id = body.value(QStringLiteral("id"));
        QJsonObject inner = body.value(QStringLiteral("body")).toObject();
        inner.insert(QStringLiteral("by"), peer);
        server.request(body.value(QStringLiteral("target")).toString(), body.value(QStringLiteral("kind")).toString(), inner,
                       [&server, peer, id](const QJsonObject& ack) { server.answer(peer, id, ack); }, 3000);
        QJsonObject deferred;
        deferred.insert(QStringLiteral("deferred"), true);
        return deferred;
    });
    ASSERT_TRUE(server.start()) << server.error().toStdString();

    auto makeClient = [&](SyncService& client, const QString& name) {
        ASSERT_TRUE(client.setIdentity(clientPem, &error)) << error.toStdString();
        client.setLocalInfo(name, QStringLiteral("client"), QStringLiteral("test"));
        client.setTrustCheck([&](const QString& sha, const QString&, bool incoming) {
            return !incoming && sha == serverFp ? SyncService::Write : SyncService::None;
        });
        client.setRequestHandler([name](const QString&, const QString& kind, const QJsonObject& body, int access) {
            QJsonObject ack;
            ack.insert(QStringLiteral("ok"), access >= SyncService::Write && kind == QLatin1String("set"));
            ack.insert(QStringLiteral("text"), QStringLiteral("applied on %1 for %2").arg(name, body.value(QStringLiteral("by")).toString()));
            return ack;
        });
        ASSERT_TRUE(client.start()) << client.error().toStdString();
        client.connectTo(QStringLiteral("desk-win"), QStringLiteral("127.0.0.1"), quint16(server.port()));
    };
    SyncService macbook, mini;
    makeClient(macbook, QStringLiteral("macbook"));
    makeClient(mini, QStringLiteral("mini"));
    ASSERT_TRUE(waitFor([&]() { return macbook.serverConnected() && mini.serverConnected() && server.connectedCount() == 2; }));
    ASSERT_TRUE(waitFor([&]() { return macbook.accessAt(QStringLiteral("desk-win")) == SyncService::Read; }));

    QJsonObject set;
    set.insert(QStringLiteral("key"), QStringLiteral("logLevel"));
    set.insert(QStringLiteral("value"), 4);
    QJsonObject relay;
    relay.insert(QStringLiteral("target"), QStringLiteral("mini"));
    relay.insert(QStringLiteral("kind"), QStringLiteral("set"));
    relay.insert(QStringLiteral("body"), set);

    // read-only on the server: nothing relayed
    QJsonObject gotAck;
    macbook.request(QStringLiteral("desk-win"), QStringLiteral("relay"), relay, [&](const QJsonObject& ack) { gotAck = ack; });
    ASSERT_TRUE(waitFor([&]() { return !gotAck.isEmpty(); }));
    EXPECT_FALSE(gotAck.value(QStringLiteral("ok")).toBool());

    // the server now trusts it: told at once, and the relay goes through
    clientAccess = SyncService::Write;
    server.reevaluateAccess();
    ASSERT_TRUE(waitFor([&]() { return macbook.accessAt(QStringLiteral("desk-win")) == SyncService::Write; }));
    gotAck = QJsonObject();
    macbook.request(QStringLiteral("desk-win"), QStringLiteral("relay"), relay, [&](const QJsonObject& ack) { gotAck = ack; });
    ASSERT_TRUE(waitFor([&]() { return !gotAck.isEmpty(); }));
    EXPECT_TRUE(gotAck.value(QStringLiteral("ok")).toBool());
    EXPECT_EQ(gotAck.value(QStringLiteral("text")).toString(), QStringLiteral("applied on mini for macbook"));

    // a target that is not there: the server's own failure comes back
    relay.insert(QStringLiteral("target"), QStringLiteral("nobody"));
    gotAck = QJsonObject();
    macbook.request(QStringLiteral("desk-win"), QStringLiteral("relay"), relay, [&](const QJsonObject& ack) { gotAck = ack; });
    ASSERT_TRUE(waitFor([&]() { return !gotAck.isEmpty(); }));
    EXPECT_FALSE(gotAck.value(QStringLiteral("ok")).toBool());

    // trust withdrawn: dropped
    clientAccess = SyncService::None;
    server.reevaluateAccess();
    ASSERT_TRUE(waitFor([&]() { return server.connectedCount() == 0; }));
}
