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

#include "inputleap/protocol_types.h"
#include <cstdint>
#include <vector>

namespace inputleap {

// One monitor of a computer, in that computer's own pixels.
struct DisplayRect {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t w = 0;
    std::int32_t h = 0;

    bool contains(std::int32_t px, std::int32_t py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    bool operator==(const DisplayRect& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
    bool operator!=(const DisplayRect& o) const { return !(*this == o); }
};

/* A computer's desktop is the box around its monitors; where they don't fill
 * it (a laptop under a wider monitor), the core alone would let the cursor
 * wander off them and only leave at the box's edges. These keep it on the
 * monitors: a monitor's edge with nothing of the same computer beyond it
 * faces out, and leads where the box's edge would at that row or column.
 * Directions are the way the cursor moves. */
namespace display_geometry {

// The monitor holding the point, or -1.
int displayAt(const std::vector<DisplayRect>& displays, std::int32_t x, std::int32_t y);

// Whether `side` of displays[index] faces out at `along` (a row for left and
// right, a column for up and down): no other monitor lies beyond it there.
bool facesOut(const std::vector<DisplayRect>& displays, int index, EDirection side, std::int32_t along);

// The point moved onto displays[index], to its nearest pixel.
void clampInto(const DisplayRect& display, std::int32_t& x, std::int32_t& y);

// Entering a computer moving in `dir`, where its box's edge would put the
// cursor: moved across to the monitor that faces that way at that row or
// column, as far in as it was in the box. Onto the nearest monitor when
// none does.
void enter(const std::vector<DisplayRect>& displays, const DisplayRect& box, EDirection dir,
           std::int32_t& x, std::int32_t& y);

// The cursor moved from (xOld, yOld), on a monitor, to (x, y), on none. Past
// an edge facing out, it is carried as far past the box's edge, so the core
// leaves the computer there; true when so. Otherwise left alone.
bool leave(const std::vector<DisplayRect>& displays, const DisplayRect& box, std::int32_t xOld,
           std::int32_t yOld, std::int32_t& x, std::int32_t& y);

// The cursor stays on this computer: kept on the monitor it was on unless
// it reached another.
void stay(const std::vector<DisplayRect>& displays, std::int32_t xOld, std::int32_t yOld,
          std::int32_t& x, std::int32_t& y);

// The real cursor on (x, y) within `zone` of a monitor's edge facing out:
// the same distance from the box's edge, where the core looks for it.
void toBoxEdge(const std::vector<DisplayRect>& displays, const DisplayRect& box, std::int32_t zone,
               std::int32_t& x, std::int32_t& y);

} // namespace display_geometry
} // namespace inputleap
