#pragma once

#include <cstddef>

namespace pa::pinex
{

// ============================================================================
// Backpressure Controller
// ============================================================================

class backpressure_controller
{
    size_t low_watermark_;
    size_t high_watermark_;
    bool paused_{false};

public:
    backpressure_controller(size_t low, size_t high)
        : low_watermark_(low), high_watermark_(high) {}

    bool should_pause(size_t current_size)
    {
        if (!paused_ && current_size > high_watermark_)
        {
            paused_ = true;
            return true;
        }
        return false;
    }

    bool should_resume(size_t current_size)
    {
        if (paused_ && current_size < low_watermark_)
        {
            paused_ = false;
            return true;
        }
        return false;
    }

    bool is_paused() const { return paused_; }

    void reset()
    {
        paused_ = false;
    }

    size_t low_watermark() const { return low_watermark_; }
    size_t high_watermark() const { return high_watermark_; }

    void set_watermarks(size_t low, size_t high)
    {
        low_watermark_ = low;
        high_watermark_ = high;
    }
};

} // namespace pa::pinex