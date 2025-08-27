#include "inputleap/ScreenExtension.h"
#include "inputleap/Screen.h"

#include "base/LogExtension.h"

namespace inputleap {

ScreenExtension::ScreenExtension() {}
ScreenExtension::~ScreenExtension() {}

Screen* ScreenExtension::host() const {
    return static_cast<Screen*>(const_cast<ScreenExtension*>(this));
}

void ScreenExtension::fork_dimScreen(bool dim) {
    LOG_DEBUG("Screen::fork_dimScreen forwarding dim=%d", dim ? 1 : 0);

    Screen* screen = host();
    if (!screen->m_screen) return;

    screen->m_screen->fork_dimScreen(dim);
}

} // namespace inputleap
