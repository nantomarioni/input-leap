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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class ZeroconfBrowser;
class ZeroconfRegister;
class ZeroconfServer;

/* Finds the other Input Leap computers on the LAN with mDNS (Bonjour on
 * macOS and Windows, Avahi's compatibility layer on Linux): a server
 * advertises itself and looks for clients, a client advertises itself and
 * looks for servers. The record types and names are the legacy GUI's, so a
 * new window finds old ones and the other way round.
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
    Q_PROPERTY(QString advertising READ advertising NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString unavailableReason READ unavailableReason CONSTANT)

public:
    explicit Discovery(QObject* parent = nullptr);
    ~Discovery() override;

    bool available() const { return m_available; }
    QString unavailableReason() const { return m_unavailableReason; }
    bool active() const { return m_active; }
    QStringList servers() const { return m_servers; }
    QStringList clients() const { return m_clients; }
    QString advertising() const { return m_advertising; }
    QString error() const { return m_error; }

    void start(bool asServer, const QString& screenName);
    void stop();
    Q_INVOKABLE void restart();

signals:
    void serverFound(const QString& name);
    void clientFound(const QString& name);
    void changed();

private:
    void browse(const QString& type);

    bool m_available = true;
    QString m_unavailableReason;
    bool m_active = false;
    bool m_asServer = false;
    QString m_screenName;
    QString m_advertising;
    QString m_error;
    QStringList m_servers;
    QStringList m_clients;

    std::unique_ptr<ZeroconfServer> m_server;
    std::unique_ptr<ZeroconfRegister> m_register;
    std::unique_ptr<ZeroconfBrowser> m_browser;
};
