
/* MSWindowsScreenExtension - default extension for MSWindowsScreen in fork
 */

#pragma once

#include <ApplicationServices/ApplicationServices.h>
#include <vector>
#include <string>
#include <cstdint>
#include "inputleap/option_types.h"
#include "PlatformScreenExtension.h"

namespace inputleap {

class OSXScreen;

class OSXScreenExtension: public virtual PlatformScreenExtension {
public:
    OSXScreenExtension();
    virtual ~OSXScreenExtension();

    virtual void fork_dimScreen(bool dim) override;
    virtual void fork_setOptions(const OptionsList& options);
    double fork_getLocalIdleSeconds() const override;

protected:
    class OSXScreen* host() const;

private:
    // Screen dimming support
    struct DisplayGammaInfo {
        CGDirectDisplayID displayID;
        CGGammaValue originalRed[256];
        CGGammaValue originalGreen[256];
        CGGammaValue originalBlue[256];
        bool gammaStored;
    };
    std::vector<DisplayGammaInfo> m_displayGammaInfo;
    bool m_isDimmed;
    
    // dimming configuration options
    bool m_dimmingEnabled;
    int m_dimmingPercentage;
};

// Called first on the client's event-loop thread, the one that posts every
// injected mouse and key event. CGEventPost waits for WindowServer, which
// serves requests by their thread's QoS; at the default QoS a busy
// WindowServer (a Zoom meeting, a Spaces switch) answers late and the shared
// cursor lags while the local trackpad stays smooth.
void fork_raiseInputThreadPriority();

} // namespace inputleap
