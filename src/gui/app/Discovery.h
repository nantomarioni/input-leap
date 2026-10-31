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

#include <QHash>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class ZeroconfBrowser;
class ZeroconfRecord;
class ZeroconfRegister;
class ZeroconfResolver;

/* Finds the other Input Leap computers on the LAN with mDNS (Bonjour on
 * macOS and Windows, Avahi's compatibility layer on Linux): a server
 * advertises itself and looks for clients, a client advertises itself and
 * looks for servers. The record types and names are the legacy GUI's, so a
 * new window finds old ones and the other way round. Each record found is
 * resolved to a host, a port and its TXT data: the port is the GUI's
 * settings channel (docs/ui-redesign.md §4.7), the TXT says `sync=1` and
 * carries the core's port.
 */
class Discovery : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.discovery")

    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QStringList servers READ servers NOTIFY changed)
    Q_PROPERTY(QStringList clients READ clients NOTIFY changed)
    Q_PROPERTY(QVariantList peers READ peers NOTIFY changed)
    Q_PROPERTY(QString advertising READ advertising NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString unavailableReason READ unavailableReason CONSTANT)

public:
    struct Peer
    {
        QString name;
        QString host;          // "desk-win.local"
        quint16 port = 0;      // the GUI's settings channel
        QMap<QString, QString> txt;
        bool sync() const { return txt.value(QStringLiteral("sync")) == QLatin1String("1"); }
        int corePort() const { return txt.value(QStringLiteral("port")).toInt(); }
    };

    explicit Discovery(QObject* parent = nullptr);
    ~Discovery() override;

    bool available() const { return m_available; }
    QString unavailableReason() const { return m_unavailableReason; }
    bool active() const { return m_active; }
    QStringList servers() const { return m_servers; }
    QStringList clients() const { return m_clients; }
    QVariantList peers() const;
    QString advertising() const { return m_advertising; }
    QString error() const { return m_error; }

    // Resolved details of a peer found so far; a null name when unknown.
    Q_INVOKABLE QVariantMap peer(const QString& name) const;
    bool resolvedPeer(const QString& name, Peer& out) const;

    // advertisedPort/txt: what this computer publishes in its record.
    void start(bool asServer, const QString& screenName, quint16 advertisedPort = 0,
               const QMap<QString, QString>& txt = QMap<QString, QString>());
    void stop();
    Q_INVOKABLE void restart();

signals:
    void serverFound(const QString& name);
    void clientFound(const QString& name);
    void peerResolved(const QString& name, const QString& host, quint16 port, const QMap<QString, QString>& txt);
    void peerLost(const QString& name);
    void changed();

private:
    void browse(const QString& type);
    void resolveRecord(const ZeroconfRecord& record);

    bool m_available = true;
    QString m_unavailableReason;
    bool m_active = false;
    bool m_asServer = false;
    QString m_screenName;
    quint16 m_advertisedPort = 0;
    QMap<QString, QString> m_txt;
    QString m_advertising;
    QString m_error;
    QStringList m_servers;
    QStringList m_clients;
    QHash<QString, Peer> m_peers;
    QHash<QString, std::shared_ptr<ZeroconfResolver>> m_resolvers;

    std::unique_ptr<ZeroconfRegister> m_register;
    std::unique_ptr<ZeroconfBrowser> m_browser;
};
