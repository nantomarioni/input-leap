

/* MSWindowsScreenExtension - default extension for MSWindowsScreen in fork
 */

#pragma once

#include "../platform/PlatformScreenExtension.h"

namespace inputleap {

class PlatformScreenLoggingWrapper;

class PlatformScreenLoggingWrapperExtension: public virtual PlatformScreenExtension {
public:
    PlatformScreenLoggingWrapperExtension();
    virtual ~PlatformScreenLoggingWrapperExtension();

    // default implementation to not break tests
    // platform-specific implementation should override this
    void fork_dimScreen(bool dim) override;

protected:
    class PlatformScreenLoggingWrapper* host() const;
};

} // namespace inputleap
