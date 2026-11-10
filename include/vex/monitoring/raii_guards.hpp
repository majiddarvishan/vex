#pragma once

#include <vex/monitoring/metrics_manager.hpp>

namespace vex
{

    // ScopedCounter is an EPHEMERAL series owner. All overlapping guards
    // with identical family/labels share the series, which is removed when
    // the final guard expires. Do not use for *_total counters: use a
    // persistent CreateCounter() reference instead.
    // The owning Family/Registry MUST outlive every guard.
    class ScopedCounter
    {
    public:
        ScopedCounter(
          prometheus::Family<prometheus::Counter>& family,
          const std::map<std::string, std::string>& labels
        );
        ~ScopedCounter();

        ScopedCounter(const ScopedCounter&)            = delete;
        ScopedCounter& operator=(const ScopedCounter&) = delete;

        ScopedCounter(ScopedCounter&& other) noexcept;
        ScopedCounter& operator=(ScopedCounter&& other) noexcept;

        prometheus::Counter& operator*()
        {
            return *metric_;
        }
        prometheus::Counter* operator->()
        {
            return metric_;
        }
        prometheus::Counter* get()
        {
            return metric_;
        }

    private:
        prometheus::Family<prometheus::Counter>* family_;
        prometheus::Counter* metric_;
    };

    // Ephemeral series; overlapping guards for identical labels share it.
    // The owning Family/Registry MUST outlive every guard.
    class ScopedGauge
    {
    public:
        ScopedGauge(
          prometheus::Family<prometheus::Gauge>& family,
          const std::map<std::string, std::string>& labels
        );
        ~ScopedGauge();

        ScopedGauge(const ScopedGauge&)            = delete;
        ScopedGauge& operator=(const ScopedGauge&) = delete;

        ScopedGauge(ScopedGauge&& other) noexcept;
        ScopedGauge& operator=(ScopedGauge&& other) noexcept;

        prometheus::Gauge& operator*()
        {
            return *metric_;
        }
        prometheus::Gauge* operator->()
        {
            return metric_;
        }
        prometheus::Gauge* get()
        {
            return metric_;
        }

    private:
        prometheus::Family<prometheus::Gauge>* family_;
        prometheus::Gauge* metric_;
    };

    // Use for tracking in-flight operations without removing the underlying
    // metric or resetting it. The Gauge/Registry MUST outlive the guard.
    class ScopedInFlightGauge
    {
    public:
        explicit ScopedInFlightGauge(prometheus::Gauge& gauge);
        ~ScopedInFlightGauge() noexcept;

        ScopedInFlightGauge(const ScopedInFlightGauge&) = delete;
        ScopedInFlightGauge& operator=(const ScopedInFlightGauge&) = delete;
        ScopedInFlightGauge(ScopedInFlightGauge&& other) noexcept;
        ScopedInFlightGauge& operator=(ScopedInFlightGauge&& other) noexcept;

    private:
        prometheus::Gauge* gauge_;
    };

} // namespace vex