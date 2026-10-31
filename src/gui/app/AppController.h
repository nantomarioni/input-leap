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

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "AppSettings.h"
#include "CoreProcess.h"
#include "Discovery.h"
#include "LayoutModel.h"
#include "LogModel.h"
#include "SyncService.h"
#include "TlsIdentity.h"

class QFileSystemWatcher;
class QQmlEngine;
class QJSEngine;
class QTimer;
struct CoreEvent;
namespace inputleap { namespace fork_gui { class UpdateChecker; } }

/* The facade the window binds to (docs/ui-redesign.md §4.3): owns the
 * application layer, turns the core's lines into state, and holds the
 * inline requests (join, trust, permission, error) that replaced the old
 * pop-ups. One instance per process, handed to QML as the `App` singleton.
 */
class AppController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(AppSettings* settings READ settings CONSTANT)
    Q_PROPERTY(LayoutModel* layout READ layout NOTIFY layoutSourceChanged)
    Q_PROPERTY(QString layoutSource READ layoutSource NOTIFY layoutSourceChanged)
    Q_PROPERTY(TlsIdentity* identity READ identity CONSTANT)
    Q_PROPERTY(Discovery* discovery READ discovery CONSTANT)
    Q_PROPERTY(CoreProcess* core READ core CONSTANT)
    Q_PROPERTY(LogModel* logModel READ logModel CONSTANT)
    Q_PROPERTY(LogFilterModel* log READ log CONSTANT)
    Q_PROPERTY(SyncService* sync READ sync CONSTANT)
    Q_PROPERTY(LayoutModel* mirrorLayout READ mirrorLayout CONSTANT)
    Q_PROPERTY(bool mirrorAvailable READ mirrorAvailable NOTIFY stateChanged)

    Q_PROPERTY(Role role READ role WRITE setRole NOTIFY roleChanged)
    Q_PROPERTY(RunState runState READ runState NOTIFY stateChanged)
    Q_PROPERTY(bool canSwitchRole READ canSwitchRole NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString lastEvent READ lastEvent NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QStringList connectedClients READ connectedClients NOTIFY stateChanged)
    Q_PROPERTY(QString activeScreen READ activeScreen NOTIFY stateChanged)
    Q_PROPERTY(bool cursorLocked READ cursorLocked NOTIFY stateChanged)
    Q_PROPERTY(bool cursorLockedElsewhere READ cursorLockedElsewhere NOTIFY stateChanged)
    Q_PROPERTY(bool dimmed READ dimmed NOTIFY stateChanged)
    Q_PROPERTY(QString connectedServer READ connectedServer NOTIFY stateChanged)
    Q_PROPERTY(QString serverAddress READ serverAddress NOTIFY stateChanged)
    Q_PROPERTY(QDateTime since READ since NOTIFY stateChanged)
    Q_PROPERTY(QString screenName READ screenName NOTIFY stateChanged)
    Q_PROPERTY(QStringList addresses READ addresses CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString monoFontFamily READ monoFontFamily CONSTANT)
    Q_PROPERTY(bool firstRun READ firstRun NOTIFY stateChanged)
    Q_PROPERTY(bool accessibilityGranted READ accessibilityGranted NOTIFY stateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString updateName READ updateName NOTIFY stateChanged)
    Q_PROPERTY(bool updateInstallable READ updateInstallable NOTIFY stateChanged)
    Q_PROPERTY(QString updateStatus READ updateStatus NOTIFY stateChanged)
    Q_PROPERTY(QVariantList requests READ requests NOTIFY requestsChanged)
    Q_PROPERTY(QVariantList computers READ computers NOTIFY stateChanged)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY stateChanged)
    Q_PROPERTY(QString generatedConfigPath READ generatedConfigPath CONSTANT)
    Q_PROPERTY(QString dataFolder READ dataFolder CONSTANT)
    Q_PROPERTY(QString platformName READ platformName CONSTANT)
    Q_PROPERTY(bool isWindows READ isWindows CONSTANT)
    Q_PROPERTY(bool isMac READ isMac CONSTANT)
    Q_PROPERTY(QString initialComputer READ initialComputer CONSTANT)   // INPUTLEAP_UI_COMPUTER, for the grabs
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString guiLogPath READ guiLogPath CONSTANT)

public:
    enum Role { Server, Client };
    Q_ENUM(Role)
    enum RunState { Stopped, Starting, Running, Stopping, Failed };
    Q_ENUM(RunState)

    static AppController* instance();
    static AppController* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    AppSettings* settings() const { return m_settings; }
    // The layout the server runs: the editable one kept in the settings, or
    // a read-only reading of the configuration file when one is used.
    LayoutModel* layout() const { return m_externalActive ? m_external : m_layout; }
    QString layoutSource() const;
    TlsIdentity* identity() const { return m_identity; }
    Discovery* discovery() const { return m_discovery; }
    CoreProcess* core() const { return m_core; }
    LogModel* logModel() const { return m_logModel; }
    LogFilterModel* log() const { return m_log; }
    SyncService* sync() const { return m_sync; }
    // The server's configuration as it shares it with this client (phase C).
    LayoutModel* mirrorLayout() const { return m_mirror; }
    bool mirrorAvailable() const { return m_mirrorAvailable; }

    Role role() const;
    RunState runState() const { return m_runState; }
    bool canSwitchRole() const { return m_runState == Stopped || m_runState == Failed; }
    bool running() const { return m_runState == Running; }
    bool connected() const;
    QString statusText() const;
    QString lastEvent() const { return m_lastEvent; }
    QString lastError() const { return m_lastError; }
    QStringList connectedClients() const { return m_connectedClients.values(); }
    QString activeScreen() const { return m_activeScreen; }
    bool cursorLocked() const { return m_cursorLocked; }
    bool cursorLockedElsewhere() const { return m_cursorLockedElsewhere; }
    bool dimmed() const { return m_dimmed; }
    QString connectedServer() const { return m_connectedServer; }
    QString serverAddress() const;
    QDateTime since() const { return m_since; }
    QString screenName() const;
    QStringList addresses() const { return m_addresses; }
    QString version() const { return m_version; }
    QString monoFontFamily() const { return m_monoFontFamily; }
    bool firstRun() const;
    bool accessibilityGranted() const { return m_accessibilityGranted; }
    bool updateAvailable() const { return !m_updateName.isEmpty(); }
    QString updateName() const { return m_updateName; }
    bool updateInstallable() const { return !m_updateAssetUrl.isEmpty(); }
    QString updateStatus() const { return m_updateStatus; }
    QVariantList requests() const { return m_requests; }
    QVariantList computers() const;
    QVariantList processes() const;
    QString generatedConfigPath() const;
    QString dataFolder() const;
    QString platformName() const;
    bool isWindows() const;
    bool isMac() const;
    QString initialComputer() const { return qEnvironmentVariable("INPUTLEAP_UI_COMPUTER"); }
    bool trayAvailable() const { return m_trayAvailable; }
    void setTrayAvailable(bool available);
    QString guiLogPath() const { return dataFolder() + QStringLiteral("/gui.log"); }

    // Called by main() once the window exists.
    void startup();

public slots:
    void start();
    void stop();
    void restart();
    void quit();          // stops the core, then the app
    void setRole(int role);
    void applyLayout();

    void placeJoin(const QString& name, int direction);
    void ignoreJoin(const QString& name);
    void acceptTrust(const QString& id);
    void rejectTrust(const QString& id);
    void dismissRequest(const QString& id);

    void checkForUpdates();
    void installUpdate();
    void restartSync();
    // Phase D: change one of another computer's own settings, or ask it to
    // act; the answer lands in remoteResult(computer, key).
    void setRemote(const QString& computer, const QString& key, const QVariant& value);
    void remoteAction(const QString& computer, const QString& action);
    Q_INVOKABLE QVariantMap remoteResult(const QString& computer, const QString& key) const;
    void refreshAccessibility();
    void requestAccessibility();
    void openLogsFolder();
    void openDataFolder();
    void copyLogToClipboard();
    void appendLog(const QString& line);
    void setLaunchAtLogin(bool enabled);
    void copyText(const QString& text);
    QString pickFile(const QString& title, const QString& start, bool save);
    int addHotkey(QObject* capture);
    bool addHotkeyAction(int hotkeyIndex, const QVariantMap& action, QObject* capture);

signals:
    void roleChanged();
    void stateChanged();
    void layoutSourceChanged();
    void requestsChanged();
    void notify(const QString& title, const QString& text);   // tray balloon / system notification

private:
    explicit AppController(QObject* parent = nullptr);

    void setRunState(RunState state);
    void handleCoreLine(const QString& line);
    void handleEvent(const CoreEvent& event);
    void onCoreStateChanged();
    void onCoreExited(int exitCode, bool unexpected);
    void onClientFound(const QString& name);
    void onServerFound(const QString& name);
    void onPeerResolved(const QString& name, const QString& host, quint16 port, const QMap<QString, QString>& txt);
    void startDiscovery();
    void startSync();
    void reloadExternalLayout();
    void dialServerAddress();
    void publishSync();
    int syncTrusted(const QString& sha256, const QString& name, bool incoming) const;
    QJsonObject handleSyncRequest(const QString& peer, const QString& kind, const QJsonObject& body, int access);
    int serverPort() const;
    bool buildLaunch(QString& program, QStringList& arguments, QString& error);
    void resetLiveState();
    void addRequest(const QString& id, const QString& kind, const QVariantMap& fields);
    bool hasRequest(const QString& id) const;
    void removeRequest(const QString& id);
    void requestJoin(const QString& name);
    void log(const QString& text, const QString& level = QStringLiteral("INFO"));

    AppSettings* m_settings = nullptr;
    LayoutModel* m_layout = nullptr;
    TlsIdentity* m_identity = nullptr;
    Discovery* m_discovery = nullptr;
    CoreProcess* m_core = nullptr;
    SyncService* m_sync = nullptr;
    LayoutModel* m_mirror = nullptr;
    LayoutModel* m_external = nullptr;
    QFileSystemWatcher* m_externalWatcher = nullptr;
    bool m_externalActive = false;
    QString m_externalPath;
    bool m_mirrorAvailable = false;
    bool m_dryRun = false;
    bool m_trayAvailable = false;
    QString m_syncName;
    QString m_syncServerHost;
    QHash<QString, QHash<QString, QVariantMap>> m_remoteResults;
    LogModel* m_logModel = nullptr;
    LogFilterModel* m_log = nullptr;
    inputleap::fork_gui::UpdateChecker* m_updates = nullptr;
    QTimer* m_restartTimer = nullptr;

    RunState m_runState = Stopped;
    bool m_wantRunning = false;
    bool m_pausedForTrust = false;
    int m_unexpectedExits = 0;
    QDateTime m_lastStart;
    QString m_lastEvent;
    QString m_lastError;
    QSet<QString> m_connectedClients;
    QString m_activeScreen;
    bool m_cursorLocked = false;
    bool m_cursorLockedElsewhere = false;
    bool m_dimmed = false;
    bool m_listening = false;
    bool m_connectedToServer = false;
    QString m_connectedServer;
    QDateTime m_since;
    QStringList m_addresses;
    QString m_version;
    QString m_monoFontFamily;
    bool m_accessibilityGranted = true;
    QString m_updateName;
    QString m_updateAssetUrl;
    QString m_updateAssetName;
    QString m_updateStatus;
    QVariantList m_requests;
    QSet<QString> m_ignoredJoins;
    QSet<QString> m_knownFingerprints;
};
