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

#include "TlsIdentity.h"
#include "AppSettings.h"

#include "base/finally.h"
#include "common/DataDirectories.h"
#include "io/filesystem.h"
#include "net/FingerprintDatabase.h"
#include "net/SecureUtils.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QVariantMap>

namespace {

std::vector<std::uint8_t> bytesFromHex(const QString& hex)
{
    const QByteArray raw = QByteArray::fromHex(TlsIdentity::normaliseHex(hex).remove(QLatin1Char(':')).toLatin1());
    return std::vector<std::uint8_t>(raw.begin(), raw.end());
}

inputleap::FingerprintData sha256Data(const QString& hex)
{
    return {inputleap::fingerprint_type_to_string(inputleap::FingerprintType::SHA256), bytesFromHex(hex)};
}

inputleap::fs::path databasePath(bool clients)
{
    return clients ? inputleap::DataDirectories::trusted_clients_ssl_fingerprints_path()
                   : inputleap::DataDirectories::trusted_servers_ssl_fingerprints_path();
}

bool certificateIsStrong(const inputleap::fs::path& path)
{
    FILE* fp = inputleap::fopen_utf8_path(path, "r");
    if (fp == nullptr) {
        return false;
    }
    auto closeFile = inputleap::finally([fp]() { std::fclose(fp); });
    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    if (cert == nullptr) {
        return false;
    }
    auto freeCert = inputleap::finally([cert]() { X509_free(cert); });
    EVP_PKEY* key = X509_get_pubkey(cert);
    if (key == nullptr) {
        return false;
    }
    auto freeKey = inputleap::finally([key]() { EVP_PKEY_free(key); });
    const int type = EVP_PKEY_type(EVP_PKEY_id(key));
    return (type == EVP_PKEY_RSA || type == EVP_PKEY_DSA) && EVP_PKEY_bits(key) >= 2048;
}

} // namespace

TlsIdentity::TlsIdentity(AppSettings* settings, QObject* parent) :
    QObject(parent),
    m_settings(settings)
{
}

QString TlsIdentity::normaliseHex(const QString& hex)
{
    QString out;
    for (const QChar c : hex) {
        if (c.isLetterOrNumber()) {
            out += c.toUpper();
        }
    }
    QString spaced;
    for (int i = 0; i < out.size(); i += 2) {
        if (!spaced.isEmpty()) spaced += QLatin1Char(':');
        spaced += out.mid(i, 2);
    }
    return spaced;
}

QString TlsIdentity::sha256Short() const
{
    if (m_sha256.isEmpty()) {
        return QString();
    }
    return m_sha256.left(11) + QStringLiteral(" … ") + m_sha256.right(5);
}

QString TlsIdentity::certificatePath() const
{
    return QString::fromStdString(inputleap::DataDirectories::ssl_certificate_path().u8string());
}

bool TlsIdentity::ensureCertificate(QString* message)
{
    const auto certPath = inputleap::DataDirectories::ssl_certificate_path();
    try {
        if (!inputleap::fs::exists(certPath) || !certificateIsStrong(certPath)) {
            const auto dir = certPath.parent_path();
            if (!inputleap::fs::exists(dir)) {
                inputleap::fs::create_directories(dir);
            }
            inputleap::generate_pem_self_signed_cert(certPath.u8string());
            emit this->message(tr("TLS certificate generated"), false);
        }
        const auto localPath = inputleap::DataDirectories::local_ssl_fingerprints_path();
        const auto localDir = localPath.parent_path();
        if (!inputleap::fs::exists(localDir)) {
            inputleap::fs::create_directories(localDir);
        }
        inputleap::FingerprintDatabase db;
        db.add_trusted(inputleap::get_pem_file_cert_fingerprint(certPath.u8string(), inputleap::FingerprintType::SHA1));
        db.add_trusted(inputleap::get_pem_file_cert_fingerprint(certPath.u8string(), inputleap::FingerprintType::SHA256));
        db.write(localPath);
    } catch (const std::exception& e) {
        const QString text = tr("TLS certificate problem: %1").arg(QString::fromUtf8(e.what()));
        if (message != nullptr) *message = text;
        emit this->message(text, true);
        m_ready = false;
        emit changed();
        return false;
    }
    return readLocalFingerprints();
}

bool TlsIdentity::regenerateCertificate()
{
    const auto certPath = inputleap::DataDirectories::ssl_certificate_path();
    try {
        if (inputleap::fs::exists(certPath)) {
            inputleap::fs::remove(certPath);
        }
    } catch (const std::exception& e) {
        emit message(tr("could not remove the old certificate: %1").arg(QString::fromUtf8(e.what())), true);
        return false;
    }
    return ensureCertificate();
}

bool TlsIdentity::readLocalFingerprints()
{
    const auto localPath = inputleap::DataDirectories::local_ssl_fingerprints_path();
    m_ready = false;
    if (inputleap::fs::exists(localPath)) {
        inputleap::FingerprintDatabase db;
        db.read(localPath);
        for (const auto& fp : db.fingerprints()) {
            if (fp.algorithm == "sha1") {
                m_sha1 = QString::fromStdString(inputleap::format_ssl_fingerprint(fp.data));
            } else if (fp.algorithm == "sha256") {
                m_sha256 = QString::fromStdString(inputleap::format_ssl_fingerprint(fp.data));
                m_sha256Columns = QString::fromStdString(inputleap::format_ssl_fingerprint_columns(fp.data));
                m_randomart = QString::fromStdString(inputleap::create_fingerprint_randomart(fp.data));
                m_ready = true;
            }
        }
    }
    emit changed();
    return m_ready;
}

bool TlsIdentity::isTrusted(bool serverRole, const QString& sha256Hex) const
{
    const auto path = databasePath(serverRole);
    if (!inputleap::fs::exists(path)) {
        return false;
    }
    inputleap::FingerprintDatabase db;
    db.read(path);
    return db.is_trusted(sha256Data(sha256Hex));
}

bool TlsIdentity::trust(bool serverRole, const QString& sha1Hex, const QString& sha256Hex, const QString& name)
{
    Q_UNUSED(sha1Hex);
    const auto path = databasePath(serverRole);
    try {
        const auto dir = path.parent_path();
        if (!inputleap::fs::exists(dir)) {
            inputleap::fs::create_directories(dir);
        }
        inputleap::FingerprintDatabase db;
        if (inputleap::fs::exists(path)) {
            db.read(path);
        }
        db.add_trusted(sha256Data(sha256Hex));
        db.write(path);
    } catch (const std::exception& e) {
        emit message(tr("could not save the trusted fingerprint: %1").arg(QString::fromUtf8(e.what())), true);
        return false;
    }
    if (!name.isEmpty()) {
        m_settings->setPeerName(sha256Hex, name);
    }
    emit changed();
    return true;
}

bool TlsIdentity::forget(bool serverRole, const QString& sha256Hex)
{
    const auto path = databasePath(serverRole);
    if (!inputleap::fs::exists(path)) {
        return false;
    }
    const auto target = sha256Data(sha256Hex);
    inputleap::FingerprintDatabase db;
    db.read(path);
    inputleap::FingerprintDatabase kept;
    for (const auto& fp : db.fingerprints()) {
        if (!(fp.algorithm == target.algorithm && fp.data == target.data)) {
            kept.add_trusted(fp);
        }
    }
    try {
        kept.write(path);
    } catch (const std::exception& e) {
        emit message(tr("could not update the trusted fingerprints: %1").arg(QString::fromUtf8(e.what())), true);
        return false;
    }
    emit changed();
    return true;
}

QVariantList TlsIdentity::trusted(bool clients) const
{
    QVariantList list;
    const auto path = databasePath(clients);
    if (!inputleap::fs::exists(path)) {
        return list;
    }
    inputleap::FingerprintDatabase db;
    db.read(path);
    for (const auto& fp : db.fingerprints()) {
        if (fp.algorithm != "sha256") {
            continue;
        }
        const QString hex = QString::fromStdString(inputleap::format_ssl_fingerprint(fp.data));
        QVariantMap entry;
        entry.insert(QStringLiteral("sha256"), hex);
        entry.insert(QStringLiteral("sha256Short"), hex.left(11) + QStringLiteral(" … ") + hex.right(5));
        entry.insert(QStringLiteral("name"), m_settings->peerName(hex));
        entry.insert(QStringLiteral("role"), clients ? QStringLiteral("client") : QStringLiteral("server"));
        list << entry;
    }
    return list;
}

QString TlsIdentity::randomartFor(const QString& sha256Hex)
{
    const auto bytes = bytesFromHex(sha256Hex);
    return bytes.empty() ? QString() : QString::fromStdString(inputleap::create_fingerprint_randomart(bytes));
}

QString TlsIdentity::columnsFor(const QString& sha256Hex)
{
    const auto bytes = bytesFromHex(sha256Hex);
    return bytes.empty() ? QString() : QString::fromStdString(inputleap::format_ssl_fingerprint_columns(bytes));
}
