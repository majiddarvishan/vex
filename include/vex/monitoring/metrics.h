#pragma once

#include <string>

namespace vex::monitoring
{

    // Header-only simple metrics helper
    inline void record_metric(const std::string& name, double value)
    {
        // no-op for example
        (void) name;
        (void) value;
    }

} // namespace vex::monitoring
