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

const int W = LayoutModel::kDefaultWidth;
const int H = LayoutModel::kDefaultHeight;

QVariantMap displayMap(int x, int y, int width, int height)
{
    QVariantMap map;
    map.insert(QStringLiteral("x"), x);
    map.insert(QStringLiteral("y"), y);
    map.insert(QStringLiteral("width"), width);
    map.insert(QStringLiteral("height"), height);
    return map;
}

QRect desktopOf(const LayoutModel& layout, const QString& name)
{
    const int i = layout.indexOf(name);
    if (i < 0) return QRect();
    return layout.screens()[i].desktop();
}

} // namespace

TEST(LayoutModel, startsEmpty)
{
    Store store;
    LayoutModel layout(&store.settings);
    EXPECT_EQ(layout.rowCount(), 0);
    EXPECT_EQ(layout.screenCount(), 0);
    EXPECT_TRUE(layout.bounds().isNull());
}

TEST(LayoutModel, placesTheServerAndClientsAroundIt)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    EXPECT_EQ(layout.indexOf(QStringLiteral("desk-win")), 0);
    EXPECT_EQ(desktopOf(layout, QStringLiteral("desk-win")), QRect(0, 0, W, H));

    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("mac-studio"), QStringLiteral("desk-win"), LayoutModel::Right));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("mac-studio")), QRect(W, 0, W, H));
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("macbook"), QStringLiteral("desk-win"), LayoutModel::Left));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")), QRect(-W, 0, W, H));
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("tablet"), QStringLiteral("desk-win"), LayoutModel::Up));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("tablet")), QRect(0, -H, W, H));
    // the next one to the right goes past the one already there
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("mini"), QStringLiteral("desk-win"), LayoutModel::Right));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("mini")), QRect(2 * W, 0, W, H));
    EXPECT_FALSE(layout.addScreen(QStringLiteral("mini"), 0, 5 * H));          // duplicate name
    EXPECT_FALSE(layout.addScreen(QStringLiteral("extra"), W / 2, H / 2));     // on top of the server

    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("mac-studio"))), QStringLiteral("right of desk-win"));
    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("tablet"))), QStringLiteral("above desk-win"));
    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("mini"))), QStringLiteral("right of mac-studio"));
    EXPECT_EQ(layout.positionText(0), QStringLiteral("centre"));
    EXPECT_EQ(layout.bounds(), QRect(-W, -H, 4 * W, 2 * H));

    const QVariantList links = layout.linksOf(0);
    ASSERT_EQ(links.size(), 3);
    EXPECT_TRUE(layout.addScreenAnywhere(QStringLiteral("far")));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("far")), QRect(3 * W, 0, W, H));
}

TEST(LayoutModel, linksCarryRangesWhenEdgesTouchPartly)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    // a 1000x700 tablet against the right edge, starting 500 px down
    EXPECT_TRUE(layout.addScreen(QStringLiteral("tablet"), W, 500));
    QVariantMap settings;
    settings.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 1000, 700)});
    EXPECT_TRUE(layout.setScreenSettings(layout.indexOf(QStringLiteral("tablet")), settings));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("tablet")), QRect(W, 500, 1000, 700));

    const QString conf = layout.configText();
    EXPECT_TRUE(conf.contains(QStringLiteral("\tdesk-win:\n\t\tright(46,100) = tablet(0,83)\n"))) << conf.toStdString();
    EXPECT_TRUE(conf.contains(QStringLiteral("\ttablet:\n\t\tleft(0,83) = desk-win(46,100)\n"))) << conf.toStdString();
    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("tablet"))), QStringLiteral("right of desk-win"));

    // moved away from the edge: no link, and the page can say so
    EXPECT_TRUE(layout.moveTo(layout.indexOf(QStringLiteral("tablet")), W + 10, 500));
    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("tablet"))), QStringLiteral("not linked"));
    EXPECT_FALSE(layout.data(layout.index(1), LayoutModel::LinkedRole).toBool());
    EXPECT_TRUE(layout.data(layout.index(0), LayoutModel::LinkedRole).toBool());
}

TEST(LayoutModel, aComputerIsTheBoundingBoxOfItsDisplays)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    QVariantMap settings;
    settings.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 2560, 1440), displayMap(2560, 200, 1080, 1920)});
    EXPECT_TRUE(layout.setScreenSettings(0, settings));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("desk-win")), QRect(0, 0, 3640, 2120));
    EXPECT_EQ(layout.screenSettings(0).value(QStringLiteral("displays")).toList().size(), 2);

    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("macbook"), QStringLiteral("desk-win"), LayoutModel::Right));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")), QRect(3640, 0, W, H));
    const QString conf = layout.configText();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tright(0,51) = macbook\n"))) << conf.toStdString();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tleft = desk-win(0,51)\n"))) << conf.toStdString();

    // growing the desktop into a neighbour is refused
    settings.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 4000, 1440)});
    EXPECT_FALSE(layout.setScreenSettings(0, settings));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("desk-win")).width(), 3640);

    EXPECT_TRUE(layout.addDisplay(1));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")).width(), 2 * W);
    EXPECT_TRUE(layout.removeDisplay(1, 0));
    EXPECT_FALSE(layout.removeDisplay(1, 0));   // the last display stays
    // the remaining display stays where it was on the desk
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")), QRect(3640 + W, 0, W, H));
}

TEST(LayoutModel, snapsToNearbyEdgesAndRefusesOverlaps)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    EXPECT_TRUE(layout.addScreen(QStringLiteral("macbook"), 3 * W, 0));
    const int mb = layout.indexOf(QStringLiteral("macbook"));

    QVariantMap snapped = layout.snapPosition(mb, W + 30, 40, 100);
    EXPECT_EQ(snapped.value(QStringLiteral("x")).toInt(), W);
    EXPECT_EQ(snapped.value(QStringLiteral("y")).toInt(), 0);
    EXPECT_TRUE(snapped.value(QStringLiteral("snapped")).toBool());

    snapped = layout.snapPosition(mb, 3 * W, 3 * H, 100);   // nothing nearby
    EXPECT_FALSE(snapped.value(QStringLiteral("snapped")).toBool());

    EXPECT_FALSE(layout.moveTo(mb, W / 2, 0));   // onto the server
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")).x(), 3 * W);
    EXPECT_TRUE(layout.moveTo(mb, W, 0));
    EXPECT_EQ(layout.positionText(mb), QStringLiteral("right of desk-win"));
}

TEST(LayoutModel, migratesTheGridOfTheWidgetsGui)
{
    Store store;
    {
        QSettings& s = store.settings;
        s.beginGroup(QStringLiteral("internalConfig"));
        s.setValue(QStringLiteral("numColumns"), 5);
        s.setValue(QStringLiteral("numRows"), 3);
        s.setValue(QStringLiteral("switchDelay"), 300);
        s.beginWriteArray(QStringLiteral("screens"), 15);
        s.setArrayIndex(7);
        s.setValue(QStringLiteral("name"), QStringLiteral("desk-win"));
        s.setArrayIndex(8);
        s.setValue(QStringLiteral("name"), QStringLiteral("mac-studio"));
        s.beginWriteArray(QStringLiteral("aliasArray"));
        s.setArrayIndex(0);
        s.setValue(QStringLiteral("alias"), QStringLiteral("studio.local"));
        s.endArray();
        s.setArrayIndex(2);
        s.setValue(QStringLiteral("name"), QStringLiteral("tablet"));
        s.endArray();
        s.endGroup();
        s.sync();
    }
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    EXPECT_EQ(layout.screenCount(), 3);
    EXPECT_EQ(desktopOf(layout, QStringLiteral("desk-win")), QRect(2 * W, H, W, H));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("mac-studio")), QRect(3 * W, H, W, H));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("tablet")), QRect(2 * W, 0, W, H));
    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("mac-studio"))), QStringLiteral("right of desk-win"));
    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("tablet"))), QStringLiteral("above desk-win"));
    EXPECT_EQ(layout.screenSettings(layout.indexOf(QStringLiteral("mac-studio"))).value(QStringLiteral("aliases")).toStringList(),
              QStringList{QStringLiteral("studio.local")});
    EXPECT_EQ(layout.switchDelay(), 300);

    // saved in the new shape; loads the same way again
    layout.setHasSwitchDelay(true);
    store.settings.sync();
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/layoutVersion")).toInt(), 2);
    // cells migrate in cell order: the tablet (cell 2) comes first
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/screens/1/name")).toString(), QStringLiteral("tablet"));
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/screens/2/name")).toString(), QStringLiteral("desk-win"));
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/screens/2/x")).toInt(), 2 * W);
    LayoutModel again(&store.settings);
    again.setServerName(QStringLiteral("desk-win"));
    EXPECT_EQ(desktopOf(again, QStringLiteral("tablet")), QRect(2 * W, 0, W, H));
    EXPECT_TRUE(again.hasSwitchDelay());
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
    EXPECT_TRUE(layout.setScreenSettings(layout.indexOf(QStringLiteral("mac-studio")), settings));

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

TEST(LayoutModel, roundTripsThroughTheSettings)
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
        settings.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 1440, 900), displayMap(-1440, 0, 1440, 900)});
        EXPECT_TRUE(layout.setScreenSettings(layout.indexOf(QStringLiteral("macbook")), settings));
        layout.setSwitchDelay(300);
        layout.setHasSwitchDelay(true);
        layout.setSwitchCorner(ScreenConfig::TopRight, true);
        layout.setClipboardSharingSizeMb(42);
    }
    store.settings.sync();
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/switchDelay")).toInt(), 300);
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/screens/2/name")).toString(), QStringLiteral("macbook"));
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/screens/2/aliasArray/2/alias")).toString(), QStringLiteral("mb.local"));
    EXPECT_EQ(store.settings.value(QStringLiteral("internalConfig/screens/2/displayArray/2/x")).toInt(), 0);   // normalised

    LayoutModel again(&store.settings);
    again.setServerName(QStringLiteral("desk-win"));
    const int mb = again.indexOf(QStringLiteral("macbook"));
    ASSERT_GE(mb, 0);
    // the second display sits left of the first, so the desktop grew leftwards
    EXPECT_EQ(desktopOf(again, QStringLiteral("macbook")), QRect(-W - 1440, 0, 2880, 900));
    EXPECT_EQ(again.screenSettings(mb).value(QStringLiteral("aliases")).toStringList().size(), 2);
    EXPECT_EQ(again.screenSettings(mb).value(QStringLiteral("cornerSize")).toInt(), 20);
    EXPECT_EQ(again.screenSettings(mb).value(QStringLiteral("displays")).toList().size(), 2);
    EXPECT_TRUE(again.hasSwitchDelay());
    EXPECT_EQ(again.switchDelay(), 300);
    EXPECT_TRUE(again.switchCorner(ScreenConfig::TopRight));
    EXPECT_EQ(again.clipboardSharingSizeMb(), 42);
}

TEST(LayoutModel, renamesAndRemoves)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    EXPECT_TRUE(layout.addScreen(QStringLiteral("a"), W, 0));
    EXPECT_TRUE(layout.renameScreen(0, QStringLiteral("desk-win-2")));
    EXPECT_EQ(layout.serverName(), QStringLiteral("desk-win-2"));
    EXPECT_FALSE(layout.renameScreen(1, QStringLiteral("desk-win-2")));   // taken
    layout.removeScreen(1);
    EXPECT_EQ(layout.screenCount(), 1);
    layout.setLiveState(QStringLiteral("desk-win-2"), QStringLiteral("connected"));
    EXPECT_EQ(layout.data(layout.index(0), LayoutModel::StateRole).toString(), QStringLiteral("connected"));
    // a server renamed from the settings keeps its desktop
    layout.setServerName(QStringLiteral("desk-win-3"));
    EXPECT_EQ(layout.indexOf(QStringLiteral("desk-win-3")), 0);
}
