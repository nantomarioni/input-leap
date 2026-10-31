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

#import <AppKit/AppKit.h>

#include <QWidget>

namespace inputleap {
namespace fork_gui {

// The HUD must appear over whatever Space is current, full-screen apps
// included, without making macOS switch to the Space the window was first
// shown on (which is what a plain tool window does: showing it while a
// full-screen app is active flips the desktop to the icons Space).
void keepOverlayOnEverySpace(QWidget* widget)
{
    NSView* view = reinterpret_cast<NSView*>(widget->winId());
    NSWindow* window = [view window];
    if (window == nil) {
        return;
    }
    window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                              | NSWindowCollectionBehaviorFullScreenAuxiliary
                              | NSWindowCollectionBehaviorStationary
                              | NSWindowCollectionBehaviorIgnoresCycle;
    window.level = NSPopUpMenuWindowLevel;
    window.hidesOnDeactivate = NO;
}

} // namespace fork_gui
} // namespace inputleap
