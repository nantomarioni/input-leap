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

#include "Discovery.h"
#include "ZeroconfBrowser.h"
#include "ZeroconfRegister.h"
#include "ZeroconfResolver.h"

#include <cstdlib>

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

const char kServerType[] = "_inputLeapServerZeroconf._tcp";
const char kClientType[] = "_inputLeapClientZeroconf._tcp";

#if defined(Q_OS_WIN)
bool bonjourServiceRunning()
{
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (manager == nullptr) {
        return false;
    }
    bool running = false;
    SC_HANDLE service = OpenServiceW(manager, L"Bonjour Service", SERVICE_QUERY_STATUS);
    if (service != nullptr) {
        SERVICE_STATUS status;
        if (QueryServiceStatus(service, &status)) {
            running = status.dwCurrentState == SERVICE_RUNNING;
        }
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return running;
}
#endif

void silenceAvahiWarning()
{
    // libavahi's Bonjour compatibility layer prints a warning unless told
    // not to; harmless elsewhere.
#if defined(Q_OS_WIN)
    SetEnvironmentVariableW(L"AVAHI_COMPAT_NOWARN", L"1");
#else
    setenv("AVAHI_COMPAT_NOWARN", "1", 1);
#endif
}

} // namespace

Discovery::Discovery(QObject* parent) :
    QObject(parent)
{
    silenceAvahiWarning();
#if defined(Q_OS_WIN)
    m_available = bonjourServiceRunning();
    if (!m_available) {
        m_unavailableReason = tr("Bonjour is not installed or its service is not running");
    }
#endif
}

Discovery::~Discovery()
{
    stop();
}

QVariantList Discovery::peers() const
{
    QVariantList list;
    const QStringList names = m_asServer ? m_clients : m_servers;
    for (const QString& name : names) {
        list << peer(name);
    }
    return list;
}

QVariantMap Discovery::peer(const QString& name) const
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), name);
    const auto it = m_peers.constFind(name);
    if (it == m_peers.constEnd()) {
        map.insert(QStringLiteral("resolved"), false);
        return map;
    }
    map.insert(QStringLiteral("resolved"), true);
    map.insert(QStringLiteral("host"), it->host);
    map.insert(QStringLiteral("port"), it->port);
    map.insert(QStringLiteral("sync"), it->sync());
    map.insert(QStringLiteral("corePort"), it->corePort());
    return map;
}

bool Discovery::resolvedPeer(const QString& name, Peer& out) const
{
    const auto it = m_peers.constFind(name);
    if (it == m_peers.constEnd()) {
        return false;
    }
    out = *it;
    return true;
}

void Discovery::start(bool asServer, const QString& screenName, quint16 advertisedPort, const QMap<QString, QString>& txt)
{
    stop();
    if (!m_available) {
        return;
    }
    m_asServer = asServer;
    m_screenName = screenName;
    m_advertisedPort = advertisedPort;
    m_txt = txt;
    m_error.clear();

    m_register = std::make_unique<ZeroconfRegister>();
    connect(m_register.get(), &ZeroconfRegister::error, this, [this](DNSServiceErrorType code) {
        m_error = tr("registration failed (code %1)").arg(code);
        emit changed();
    });
    const QString type = QLatin1String(asServer ? kServerType : kClientType);
    m_register->registerService(ZeroconfRecord(screenName, type, QString()), advertisedPort, txt);
    m_advertising = screenName + QStringLiteral(" as ") + type;

    browse(QLatin1String(asServer ? kClientType : kServerType));
    m_active = true;
    emit changed();
}

void Discovery::browse(const QString& type)
{
    m_browser = std::make_unique<ZeroconfBrowser>();
    connect(m_browser.get(), &ZeroconfBrowser::currentRecordsChanged, this,
            [this](const QList<ZeroconfRecord>& records) {
                QStringList& list = m_asServer ? m_clients : m_servers;
                QStringList seen;
                for (const ZeroconfRecord& record : records) {
                    const QString name = record.serviceName;
                    if (name.isEmpty() || name == m_screenName) {
                        continue;
                    }
                    seen << name;
                    if (!list.contains(name)) {
                        list << name;
                        resolveRecord(record);
                        emit changed();
                        if (m_asServer) {
                            emit clientFound(name);
                        } else {
                            emit serverFound(name);
                        }
                    }
                }
                // records that went away
                for (int i = list.size() - 1; i >= 0; --i) {
                    if (!seen.contains(list[i])) {
                        const QString gone = list.takeAt(i);
                        m_peers.remove(gone);
                        m_resolvers.remove(gone);
                        emit peerLost(gone);
                        emit changed();
                    }
                }
            });
    connect(m_browser.get(), &ZeroconfBrowser::error, this, [this](DNSServiceErrorType code) {
        m_error = tr("browsing failed (code %1)").arg(code);
        emit changed();
    });
    m_browser->browseForType(type);
}

void Discovery::resolveRecord(const ZeroconfRecord& record)
{
    auto resolver = std::make_shared<ZeroconfResolver>();
    connect(resolver.get(), &ZeroconfResolver::resolved, this,
            [this](const ZeroconfRecord& rec, const QString& host, quint16 port, const QMap<QString, QString>& txt) {
                Peer peer;
                peer.name = rec.serviceName;
                peer.host = host;
                peer.port = port;
                peer.txt = txt;
                m_peers.insert(peer.name, peer);
                emit peerResolved(peer.name, host, port, txt);
                emit changed();
            });
    m_resolvers.insert(record.serviceName, resolver);
    resolver->resolve(record);
}

void Discovery::stop()
{
    const bool wasActive = m_active;
    m_browser.reset();
    m_register.reset();
    m_resolvers.clear();
    m_peers.clear();
    m_active = false;
    m_advertising.clear();
    m_servers.clear();
    m_clients.clear();
    if (wasActive) {
        emit changed();
    }
}

void Discovery::restart()
{
    if (m_active || !m_screenName.isEmpty()) {
        start(m_asServer, m_screenName, m_advertisedPort, m_txt);
    }
}
