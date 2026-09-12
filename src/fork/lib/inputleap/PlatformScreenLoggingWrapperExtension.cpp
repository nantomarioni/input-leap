#include "PlatformScreenLoggingWrapperExtension.h"
#include "inputleap/PlatformScreenLoggingWrapper.h"

#include <typeinfo>

namespace inputleap {

PlatformScreenLoggingWrapperExtension::PlatformScreenLoggingWrapperExtension() {}
PlatformScreenLoggingWrapperExtension::~PlatformScreenLoggingWrapperExtension() {}

PlatformScreenLoggingWrapper* PlatformScreenLoggingWrapperExtension::host() const {
    return static_cast<PlatformScreenLoggingWrapper*>(const_cast<PlatformScreenLoggingWrapperExtension*>(this));
}

void PlatformScreenLoggingWrapperExtension::fork_dimScreen(bool dim) {
    PlatformScreenLoggingWrapper* wrapper = host();
    if (!wrapper) return;

    wrapper->screen_->fork_dimScreen(dim);
}

double PlatformScreenLoggingWrapperExtension::fork_getLocalIdleSeconds() const {
    PlatformScreenLoggingWrapper* wrapper = host();
    if (!wrapper) return -1.0;

    return wrapper->screen_->fork_getLocalIdleSeconds();
}

} // namespace inputleap
