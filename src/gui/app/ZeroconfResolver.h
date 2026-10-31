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

#include "ZeroconfRecord.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>
#define _MSL_STDINT_H
#include <stdint.h>
#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <dns_sd.h>

#include <memory>

class QSocketNotifier;

/* Resolves one browsed record to its host, port and TXT data: what the
 * settings channel needs to reach a peer (docs/ui-redesign.md §4.7).
 */
class ZeroconfResolver : public QObject
{
    Q_OBJECT

public:
    explicit ZeroconfResolver(QObject* parent = nullptr);
    ~ZeroconfResolver() override;

    bool resolve(const ZeroconfRecord& record);
    const ZeroconfRecord& record() const { return m_record; }

Q_SIGNALS:
    void resolved(const ZeroconfRecord& record, const QString& host, quint16 port, const QMap<QString, QString>& txt);
    void error(DNSServiceErrorType err);

private slots:
    void socketReadyRead();

private:
    static void DNSSD_API resolveReply(DNSServiceRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                       DNSServiceErrorType errorCode, const char* fullname, const char* hosttarget,
                                       uint16_t port, uint16_t txtLen, const unsigned char* txtRecord, void* context);

    DNSServiceRef m_ref = nullptr;
    std::unique_ptr<QSocketNotifier> m_socket;
    ZeroconfRecord m_record;
};
