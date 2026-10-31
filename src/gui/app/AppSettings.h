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
#include <QSettings>
#include <QString>
#include <QtQml/qqmlregistration.h>

/* Typed, write-through access to the app's QSettings (docs/ui-redesign.md
 * §4.3). Keys the legacy GUI wrote keep their names, so the two GUIs read and
 * write the same store and a rollback never loses anything; keys that only
 * the new window has live under "ui/". One `changed` signal serves every
 * property: settings change rarely and QML re-evaluates cheaply.
 */
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.settings")

    // role & core
    Q_PROPERTY(int role READ role WRITE setRole NOTIFY changed)
    Q_PROPERTY(QString screenName READ screenName WRITE setScreenName NOTIFY changed)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY changed)
    Q_PROPERTY(QString networkInterface READ networkInterface WRITE setNetworkInterface NOTIFY changed)
    Q_PROPERTY(int logLevel READ logLevel WRITE setLogLevel NOTIFY changed)
    Q_PROPERTY(bool logToFile READ logToFile WRITE setLogToFile NOTIFY changed)
    Q_PROPERTY(QString logFilename READ logFilename WRITE setLogFilename NOTIFY changed)
    Q_PROPERTY(bool cryptoEnabled READ cryptoEnabled WRITE setCryptoEnabled NOTIFY changed)
    Q_PROPERTY(bool requireClientCertificate READ requireClientCertificate WRITE setRequireClientCertificate NOTIFY changed)
    Q_PROPERTY(int elevateMode READ elevateMode WRITE setElevateMode NOTIFY changed)
    Q_PROPERTY(bool useExternalConfig READ useExternalConfig WRITE setUseExternalConfig NOTIFY changed)
    Q_PROPERTY(QString configFile READ configFile WRITE setConfigFile NOTIFY changed)
    Q_PROPERTY(QString serverHostname READ serverHostname WRITE setServerHostname NOTIFY changed)
    Q_PROPERTY(bool discovery READ discovery WRITE setDiscovery NOTIFY changed)
    Q_PROPERTY(int scrollStep READ scrollStep WRITE setScrollStep NOTIFY changed)
    Q_PROPERTY(bool startedBefore READ startedBefore WRITE setStartedBefore NOTIFY changed)
    // window & startup
    Q_PROPERTY(bool startCoreOnLaunch READ startCoreOnLaunch WRITE setStartCoreOnLaunch NOTIFY changed)
    Q_PROPERTY(bool openHidden READ openHidden WRITE setOpenHidden NOTIFY changed)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY changed)
    Q_PROPERTY(bool launchAtLogin READ launchAtLogin WRITE setLaunchAtLogin NOTIFY changed)
    Q_PROPERTY(QString colorScheme READ colorScheme WRITE setColorScheme NOTIFY changed)
    // notifications & updates
    Q_PROPERTY(bool overlayHints READ overlayHints WRITE setOverlayHints NOTIFY changed)
    Q_PROPERTY(bool systemNotifications READ systemNotifications WRITE setSystemNotifications NOTIFY changed)
    Q_PROPERTY(bool clipboardHint READ clipboardHint WRITE setClipboardHint NOTIFY changed)
    Q_PROPERTY(QString updateChannel READ updateChannel WRITE setUpdateChannel NOTIFY changed)
    Q_PROPERTY(bool autoCheckUpdates READ autoCheckUpdates WRITE setAutoCheckUpdates NOTIFY changed)
    Q_PROPERTY(bool logDrawerOpen READ logDrawerOpen WRITE setLogDrawerOpen NOTIFY changed)

public:
    enum Role { Server = 0, Client = 1 };
    Q_ENUM(Role)
    enum ElevateMode { ElevateAsNeeded = 0, ElevateAlways = 1, ElevateNever = 2 };
    Q_ENUM(ElevateMode)

    explicit AppSettings(QObject* parent = nullptr);
    // tests: a store of their own
    explicit AppSettings(QSettings* store, QObject* parent = nullptr);

    QSettings& store() { return *m_store; }

    int role() const;
    QString screenName() const;
    int port() const;
    QString networkInterface() const;
    int logLevel() const;
    QString logLevelName() const;
    bool logToFile() const;
    QString logFilename() const;
    bool cryptoEnabled() const;
    bool requireClientCertificate() const;
    int elevateMode() const;
    bool useExternalConfig() const;
    QString configFile() const;
    QString serverHostname() const;
    bool discovery() const;
    int scrollStep() const;
    bool startedBefore() const;
    bool startCoreOnLaunch() const;
    bool openHidden() const;
    bool minimizeToTray() const;
    bool launchAtLogin() const;
    QString colorScheme() const;
    bool overlayHints() const;
    bool systemNotifications() const;
    bool clipboardHint() const;
    QString updateChannel() const;
    bool autoCheckUpdates() const;
    bool logDrawerOpen() const;

    // A trusted peer's display name, kept beside the fingerprint database.
    QString peerName(const QString& sha256) const;
    void setPeerName(const QString& sha256, const QString& name);

    static QString defaultLogFilename();
    static QString defaultConfigFile();

public slots:
    void setRole(int role);
    void setScreenName(const QString& name);
    void setPort(int port);
    void setNetworkInterface(const QString& address);
    void setLogLevel(int level);
    void setLogToFile(bool on);
    void setLogFilename(const QString& path);
    void setCryptoEnabled(bool on);
    void setRequireClientCertificate(bool on);
    void setElevateMode(int mode);
    void setUseExternalConfig(bool on);
    void setConfigFile(const QString& path);
    void setServerHostname(const QString& host);
    void setDiscovery(bool on);
    void setScrollStep(int step);
    void setStartedBefore(bool on);
    void setStartCoreOnLaunch(bool on);
    void setOpenHidden(bool on);
    void setMinimizeToTray(bool on);
    void setLaunchAtLogin(bool on);
    void setColorScheme(const QString& scheme);
    void setOverlayHints(bool on);
    void setSystemNotifications(bool on);
    void setClipboardHint(bool on);
    void setUpdateChannel(const QString& channel);
    void setAutoCheckUpdates(bool on);
    void setLogDrawerOpen(bool on);
    void sync();

signals:
    void changed();

private:
    void migrate();
    QVariant value(const char* key, const QVariant& fallback) const;
    void setValue(const char* key, const QVariant& value);

    QSettings* m_store;
};
