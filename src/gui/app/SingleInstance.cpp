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

#include "SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace {
const int kConnectTimeoutMs = 500;
} // namespace

SingleInstance::SingleInstance(const QString& key, QObject* parent) :
    QObject(parent),
    m_key(key)
{
    // Somebody listening already? Then this process is a secondary.
    QLocalSocket probe;
    probe.connectToServer(m_key);
    if (probe.waitForConnected(kConnectTimeoutMs)) {
        probe.disconnectFromServer();
        return;
    }
    // A socket file left behind by a crash (Unix) would block listen().
    QLocalServer::removeServer(m_key);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_key)) {
        m_error = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket* client = m_server->nextPendingConnection()) {
            connect(client, &QLocalSocket::readyRead, this, [this, client]() {
                const QByteArray data = client->readAll();
                for (const QByteArray& line : data.split('\n')) {
                    if (!line.trimmed().isEmpty()) {
                        emit messageReceived(line.trimmed());
                    }
                }
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });
}

SingleInstance::~SingleInstance()
{
    if (m_server != nullptr) {
        m_server->close();
    }
}

bool SingleInstance::notifyPrimary(const QByteArray& message)
{
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        return false;
    }
    socket.write(message + '\n');
    const bool written = socket.waitForBytesWritten(kConnectTimeoutMs);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState) {
        socket.waitForDisconnected(kConnectTimeoutMs);
    }
    return written;
}
