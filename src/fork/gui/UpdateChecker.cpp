/*  InputLeap (nantomarioni fork) -- mouse and keyboard sharing utility
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

#include "UpdateChecker.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace inputleap {
namespace fork_gui {

namespace {
const char kReleaseApiUrl[] =
    "https://api.github.com/repos/nantomarioni/input-leap/releases/tags/latest-build";
const char kReleasePageUrl[] =
    "https://github.com/nantomarioni/input-leap/releases/tag/latest-build";
const char kSettingsOrg[] = "InputLeap";
const char kSettingsApp[] = "fork-updater";
const char kLastCheckKey[] = "lastCheckUtc";
const int kStartupDelayMs = 15 * 1000;
const qint64 kMinSecondsBetweenStartupChecks = 20 * 60 * 60; // ~daily
} // namespace

UpdateChecker::UpdateChecker(QWidget* parentWindow) :
    QObject(parentWindow),
    m_parentWindow(parentWindow),
    m_network(new QNetworkAccessManager(this))
{
}

QString UpdateChecker::currentCommitHash()
{
    // INPUTLEAP_VERSION looks like "3.0.3-git-2026-09-10-eb2961d0" for
    // commit-versioned builds; the hash is the last dash-separated token.
    const QString version = QStringLiteral(INPUTLEAP_VERSION);
    const QString desc = version.section(QLatin1Char('-'), 1);
    if (!desc.startsWith(QLatin1String("git-"))) {
        return {};
    }
    return version.section(QLatin1Char('-'), -1);
}

QString UpdateChecker::platformAssetSuffix()
{
#if defined(Q_OS_MACOS)
    return QStringLiteral(".dmg");
#elif defined(Q_OS_WIN)
    return QStringLiteral(".exe");
#else
    return {};
#endif
}

void UpdateChecker::maybeCheckOnStartup()
{
    QSettings settings(kSettingsOrg, kSettingsApp);
    const qint64 last = settings.value(kLastCheckKey, 0).toLongLong();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - last < kMinSecondsBetweenStartupChecks) {
        return;
    }
    QTimer::singleShot(kStartupDelayMs, this, [this]() { check(true); });
}

void UpdateChecker::check(bool quiet)
{
    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.setValue(kLastCheckKey, QDateTime::currentSecsSinceEpoch());

    QNetworkRequest request{QUrl(QString::fromLatin1(kReleaseApiUrl))};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "input-leap-fork-updater");
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, quiet]() { handleReply(reply, quiet); });
}

void UpdateChecker::handleReply(QNetworkReply* reply, bool quiet)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (!quiet) {
            QMessageBox::warning(m_parentWindow, tr("Check for Updates"),
                                 tr("Could not reach GitHub:\n%1").arg(reply->errorString()));
        }
        return;
    }

    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    const QString remoteCommit = release.value(QStringLiteral("target_commitish")).toString();
    const QString releaseName = release.value(QStringLiteral("name")).toString();
    const QString localHash = currentCommitHash();

    if (localHash.isEmpty() || remoteCommit.isEmpty()) {
        if (!quiet) {
            QMessageBox::information(m_parentWindow, tr("Check for Updates"),
                tr("Cannot compare versions.\nInstalled: %1\nLatest build: %2")
                    .arg(QStringLiteral(INPUTLEAP_VERSION), releaseName));
        }
        return;
    }

    if (remoteCommit.startsWith(localHash)) {
        if (!quiet) {
            QMessageBox::information(m_parentWindow, tr("Check for Updates"),
                tr("You are running the latest build (%1).")
                    .arg(QStringLiteral(INPUTLEAP_VERSION)));
        }
        return;
    }

    // Newer build available — offer one-click self-update (fall back to
    // the release page when no matching asset is found).
    QString assetUrl;
    QString assetName;
    const QString suffix = platformAssetSuffix();
    if (!suffix.isEmpty()) {
        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const auto& value : assets) {
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("name")).toString().endsWith(suffix)) {
                assetUrl = asset.value(QStringLiteral("browser_download_url")).toString();
                assetName = asset.value(QStringLiteral("name")).toString();
                break;
            }
        }
    }

    QMessageBox box(m_parentWindow);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Update Available"));
    box.setText(tr("A newer build is available.\n\nInstalled: %1\n%2")
                    .arg(QStringLiteral(INPUTLEAP_VERSION), releaseName));
    QPushButton* install = nullptr;
    if (!assetUrl.isEmpty()) {
        install = box.addButton(tr("Install and Relaunch"), QMessageBox::AcceptRole);
    }
    QPushButton* browse = box.addButton(tr("Open Release Page"), QMessageBox::ActionRole);
    box.addButton(tr("Later"), QMessageBox::RejectRole);
    box.exec();
    if (install != nullptr && box.clickedButton() == install) {
        startSelfUpdate(assetUrl, assetName);
    }
    else if (box.clickedButton() == browse) {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kReleasePageUrl)));
    }
}

void UpdateChecker::startSelfUpdate(const QString& assetUrl, const QString& assetName)
{
    const QString target =
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(assetName);

    auto* progress = new QProgressDialog(tr("Downloading %1...").arg(assetName),
                                         tr("Cancel"), 0, 100, m_parentWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);

    QNetworkRequest request{QUrl(assetUrl)};
    request.setRawHeader("User-Agent", "input-leap-fork-updater");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);

    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::downloadProgress, progress,
            [progress](qint64 done, qint64 total) {
                if (total > 0) {
                    progress->setValue(static_cast<int>(done * 100 / total));
                }
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply, progress, target]() {
        reply->deleteLater();
        progress->deleteLater();
        progress->close();

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() != QNetworkReply::OperationCanceledError) {
                QMessageBox::warning(m_parentWindow, tr("Update"),
                                     tr("Download failed:\n%1").arg(reply->errorString()));
            }
            return;
        }
        QFile out(target);
        if (!out.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(m_parentWindow, tr("Update"),
                                 tr("Could not write %1").arg(target));
            return;
        }
        out.write(reply->readAll());
        out.close();
        finishSelfUpdate(target);
    });
}

void UpdateChecker::finishSelfUpdate(const QString& installerPath)
{
#if defined(Q_OS_WIN)
    // The installer stops all InputLeap processes itself (CurStepChanged in
    // the iss script) and relaunches the app afterwards ([Run] postinstall).
    QProcess::startDetached(installerPath, {QStringLiteral("/SILENT")});
    QCoreApplication::quit();
#elif defined(Q_OS_MACOS)
    // Replace the running bundle: a detached helper waits for this process
    // to exit, mounts the dmg, verifies the code signature, swaps the .app
    // and relaunches. Works even when a LaunchAgent relaunches the app —
    // the helper kills stragglers right before the swap.
    const QString bundlePath =
        QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../.."))
            .canonicalPath();
    if (!bundlePath.endsWith(QLatin1String(".app"))) {
        QMessageBox::warning(m_parentWindow, tr("Update"),
                             tr("Not running from an app bundle (%1); "
                                "install manually from the dmg.").arg(bundlePath));
        return;
    }

    const QString script =
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("inputleap-selfupdate.sh"));
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(m_parentWindow, tr("Update"), tr("Could not write helper script"));
        return;
    }
    f.write(
        "#!/bin/sh\n"
        "# InputLeap fork self-update helper (generated; safe to delete)\n"
        "PID=\"$1\"; DMG=\"$2\"; APP=\"$3\"\n"
        "LOG=\"${TMPDIR:-/tmp}/inputleap_fork_debug.log\"\n"
        "echo \"[selfupdate] waiting for pid $PID\" >> \"$LOG\"\n"
        "for i in $(seq 1 50); do kill -0 \"$PID\" 2>/dev/null || break; sleep 0.2; done\n"
        "MNT=$(mktemp -d)\n"
        "hdiutil attach -nobrowse -quiet -mountpoint \"$MNT\" \"$DMG\" || exit 1\n"
        "NEWAPP=$(ls -d \"$MNT\"/*.app | head -1)\n"
        "if ! codesign --verify --deep \"$NEWAPP\" 2>> \"$LOG\"; then\n"
        "  echo \"[selfupdate] signature verify FAILED, aborting\" >> \"$LOG\"\n"
        "  hdiutil detach -quiet \"$MNT\"; exit 1\n"
        "fi\n"
        "# kill anything a LaunchAgent may have resurrected meanwhile\n"
        "pkill -f \"$APP\" 2>/dev/null; sleep 0.5\n"
        "rm -rf \"$APP\" && ditto \"$NEWAPP\" \"$APP\"\n"
        "RC=$?\n"
        "hdiutil detach -quiet \"$MNT\"\n"
        "rm -f \"$DMG\"\n"
        "echo \"[selfupdate] swap rc=$RC, relaunching\" >> \"$LOG\"\n"
        "open \"$APP\"\n");
    f.close();
    QFile::setPermissions(script, QFile::permissions(script) | QFileDevice::ExeOwner);

    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            {script,
                             QString::number(QCoreApplication::applicationPid()),
                             installerPath, bundlePath});
    QCoreApplication::quit();
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(installerPath));
#endif
}

QAction* createUpdateCheckAction(QWidget* parentWindow)
{
    // Cache per window so the action can be shared across menus (menu bar +
    // tray) with a single checker and a single startup check behind it.
    static QPointer<QWidget> cachedWindow;
    static QPointer<QAction> cachedAction;
    if (cachedWindow == parentWindow && !cachedAction.isNull()) {
        return cachedAction;
    }

    auto* checker = new UpdateChecker(parentWindow);
    auto* action = new QAction(QObject::tr("Check for &Updates..."), parentWindow);
    QObject::connect(action, &QAction::triggered, checker,
                     [checker]() { checker->check(false); });
    checker->maybeCheckOnStartup();

    cachedWindow = parentWindow;
    cachedAction = action;
    return action;
}

} // namespace fork_gui
} // namespace inputleap
