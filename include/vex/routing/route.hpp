#pragma once

#include <string>

namespace routing
{

/// A raw route entry as provided by the user (e.g. loaded from JSON/config).
struct Route
{
    int id;
    int priority;
    std::string from;
    std::string source_address;
    std::string destination_address;
    std::string pdu_type;
    std::string target;
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
