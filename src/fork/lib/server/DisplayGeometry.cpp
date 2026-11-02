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

#include "DisplayGeometry.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace inputleap {
namespace display_geometry {

namespace {

bool upright(EDirection side)
{
    return side == kLeft || side == kRight;
}

// The span of a monitor across `side`: its rows for left and right.
bool spans(const DisplayRect& d, EDirection side, std::int32_t along)
{
    return upright(side) ? along >= d.y && along < d.y + d.h : along >= d.x && along < d.x + d.w;
}

int nearest(const std::vector<DisplayRect>& displays, std::int32_t x, std::int32_t y)
{
    int best = -1;
    std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
    for (int i = 0; i < static_cast<int>(displays.size()); ++i) {
        std::int32_t cx = x, cy = y;
        clampInto(displays[i], cx, cy);
        const std::int64_t dx = cx - x, dy = cy - y;
        if (dx * dx + dy * dy < bestDistance) {
            bestDistance = dx * dx + dy * dy;
            best = i;
        }
    }
    return best;
}

} // namespace

int displayAt(const std::vector<DisplayRect>& displays, std::int32_t x, std::int32_t y)
{
    for (int i = 0; i < static_cast<int>(displays.size()); ++i) {
        if (displays[i].contains(x, y)) {
            return i;
        }
    }
    return -1;
}

bool facesOut(const std::vector<DisplayRect>& displays, int index, EDirection side, std::int32_t along)
{
    const DisplayRect& d = displays[index];
    for (int i = 0; i < static_cast<int>(displays.size()); ++i) {
        const DisplayRect& e = displays[i];
        if (i == index || !spans(e, side, along)) {
            continue;
        }
        switch (side) {
        case kLeft: if (e.x + e.w <= d.x) return false; break;
        case kRight: if (e.x >= d.x + d.w) return false; break;
        case kTop: if (e.y + e.h <= d.y) return false; break;
        case kBottom: if (e.y >= d.y + d.h) return false; break;
        default: break;
        }
    }
    return true;
}

void clampInto(const DisplayRect& display, std::int32_t& x, std::int32_t& y)
{
    x = std::clamp(x, display.x, display.x + display.w - 1);
    y = std::clamp(y, display.y, display.y + display.h - 1);
}

void enter(const std::vector<DisplayRect>& displays, const DisplayRect& box, EDirection dir,
           std::int32_t& x, std::int32_t& y)
{
    if (displays.empty() || displayAt(displays, x, y) >= 0) {
        return;
    }
    // moving right enters by the left side: the monitor whose left edge is
    // first at this row
    const EDirection side = dir == kLeft ? kRight : dir == kRight ? kLeft : dir == kTop ? kBottom : kTop;
    const std::int32_t along = upright(side) ? y : x;
    int pick = -1;
    for (int i = 0; i < static_cast<int>(displays.size()); ++i) {
        if (spans(displays[i], side, along) && facesOut(displays, i, side, along)) {
            pick = i;
            break;
        }
    }
    if (pick < 0) {
        pick = nearest(displays, x, y);
    } else {
        const DisplayRect& d = displays[pick];
        switch (side) {
        case kLeft: x += d.x - box.x; break;
        case kRight: x -= (box.x + box.w) - (d.x + d.w); break;
        case kTop: y += d.y - box.y; break;
        case kBottom: y -= (box.y + box.h) - (d.y + d.h); break;
        default: break;
        }
    }
    clampInto(displays[pick], x, y);
}

bool leave(const std::vector<DisplayRect>& displays, const DisplayRect& box, std::int32_t xOld,
           std::int32_t yOld, std::int32_t& x, std::int32_t& y)
{
    if (displays.empty() || displayAt(displays, x, y) >= 0) {
        return false;
    }
    int from = displayAt(displays, xOld, yOld);
    if (from < 0) {
        from = nearest(displays, xOld, yOld);
    }
    const DisplayRect& d = displays[from];
    std::int32_t cx = x, cy = y;
    clampInto(d, cx, cy);
    // the way it left that monitor, sideways first as the core does
    EDirection side;
    if (x < d.x) side = kLeft;
    else if (x >= d.x + d.w) side = kRight;
    else if (y < d.y) side = kTop;
    else side = kBottom;
    if (!facesOut(displays, from, side, upright(side) ? cy : cx)) {
        return false;
    }
    switch (side) {
    case kLeft: x = box.x - (d.x - x); break;
    case kRight: x = box.x + box.w - 1 + (x - (d.x + d.w - 1)); break;
    case kTop: y = box.y - (d.y - y); break;
    default: y = box.y + box.h - 1 + (y - (d.y + d.h - 1)); break;
    }
    return true;
}

void stay(const std::vector<DisplayRect>& displays, std::int32_t xOld, std::int32_t yOld,
          std::int32_t& x, std::int32_t& y)
{
    if (displays.empty() || displayAt(displays, x, y) >= 0) {
        return;
    }
    int from = displayAt(displays, xOld, yOld);
    if (from < 0) {
        from = nearest(displays, x, y);
    }
    clampInto(displays[from], x, y);
}

void toBoxEdge(const std::vector<DisplayRect>& displays, const DisplayRect& box, std::int32_t zone,
               std::int32_t& x, std::int32_t& y)
{
    const int at = displayAt(displays, x, y);
    if (at < 0) {
        return;
    }
    const DisplayRect& d = displays[at];
    const std::int32_t px = x, py = y;
    if (d.x > box.x && px < d.x + zone && facesOut(displays, at, kLeft, py)) {
        x = box.x + (px - d.x);
    } else if (d.x + d.w < box.x + box.w && px >= d.x + d.w - zone && facesOut(displays, at, kRight, py)) {
        x = box.x + box.w - (d.x + d.w - px);
    }
    if (d.y > box.y && py < d.y + zone && facesOut(displays, at, kTop, px)) {
        y = box.y + (py - d.y);
    } else if (d.y + d.h < box.y + box.h && py >= d.y + d.h - zone && facesOut(displays, at, kBottom, px)) {
        y = box.y + box.h - (d.y + d.h - py);
    }
}

} // namespace display_geometry
} // namespace inputleap
