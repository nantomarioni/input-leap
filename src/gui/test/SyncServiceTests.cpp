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
    ASSERT_TRUE(server.setIdentity(serverPem, &error)) << error.toStdString();
    server.setLocalInfo(QStringLiteral("desk-win"), QStringLiteral("server"), QStringLiteral("test"));
    server.setTrustCheck([&](const QString& sha, const QString& name, bool incoming) {
        return incoming && sha == clientFp && name == QLatin1String("macbook");
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
        return !incoming && sha == serverFp;
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
    server.setTrustCheck([](const QString&, const QString&, bool) { return false; });
    ASSERT_TRUE(server.start());
    QJsonObject secret;
    secret.insert(QStringLiteral("conf"), QStringLiteral("private"));
    server.setDocument(QStringLiteral("server-config"), secret);

    SyncService rogue;
    ASSERT_TRUE(rogue.setIdentity(roguePem));
    rogue.setLocalInfo(QStringLiteral("intruder"), QStringLiteral("client"), QStringLiteral("test"));
    rogue.setTrustCheck([&](const QString& sha, const QString&, bool) { return sha == serverFp; });
    ASSERT_TRUE(rogue.start());
    rogue.connectTo(QStringLiteral("desk-win"), QStringLiteral("127.0.0.1"), quint16(server.port()));

    QTest::qWait(1500);
    EXPECT_EQ(server.connectedCount(), 0);
    EXPECT_FALSE(rogue.serverConnected());
    EXPECT_TRUE(rogue.serverDocument(QStringLiteral("server-config")).isEmpty());
}
