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

#include <QCryptographicHash>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTimer>
#include <QUuid>

namespace {

const int kMaxLineBytes = 1024 * 1024;
const int kHelloTimeoutMs = 8000;
const int kFirstRetryMs = 3000;
const int kMaxRetryMs = 60000;
const int kBroadcastDelayMs = 150;
const int kAckTimeoutMs = 10000;
const int kProtocol = 3;   // 1: snapshots; 2: access, set, action, ack; 3: relay, set-config

QJsonObject failedAck(const QString& text)
{
    QJsonObject ack;
    ack.insert(QStringLiteral("ok"), false);
    ack.insert(QStringLiteral("text"), text);
    return ack;
}

// A QTcpServer that hands the raw descriptor over, so the socket can be a
// QSslSocket from the start.
class Listener : public QTcpServer
{
public:
    explicit Listener(std::function<void(qintptr)> accept, QObject* parent) :
        QTcpServer(parent),
        m_accept(std::move(accept))
    {
    }

protected:
    void incomingConnection(qintptr descriptor) override { m_accept(descriptor); }

private:
    std::function<void(qintptr)> m_accept;
};

// The errors a self-signed certificate produces; everything else, including
// a missing peer certificate, fails the handshake.
bool tolerable(const QSslError& error)
{
    switch (error.error()) {
    case QSslError::SelfSignedCertificate:
    case QSslError::SelfSignedCertificateInChain:
    case QSslError::CertificateUntrusted:
    case QSslError::HostNameMismatch:
    case QSslError::UnableToGetLocalIssuerCertificate:
    case QSslError::UnableToVerifyFirstCertificate:
    case QSslError::UnableToGetIssuerCertificate:
        return true;
    default:
        return false;
    }
}

QByteArray privateKeyBlock(const QByteArray& pem)
{
    const int begin = pem.indexOf("-----BEGIN");
    int keyBegin = -1;
    for (int at = begin; at >= 0; at = pem.indexOf("-----BEGIN", at + 1)) {
        const int lineEnd = pem.indexOf('\n', at);
        const QByteArray header = pem.mid(at, lineEnd - at);
        if (header.contains("PRIVATE KEY")) {
            keyBegin = at;
            break;
        }
    }
    if (keyBegin < 0) {
        return QByteArray();
    }
    const int end = pem.indexOf("-----END", keyBegin);
    const int endLine = end >= 0 ? pem.indexOf('\n', end) : -1;
    return endLine >= 0 ? pem.mid(keyBegin, endLine - keyBegin + 1) : pem.mid(keyBegin);
}

} // namespace

SyncService::SyncService(QObject* parent) :
    QObject(parent)
{
    // OpenSSL is the backend that takes a certificate and key straight from
    // the core's PEM file; Secure Transport and Schannel want a keychain.
    if (QSslSocket::availableBackends().contains(QStringLiteral("openssl")) &&
        QSslSocket::activeBackend() != QLatin1String("openssl")) {
        QSslSocket::setActiveBackend(QStringLiteral("openssl"));
    }
    m_broadcastTimer = new QTimer(this);
    m_broadcastTimer->setSingleShot(true);
    m_broadcastTimer->setInterval(kBroadcastDelayMs);
    connect(m_broadcastTimer, &QTimer::timeout, this, &SyncService::broadcast);
}

SyncService::~SyncService()
{
    stop();
}

QString SyncService::fingerprintOf(const QSslCertificate& certificate)
{
    if (certificate.isNull()) {
        return QString();
    }
    const QByteArray hex = QCryptographicHash::hash(certificate.toDer(), QCryptographicHash::Sha256).toHex().toUpper();
    QString out;
    for (int i = 0; i < hex.size(); i += 2) {
        if (!out.isEmpty()) out += QLatin1Char(':');
        out += QString::fromLatin1(hex.mid(i, 2));
    }
    return out;
}

bool SyncService::setIdentity(const QString& pemPath, QString* error)
{
    m_identityReady = false;
    auto fail = [&](const QString& why) {
        if (error != nullptr) *error = why;
        m_error = why;
        return false;
    };
    if (!QSslSocket::supportsSsl()) {
        return fail(tr("TLS is not available in this build of Qt"));
    }
    QFile file(pemPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(tr("cannot read the certificate file %1").arg(pemPath));
    }
    const QByteArray pem = file.readAll();
    const QList<QSslCertificate> certificates = QSslCertificate::fromData(pem, QSsl::Pem);
    if (certificates.isEmpty()) {
        return fail(tr("no certificate in %1").arg(pemPath));
    }
    const QByteArray keyPem = privateKeyBlock(pem);
    QSslKey key(keyPem, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (key.isNull()) {
        key = QSslKey(keyPem, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
    }
    if (key.isNull()) {
        return fail(tr("no usable private key in %1").arg(pemPath));
    }
    m_tls = QSslConfiguration::defaultConfiguration();
    m_tls.setLocalCertificate(certificates.first());
    m_tls.setPrivateKey(key);
    m_tls.setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_tls.setProtocol(QSsl::TlsV1_2OrLater);
    m_identityReady = true;
    m_error.clear();
    return true;
}

void SyncService::setTrustCheck(TrustCheck check)
{
    m_trust = std::move(check);
}

void SyncService::setRequestHandler(RequestHandler handler)
{
    m_handler = std::move(handler);
}

void SyncService::setLocalInfo(const QString& name, const QString& role, const QString& version)
{
    const bool changed = m_name != name || m_role != role || m_version != version;
    m_name = name;
    m_role = role;
    m_version = version;
    if (changed) {
        for (Peer* peer : std::as_const(m_peers)) {
            if (peer->verified) {
                sendDocument(peer, QStringLiteral("hello"), hello());
            }
        }
    }
}

bool SyncService::start()
{
    stop();
    if (!m_identityReady) {
        if (m_error.isEmpty()) {
            m_error = tr("no TLS identity yet");
        }
        emit changed();
        return false;
    }
    m_listener = new Listener([this](qintptr descriptor) { acceptIncoming(descriptor); }, this);
    // the well-known port first (one window per machine normally has it), any port otherwise
    if (!m_listener->listen(QHostAddress::Any, kDefaultPort) && !m_listener->listen(QHostAddress::Any, 0)) {
        m_error = tr("cannot listen: %1").arg(m_listener->errorString());
        delete m_listener;
        m_listener = nullptr;
        emit changed();
        return false;
    }
    m_error.clear();
    emit changed();
    return true;
}

void SyncService::stop()
{
    for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
        delete it->timer;
    }
    m_targets.clear();
    const QStringList pending = m_pending.keys();
    for (const QString& id : pending) {
        finishPending(id, failedAck(tr("the channel stopped")));
    }
    while (!m_peers.isEmpty()) {
        Peer* peer = m_peers.takeLast();
        if (peer->socket != nullptr) {
            peer->socket->abort();
            peer->socket->deleteLater();
        }
        delete peer->helloTimer;
        delete peer;
    }
    if (m_listener != nullptr) {
        m_listener->close();
        delete m_listener;
        m_listener = nullptr;
    }
    emit changed();
    emit serverConfigChanged();
}

bool SyncService::listening() const
{
    return m_listener != nullptr && m_listener->isListening();
}

int SyncService::port() const
{
    return listening() ? m_listener->serverPort() : 0;
}

QString SyncService::backend() const
{
    return QSslSocket::supportsSsl() ? QSslSocket::activeBackend() : QString();
}

// ---- documents -------------------------------------------------------------

void SyncService::setDocument(const QString& kind, const QJsonObject& document)
{
    if (m_documents.value(kind) == document) {
        return;
    }
    m_documents.insert(kind, document);
    if (!m_dirty.contains(kind)) {
        m_dirty << kind;
    }
    m_broadcastTimer->start();
}

void SyncService::broadcast()
{
    const QStringList kinds = m_dirty;
    m_dirty.clear();
    for (Peer* peer : std::as_const(m_peers)) {
        if (!peer->verified) continue;
        for (const QString& kind : kinds) {
            sendDocument(peer, kind, m_documents.value(kind));
        }
    }
}

void SyncService::sendDocument(Peer* peer, const QString& kind, const QJsonObject& document)
{
    if (peer->socket == nullptr) {
        return;   // a demo peer
    }
    QJsonObject out = document;
    out.insert(QStringLiteral("type"), kind);
    peer->socket->write(QJsonDocument(out).toJson(QJsonDocument::Compact) + '\n');
}

void SyncService::sendAll(Peer* peer)
{
    for (auto it = m_documents.constBegin(); it != m_documents.constEnd(); ++it) {
        sendDocument(peer, it.key(), it.value());
    }
}

QJsonObject SyncService::hello() const
{
    QJsonObject doc;
    doc.insert(QStringLiteral("name"), m_name);
    doc.insert(QStringLiteral("role"), m_role);
    doc.insert(QStringLiteral("version"), m_version);
    doc.insert(QStringLiteral("protocol"), kProtocol);
    return doc;
}

// ---- requests --------------------------------------------------------------

QString SyncService::request(const QString& peerName, const QString& kind, const QJsonObject& body, AckHandler handler, int timeoutMs)
{
    Peer* peer = mutablePeerNamed(peerName);
    if (peer == nullptr) {
        if (handler) handler(failedAck(tr("%1 is not connected").arg(peerName)));
        return QString();
    }
    if (peer->protocol < 2) {
        if (handler) handler(failedAck(tr("%1 runs an older build that only shares, it does not accept changes").arg(peerName)));
        return QString();
    }
    if (peer->protocol < 3 && (kind == QLatin1String("relay") || kind == QLatin1String("set-config"))) {
        if (handler) handler(failedAck(tr("%1 runs an older build: update it to edit the whole fleet from here").arg(peerName)));
        return QString();
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject doc = body;
    doc.insert(QStringLiteral("id"), id);
    sendDocument(peer, kind, doc);

    Pending pending;
    pending.peerName = peerName;
    pending.handler = std::move(handler);
    pending.timer = new QTimer(this);
    pending.timer->setSingleShot(true);
    connect(pending.timer, &QTimer::timeout, this, [this, id, peerName]() {
        finishPending(id, failedAck(tr("no answer from %1").arg(peerName)));
    });
    pending.timer->start(timeoutMs > 0 ? timeoutMs : kAckTimeoutMs);
    m_pending.insert(id, pending);
    return id;
}

void SyncService::finishPending(const QString& id, const QJsonObject& ack)
{
    auto it = m_pending.find(id);
    if (it == m_pending.end()) {
        return;
    }
    Pending pending = *it;
    m_pending.erase(it);
    delete pending.timer;
    if (pending.handler) {
        pending.handler(ack);
    }
}

// ---- connections -----------------------------------------------------------

void SyncService::acceptIncoming(qintptr descriptor)
{
    auto* socket = new QSslSocket(this);
    socket->setSslConfiguration(m_tls);
    if (!socket->setSocketDescriptor(descriptor)) {
        socket->deleteLater();
        return;
    }
    attach(socket, true, nullptr);
    socket->startServerEncryption();
}

void SyncService::connectTo(const QString& name, const QString& host, quint16 port)
{
    if (host.isEmpty() || port == 0) {
        return;
    }
    const QString key = name.isEmpty() ? host + QLatin1Char(':') + QString::number(port) : name;
    Target& target = m_targets[key];
    const bool known = !target.key.isEmpty();
    if (known && target.host == host && target.port == port) {
        for (const Peer* peer : std::as_const(m_peers)) {
            if (!peer->incoming && peer->targetKey == key) {
                return;   // connected or connecting there already
            }
        }
    }
    target.key = key;
    target.name = name;
    target.host = host;
    target.port = port;
    target.retryMs = 0;
    if (target.timer == nullptr) {
        target.timer = new QTimer(this);
        target.timer->setSingleShot(true);
        connect(target.timer, &QTimer::timeout, this, [this, key]() {
            auto it = m_targets.find(key);
            if (it != m_targets.end()) {
                attempt(*it);
            }
        });
    }
    // a stale connection to the old address goes
    for (Peer* peer : std::as_const(m_peers)) {
        if (!peer->incoming && peer->targetKey == key) {
            dropPeer(peer, tr("reconnecting to %1").arg(key));
            break;
        }
    }
    attempt(target);
}

void SyncService::forget(const QString& nameOrHostPort)
{
    auto it = m_targets.find(nameOrHostPort);
    if (it != m_targets.end()) {
        delete it->timer;
        m_targets.erase(it);
    }
    for (Peer* peer : std::as_const(m_peers)) {
        if (!peer->incoming && peer->targetKey == nameOrHostPort) {
            dropPeer(peer, tr("%1 forgotten").arg(nameOrHostPort));
            break;
        }
    }
}

void SyncService::answer(const QString& peerName, const QJsonValue& id, QJsonObject ack)
{
    Peer* peer = mutablePeerNamed(peerName);
    if (peer == nullptr) {
        return;   // it went away meanwhile
    }
    ack.remove(QStringLiteral("deferred"));
    ack.insert(QStringLiteral("id"), id);
    sendDocument(peer, QStringLiteral("ack"), ack);
}

void SyncService::reevaluateAccess()
{
    if (!m_trust) {
        return;
    }
    const QList<Peer*> peers = m_peers;
    for (Peer* peer : peers) {
        if (!peer->verified) continue;
        const int access = qBound(int(None), m_trust(peer->fingerprint, peer->name, peer->incoming), int(Write));
        if (access == peer->access) continue;
        if (access <= int(None)) {
            dropPeer(peer, tr("%1 is no longer trusted: nothing shared with it").arg(peer->name));
            continue;
        }
        peer->access = access;
        QJsonObject level;
        level.insert(QStringLiteral("level"), access);
        sendDocument(peer, QStringLiteral("access"), level);
        emit message(tr("settings channel: %1 %2").arg(peer->name, access >= Write ? tr("may now change this computer's settings")
                                                                                   : tr("is now read-only")), false);
    }
    emit changed();
}

void SyncService::retryNow()
{
    for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
        bool busy = false;
        for (const Peer* peer : std::as_const(m_peers)) {
            if (!peer->incoming && peer->targetKey == it->key) busy = true;
        }
        if (!busy) {
            it->retryMs = 0;
            attempt(*it);
        }
    }
}

void SyncService::attempt(Target& target)
{
    if (!m_identityReady || !listening()) {
        return;
    }
    for (Peer* peer : std::as_const(m_peers)) {
        if (!peer->incoming && peer->targetKey == target.key) {
            return;   // an attempt is already under way
        }
    }
    target.timer->stop();
    auto* socket = new QSslSocket(this);
    socket->setSslConfiguration(m_tls);
    attach(socket, false, &target);
    socket->connectToHostEncrypted(target.host, target.port);
}

void SyncService::scheduleRetry(const QString& targetKey, bool reset)
{
    auto it = m_targets.find(targetKey);
    if (it == m_targets.end()) {
        return;
    }
    it->retryMs = reset ? kFirstRetryMs : qMin(kMaxRetryMs, it->retryMs == 0 ? kFirstRetryMs : it->retryMs * 2);
    it->timer->start(it->retryMs);
}

SyncService::Peer* SyncService::attach(QSslSocket* socket, bool incoming, const Target* target)
{
    auto* peer = new Peer;
    peer->socket = socket;
    peer->incoming = incoming;
    if (target != nullptr) {
        peer->targetKey = target->key;
        peer->targetName = target->name;
    }
    peer->since = QDateTime::currentDateTime();
    peer->helloTimer = new QTimer(this);
    peer->helloTimer->setSingleShot(true);
    connect(peer->helloTimer, &QTimer::timeout, this, [this, peer]() {
        if (m_peers.contains(peer) && !peer->verified) {
            dropPeer(peer, tr("no hello from %1").arg(peer->socket->peerAddress().toString()));
        }
    });
    peer->helloTimer->start(kHelloTimeoutMs);
    m_peers << peer;

    connect(socket, &QSslSocket::sslErrors, this, [this, peer](const QList<QSslError>& errors) {
        for (const QSslError& error : errors) {
            if (!tolerable(error)) {
                emit message(tr("settings channel: %1 refused, %2").arg(peer->socket->peerAddress().toString(), error.errorString()), true);
                return;   // the handshake fails and the socket disconnects
            }
        }
        peer->socket->ignoreSslErrors();
    });
    connect(socket, &QSslSocket::encrypted, this, [this, peer]() { onEncrypted(peer); });
    connect(socket, &QSslSocket::readyRead, this, [this, peer]() { onReadyRead(peer); });
    connect(socket, &QSslSocket::disconnected, this, [this, peer]() {
        if (m_peers.contains(peer)) {
            dropPeer(peer, peer->verified ? tr("%1 went away").arg(peer->name) : QString());
        }
    });
    connect(socket, &QSslSocket::errorOccurred, this, [this, peer](QAbstractSocket::SocketError) {
        if (m_peers.contains(peer)) {
            dropPeer(peer, peer->verified ? tr("%1: %2").arg(peer->name, peer->socket->errorString()) : QString());
        }
    });
    return peer;
}

void SyncService::onEncrypted(Peer* peer)
{
    peer->fingerprint = fingerprintOf(peer->socket->peerCertificate());
    if (peer->fingerprint.isEmpty()) {
        dropPeer(peer, tr("%1 presented no certificate").arg(peer->socket->peerAddress().toString()));
        return;
    }
    sendDocument(peer, QStringLiteral("hello"), hello());
}

void SyncService::onReadyRead(Peer* peer)
{
    peer->buffer += peer->socket->readAll();
    if (peer->buffer.size() > kMaxLineBytes) {
        dropPeer(peer, tr("%1 sent an oversized message").arg(peer->name));
        return;
    }
    int newline;
    while (m_peers.contains(peer) && (newline = peer->buffer.indexOf('\n')) >= 0) {
        const QByteArray line = peer->buffer.left(newline);
        peer->buffer.remove(0, newline + 1);
        handleLine(peer, line);
    }
}

void SyncService::handleLine(Peer* peer, const QByteArray& line)
{
    const QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject()) {
        return;
    }
    QJsonObject object = doc.object();
    const QString kind = object.take(QStringLiteral("type")).toString();
    if (kind == QLatin1String("hello") && peer->verified) {
        // a peer renamed itself (or changed role): keep the connection
        const QString name = object.value(QStringLiteral("name")).toString();
        const bool wasServer = peer->role == QLatin1String("server");
        if (!name.isEmpty()) {
            peer->name = name;
        }
        peer->role = object.value(QStringLiteral("role")).toString();
        peer->version = object.value(QStringLiteral("version")).toString();
        emit changed();
        if (wasServer || peer->role == QLatin1String("server")) {
            emit serverConfigChanged();
        }
        return;
    }
    if (kind == QLatin1String("hello")) {
        peer->name = object.value(QStringLiteral("name")).toString();
        peer->role = object.value(QStringLiteral("role")).toString();
        peer->version = object.value(QStringLiteral("version")).toString();
        if (peer->name.isEmpty() || (m_trust && m_trust(peer->fingerprint, peer->name, peer->incoming) <= int(None))) {
            dropPeer(peer, tr("%1 (%2) is not trusted: nothing shared with it")
                               .arg(peer->name.isEmpty() ? peer->socket->peerAddress().toString() : peer->name,
                                    peer->fingerprint.left(11)));
            return;
        }
        if (!peer->incoming && !peer->targetName.isEmpty() && peer->name != peer->targetName) {
            dropPeer(peer, tr("%1 answered as %2").arg(peer->targetName, peer->name));
            return;
        }
        peer->access = m_trust ? qBound(int(None), m_trust(peer->fingerprint, peer->name, peer->incoming), int(Write)) : int(Read);
        peer->protocol = object.value(QStringLiteral("protocol")).toInt(1);
        // one connection per peer: a newer one replaces the old
        for (Peer* other : std::as_const(m_peers)) {
            if (other != peer && other->verified && other->name == peer->name) {
                dropPeer(other, QString());
                break;
            }
        }
        peer->verified = true;
        peer->helloTimer->stop();
        emit message(tr("settings channel: %1 (%2) connected, %3").arg(peer->name, peer->role,
                        peer->access >= Write ? tr("may change this computer's settings") : tr("read-only")), false);
        QJsonObject access;
        access.insert(QStringLiteral("level"), peer->access);
        sendDocument(peer, QStringLiteral("access"), access);
        sendAll(peer);
        emit changed();
        if (peer->role == QLatin1String("server")) {
            emit serverConfigChanged();
        }
        return;
    }
    if (!peer->verified || kind.isEmpty()) {
        return;
    }
    if (kind == QLatin1String("access")) {
        peer->accessThere = qBound(int(None), object.value(QStringLiteral("level")).toInt(), int(Write));
        emit changed();
        return;
    }
    if (kind == QLatin1String("ack")) {
        finishPending(object.value(QStringLiteral("id")).toString(), object);
        return;
    }
    if (kind == QLatin1String("set") || kind == QLatin1String("action") || kind == QLatin1String("relay")
        || kind == QLatin1String("set-config")) {
        QJsonObject ack = m_handler ? m_handler(peer->name, kind, object, peer->access)
                                    : failedAck(tr("%1 does not take changes").arg(m_name));
        if (ack.value(QStringLiteral("deferred")).toBool()) {
            return;   // the handler answers later, through answer()
        }
        ack.insert(QStringLiteral("id"), object.value(QStringLiteral("id")));
        sendDocument(peer, QStringLiteral("ack"), ack);
        return;
    }
    peer->documents.insert(kind, object);
    emit peerDocumentChanged(peer->name, kind);
    emit changed();
    if (peer->role == QLatin1String("server")) {
        emit serverConfigChanged();
    }
}

void SyncService::dropPeer(Peer* peer, const QString& why)
{
    if (!m_peers.removeOne(peer)) {
        return;
    }
    const bool wasServer = peer->verified && peer->role == QLatin1String("server");
    const QString targetKey = peer->targetKey;
    const bool incoming = peer->incoming;
    if (peer->verified) {
        const QStringList ids = m_pending.keys();
        for (const QString& id : ids) {
            if (m_pending.value(id).peerName == peer->name) {
                finishPending(id, failedAck(tr("%1 went away before answering").arg(peer->name)));
            }
        }
    }
    if (!why.isEmpty()) {
        emit message(tr("settings channel: %1").arg(why), false);
    }
    if (peer->socket != nullptr) {
        peer->socket->abort();
        peer->socket->deleteLater();
    }
    delete peer->helloTimer;
    delete peer;
    if (!incoming) {
        scheduleRetry(targetKey, false);
    }
    emit changed();
    if (wasServer) {
        emit serverConfigChanged();
    }
}

// ---- what the window sees --------------------------------------------------

const SyncService::Peer* SyncService::serverPeer() const
{
    for (const Peer* peer : m_peers) {
        if (peer->verified && peer->role == QLatin1String("server")) {
            return peer;
        }
    }
    return nullptr;
}

const SyncService::Peer* SyncService::peerNamed(const QString& name) const
{
    for (const Peer* peer : m_peers) {
        if (peer->verified && peer->name.compare(name, Qt::CaseInsensitive) == 0) {
            return peer;   // screen names are compared without case, as the core does
        }
    }
    return nullptr;
}

SyncService::Peer* SyncService::mutablePeerNamed(const QString& name)
{
    for (Peer* peer : std::as_const(m_peers)) {
        if (peer->verified && peer->name.compare(name, Qt::CaseInsensitive) == 0) {
            return peer;
        }
    }
    return nullptr;
}

int SyncService::accessAt(const QString& name) const
{
    const Peer* peer = peerNamed(name);
    return peer != nullptr ? peer->accessThere : int(None);
}

int SyncService::connectedCount() const
{
    int count = 0;
    for (const Peer* peer : m_peers) {
        if (peer->verified) ++count;
    }
    return count;
}

QString SyncService::serverName() const
{
    const Peer* server = serverPeer();
    return server != nullptr ? server->name : QString();
}

QVariantMap SyncService::serverSettings() const
{
    return serverDocument(QStringLiteral("server-config")).value(QStringLiteral("settings")).toObject().toVariantMap();
}

QVariantMap SyncService::serverState() const
{
    return serverDocument(QStringLiteral("state")).toVariantMap();
}

QJsonObject SyncService::serverDocument(const QString& kind) const
{
    const Peer* server = serverPeer();
    return server != nullptr ? server->documents.value(kind) : QJsonObject();
}

QJsonObject SyncService::peerDocument(const QString& name, const QString& kind) const
{
    const Peer* peer = peerNamed(name);
    return peer != nullptr ? peer->documents.value(kind) : QJsonObject();
}

QVariantMap SyncService::describe(const Peer* peer) const
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), peer->name);
    map.insert(QStringLiteral("role"), peer->role);
    map.insert(QStringLiteral("version"), peer->version);
    map.insert(QStringLiteral("fingerprint"), peer->fingerprint);
    map.insert(QStringLiteral("host"), peer->socket != nullptr ? peer->socket->peerAddress().toString() : QStringLiteral("demo"));
    map.insert(QStringLiteral("incoming"), peer->incoming);
    map.insert(QStringLiteral("connected"), peer->verified);
    map.insert(QStringLiteral("access"), peer->access);
    map.insert(QStringLiteral("accessThere"), peer->accessThere);
    map.insert(QStringLiteral("protocol"), peer->protocol);
    map.insert(QStringLiteral("since"), peer->since.toString(QStringLiteral("HH:mm")));
    map.insert(QStringLiteral("settings"), peer->documents.value(QStringLiteral("settings")).toVariantMap());
    const QJsonObject state = peer->documents.value(QStringLiteral("state"));
    map.insert(QStringLiteral("state"), state.toVariantMap());
    map.insert(QStringLiteral("processes"), state.value(QStringLiteral("processes")).toArray().toVariantList());
    return map;
}

void SyncService::addDemoPeer(const QString& name, const QString& role, const QJsonObject& settings, const QJsonObject& state, int accessThere,
                              const QJsonObject& serverConfig)
{
    auto* peer = new Peer;
    peer->incoming = role != QLatin1String("server");
    peer->verified = true;
    peer->access = Write;
    peer->accessThere = accessThere;
    peer->protocol = kProtocol;
    peer->name = name;
    peer->role = role;
    peer->version = QStringLiteral("demo");
    peer->fingerprint = QStringLiteral("DE:MO");
    peer->since = QDateTime::currentDateTime();
    peer->documents.insert(QStringLiteral("settings"), settings);
    peer->documents.insert(QStringLiteral("state"), state);
    if (!serverConfig.isEmpty()) {
        peer->documents.insert(QStringLiteral("server-config"), serverConfig);
    }
    m_peers << peer;
    emit changed();
    if (role == QLatin1String("server")) {
        emit serverConfigChanged();
    }
}

QVariantList SyncService::peers() const
{
    QVariantList list;
    for (const Peer* peer : m_peers) {
        if (peer->verified) {
            list << describe(peer);
        }
    }
    return list;
}

QVariantMap SyncService::peer(const QString& name) const
{
    const Peer* found = peerNamed(name);
    return found != nullptr ? describe(found) : QVariantMap();
}
