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
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class AppSettings;

/* This computer's TLS certificate and the fingerprints it trusts, on top of
 * the core's own files (DataDirectories, FingerprintDatabase, SecureUtils):
 * the same certificate and trust databases the daemons read.
 */
class TlsIdentity : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.identity")

    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString sha256 READ sha256 NOTIFY changed)
    Q_PROPERTY(QString sha256Short READ sha256Short NOTIFY changed)
    Q_PROPERTY(QString sha256Columns READ sha256Columns NOTIFY changed)
    Q_PROPERTY(QString sha1 READ sha1 NOTIFY changed)
    Q_PROPERTY(QString randomart READ randomart NOTIFY changed)
    Q_PROPERTY(QString certificatePath READ certificatePath CONSTANT)
    Q_PROPERTY(QVariantList trustedServers READ trustedServers NOTIFY changed)
    Q_PROPERTY(QVariantList trustedClients READ trustedClients NOTIFY changed)

public:
    explicit TlsIdentity(AppSettings* settings, QObject* parent = nullptr);

    bool ready() const { return m_ready; }
    QString sha256() const { return m_sha256; }
    QString sha256Short() const;
    QString sha256Columns() const { return m_sha256Columns; }
    QString sha1() const { return m_sha1; }
    QString randomart() const { return m_randomart; }
    QString certificatePath() const;
    QVariantList trustedServers() const { return trusted(false); }
    QVariantList trustedClients() const { return trusted(true); }

    // Generates the certificate when missing or weak, then reads the local
    // fingerprints. Returns false and sets message on failure.
    bool ensureCertificate(QString* message = nullptr);
    Q_INVOKABLE bool regenerateCertificate();

    // serverRole = true checks/edits the trusted *clients* database.
    Q_INVOKABLE bool isTrusted(bool serverRole, const QString& sha256Hex) const;
    Q_INVOKABLE bool trust(bool serverRole, const QString& sha1Hex, const QString& sha256Hex, const QString& name);
    Q_INVOKABLE bool forget(bool serverRole, const QString& sha256Hex);

    Q_INVOKABLE static QString randomartFor(const QString& sha256Hex);
    Q_INVOKABLE static QString columnsFor(const QString& sha256Hex);
    static QString normaliseHex(const QString& hex);

signals:
    void changed();
    void message(const QString& text, bool isError);

private:
    bool readLocalFingerprints();
    QVariantList trusted(bool clients) const;

    AppSettings* m_settings;
    bool m_ready = false;
    QString m_sha256;
    QString m_sha256Columns;
    QString m_sha1;
    QString m_randomart;
};
