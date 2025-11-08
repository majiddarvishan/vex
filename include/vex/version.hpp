#pragma once

#include <string_view>

namespace vex
{
    static constexpr auto VEX_ASCII_LOGO = R"(
    __     __
   /  \   /  \
   \   \ /   /
    \__//__/
     VEX
)";

    inline constexpr int version_major       = 0;
    inline constexpr int version_minor       = 0;
    inline constexpr int version_patch       = 1;
    inline constexpr const char* version_tag = "";

    inline constexpr std::string_view version()
    {
        if constexpr (version_tag[0] == '\0')
        {
            return "0.0.1";
        }
        else
        {
            return "0.0.1-";
        }
    }

    inline constexpr std::string_view version_full()
    {
        return "Vex v0.0.1 — Fast. Simple. Professional.";
    }
} // namespace vex
