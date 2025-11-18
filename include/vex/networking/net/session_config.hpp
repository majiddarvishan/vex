#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>

namespace pa::pinex
{

// ============================================================================
// Session Configuration
// ============================================================================

struct session_config
{
    // Buffer settings
    size_t send_buf_capacity{1024 * 1024};
    size_t send_buf_threshold{1024 * 1024};
    size_t receive_buf_size{1024 * 1024};
    size_t small_body_size{256};  // Threshold for stack vs heap allocation

    // Protocol limits
    uint32_t max_command_length{10 * 1024 * 1024};  // 10MB
    std::chrono::seconds unbind_timeout{5};

    // Backpressure settings
    size_t backpressure_low_watermark{512 * 1024};
    size_t backpressure_high_watermark{1024 * 1024};

    // Validation
    bool is_valid() const
    {
        return send_buf_capacity > 0 &&
               send_buf_threshold <= send_buf_capacity &&
               receive_buf_size > 0 &&
               max_command_length > 0 &&
               backpressure_low_watermark <= backpressure_high_watermark &&
               backpressure_high_watermark <= send_buf_capacity;
    }
};

} // namespace pa::pinex