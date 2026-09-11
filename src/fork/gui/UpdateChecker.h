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

#pragma once

#include <QObject>
#include <QAction>

class QNetworkAccessManager;
class QNetworkReply;
class QWidget;

namespace inputleap {
namespace fork_gui {

/* Checks the fork's rolling `latest-build` GitHub Release for a newer build.
 *
 * The dev-channel release is force-updated by CI on every push to `fork`
 * (see .github/workflows/release.yml). The compiled-in version string
 * (INPUTLEAP_VERSION, e.g. "3.0.3-git-2026-09-10-eb2961d0") carries the
 * commit short-hash; a build is outdated when the release's target commit
 * doesn't start with it.
 */
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QWidget* parentWindow);

    // Check now. When quiet, only speak up if an update is available
    // (used for the rate-limited startup check); manual checks also
    // report "up to date" / errors.
    void check(bool quiet);

    // Startup entry point: checks at most once per day, quietly,
    // shortly after launch.
    void maybeCheckOnStartup();

private:
    void handleReply(QNetworkReply* reply, bool quiet);
    void startSelfUpdate(const QString& assetUrl, const QString& assetName);
    void finishSelfUpdate(const QString& installerPath);
    static QString currentCommitHash();
    static QString platformAssetSuffix();

    QWidget* m_parentWindow;
    QNetworkAccessManager* m_network;
};

// One-line hook for MainWindow: returns a "Check for Updates..." action and
// schedules the rate-limited startup check. The checker parents to
// `parentWindow` for lifetime.
QAction* createUpdateCheckAction(QWidget* parentWindow);

} // namespace fork_gui
} // namespace inputleap
