// ============================================================================
// Compile-time metric name validation
// ============================================================================
#pragma once

#include <prometheus_cpp_wrapper/metrics_manager.hpp>

namespace vex::metrics {

namespace detail {
    constexpr bool IsValidFirstChar(char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               c == '_' || c == ':';
    }

    constexpr bool IsValidChar(char c) {
        return IsValidFirstChar(c) || (c >= '0' && c <= '9');
    }

    template<size_t N>
    constexpr bool ValidateName(const char (&name)[N]) {
        if (N <= 1) return false;
        if (!IsValidFirstChar(name[0])) return false;
        if (N >= 3 && name[0] == '_' && name[1] == '_') return false;

        for (size_t i = 1; i < N - 1; ++i) {
            if (!IsValidChar(name[i])) return false;
        }
        return true;
    }
}

} // namespace vex::metrics

#define SAFE_CREATE_COUNTER(registry, name, help, labels) \
    []() { \
        static_assert(vex::metrics::detail::ValidateName(name), \
                     "Invalid metric name: " name); \
        return vex::metrics::MetricsManager::CreateCounter(registry, name, help, labels); \
    }()

#define SAFE_CREATE_GAUGE(registry, name, help, labels) \
    []() { \
        static_assert(vex::metrics::detail::ValidateName(name), \
                     "Invalid metric name: " name); \
        return vex::metrics::MetricsManager::CreateGauge(registry, name, help, labels); \
    }()

#define SAFE_CREATE_HISTOGRAM(registry, name, help, labels, buckets) \
    []() { \
        static_assert(vex::metrics::detail::ValidateName(name), \
                     "Invalid metric name: " name); \
        return vex::metrics::MetricsManager::CreateHistogram(registry, name, help, labels, buckets); \
    }()