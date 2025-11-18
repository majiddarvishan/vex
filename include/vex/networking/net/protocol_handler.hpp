#pragma once

#include <vex/networking/common.hpp>
#include <vex/networking/pdu.hpp>

#include <variant>

namespace pa::pinex
{

// ============================================================================
// Protocol Types
// ============================================================================

using request = std::variant<std::monostate, bind_request, stream_request>;
using response = std::variant<std::monostate, bind_response, stream_response>;

// ============================================================================
// Protocol Handler Interface
// ============================================================================

class protocol_handler
{
public:
    virtual ~protocol_handler() = default;
    virtual void on_request(request&& req, uint32_t sequence_number) = 0;
    virtual void on_response(response&& resp, uint32_t sequence_number, command_status status) = 0;
};

} // namespace pa::pinex