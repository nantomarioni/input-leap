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

#include "../../../fork/lib/server/DisplayGeometry.h"
#include "server/Config.h"

#include <gtest/gtest.h>
#include <sstream>

namespace inputleap {

namespace dg = display_geometry;

namespace {

// A Mac with a 2560x1440 monitor and its 1512x982 laptop under it, 220 px in.
const DisplayRect kBox{0, 0, 2560, 2422};
const std::vector<DisplayRect> kMac{{0, 0, 2560, 1440}, {220, 1440, 1512, 982}};

} // namespace

TEST(DisplayGeometryTests, facesOutWhereNothingOfItsOwnIsBeyond)
{
    EXPECT_TRUE(dg::facesOut(kMac, 1, kLeft, 2000));     // the laptop's left edge
    EXPECT_TRUE(dg::facesOut(kMac, 1, kBottom, 1000));
    EXPECT_FALSE(dg::facesOut(kMac, 1, kTop, 1000));     // the monitor is above it
    EXPECT_TRUE(dg::facesOut(kMac, 0, kBottom, 100));    // left of the laptop
    EXPECT_FALSE(dg::facesOut(kMac, 0, kBottom, 1000));
}

TEST(DisplayGeometryTests, entersAtTheMonitorFacingTheWayItComes)
{
    // moving right, the box's left edge at the laptop's row: on the laptop's
    // left edge, as far in as it was
    std::int32_t x = 1, y = 2000;
    dg::enter(kMac, kBox, kRight, x, y);
    EXPECT_EQ(x, 221);
    EXPECT_EQ(y, 2000);

    // moving left, at the laptop's row: its right edge
    x = 2559, y = 2000;
    dg::enter(kMac, kBox, kLeft, x, y);
    EXPECT_EQ(x, 1731);
    EXPECT_EQ(y, 2000);

    // at the monitor's row nothing changes
    x = 0, y = 500;
    dg::enter(kMac, kBox, kRight, x, y);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 500);

    // moving up into a column the laptop doesn't reach: the monitor's bottom
    x = 100, y = 2421;
    dg::enter(kMac, kBox, kTop, x, y);
    EXPECT_EQ(x, 100);
    EXPECT_EQ(y, 1439);
}

TEST(DisplayGeometryTests, leavesByAnEdgeFacingOut)
{
    // off the laptop's left edge: as far past the box's
    std::int32_t x = 215, y = 2000;
    EXPECT_TRUE(dg::leave(kMac, kBox, 221, 2000, x, y));
    EXPECT_EQ(x, -5);
    EXPECT_EQ(y, 2000);

    // off its right edge
    x = 1740, y = 2000;
    EXPECT_TRUE(dg::leave(kMac, kBox, 1731, 2000, x, y));
    EXPECT_EQ(x, 2559 + 9);

    // off the monitor's bottom, left of the laptop
    x = 100, y = 1445;
    EXPECT_TRUE(dg::leave(kMac, kBox, 100, 1439, x, y));
    EXPECT_EQ(y, 2421 + 6);

    // onto another monitor: nothing to do
    x = 500, y = 1445;
    EXPECT_FALSE(dg::leave(kMac, kBox, 500, 1439, x, y));
    EXPECT_EQ(y, 1445);
}

TEST(DisplayGeometryTests, staysOnTheMonitorItWasOn)
{
    // pushed off the laptop's right edge near the monitor's bottom: still on
    // the laptop, not jumping up to the monitor
    std::int32_t x = 1740, y = 1445;
    dg::stay(kMac, 1731, 1445, x, y);
    EXPECT_EQ(x, 1731);
    EXPECT_EQ(y, 1445);

    // from nowhere: the nearest monitor
    x = 0, y = 2000;
    dg::stay(kMac, x, y, x, y);
    EXPECT_EQ(x, 220);
    EXPECT_EQ(y, 2000);
}

TEST(DisplayGeometryTests, aMonitorEdgeFacingOutCountsAsTheBoxEdge)
{
    // the real cursor at the laptop's left edge, within the jump zone
    std::int32_t x = 220, y = 2000;
    dg::toBoxEdge(kMac, kBox, 1, x, y);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 2000);

    // its bottom edge reaches the box's already
    x = 1000, y = 2421;
    dg::toBoxEdge(kMac, kBox, 1, x, y);
    EXPECT_EQ(x, 1000);
    EXPECT_EQ(y, 2421);

    // away from any edge
    x = 1000, y = 2000;
    dg::toBoxEdge(kMac, kBox, 1, x, y);
    EXPECT_EQ(x, 1000);
}

TEST(DisplayGeometryTests, theConfigurationCarriesEachComputersMonitors)
{
    std::istringstream text(
        "section: screens\n"
        "\tpc:\n"
        "\tmac:\n"
        "end\n"
        "section: aliases\n"
        "\tmac:\n"
        "\t\tmac.local\n"
        "end\n"
        "section: displays\n"
        "\tmac.local = 0,0,2560,1440 220,1440,1512,982\n"
        "end\n");
    Config config;
    text >> config;
    const std::vector<DisplayRect>* displays = config.fork_displays("MAC");
    ASSERT_NE(displays, nullptr);
    EXPECT_EQ(*displays, kMac);
    EXPECT_EQ(config.fork_displays("pc"), nullptr);

    std::ostringstream written;
    written << config;
    EXPECT_NE(written.str().find("section: displays\n\tmac = 0,0,2560,1440 220,1440,1512,982\nend\n"), std::string::npos)
        << written.str();

    std::istringstream again(written.str());
    Config reread;
    again >> reread;
    EXPECT_TRUE(reread == config);

    std::istringstream bad("section: screens\n\tmac:\nend\nsection: displays\n\tmac = 0,0,10\nend\n");
    Config broken;
    EXPECT_THROW(bad >> broken, XConfigRead);
}

} // namespace inputleap
