#pragma once

#include <string>
#include <algorithm>
#include <string_view>

namespace vex
{
namespace routing
{
/// A raw route entry as provided by the user (e.g. loaded from JSON/config).
struct Route
{
    uint32_t id;
    uint32_t priority;
    std::string from;
    std::string source_address;
    std::string destination_address;
    std::string pdu_type;
    std::string target;

    Route(
        uint32_t route_id,
        uint32_t priority,
        std::string_view from,
        std::string_view src_addr,
        std::string_view dst_addr,
        std::string_view pdu_type,
        std::string_view target
    )
        : id(route_id)
        , priority(priority)
        , from(from)
        , source_address(src_addr)
        , destination_address(dst_addr)
        , pdu_type(pdu_type)
        , target(target)
    {
        normalize();
    }

    private:
    static void normalize_wildcard(std::string& s)
    {
        if (s == "*")
            s.clear();
    }

    static void to_lower(std::string& s)
    {
        // in some sytems ::tolower is technically unsafe for signed char.
        // transform(s.begin(), s.end(), s.begin(), ::tolower);
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
    }

    void normalize()
    {
        normalize_wildcard(from);
        normalize_wildcard(source_address);
        normalize_wildcard(destination_address);
        normalize_wildcard(target);
        normalize_wildcard(pdu_type);

        to_lower(from);
    }
};

/// The fields of an incoming message used to select a route.
struct MessageContext
{
    std::string from;
    std::string source_address;
    std::string destination_address;
    std::string pdu_type;
};
}  // namespace routing
}