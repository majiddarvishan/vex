#pragma once

#include <vex/monitoring/metrics_manager.hpp>

namespace vex
{

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

} // namespace vex