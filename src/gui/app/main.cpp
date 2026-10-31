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

#include "AppController.h"
#include "AppSettings.h"
#include "LayoutModel.h"
#include "SyncService.h"
#include "TlsIdentity.h"
#include "TrayController.h"
#include "../../fork/gui/OverlayNotification.h"

#include <QApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSslSocket>
#include <QStyleHints>
#include <QTimer>

#include <cstdio>

namespace {

void applyColorScheme(const QString& scheme)
{
    if (scheme == QLatin1String("dark")) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    } else if (scheme == QLatin1String("light")) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    } else {
        QGuiApplication::styleHints()->unsetColorScheme();
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("InputLeap"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("github.com"));
    QCoreApplication::setApplicationName(QStringLiteral("InputLeap"));

    // QApplication (not QGuiApplication): the tray menu and the overlay HUD
    // are QtWidgets; the window itself is Qt Quick.
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
#if defined(Q_OS_MACOS)
    // the deployed bundle carries libssl/libcrypto next to the Qt frameworks;
    // Qt's OpenSSL backend searches the library paths for them
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../Frameworks"));
#endif
    app.setWindowIcon(QIcon(QStringLiteral(":/tray/icons/256x256/input-leap.png")));
    app.setDesktopFileName(QStringLiteral("io.github.input_leap.input-leap"));

    // One look on every platform; light/dark follow the system unless set.
    QQuickStyle::setStyle(QStringLiteral("FluentWinUI3"));

    AppController* controller = AppController::instance();
    const QString forcedScheme = qEnvironmentVariable("INPUTLEAP_UI_COLOR_SCHEME");
    applyColorScheme(forcedScheme.isEmpty() ? controller->settings()->colorScheme() : forcedScheme);
    QObject::connect(controller->settings(), &AppSettings::changed, &app, [controller]() {
        applyColorScheme(controller->settings()->colorScheme());
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("InputLeap.Ui", "Main");

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
    if (window == nullptr) {
        std::fprintf(stderr, "input-leap: the main window did not load\n");
        return 1;
    }

    TrayController tray(window);
    const bool openHidden = controller->settings()->openHidden() && tray.available()
                            && !qEnvironmentVariableIsSet("INPUTLEAP_UI_GRAB");
    if (openHidden) {
        window->hide();
    }

    controller->startup();

    // Development hooks. INPUTLEAP_UI_DEMO_OVERLAY=1 shows the HUD overlay
    // once; INPUTLEAP_UI_GRAB=<file.png> renders the window to a file and
    // exits (headless with QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software).
    if (qEnvironmentVariableIsSet("INPUTLEAP_UI_DEMO_OVERLAY")) {
        QTimer::singleShot(400, window, []() {
            inputleap::fork_gui::OverlayNotification::instance()->showTransient(
                QObject::tr("Input Leap connected"),
                inputleap::fork_gui::OverlayNotification::Tone::Success);
        });
    }
    // INPUTLEAP_UI_TLS_CHECK=1 reports what the settings channel would use.
    if (qEnvironmentVariableIsSet("INPUTLEAP_UI_TLS_CHECK")) {
        QString error;
        SyncService probe;
        const bool identity = probe.setIdentity(controller->identity()->certificatePath(), &error);
        std::fprintf(stderr, "input-leap: tls %s, backend %s, library %s, identity %s%s\n",
                     QSslSocket::supportsSsl() ? "available" : "MISSING",
                     qPrintable(QSslSocket::supportsSsl() ? QSslSocket::activeBackend() : QStringLiteral("none")),
                     qPrintable(QSslSocket::sslLibraryVersionString()),
                     identity ? "ok" : "FAILED: ", identity ? "" : qPrintable(error));
    }
    // INPUTLEAP_UI_DEMO_PEER=<name>[:server] pretends that computer shares its
    // settings over the channel, so the remote tab (or, for a server, the
    // mirrored Server and Layout pages) can be looked at on one machine.
    const QString demoSpec = qEnvironmentVariable("INPUTLEAP_UI_DEMO_PEER");
    if (!demoSpec.isEmpty()) {
        const QString demoPeer = demoSpec.section(QLatin1Char(':'), 0, 0);
        const bool demoServer = demoSpec.section(QLatin1Char(':'), 1, 1) == QLatin1String("server");
        QJsonObject settings;
        settings.insert(QStringLiteral("screenName"), demoPeer);
        settings.insert(QStringLiteral("role"), demoServer ? QStringLiteral("server") : QStringLiteral("client"));
        settings.insert(QStringLiteral("port"), 24800);
        settings.insert(QStringLiteral("networkInterface"), QString());
        settings.insert(QStringLiteral("serverHostname"), QString());
        settings.insert(QStringLiteral("discovery"), true);
        settings.insert(QStringLiteral("scrollStep"), 120);
        settings.insert(QStringLiteral("overlayHints"), true);
        settings.insert(QStringLiteral("logLevel"), 3);
        settings.insert(QStringLiteral("logLevelName"), QStringLiteral("INFO"));
        settings.insert(QStringLiteral("launchAtLogin"), true);
        settings.insert(QStringLiteral("startCoreOnLaunch"), true);
        settings.insert(QStringLiteral("cryptoEnabled"), true);
        settings.insert(QStringLiteral("fingerprint"), QStringLiteral("2B:7C:11:9A:0F:33:C4:5E:88:D1:AA:67:42:F0:19:3B:6D:E5:7A:C8:90:1F:4B:AC:52:0E:8D:73:B9:26:F1:64"));
        settings.insert(QStringLiteral("platform"), QStringLiteral("macOS"));
        settings.insert(QStringLiteral("addresses"), QJsonArray{QStringLiteral("192.168.50.140")});
        QJsonObject state;
        state.insert(QStringLiteral("runState"), 2);
        state.insert(QStringLiteral("statusText"), QStringLiteral("Connected to Mac.local"));
        QJsonArray processes;
        auto piece = [](const char* name, const char* detail, bool ok) {
            QJsonObject o;
            o.insert(QStringLiteral("name"), QLatin1String(name));
            o.insert(QStringLiteral("detail"), QLatin1String(detail));
            o.insert(QStringLiteral("ok"), ok);
            o.insert(QStringLiteral("relevant"), true);
            return o;
        };
        processes << piece("App", "3.0.3 · launched at 08:12", true)
                  << piece("Core · input-leapc", "Running · PID 4120", true)
                  << piece("Discovery · Bonjour", "Advertising mac-studio · 1 found", true)
                  << piece("Settings channel", "Port 52114 · mirroring Mac", true);
        state.insert(QStringLiteral("processes"), processes);
        QJsonObject serverConfig;
        if (demoServer) {
            QJsonObject server;
            server.insert(QStringLiteral("port"), 24800);
            server.insert(QStringLiteral("networkInterface"), QString());
            server.insert(QStringLiteral("cryptoEnabled"), true);
            server.insert(QStringLiteral("requireClientCertificate"), false);
            server.insert(QStringLiteral("useExternalConfig"), false);
            server.insert(QStringLiteral("configFile"), QString());
            server.insert(QStringLiteral("generatedConfigPath"), QStringLiteral("/Users/demo/Library/Application Support/InputLeap/generated.conf"));
            serverConfig.insert(QStringLiteral("settings"), server);
            LayoutModel sample(nullptr);
            sample.setServerName(demoPeer);
            sample.ensureServerPlaced();
            sample.placeNextTo(controller->screenName(), demoPeer, LayoutModel::Left);
            sample.placeNextTo(QStringLiteral("tablet-pro"), demoPeer, LayoutModel::Up);
            serverConfig.insert(QStringLiteral("layout"), sample.toJson());
            serverConfig.insert(QStringLiteral("conf"), sample.configText());
        }
        controller->sync()->addDemoPeer(demoPeer, demoServer ? QStringLiteral("server") : QStringLiteral("client"), settings, state, 2, serverConfig);
    }
    // INPUTLEAP_UI_PAGE=<index> opens that page (for the grabs).
    bool pageOk = false;
    const int page = qEnvironmentVariableIntValue("INPUTLEAP_UI_PAGE", &pageOk);
    if (pageOk) {
        window->setProperty("currentPage", page);
    }
    const QString grabPath = qEnvironmentVariable("INPUTLEAP_UI_GRAB");
    if (!grabPath.isEmpty()) {
        QTimer::singleShot(1500, window, [window, grabPath]() {
            const QImage image = window->grabWindow();
            const bool saved = !image.isNull() && image.save(grabPath);
            std::fprintf(stderr, "input-leap: grab %s (%dx%d)\n", saved ? "saved" : "FAILED",
                         image.width(), image.height());
            QCoreApplication::exit(saved ? 0 : 2);
        });
    }

    return app.exec();
}
