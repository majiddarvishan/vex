#pragma once

#include <atomic>
#include <chrono>

namespace pa::pinex
{

// ============================================================================
// Metrics
// ============================================================================

struct session_metrics
{
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> buffer_compactions{0};
    std::chrono::steady_clock::time_point created_at{std::chrono::steady_clock::now()};
    std::atomic<bool> is_closed{false};

    void reset()
    {
        bytes_sent = 0;
        bytes_received = 0;
        messages_sent = 0;
        messages_received = 0;
        errors = 0;
        buffer_compactions = 0;
        created_at = std::chrono::steady_clock::now();
        is_closed = false;
    }

    std::chrono::milliseconds uptime() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - created_at);
    }
};

} // namespace pa::pinex