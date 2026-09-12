
/* MSWindowsScreenExtension - default extension for MSWindowsScreen in fork
 */

#pragma once

#include <string>
#include <cstdint>
#include <array>
#include "inputleap/option_types.h"
#include "PlatformScreenExtension.h"

namespace inputleap {

class MSWindowsScreen;

class MSWindowsScreenExtension: public virtual PlatformScreenExtension {
public:
    MSWindowsScreenExtension();
    virtual ~MSWindowsScreenExtension();

    virtual void fork_dimScreen(bool dim) override;
    virtual void fork_setOptions(const OptionsList& options);

protected:
    class MSWindowsScreen* host() const;

private:
    std::array<std::uint16_t, 256 * 3> m_originalGamma;
    bool m_isDimmed;
    bool m_dimmingEnabled;
    int m_dimmingPercentage;
};

} // namespace inputleap
