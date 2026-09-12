
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

} // namespace inputleap
