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

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSslConfiguration>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QSslCertificate;
class QSslSocket;
class QTcpServer;
class QTimer;

/* The GUI-to-GUI settings channel (docs/ui-redesign.md §4.7, phases C and
 * D): every app listens on a TLS socket with the certificate its core
 * already has and talks only to peers the trust check accepts. Clients
 * connect to the server's GUI (the port travels in the zeroconf record);
 * the server accepts. Messages are one JSON document per line:
 *
 * - snapshots, kept per peer, resent whenever they change: "hello",
 *   "settings", "state", "server-config";
 * - "access": what the sender lets this peer do (Read or Write);
 * - requests, answered with an "ack" carrying the same id: "set" (one
 *   setting of the other computer) and "action" (restart its core, …).
 */
class SyncService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.sync")

    Q_PROPERTY(bool listening READ listening NOTIFY changed)
    Q_PROPERTY(int port READ port NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString backend READ backend CONSTANT)
    Q_PROPERTY(QVariantList peers READ peers NOTIFY changed)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY changed)
    Q_PROPERTY(bool serverConnected READ serverConnected NOTIFY changed)
    Q_PROPERTY(QString serverName READ serverName NOTIFY changed)
    Q_PROPERTY(QVariantMap serverSettings READ serverSettings NOTIFY serverConfigChanged)
    Q_PROPERTY(QVariantMap serverState READ serverState NOTIFY serverConfigChanged)

public:
    // What a peer may do here: nothing (dropped), read the snapshots, or
    // also change this computer's own settings.
    enum Access { None = 0, Read = 1, Write = 2 };
    Q_ENUM(Access)

    // sha256: "AB:CD:…" of the peer's certificate; name: what the peer calls
    // itself in its hello; incoming: it connected to us.
    using TrustCheck = std::function<int(const QString& sha256, const QString& name, bool incoming)>;
    // Answers a request from a verified peer; returns the ack body
    // ({"ok": bool, "text": "…", …}). access: that peer's level here.
    using RequestHandler = std::function<QJsonObject(const QString& peer, const QString& kind, const QJsonObject& body, int access)>;
    using AckHandler = std::function<void(const QJsonObject& ack)>;

    explicit SyncService(QObject* parent = nullptr);
    ~SyncService() override;

    static QString fingerprintOf(const QSslCertificate& certificate);

    // The core's PEM file: certificate and private key.
    bool setIdentity(const QString& pemPath, QString* error = nullptr);
    void setTrustCheck(TrustCheck check);
    void setRequestHandler(RequestHandler handler);
    void setLocalInfo(const QString& name, const QString& role, const QString& version);

    bool start();      // listens on a free port; false (and error()) when it cannot
    void stop();
    bool listening() const;
    int port() const;
    QString error() const { return m_error; }
    QString backend() const;

    // What this computer publishes; sent to every trusted peer, resent on change.
    void setDocument(const QString& kind, const QJsonObject& document);
    QJsonObject document(const QString& kind) const { return m_documents.value(kind); }

    // A request to one peer ("set", "action"); the handler gets the ack, or
    // a failed one when the peer is away or silent. Returns the request id.
    QString request(const QString& peerName, const QString& kind, const QJsonObject& body, AckHandler handler);

    // Client role: reach the server's GUI; retried until forget() or stop().
    void connectTo(const QString& name, const QString& host, quint16 port);
    void forget(const QString& name);
    Q_INVOKABLE void retryNow();   // the trust database changed: try the pending peers again

    QVariantList peers() const;
    int connectedCount() const;
    bool serverConnected() const { return serverPeer() != nullptr; }
    QString serverName() const;
    QVariantMap serverSettings() const;
    QVariantMap serverState() const;
    QJsonObject serverDocument(const QString& kind) const;
    QJsonObject peerDocument(const QString& name, const QString& kind) const;
    Q_INVOKABLE QVariantMap peer(const QString& name) const;
    Q_INVOKABLE int accessAt(const QString& name) const;   // what we may do on that peer

    // Development only (INPUTLEAP_UI_DEMO_PEER): a peer that never answers.
    void addDemoPeer(const QString& name, const QString& role, const QJsonObject& settings, const QJsonObject& state, int accessThere,
                     const QJsonObject& serverConfig = QJsonObject());

signals:
    void changed();
    void serverConfigChanged();
    void peerDocumentChanged(const QString& name, const QString& kind);
    void message(const QString& text, bool isError);

private:
    struct Peer
    {
        QSslSocket* socket = nullptr;
        bool incoming = true;
        bool verified = false;
        int access = None;         // what it may do here
        int accessThere = None;    // what we may do there (its "access" message)
        int protocol = 1;
        QString targetName;        // outgoing: the name we were told
        QString name;              // from its hello
        QString role;
        QString version;
        QString fingerprint;
        QByteArray buffer;
        QHash<QString, QJsonObject> documents;
        QDateTime since;
        QTimer* helloTimer = nullptr;
    };
    struct Target
    {
        QString name;
        QString host;
        quint16 port = 0;
        int retryMs = 0;
        QTimer* timer = nullptr;
    };
    struct Pending
    {
        QString peerName;
        AckHandler handler;
        QTimer* timer = nullptr;
    };

    void acceptIncoming(qintptr descriptor);
    void attempt(Target& target);
    Peer* attach(QSslSocket* socket, bool incoming, const QString& targetName);
    void onEncrypted(Peer* peer);
    void onReadyRead(Peer* peer);
    void handleLine(Peer* peer, const QByteArray& line);
    void dropPeer(Peer* peer, const QString& why);
    void scheduleRetry(const QString& targetName, bool reset);
    void sendDocument(Peer* peer, const QString& kind, const QJsonObject& document);
    void sendAll(Peer* peer);
    void broadcast();
    void finishPending(const QString& id, const QJsonObject& ack);
    const Peer* serverPeer() const;
    const Peer* peerNamed(const QString& name) const;
    Peer* mutablePeerNamed(const QString& name);
    QJsonObject hello() const;
    QVariantMap describe(const Peer* peer) const;

    QTcpServer* m_listener = nullptr;
    QSslConfiguration m_tls;
    bool m_identityReady = false;
    TrustCheck m_trust;
    RequestHandler m_handler;
    QString m_name;
    QString m_role;
    QString m_version;
    QString m_error;
    QHash<QString, QJsonObject> m_documents;
    QStringList m_dirty;
    QTimer* m_broadcastTimer = nullptr;
    QList<Peer*> m_peers;
    QHash<QString, Target> m_targets;
    QHash<QString, Pending> m_pending;
};
