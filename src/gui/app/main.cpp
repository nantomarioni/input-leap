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
#include "TrayController.h"
#include "../../fork/gui/OverlayNotification.h"

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
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
