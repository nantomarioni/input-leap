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
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSettings>
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

    // Newer build available — offer the platform installer (fall back to
    // the release page when no matching asset is found).
    QString downloadUrl = QString::fromLatin1(kReleasePageUrl);
    const QString suffix = platformAssetSuffix();
    if (!suffix.isEmpty()) {
        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const auto& value : assets) {
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("name")).toString().endsWith(suffix)) {
                downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
                break;
            }
        }
    }

    QMessageBox box(m_parentWindow);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Update Available"));
    box.setText(tr("A newer build is available.\n\nInstalled: %1\n%2")
                    .arg(QStringLiteral(INPUTLEAP_VERSION), releaseName));
    QPushButton* download = box.addButton(tr("Download"), QMessageBox::AcceptRole);
    box.addButton(tr("Later"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == download) {
        QDesktopServices::openUrl(QUrl(downloadUrl));
    }
}

QAction* createUpdateCheckAction(QWidget* parentWindow)
{
    auto* checker = new UpdateChecker(parentWindow);
    auto* action = new QAction(QObject::tr("Check for &Updates..."), parentWindow);
    QObject::connect(action, &QAction::triggered, checker,
                     [checker]() { checker->check(false); });
    checker->maybeCheckOnStartup();
    return action;
}

} // namespace fork_gui
} // namespace inputleap
