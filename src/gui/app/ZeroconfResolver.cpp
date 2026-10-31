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

#include "ZeroconfResolver.h"

#include <QtCore/QSocketNotifier>

ZeroconfResolver::ZeroconfResolver(QObject* parent) :
    QObject(parent)
{
}

ZeroconfResolver::~ZeroconfResolver()
{
    if (m_ref != nullptr) {
        DNSServiceRefDeallocate(m_ref);
        m_ref = nullptr;
    }
}

bool ZeroconfResolver::resolve(const ZeroconfRecord& record)
{
    if (m_ref != nullptr) {
        return false;
    }
    m_record = record;
    const DNSServiceErrorType err = DNSServiceResolve(&m_ref, 0, kDNSServiceInterfaceIndexAny,
        record.serviceName.toUtf8().constData(), record.registeredType.toUtf8().constData(),
        record.replyDomain.isEmpty() ? "local." : record.replyDomain.toUtf8().constData(),
        resolveReply, this);
    if (err != kDNSServiceErr_NoError) {
        Q_EMIT error(err);
        return false;
    }
    const int fd = DNSServiceRefSockFD(m_ref);
    if (fd == -1) {
        Q_EMIT error(kDNSServiceErr_Invalid);
        return false;
    }
    m_socket = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read, this);
    connect(m_socket.get(), &QSocketNotifier::activated, this, &ZeroconfResolver::socketReadyRead);
    return true;
}

void ZeroconfResolver::socketReadyRead()
{
    const DNSServiceErrorType err = DNSServiceProcessResult(m_ref);
    if (err != kDNSServiceErr_NoError) {
        Q_EMIT error(err);
    }
}

void ZeroconfResolver::resolveReply(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType errorCode,
                                    const char*, const char* hosttarget, uint16_t port, uint16_t txtLen,
                                    const unsigned char* txtRecord, void* context)
{
    auto* resolver = static_cast<ZeroconfResolver*>(context);
    if (errorCode != kDNSServiceErr_NoError) {
        Q_EMIT resolver->error(errorCode);
        return;
    }
    QString host = QString::fromUtf8(hosttarget);
    while (host.endsWith(QLatin1Char('.'))) {
        host.chop(1);
    }
    // the port arrives in network byte order
    const quint16 hostPort = static_cast<quint16>(((port & 0xff) << 8) | ((port >> 8) & 0xff));

    QMap<QString, QString> txt;
    const uint16_t count = TXTRecordGetCount(txtLen, txtRecord);
    for (uint16_t i = 0; i < count; ++i) {
        char key[256];
        uint8_t valueLen = 0;
        const void* value = nullptr;
        if (TXTRecordGetItemAtIndex(txtLen, txtRecord, i, sizeof(key), key, &valueLen, &value) == kDNSServiceErr_NoError) {
            txt.insert(QString::fromUtf8(key),
                       value != nullptr ? QString::fromUtf8(static_cast<const char*>(value), valueLen) : QString());
        }
    }
    Q_EMIT resolver->resolved(resolver->m_record, host, hostPort, txt);
}
