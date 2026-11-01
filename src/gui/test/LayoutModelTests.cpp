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

#include "Hotkey.h"
#include "LayoutModel.h"
#include "Utils.h"

#include <QJsonArray>
#include <QJsonObject>
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
    EXPECT_TRUE(conf.contains(QStringLiteral("\tdesk-win:\n\t\tright(46.3,100) = tablet(0,82.86)\n"))) << conf.toStdString();
    EXPECT_TRUE(conf.contains(QStringLiteral("\ttablet:\n\t\tleft(0,82.86) = desk-win(46.3,100)\n"))) << conf.toStdString();
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
    // only the portrait display reaches the right edge, from 200 px down: the
    // corner above it is a wall, not a way onto the macbook
    const QString conf = layout.configText();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tright(9.43,50.94) = macbook(18.52,100)\n"))) << conf.toStdString();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tleft(18.52,100) = desk-win(9.43,50.94)\n"))) << conf.toStdString();

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

TEST(LayoutModel, mirrorsItselfThroughJson)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    QVariantMap settings;
    settings.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 2560, 1440), displayMap(2560, 0, 1080, 1920)});
    settings.insert(QStringLiteral("aliases"), QStringList{QStringLiteral("dw.local")});
    settings.insert(QStringLiteral("corners"), QVariantList{true, false, false, false});
    EXPECT_TRUE(layout.setScreenSettings(0, settings));
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("macbook"), QStringLiteral("desk-win"), LayoutModel::Right));
    layout.setSwitchDelay(300);
    layout.setHasSwitchDelay(true);
    layout.setDimmingPercentage(40);
    layout.setSwitchCorner(ScreenConfig::BottomLeft, true);

    const QJsonObject json = layout.toJson();
    EXPECT_EQ(json.value(QStringLiteral("screens")).toArray().size(), 2);

    LayoutModel mirror(nullptr);
    EXPECT_TRUE(mirror.isMirror());
    mirror.loadJson(json);
    EXPECT_EQ(mirror.serverName(), QStringLiteral("desk-win"));
    EXPECT_EQ(mirror.screenCount(), 2);
    EXPECT_EQ(desktopOf(mirror, QStringLiteral("desk-win")), QRect(0, 0, 3640, 1920));
    EXPECT_EQ(desktopOf(mirror, QStringLiteral("macbook")), QRect(3640, 0, W, H));
    EXPECT_EQ(mirror.screenSettings(0).value(QStringLiteral("aliases")).toStringList(), QStringList{QStringLiteral("dw.local")});
    EXPECT_TRUE(mirror.screenSettings(0).value(QStringLiteral("corners")).toList().value(0).toBool());
    EXPECT_EQ(mirror.switchDelay(), 300);
    EXPECT_TRUE(mirror.hasSwitchDelay());
    EXPECT_EQ(mirror.dimmingPercentage(), 40);
    EXPECT_TRUE(mirror.switchCorner(ScreenConfig::BottomLeft));
    EXPECT_EQ(mirror.configText(), layout.configText());
    EXPECT_EQ(mirror.positionText(1), QStringLiteral("right of desk-win"));
}

TEST(LayoutModel, readsAConfigurationFileOfTheCoresFormat)
{
    const QString text = QStringLiteral(
        "# the fleet's hand-written file\n"
        "section: screens\n"
        "\tDeepPurple:\n"
        "\tMacBookPro-R:\n"
        "\tMacBookPro-P:\n"
        "\t\tshift = ctrl\n"
        "\t\thalfDuplexCapsLock = true\n"
        "\t\tswitchCorners = none +top-left\n"
        "\tMacBookPro-A:\n"
        "end\n"
        "section: links\n"
        "\tDeepPurple:\n"
        "\t\tright = MacBookPro-R\n"
        "\t\tdown(10,60) = MacBookPro-P(0,100)\n"
        "\t\tleft = MacBookPro-A\n"
        "\tMacBookPro-P:\n"
        "\t\tup = DeepPurple(10,60)\n"
        "\tMacBookPro-R:\n"
        "\t\tleft = DeepPurple\n"
        "\tMacBookPro-A:\n"
        "\t\tright = DeepPurple\n"
        "end\n"
        "section: aliases\n"
        "\tMacBookPro-P:\n"
        "\t\tmacbookpro-p.local\n"
        "end\n"
        "section: options\n"
        "\tswitchDelay = 300\n"
        "\tscreenDimmingEnabled = true\n"
        "\tscreenDimmingPercentage = 50\n"
        "\tkeystroke(super+alt+l) = lockCursorToScreen(toggle)\n"
        "end\n");
    LayoutModel layout(nullptr);
    layout.loadConfigText(text, QStringLiteral("DEEPPURPLE"));   // the server's own name, any case

    EXPECT_EQ(layout.screenCount(), 4);
    EXPECT_EQ(layout.serverName(), QStringLiteral("DeepPurple"));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("DeepPurple")), QRect(0, 0, W, H));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("MacBookPro-R")), QRect(W, 0, W, H));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("MacBookPro-A")), QRect(-W, 0, W, H));
    // half as wide as the server (10-60 % of its bottom edge), starting at 10 %
    EXPECT_EQ(desktopOf(layout, QStringLiteral("MacBookPro-P")), QRect(W / 10, H, W / 2, H));
    EXPECT_EQ(layout.indexOf(QStringLiteral("macbookpro-p")), layout.indexOf(QStringLiteral("MacBookPro-P")));
    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("MacBookPro-P"))), QStringLiteral("below DeepPurple"));
    EXPECT_EQ(layout.positionText(layout.indexOf(QStringLiteral("MacBookPro-A"))), QStringLiteral("left of DeepPurple"));

    const QVariantMap p = layout.screenSettings(layout.indexOf(QStringLiteral("MacBookPro-P")));
    EXPECT_EQ(p.value(QStringLiteral("modifiers")).toList().value(ScreenConfig::Shift).toInt(), int(ScreenConfig::Ctrl));
    EXPECT_TRUE(p.value(QStringLiteral("fixes")).toList().value(ScreenConfig::CapsLock).toBool());
    EXPECT_TRUE(p.value(QStringLiteral("corners")).toList().value(ScreenConfig::TopLeft).toBool());
    EXPECT_EQ(p.value(QStringLiteral("aliases")).toStringList(), QStringList{QStringLiteral("macbookpro-p.local")});
    EXPECT_TRUE(layout.hasSwitchDelay());
    EXPECT_EQ(layout.switchDelay(), 300);
    EXPECT_TRUE(layout.dimmingEnabled());
    EXPECT_EQ(layout.dimmingPercentage(), 50);
    ASSERT_EQ(layout.hotkeyList().size(), 1);
    EXPECT_EQ(layout.hotkeyList().first().toMap().value(QStringLiteral("key")).toString(), QStringLiteral("keystroke(super+alt+l)"));
    // the geometry gives the ranges back
    const QString conf = layout.configText();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tdown(10,60) = MacBookPro-P\n"))) << conf.toStdString();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tup = DeepPurple(10,60)\n"))) << conf.toStdString();
}

// The file has no sizes: the reports fill them in, before the file is read
// (kept for every reload) and after it (applied live).
TEST(LayoutModel, aFileLayoutTakesTheDisplaysTheComputersReport)
{
    const QString text = QStringLiteral(
        "section: screens\n"
        "\tDeepPurple:\n"
        "\tMacBookPro-R:\n"
        "\tMacBookPro-P:\n"
        "end\n"
        "section: links\n"
        "\tDeepPurple:\n"
        "\t\tright = MacBookPro-R\n"
        "\t\tdown = MacBookPro-P\n"
        "\tMacBookPro-R:\n"
        "\t\tleft = DeepPurple\n"
        "\tMacBookPro-P:\n"
        "\t\tup = DeepPurple\n"
        "end\n");
    LayoutModel layout(nullptr);
    const DisplayConfig monitor{0, 0, 2560, 1440, 597, 336};
    EXPECT_FALSE(layout.reportDisplays(QStringLiteral("DeepPurple"), {monitor, DisplayConfig{2560, 0, 2560, 1440, 597, 336}}));
    EXPECT_FALSE(layout.reportDisplays(QStringLiteral("MacBooKPro-R"), {DisplayConfig{0, 0, 1512, 982}}));   // any case
    layout.loadConfigText(text, QStringLiteral("DeepPurple"));

    const int server = layout.indexOf(QStringLiteral("DeepPurple"));
    ASSERT_GE(server, 0);
    EXPECT_EQ(layout.screens()[server].displays.size(), 2);
    EXPECT_EQ(layout.screens()[server].nativeSize(), QSize(5120, 1440));
    EXPECT_EQ(layout.screens()[layout.indexOf(QStringLiteral("MacBookPro-R"))].nativeSize(), QSize(1512, 982));
    const QRect desk = desktopOf(layout, QStringLiteral("DeepPurple"));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("MacBookPro-R")).x(), desk.x() + desk.width());
    EXPECT_EQ(desktopOf(layout, QStringLiteral("MacBookPro-P")).y(), desk.y() + desk.height());

    EXPECT_TRUE(layout.reportDisplays(QStringLiteral("MacBookPro-P"), {DisplayConfig{0, 0, 1512, 982}}));
    EXPECT_EQ(layout.screens()[layout.indexOf(QStringLiteral("MacBookPro-P"))].nativeSize(), QSize(1512, 982));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("MacBookPro-P")).y(), desk.y() + desk.height());

    layout.loadConfigText(text, QStringLiteral("DeepPurple"));   // the file saved again
    EXPECT_EQ(layout.screens()[layout.indexOf(QStringLiteral("MacBookPro-P"))].nativeSize(), QSize(1512, 982));
}

// The configuration file option is gone: what the file had moves into the
// layout once, around wherever the server already sits.
TEST(LayoutModel, adoptsTheComputersOfAConfigurationFile)
{
    LayoutModel fromFile(nullptr);
    fromFile.loadConfigText(QStringLiteral(
        "section: screens\n\tdesk-win:\n\tmacbook:\n\t\talt = super\n\tstudio:\nend\n"
        "section: links\n\tdesk-win:\n\t\tright = macbook\n\t\tdown = studio\nend\n"), QStringLiteral("desk-win"));

    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    EXPECT_TRUE(layout.addScreen(QStringLiteral("desk-win"), 3840, 1080));
    EXPECT_TRUE(layout.addScreen(QStringLiteral("STUDIO"), 3840, 1080 + H));   // already here, any case

    EXPECT_EQ(layout.adoptScreens(fromFile), 1);
    EXPECT_EQ(layout.screenCount(), 3);
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")).topLeft(), QPoint(3840 + W, 1080));
    const QVariantMap mac = layout.screenSettings(layout.indexOf(QStringLiteral("macbook")));
    EXPECT_EQ(mac.value(QStringLiteral("modifiers")).toList().value(ScreenConfig::Alt).toInt(), int(ScreenConfig::Super));
    EXPECT_EQ(layout.adoptScreens(fromFile), 0);
}

// A monitor moved from the Mac to the PC and back: each combination keeps
// its own arrangement, and it survives a restart.
TEST(LayoutModel, keepsOneArrangementPerSetupOfMonitors)
{
    Store store;
    const DisplayConfig left{0, 0, 1920, 1080}, right{1920, 0, 1920, 1080};
    const DisplayConfig laptop{0, 0, 1512, 982}, external{1512, 0, 1920, 1080};
    {
        LayoutModel layout(&store.settings);
        layout.setServerName(QStringLiteral("pc"));
        layout.ensureServerPlaced();
        layout.reportDisplays(QStringLiteral("pc"), {left});
        layout.reportDisplays(QStringLiteral("mac"), {laptop, external});
        ASSERT_TRUE(layout.addScreen(QStringLiteral("mac"), 0, 1080));   // below
        const int mac = layout.indexOf(QStringLiteral("mac"));
        EXPECT_EQ(layout.setupCount(), 1);

        // the external monitor goes to the PC: the Mac first, then the PC
        EXPECT_TRUE(layout.reportDisplays(QStringLiteral("mac"), {laptop}));
        EXPECT_TRUE(layout.reportDisplays(QStringLiteral("pc"), {left, right}));
        EXPECT_EQ(layout.activeSetupNumber(), 1);   // the newest
        EXPECT_TRUE(layout.moveTo(mac, 1920, 1080));   // under the second monitor
        const int setups = layout.setupCount();
        EXPECT_GE(setups, 2);

        // and back: the first arrangement returns, with the Mac at the left
        EXPECT_TRUE(layout.reportDisplays(QStringLiteral("pc"), {left}));
        EXPECT_TRUE(layout.reportDisplays(QStringLiteral("mac"), {laptop, external}));
        EXPECT_EQ(desktopOf(layout, QStringLiteral("mac")).topLeft(), QPoint(0, 1080));
        EXPECT_EQ(layout.screens()[mac].displays.size(), 2);

        // and over again: the second one
        EXPECT_TRUE(layout.reportDisplays(QStringLiteral("mac"), {laptop}));
        EXPECT_TRUE(layout.reportDisplays(QStringLiteral("pc"), {left, right}));
        EXPECT_EQ(desktopOf(layout, QStringLiteral("mac")).topLeft(), QPoint(1920, 1080));
        EXPECT_EQ(layout.setupCount(), setups);   // nothing new was stored
    }
    LayoutModel reloaded(&store.settings);
    reloaded.load();
    EXPECT_GE(reloaded.setupCount(), 2);
    EXPECT_TRUE(reloaded.reportDisplays(QStringLiteral("pc"), {left}));
    EXPECT_TRUE(reloaded.reportDisplays(QStringLiteral("mac"), {laptop, external}));
    EXPECT_EQ(desktopOf(reloaded, QStringLiteral("mac")).topLeft(), QPoint(0, 1080));
}

// Two laptops each centred under one monitor of the PC: a gap between them,
// bridged by hand, both ways; only the free part of an edge is used.
TEST(LayoutModel, linksAcrossAGap)
{
    Store store;
    {
        LayoutModel layout(&store.settings);
        layout.setServerName(QStringLiteral("pc"));
        layout.reportDisplays(QStringLiteral("pc"), {DisplayConfig{0, 0, 1920, 1080}, DisplayConfig{1920, 0, 1920, 1080}});
        layout.reportDisplays(QStringLiteral("mac1"), {DisplayConfig{0, 0, 1512, 982}});
        layout.reportDisplays(QStringLiteral("mac2"), {DisplayConfig{0, 0, 1512, 982}});
        ASSERT_TRUE(layout.addScreen(QStringLiteral("pc"), 0, 0));
        ASSERT_TRUE(layout.addScreen(QStringLiteral("mac1"), 204, 1080));
        ASSERT_TRUE(layout.addScreen(QStringLiteral("mac2"), 1920 + 204, 1080));
        const int mac1 = layout.indexOf(QStringLiteral("mac1")), mac2 = layout.indexOf(QStringLiteral("mac2"));

        EXPECT_FALSE(layout.configText().contains(QStringLiteral("= mac2\n"))
                     && layout.configText().contains(QStringLiteral("right = mac2")));
        EXPECT_TRUE(layout.addGapLink(mac1, LayoutModel::Right, mac2));
        EXPECT_FALSE(layout.addGapLink(mac2, LayoutModel::Left, mac1));   // the same link, seen from the other side
        const QString conf = layout.configText();
        EXPECT_TRUE(conf.contains(QStringLiteral("\t\tright = mac2\n"))) << conf.toStdString();
        EXPECT_TRUE(conf.contains(QStringLiteral("\t\tleft = mac1\n"))) << conf.toStdString();
        EXPECT_EQ(layout.gapLinksOf(mac2).size(), 1);
        EXPECT_EQ(layout.gapLinksOf(mac2).first().toMap().value(QStringLiteral("side")).toInt(), int(LayoutModel::Left));
        EXPECT_EQ(layout.gapCrossings().size(), 1);
        EXPECT_EQ(layout.crossings().size(), 2);   // the touching ones only: each laptop under the PC
    }
    LayoutModel reloaded(&store.settings);
    reloaded.load();
    EXPECT_TRUE(reloaded.configText().contains(QStringLiteral("\t\tright = mac2\n")));
    EXPECT_TRUE(reloaded.renameScreen(reloaded.indexOf(QStringLiteral("mac2")), QStringLiteral("studio")));
    EXPECT_TRUE(reloaded.configText().contains(QStringLiteral("\t\tright = studio\n")));
    reloaded.removeScreen(reloaded.indexOf(QStringLiteral("studio")));
    EXPECT_TRUE(reloaded.gapLinksOf(reloaded.indexOf(QStringLiteral("mac1"))).isEmpty());
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

TEST(LayoutModel, takesTheDisplaysTheComputersReport)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("macbook"), QStringLiteral("desk-win"), LayoutModel::Right));
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("below"), QStringLiteral("desk-win"), LayoutModel::Down));

    // the server's two monitors, the second left of the primary (negative x):
    // the desktop grows and the computer on its right moves along
    DisplayConfig left{-1920, 0, 1920, 1080}, primary{0, 0, 3840, 2160};
    EXPECT_TRUE(layout.reportDisplays(QStringLiteral("desk-win"), {primary, left}));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("desk-win")), QRect(0, 0, 5760, 2160));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")), QRect(5760, 0, W, H));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("below")), QRect(0, 2160, W, H));
    const QVariantMap server = layout.screenSettings(0);
    EXPECT_TRUE(server.value(QStringLiteral("displaysReported")).toBool());
    EXPECT_FALSE(server.value(QStringLiteral("reportDiffers")).toBool());
    EXPECT_EQ(server.value(QStringLiteral("displays")).toList().value(1).toMap().value(QStringLiteral("x")).toInt(), 0);

    // the same report again changes nothing
    EXPECT_FALSE(layout.reportDisplays(QStringLiteral("desk-win"), {primary, left}));

    // typed by hand: a report is only offered
    const int mac = layout.indexOf(QStringLiteral("macbook"));
    QVariantMap typed;
    typed.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 1440, 900)});
    EXPECT_TRUE(layout.setScreenSettings(mac, typed));
    EXPECT_FALSE(layout.reportDisplays(QStringLiteral("macbook"), {DisplayConfig{0, 0, 1512, 982}}));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")).size(), QSize(1440, 900));
    EXPECT_TRUE(layout.screenSettings(mac).value(QStringLiteral("reportDiffers")).toBool());
    EXPECT_TRUE(layout.useReportedDisplays(mac));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")), QRect(5760, 0, 1512, 982));
    EXPECT_FALSE(layout.screenSettings(mac).value(QStringLiteral("reportDiffers")).toBool());

    // a computer that reported before it was placed starts with its displays
    EXPECT_FALSE(layout.reportDisplays(QStringLiteral("studio"), {DisplayConfig{0, 0, 2560, 1440}}));
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("studio"), QStringLiteral("macbook"), LayoutModel::Right));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("studio")), QRect(5760 + 1512, 0, 2560, 1440));

    // persisted: reopened, the server's displays still follow reports
    LayoutModel reopened(&store.settings);
    EXPECT_TRUE(reopened.screenSettings(reopened.indexOf(QStringLiteral("desk-win"))).value(QStringLiteral("displaysReported")).toBool());
}

TEST(LayoutModel, aShrinkingDesktopLeavesItsNeighboursWhenPullingThemWouldCollide)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    // a wide computer above-right of the server, and one beyond it that also
    // reaches down beside the server: pulling that one left would hit the first
    EXPECT_TRUE(layout.addScreen(QStringLiteral("above"), W / 2, -H));
    QVariantMap wide;
    wide.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 2 * W, H)});
    EXPECT_TRUE(layout.setScreenSettings(1, wide));
    EXPECT_TRUE(layout.addScreen(QStringLiteral("beyond"), W / 2 + 2 * W, -H / 2));
    EXPECT_TRUE(layout.reportDisplays(QStringLiteral("desk-win"), {DisplayConfig{0, 0, W / 2, H}}));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("desk-win")), QRect(0, 0, W / 2, H));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("beyond")).topLeft(), QPoint(W / 2 + 2 * W, -H / 2));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("above")).topLeft(), QPoint(W / 2, -H));
}

TEST(LayoutModel, readsDisplayReportsDefensively)
{
    const QVector<DisplayConfig> two = {DisplayConfig{0, 0, 3024, 1964}, DisplayConfig{3024, -200, 2560, 1440}};
    const QString text = LayoutModel::displaysToText(two);
    EXPECT_EQ(text, QStringLiteral("0,0,3024x1964;3024,-200,2560x1440"));
    // the bounding box moves to (0, 0)
    const QVector<DisplayConfig> back = LayoutModel::displaysFromText(text);
    ASSERT_EQ(back.size(), 2);
    EXPECT_EQ(back[0], (DisplayConfig{0, 200, 3024, 1964}));
    EXPECT_EQ(back[1], (DisplayConfig{3024, 0, 2560, 1440}));
    EXPECT_EQ(LayoutModel::displaysFromJson(LayoutModel::displaysToJson(back)), back);

    EXPECT_TRUE(LayoutModel::displaysFromText(QStringLiteral("garbage;1,2,0x5;")).isEmpty());
    EXPECT_EQ(LayoutModel::displaysFromText(QStringLiteral("0,0,9999999x10")).size(), 0);   // too many digits
    QStringList many;
    for (int i = 0; i < 20; ++i) many << QStringLiteral("%1,0,10x10").arg(i * 10);
    EXPECT_EQ(LayoutModel::displaysFromText(many.join(QLatin1Char(';'))).size(), LayoutModel::kMaxReportedDisplays);
    QJsonArray huge;
    QJsonObject big;
    big.insert(QStringLiteral("width"), 5000000);
    big.insert(QStringLiteral("height"), -1);
    huge << big;
    EXPECT_TRUE(LayoutModel::displaysFromJson(huge).isEmpty());
    big.insert(QStringLiteral("height"), 10);
    huge = QJsonArray{big};
    EXPECT_EQ(LayoutModel::displaysFromJson(huge).value(0).width, 100000);
}

TEST(LayoutModel, aClientEditsTheServersConfigurationThroughJson)
{
    Store store;
    LayoutModel server(&store.settings);
    server.setServerName(QStringLiteral("desk-win"));
    server.ensureServerPlaced();
    EXPECT_TRUE(server.placeNextTo(QStringLiteral("macbook"), QStringLiteral("desk-win"), LayoutModel::Right));
    KeySequence keys;
    keys.appendKey(Qt::Key_F1, Qt::ControlModifier);
    const int hotkey = server.addHotkey(keys);
    ASSERT_EQ(hotkey, 0);
    Action action;
    action.setType(Action::switchToScreen);
    action.setSwitchScreenName(QStringLiteral("macbook"));
    EXPECT_TRUE(server.addAction(hotkey, action));

    // the client's copy: loading never counts as an edit
    LayoutModel mirror(nullptr);
    int edits = 0;
    QObject::connect(&mirror, &LayoutModel::edited, [&]() { ++edits; });
    mirror.loadJson(server.toJson());
    EXPECT_EQ(edits, 0);
    ASSERT_EQ(mirror.hotkeys().size(), 1u);   // whole hotkeys, not just their text
    EXPECT_EQ(mirror.hotkeys()[0].text(), server.hotkeys()[0].text());

    // edited there: moved, an option, a hotkey removed
    EXPECT_TRUE(mirror.moveTo(mirror.indexOf(QStringLiteral("macbook")), -W, 0));
    mirror.setSwitchDelay(400);
    mirror.removeHotkey(0);
    EXPECT_EQ(edits, 3);

    // and taken by the server, which keeps its own name and saves it
    int serverEdits = 0;
    QObject::connect(&server, &LayoutModel::edited, [&]() { ++serverEdits; });
    QString error;
    EXPECT_TRUE(server.replaceWith(mirror.toJson(), &error)) << error.toStdString();
    EXPECT_EQ(serverEdits, 1);
    EXPECT_EQ(server.serverName(), QStringLiteral("desk-win"));
    EXPECT_EQ(desktopOf(server, QStringLiteral("macbook")).topLeft(), QPoint(-W, 0));
    EXPECT_EQ(server.switchDelay(), 400);
    EXPECT_TRUE(server.hotkeys().empty());
    LayoutModel reopened(&store.settings);
    EXPECT_EQ(reopened.switchDelay(), 400);
    EXPECT_EQ(desktopOf(reopened, QStringLiteral("macbook")).topLeft(), QPoint(-W, 0));

    // what is not a configuration changes nothing
    QJsonObject bad = mirror.toJson();
    QJsonArray screens = bad.value(QStringLiteral("screens")).toArray();
    QJsonObject evil = screens[0].toObject();
    evil.insert(QStringLiteral("name"), QStringLiteral("bad name; with spaces"));
    screens[0] = evil;
    bad.insert(QStringLiteral("screens"), screens);
    EXPECT_FALSE(server.replaceWith(bad, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(server.replaceWith(QJsonObject(), &error));
    EXPECT_EQ(server.screenCount(), 2);
    EXPECT_EQ(serverEdits, 1);
}

TEST(LayoutModel, hotkeysRoundTripThroughJson)
{
    KeySequence keys;
    keys.appendKey(Qt::Key_L, Qt::ControlModifier | Qt::AltModifier);
    Hotkey hotkey;
    hotkey.setKeySequence(keys);
    Action lock;
    lock.setType(Action::lockCursorToScreen);
    lock.setLockCursorMode(Action::lockCursonOn);
    lock.setActiveOnRelease(true);
    hotkey.appendAction(lock);
    Action type;
    type.setType(Action::keystroke);
    type.setKeySequence(keys);
    type.setHaveScreens(true);
    type.appendTypeScreenName(QStringLiteral("macbook"));
    hotkey.appendAction(type);

    Hotkey back;
    back.loadJson(hotkey.toJson());
    EXPECT_EQ(back.text(), hotkey.text());
    ASSERT_EQ(back.actions().size(), 2u);
    EXPECT_EQ(back.actions()[0].text(), lock.text());
    EXPECT_EQ(back.actions()[1].text(), type.text());
    EXPECT_TRUE(back.actions()[0].activeOnRelease());
    EXPECT_EQ(back.actions()[1].typeScreenNames(), QStringList{QStringLiteral("macbook")});

    // out-of-range values from the wire are clamped
    QJsonObject wild = lock.toJson();
    wild.insert(QStringLiteral("type"), 999);
    Action clamped;
    clamped.loadJson(wild);
    EXPECT_EQ(clamped.type(), int(Action::mousebutton));
}

TEST(LayoutModel, edgesNoDisplayReachesAreWalls)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    // a 1440p monitor and a shorter 1080p one beside it, tops aligned: the
    // bottom edge is only real under the first
    QVariantMap two;
    two.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 2560, 1440), displayMap(2560, 0, 1920, 1080)});
    EXPECT_TRUE(layout.setScreenSettings(0, two));
    QVariantMap mac;
    mac.insert(QStringLiteral("displays"), QVariantList{displayMap(0, 0, 1512, 982)});

    // under the tall monitor: the macbook's whole top edge, onto that part
    EXPECT_TRUE(layout.addScreen(QStringLiteral("macbook"), 500, 1440));
    const int m = layout.indexOf(QStringLiteral("macbook"));
    EXPECT_TRUE(layout.setScreenSettings(m, mac));
    QString conf = layout.configText();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tdown(11.16,44.91) = macbook\n"))) << conf.toStdString();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tup = desk-win(11.16,44.91)\n"))) << conf.toStdString();

    // half under each: only the part under the tall one crosses
    EXPECT_TRUE(layout.moveTo(m, 2000, 1440));
    conf = layout.configText();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tdown(44.64,57.14) = macbook(0,37.04)\n"))) << conf.toStdString();
    const QVariantList crossings = layout.crossings();
    ASSERT_EQ(crossings.size(), 1);
    const QVariantMap line = crossings[0].toMap();
    EXPECT_EQ(QLine(line.value(QStringLiteral("x1")).toInt(), line.value(QStringLiteral("y1")).toInt(),
                    line.value(QStringLiteral("x2")).toInt(), line.value(QStringLiteral("y2")).toInt()),
              QLine(2000, 1440, 2560, 1440));

    // under the short one only: it touches the desktop but no display
    EXPECT_TRUE(layout.moveTo(m, 3000, 1440));
    EXPECT_EQ(layout.positionText(m), QStringLiteral("not linked"));
    EXPECT_FALSE(layout.configText().contains(QStringLiteral("= macbook")));

    // the canvas gets the displays in the shared space, from the desktop's corner
    const QVariantList drawn = layout.data(layout.index(0), LayoutModel::DisplaysRole).toList();
    ASSERT_EQ(drawn.size(), 2);
    EXPECT_EQ(drawn[1].toMap().value(QStringLiteral("x")).toInt(), 2560);
    EXPECT_EQ(drawn[1].toMap().value(QStringLiteral("height")).toInt(), 1080);
}

TEST(LayoutModel, drawsEachComputerAtItsRealSize)
{
    Store store;
    LayoutModel layout(&store.settings);
    layout.setServerName(QStringLiteral("desk-win"));
    layout.ensureServerPlaced();
    EXPECT_TRUE(layout.placeNextTo(QStringLiteral("macbook"), QStringLiteral("desk-win"), LayoutModel::Down));

    // a 27" 1440p monitor (597 x 336 mm) and a 14" MacBook in points (302 x 196 mm)
    DisplayConfig monitor{0, 0, 2560, 1440, 597, 336}, laptop{0, 0, 1512, 982, 302, 196};
    EXPECT_EQ(LayoutModel::measuredScale({monitor}), 882);
    EXPECT_EQ(LayoutModel::measuredScale({laptop}), 755);
    EXPECT_EQ(LayoutModel::measuredScale({DisplayConfig{0, 0, 1920, 1080, 16, 9}}), 0);   // an aspect ratio, not a size

    EXPECT_TRUE(layout.reportDisplays(QStringLiteral("desk-win"), {monitor}));
    EXPECT_TRUE(layout.reportDisplays(QStringLiteral("macbook"), {laptop}));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("desk-win")), QRect(0, 0, 2258, 1270));
    // it was below the server, which grew: it moved down along
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")), QRect(0, 1270, 1142, 741));
    // the crossing follows the real widths: the laptop spans half the monitor
    const QString conf = layout.configText();
    EXPECT_TRUE(conf.contains(QStringLiteral("\t\tdown(0,50.58) = macbook\n"))) << conf.toStdString();

    // typed: reports keep their hands off; then back to the measurement
    const int m = layout.indexOf(QStringLiteral("macbook"));
    EXPECT_TRUE(layout.setScale(m, 100));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")).size(), QSize(1512, 982));
    EXPECT_FALSE(layout.reportDisplays(QStringLiteral("macbook"), {laptop}));
    EXPECT_EQ(layout.screenSettings(m).value(QStringLiteral("measuredScale")).toDouble(), 75.5);
    EXPECT_TRUE(layout.useMeasuredScale(m));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("macbook")).size(), QSize(1142, 741));

    // persisted, sizes and scale with it
    LayoutModel reopened(&store.settings);
    EXPECT_EQ(desktopOf(reopened, QStringLiteral("macbook")), QRect(0, 1270, 1142, 741));
    EXPECT_EQ(reopened.screens()[0].displays[0].mmWidth, 597);
    LayoutModel copy(nullptr);
    copy.loadJson(layout.toJson());
    EXPECT_EQ(desktopOf(copy, QStringLiteral("desk-win")), QRect(0, 0, 2258, 1270));
}

TEST(LayoutModel, reportsReplaceSizesTypedBeforeTheyExisted)
{
    // a layout from before typing was told apart: displays that are not the
    // placeholder, never reported
    QJsonObject screen;
    screen.insert(QStringLiteral("name"), QStringLiteral("desk-win"));
    screen.insert(QStringLiteral("displays"), QJsonArray{QJsonObject{{QStringLiteral("x"), 0}, {QStringLiteral("y"), 0},
                                                                     {QStringLiteral("width"), 2560}, {QStringLiteral("height"), 1440}}});
    QJsonObject json;
    json.insert(QStringLiteral("serverName"), QStringLiteral("desk-win"));
    json.insert(QStringLiteral("screens"), QJsonArray{screen});
    LayoutModel layout(nullptr);
    layout.loadJson(json);
    EXPECT_TRUE(layout.reportDisplays(QStringLiteral("desk-win"), {DisplayConfig{0, 0, 2560, 1440}, DisplayConfig{2560, 0, 2560, 1440}}));
    EXPECT_EQ(desktopOf(layout, QStringLiteral("desk-win")).size(), QSize(5120, 1440));
}

TEST(LayoutModel, displayReportsCarryRealSizes)
{
    const QVector<DisplayConfig> two = {DisplayConfig{0, 0, 2560, 1440, 597, 336}, DisplayConfig{2560, 0, 1920, 1080}};
    EXPECT_EQ(LayoutModel::displaysToText(two), QStringLiteral("0,0,2560x1440@597x336;2560,0,1920x1080"));
    EXPECT_EQ(LayoutModel::displaysToText(two, false), QStringLiteral("0,0,2560x1440;2560,0,1920x1080"));
    EXPECT_EQ(LayoutModel::displaysFromText(LayoutModel::displaysToText(two)), two);
    EXPECT_EQ(LayoutModel::displaysFromJson(LayoutModel::displaysToJson(two)), two);
    // half a size is no size
    const QVector<DisplayConfig> odd = LayoutModel::cleanDisplays({DisplayConfig{0, 0, 100, 100, 50, 0}});
    EXPECT_EQ(odd[0].mmWidth, 0);
}
