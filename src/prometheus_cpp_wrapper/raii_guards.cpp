#include <prometheus_cpp_wrapper/raii_guards.hpp>

namespace vex::metrics {
ScopedCounter::ScopedCounter(
    prometheus::Family<prometheus::Counter>& family,
    const std::map<std::string, std::string>& labels)
    : family_(&family)
    , metric_(&MetricsManager::add_counter(family, labels))
{
}

ScopedCounter::~ScopedCounter() {
    if (metric_ && family_) {
        try {
            MetricsManager::remove_counter(*family_, metric_);
        } catch (...) {
            // Suppress exceptions in destructor
            // Consider logging if you have a logger
        }
    }
}

ScopedCounter::ScopedCounter(ScopedCounter&& other) noexcept
    : family_(other.family_)
    , metric_(other.metric_)
{
    other.family_ = nullptr;
    other.metric_ = nullptr;
}

ScopedCounter& ScopedCounter::operator=(ScopedCounter&& other) noexcept {
    if (this != &other) {
        // Clean up current metric
        if (metric_ && family_) {
            try {
                MetricsManager::remove_counter(*family_, metric_);
            } catch (...) {
                // Suppress exceptions
            }
        }

        // Move from other
        family_ = other.family_;
        metric_ = other.metric_;

        // Null out other
        other.family_ = nullptr;
        other.metric_ = nullptr;
    }
    return *this;
}


ScopedGauge::ScopedGauge(
    prometheus::Family<prometheus::Gauge>& family,
    const std::map<std::string, std::string>& labels)
    : family_(&family)
    , metric_(&MetricsManager::add_gauge(family, labels))
{
}

ScopedGauge::~ScopedGauge() {
    if (metric_ && family_) {
        try {
            MetricsManager::remove_gauge(*family_, metric_);
        } catch (...) {
            // Suppress exceptions in destructor
        }
    }
}

ScopedGauge::ScopedGauge(ScopedGauge&& other) noexcept
    : family_(other.family_)
    , metric_(other.metric_)
{
    other.family_ = nullptr;
    other.metric_ = nullptr;
}

ScopedGauge& ScopedGauge::operator=(ScopedGauge&& other) noexcept {
    if (this != &other) {
        // Clean up current metric
        if (metric_ && family_) {
            try {
                MetricsManager::remove_gauge(*family_, metric_);
            } catch (...) {
                // Suppress exceptions
            }
        }

        // Move from other
        family_ = other.family_;
        metric_ = other.metric_;

        // Null out other
        other.family_ = nullptr;
        other.metric_ = nullptr;
    }
    return *this;
}

}