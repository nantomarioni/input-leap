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

#include "LayoutModel.h"
#include "Utils.h"

#include <QSettings>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace {

struct Store
{
    QString path = getTemporaryFilename();
    QSettings settings{path, QSettings::IniFormat};
};

} // namespace

TEST(LayoutModel, startsEmptyWithTheLegacyGrid)
{
    Store store;
    LayoutModel layout(&store.settings);
    EXPECT_EQ(layout.columns(), 5);
    EXPECT_EQ(layout.rows(), 3);
    EXPECT_EQ(layout.rowCount(), 15);
    EXPECT_EQ(layout.screenCount(), 0);
}

TEST(LayoutModel, placesTheServerInTheCentreAndClientsAroundIt)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    EXPECT_EQ(layout.cellOf(QStringLiteral("desk-win")), 7);

    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("mac-studio"), QStringLiteral("desk-win"), LayoutModel::Right));
    EXPECT_EQ(layout.cellOf(QStringLiteral("mac-studio")), 8);
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("macbook"), QStringLiteral("desk-win"), LayoutModel::Left));
    EXPECT_EQ(layout.cellOf(QStringLiteral("macbook")), 6);
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("tablet"), QStringLiteral("desk-win"), LayoutModel::Up));
    EXPECT_EQ(layout.cellOf(QStringLiteral("tablet")), 2);
    // the next one to the right skips the occupied cell
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("mini"), QStringLiteral("desk-win"), LayoutModel::Right));
    EXPECT_EQ(layout.cellOf(QStringLiteral("mini")), 9);
    // no room further right
    EXPECT_FALSE(layout.placeNextTo(QStringLiteral("extra"), QStringLiteral("desk-win"), LayoutModel::Right));
    EXPECT_FALSE(layout.addScreen(QStringLiteral("mini"), 0));   // duplicate name

    EXPECT_EQ(layout.positionText(8), QStringLiteral("right of desk-win"));
    EXPECT_EQ(layout.positionText(2), QStringLiteral("above desk-win"));
    EXPECT_EQ(layout.positionText(7), QStringLiteral("centre"));
    EXPECT_EQ(layout.neighbour(7, LayoutModel::Right), QStringLiteral("mac-studio"));
}

TEST(LayoutModel, writesTheConfTheCoreReads)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    layout.placeNextTo(QStringLiteral("mac-studio"), QStringLiteral("desk-win"), LayoutModel::Right);

    QVariantMap settings;
    settings.insert(QStringLiteral("aliases"), QStringList{QStringLiteral("mac-studio.local")});
    settings.insert(QStringLiteral("modifiers"), QVariantList{ScreenConfig::Ctrl, ScreenConfig::Ctrl, ScreenConfig::Alt, ScreenConfig::Meta, ScreenConfig::Super});
    settings.insert(QStringLiteral("corners"), QVariantList{true, false, false, true});
    settings.insert(QStringLiteral("cornerSize"), 50);
    settings.insert(QStringLiteral("fixes"), QVariantList{true, false, false, false, false});
    EXPECT_TRUE(layout.setScreenSettings(layout.cellOf(QStringLiteral("mac-studio")), settings));

    layout.setHasHeartbeat(true);
    layout.setHeartbeat(4000);
    layout.setDimmingEnabled(true);
    layout.setDimmingPercentage(25);

    const QString conf = layout.configText();
    EXPECT_TRUE(conf.contains(QStringLiteral("section: screens\n\tdesk-win:\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("\tmac-studio:\n\t\tshift = ctrl\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\thalfDuplexCapsLock = true\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tswitchCorners = none +top-left +bottom-right \n\t\tswitchCornerSize = 50\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("section: aliases\n\tmac-studio:\n\t\tmac-studio.local\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("\tdesk-win:\n\t\tright = mac-studio\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("\tmac-studio:\n\t\tleft = desk-win\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("section: options\n\theartbeat = 4000\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("\tclipboardSharingSize = 100000000\n")));
    EXPECT_TRUE(conf.contains(QStringLiteral("\tscreenDimmingEnabled = true\n\tscreenDimmingPercentage = 25\n")));
}

TEST(LayoutModel, roundTripsThroughTheLegacySettingsKeys)
{
    Store store;
    {
        LayoutModel layout(&store.settings);
        layout.setServerName(QStringLiteral("desk-win"));
        layout.ensureServerPlaced();
        layout.placeNextTo(QStringLiteral("macbook"), QStringLiteral("desk-win"), LayoutModel::Left);
        QVariantMap settings;
        settings.insert(QStringLiteral("aliases"), QStringList{QStringLiteral("mb"), QStringLiteral("mb.local")});
        settings.insert(QStringLiteral("cornerSize"), 20);
        layout.setScreenSettings(layout.cellOf(QStringLiteral("macbook")), settings);
        layout.setSwitchDelay(300);
        layout.setHasSwitchDelay(true);
        layout.setSwitchCorner(ScreenConfig::TopRight, true);
        layout.setClipboardSharingSizeMb(42);
    }
    store.settings.sync();
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/numColumns")).toInt(), 5);
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/switchDelay")).toInt(), 300);
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/screens/8/name")).toString(), QStringLiteral("desk-win"));
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/screens/7/aliasArray/2/alias")).toString(), QStringLiteral("mb.local"));

    LayoutModel again(&store.settings);
    again.setServerName(QStringLiteral("desk-win"));
    EXPECT_EQ(again.cellOf(QStringLiteral("macbook")), 6);
    EXPECT_EQ(again.screenSettings(6).value(QStringLiteral("aliases")).toStringList().size(), 2);
    EXPECT_EQ(again.screenSettings(6).value(QStringLiteral("cornerSize")).toInt(), 20);
    EXPECT_TRUE(again.hasSwitchDelay());
    EXPECT_EQ(again.switchDelay(), 300);
    EXPECT_TRUE(again.switchCorner(ScreenConfig::TopRight));
    EXPECT_EQ(again.clipboardSharingSizeMb(), 42);
}

TEST(LayoutModel, movesRenamesAndRemoves)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    layout.addScreen(QStringLiteral("a"), 0);
    EXPECT_TRUE(layout.moveScreen(0, 1));
    EXPECT_EQ(layout.cellOf(QStringLiteral("a")), 1);
    EXPECT_TRUE(layout.moveScreen(1, 7));   // swaps with the server
    EXPECT_EQ(layout.cellOf(QStringLiteral("desk-win")), 1);
    EXPECT_TRUE(layout.renameScreen(1, QStringLiteral("desk-win-2")));
    EXPECT_EQ(layout.serverName(), QStringLiteral("desk-win-2"));
    EXPECT_FALSE(layout.renameScreen(7, QStringLiteral("desk-win-2")));   // taken
    layout.removeScreen(7);
    EXPECT_EQ(layout.screenCount(), 1);
    layout.setLiveState(QStringLiteral("desk-win-2"), QStringLiteral("connected"));
    EXPECT_EQ(layout.data(layout.index(1), LayoutModel::StateRole).toString(), QStringLiteral("connected"));
}
