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

#include "AppSettings.h"
#include "CoreEventParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QHostInfo>

namespace {
// The legacy wizard re-runs when this is lower than its own constant; the new
// window has no wizard, so it writes the legacy value to keep the old GUI
// quiet after a rollback.
const int kLegacyWizardVersion = 9;
} // namespace

namespace {
// INPUTLEAP_UI_SETTINGS_FILE=<ini> keeps development runs away from the
// real store (docs/ui-redesign.md §8).
QSettings* defaultStore(QObject* parent)
{
    const QString file = qEnvironmentVariable("INPUTLEAP_UI_SETTINGS_FILE");
    if (!file.isEmpty()) {
        return new QSettings(file, QSettings::IniFormat, parent);
    }
    return new QSettings(parent);
}
} // namespace

AppSettings::AppSettings(QObject* parent) :
    AppSettings(defaultStore(parent), parent)
{
}

AppSettings::AppSettings(QSettings* store, QObject* parent) :
    QObject(parent),
    m_store(store)
{
    migrate();
}

void AppSettings::migrate()
{
    // Fresh install: take the legacy Barrier store, as the old GUI did.
    if (m_store->allKeys().isEmpty()) {
        QSettings barrier{QStringLiteral("Debauchee"), QStringLiteral("Barrier")};
        const auto keys = barrier.allKeys();
        for (const QString& key : keys) {
            m_store->setValue(key, barrier.value(key));
        }
    }
    // The role used to be two checkable group boxes.
    if (!m_store->contains(QStringLiteral("groupServerChecked")) &&
        !m_store->contains(QStringLiteral("groupClientChecked"))) {
        setRole(Server);
    }
    m_store->setValue(QStringLiteral("wizardLastRun"), kLegacyWizardVersion);
}

QVariant AppSettings::value(const char* key, const QVariant& fallback) const
{
    return m_store->value(QLatin1String(key), fallback);
}

void AppSettings::setValue(const char* key, const QVariant& value)
{
    if (m_store->value(QLatin1String(key)) == value) {
        return;
    }
    m_store->setValue(QLatin1String(key), value);
    emit changed();
}

// ---- getters ---------------------------------------------------------------

int AppSettings::role() const
{
    return value("groupClientChecked", false).toBool() ? Client : Server;
}

QString AppSettings::screenName() const
{
    const QString name = value("screenName", QString()).toString().trimmed();
    return name.isEmpty() ? QHostInfo::localHostName() : name;
}

int AppSettings::port() const { return value("port", 24800).toInt(); }
QString AppSettings::networkInterface() const { return value("interface", QString()).toString(); }
int AppSettings::logLevel() const { return qBound(0, value("logLevel", 3).toInt(), 6); }
QString AppSettings::logLevelName() const { return CoreEventParser::levelName(logLevel()); }
bool AppSettings::logToFile() const { return value("logToFile", false).toBool(); }
QString AppSettings::logFilename() const { return value("logFilename", defaultLogFilename()).toString(); }
bool AppSettings::cryptoEnabled() const { return value("cryptoEnabled", true).toBool(); }
bool AppSettings::requireClientCertificate() const { return value("requireClientCertificate", false).toBool(); }

int AppSettings::elevateMode() const
{
    const QVariant v = value("elevateModeEnum", QVariant());
    if (v.isValid()) {
        return qBound(0, v.toInt(), 2);
    }
    return value("elevateMode", false).toBool() ? ElevateAlways : ElevateAsNeeded;
}

bool AppSettings::useExternalConfig() const { return value("useExternalConfig", false).toBool(); }
QString AppSettings::configFile() const { return value("configFile", defaultConfigFile()).toString(); }
QString AppSettings::serverHostname() const { return value("serverHostname", QString()).toString(); }
bool AppSettings::discovery() const { return value("autoConfig", true).toBool(); }
int AppSettings::scrollStep() const { return value("ui/scrollStep", 120).toInt(); }
bool AppSettings::startedBefore() const { return value("startedBefore", false).toBool(); }
bool AppSettings::startCoreOnLaunch() const { return value("autoStart", false).toBool(); }
bool AppSettings::openHidden() const { return value("autoHide", false).toBool(); }
bool AppSettings::minimizeToTray() const { return value("minimizeToTray", true).toBool(); }
bool AppSettings::launchAtLogin() const { return value("ui/launchAtLogin", false).toBool(); }
QString AppSettings::colorScheme() const { return value("ui/colorScheme", QStringLiteral("system")).toString(); }
bool AppSettings::overlayHints() const { return value("ui/overlayHints", true).toBool(); }
bool AppSettings::systemNotifications() const { return value("ui/systemNotifications", true).toBool(); }
bool AppSettings::clipboardHint() const { return value("ui/clipboardHint", false).toBool(); }
QString AppSettings::updateChannel() const { return value("ui/updateChannel", QStringLiteral("latest")).toString(); }
bool AppSettings::autoCheckUpdates() const { return value("ui/autoCheckUpdates", true).toBool(); }
bool AppSettings::logDrawerOpen() const { return value("ui/logDrawerOpen", false).toBool(); }
bool AppSettings::syncEnabled() const { return value("ui/syncEnabled", true).toBool(); }
qint64 AppSettings::corePid() const { return value("ui/corePid", 0).toLongLong(); }
void AppSettings::setCorePid(qint64 pid) { m_store->setValue(QStringLiteral("ui/corePid"), pid); }   // no changed(): bookkeeping

QString AppSettings::peerName(const QString& sha256) const
{
    return m_store->value(QStringLiteral("ui/peerNames/") + sha256.toUpper().remove(QLatin1Char(':'))).toString();
}

void AppSettings::setPeerName(const QString& sha256, const QString& name)
{
    m_store->setValue(QStringLiteral("ui/peerNames/") + sha256.toUpper().remove(QLatin1Char(':')), name);
}

QString AppSettings::defaultLogFilename()
{
#if defined(Q_OS_WIN)
    return QCoreApplication::applicationDirPath() + QStringLiteral("/log/input-leap.log");
#else
    return QStringLiteral("/var/log/input-leap.log");
#endif
}

QString AppSettings::defaultConfigFile()
{
#if defined(Q_OS_WIN)
    return QDir::homePath() + QStringLiteral("/input-leap.sgc");
#else
    return QDir::homePath() + QStringLiteral("/input-leap.conf");
#endif
}

// ---- setters ---------------------------------------------------------------

void AppSettings::setRole(int role)
{
    const bool client = role == Client;
    if (m_store->contains(QStringLiteral("groupClientChecked")) && this->role() == role) {
        return;
    }
    m_store->setValue(QStringLiteral("groupClientChecked"), client);
    m_store->setValue(QStringLiteral("groupServerChecked"), !client);
    emit changed();
}

void AppSettings::setScreenName(const QString& name) { setValue("screenName", name.trimmed()); }
void AppSettings::setPort(int port) { setValue("port", qBound(1, port, 65535)); }
void AppSettings::setNetworkInterface(const QString& address) { setValue("interface", address.trimmed()); }
void AppSettings::setLogLevel(int level) { setValue("logLevel", qBound(0, level, 6)); }
void AppSettings::setLogToFile(bool on) { setValue("logToFile", on); }
void AppSettings::setLogFilename(const QString& path) { setValue("logFilename", path); }
void AppSettings::setCryptoEnabled(bool on) { setValue("cryptoEnabled", on); }
void AppSettings::setRequireClientCertificate(bool on) { setValue("requireClientCertificate", on); }

void AppSettings::setElevateMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (elevateMode() == mode && m_store->contains(QStringLiteral("elevateModeEnum"))) {
        return;
    }
    m_store->setValue(QStringLiteral("elevateModeEnum"), mode);
    m_store->setValue(QStringLiteral("elevateMode"), mode == ElevateAlways);
    emit changed();
}

void AppSettings::setUseExternalConfig(bool on)
{
    if (useExternalConfig() == on && m_store->contains(QStringLiteral("useExternalConfig"))) {
        return;
    }
    m_store->setValue(QStringLiteral("useExternalConfig"), on);
    m_store->setValue(QStringLiteral("useInternalConfig"), !on);
    emit changed();
}

void AppSettings::setConfigFile(const QString& path) { setValue("configFile", path); }
void AppSettings::setServerHostname(const QString& host) { setValue("serverHostname", host.trimmed()); }
void AppSettings::setDiscovery(bool on) { setValue("autoConfig", on); }
void AppSettings::setScrollStep(int step) { setValue("ui/scrollStep", qBound(1, step, 10000)); }
void AppSettings::setStartedBefore(bool on) { setValue("startedBefore", on); }
void AppSettings::setStartCoreOnLaunch(bool on) { setValue("autoStart", on); }
void AppSettings::setOpenHidden(bool on) { setValue("autoHide", on); }
void AppSettings::setMinimizeToTray(bool on) { setValue("minimizeToTray", on); }
void AppSettings::setLaunchAtLogin(bool on) { setValue("ui/launchAtLogin", on); }
void AppSettings::setColorScheme(const QString& scheme) { setValue("ui/colorScheme", scheme); }
void AppSettings::setOverlayHints(bool on) { setValue("ui/overlayHints", on); }
void AppSettings::setSystemNotifications(bool on) { setValue("ui/systemNotifications", on); }
void AppSettings::setClipboardHint(bool on) { setValue("ui/clipboardHint", on); }
void AppSettings::setUpdateChannel(const QString& channel) { setValue("ui/updateChannel", channel); }
void AppSettings::setAutoCheckUpdates(bool on) { setValue("ui/autoCheckUpdates", on); }
void AppSettings::setLogDrawerOpen(bool on) { setValue("ui/logDrawerOpen", on); }
void AppSettings::setSyncEnabled(bool on) { setValue("ui/syncEnabled", on); }

void AppSettings::sync()
{
    m_store->sync();
}
