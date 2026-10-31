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
#include "Utils.h"

#include <QSettings>
#include <QSignalSpy>

#include <gtest/gtest.h>

TEST(AppSettings, readsTheLegacyKeysAndDefaults)
{
    QSettings store(getTemporaryFilename(), QSettings::IniFormat);
    store.setValue(QStringLiteral("groupClientChecked"), true);
    store.setValue(QStringLiteral("groupServerChecked"), false);
    store.setValue(QStringLiteral("screenName"), QStringLiteral("macbook"));
    store.setValue(QStringLiteral("port"), 24801);
    store.setValue(QStringLiteral("autoConfig"), false);
    store.setValue(QStringLiteral("elevateMode"), true);
    store.setValue(QStringLiteral("logLevel"), 4);

    AppSettings settings(&store);
    EXPECT_EQ(settings.role(), AppSettings::Client);
    EXPECT_EQ(settings.screenName(), QStringLiteral("macbook"));
    EXPECT_EQ(settings.port(), 24801);
    EXPECT_FALSE(settings.discovery());
    EXPECT_EQ(settings.elevateMode(), AppSettings::ElevateAlways);
    EXPECT_EQ(settings.logLevelName(), QStringLiteral("DEBUG"));
    EXPECT_TRUE(settings.cryptoEnabled());
    EXPECT_FALSE(settings.requireClientCertificate());
    EXPECT_EQ(settings.scrollStep(), 120);
    EXPECT_EQ(settings.colorScheme(), QStringLiteral("system"));
}

TEST(AppSettings, writesBothLegacyRoleKeysAndKeepsTheWizardQuiet)
{
    QSettings store(getTemporaryFilename(), QSettings::IniFormat);
    AppSettings settings(&store);
    EXPECT_EQ(settings.role(), AppSettings::Server);
    EXPECT_EQ(store.value(QStringLiteral("wizardLastRun")).toInt(), 9);

    QSignalSpy spy(&settings, &AppSettings::changed);
    settings.setRole(AppSettings::Client);
    EXPECT_TRUE(store.value(QStringLiteral("groupClientChecked")).toBool());
    EXPECT_FALSE(store.value(QStringLiteral("groupServerChecked")).toBool());
    settings.setRole(AppSettings::Client);   // no change, no signal
    EXPECT_EQ(spy.count(), 1);

    settings.setElevateMode(AppSettings::ElevateNever);
    EXPECT_EQ(store.value(QStringLiteral("elevateModeEnum")).toInt(), 2);
    EXPECT_FALSE(store.value(QStringLiteral("elevateMode")).toBool());

    settings.setUseExternalConfig(true);
    EXPECT_TRUE(store.value(QStringLiteral("useExternalConfig")).toBool());
    EXPECT_FALSE(store.value(QStringLiteral("useInternalConfig")).toBool());
}

TEST(AppSettings, clampsAndTrims)
{
    QSettings store(getTemporaryFilename(), QSettings::IniFormat);
    AppSettings settings(&store);
    settings.setPort(99999);
    EXPECT_EQ(settings.port(), 65535);
    settings.setLogLevel(42);
    EXPECT_EQ(settings.logLevel(), 6);
    settings.setScreenName(QStringLiteral("  desk-win  "));
    EXPECT_EQ(settings.screenName(), QStringLiteral("desk-win"));
    settings.setScreenName(QString());
    EXPECT_FALSE(settings.screenName().isEmpty());   // falls back to the host name
}

TEST(AppSettings, keepsPeerNamesBesideTheFingerprints)
{
    QSettings store(getTemporaryFilename(), QSettings::IniFormat);
    AppSettings settings(&store);
    settings.setPeerName(QStringLiteral("3f:9a:71"), QStringLiteral("mac-studio"));
    EXPECT_EQ(settings.peerName(QStringLiteral("3F:9A:71")), QStringLiteral("mac-studio"));
    EXPECT_TRUE(settings.peerName(QStringLiteral("00:00")).isEmpty());
}
