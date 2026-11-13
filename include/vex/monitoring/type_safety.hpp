#pragma once

#include <vex/monitoring/metrics_manager.hpp>

namespace vex
{
    namespace detail
    {
        /// Use inline helper to work around static_assert in expression context
        template <bool Valid>
        struct MetricNameChecker
        {
            static_assert(Valid, "Invalid metric name");
        };

        constexpr bool IsValidFirstChar(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   c == '_' || c == ':';
        }

        constexpr bool IsValidChar(char c)
        {
            return IsValidFirstChar(c) || (c >= '0' && c <= '9');
        }

        template <size_t N>
        constexpr bool ValidateName(const char (&name)[N])
        {
            if (N <= 1)
                return false;
            if (!IsValidFirstChar(name[0]))
                return false;
            if (N >= 3 && name[0] == '_' && name[1] == '_')
                return false;

            for (size_t i = 1; i < N - 1; ++i)
            {
                if (!IsValidChar(name[i]))
                    return false;
            }
            return true;
        }
    } // namespace detail

} // namespace vex

/// Macro for compile-time validated metric creation.
/// The required first variadic argument is the metric HELP text, followed by
/// optional labels/buckets; this keeps empty optional tails C++17-conforming
/// without the non-standard GCC ", ##__VA_ARGS__" comma-swallowing extension.

#define SAFE_CREATE_COUNTER(registry, name, ...)                         \
    (static_cast<void>(vex::detail::MetricNameChecker<vex::detail::ValidateName(name)> {}),            \
     vex::MetricsManager::CreateCounter(registry, name, __VA_ARGS__))

#define SAFE_CREATE_GAUGE(registry, name, ...)                           \
    (static_cast<void>(vex::detail::MetricNameChecker<vex::detail::ValidateName(name)> {}),            \
     vex::MetricsManager::CreateGauge(registry, name, __VA_ARGS__))

#define SAFE_CREATE_HISTOGRAM(registry, name, ...)                       \
    (static_cast<void>(vex::detail::MetricNameChecker<vex::detail::ValidateName(name)> {}),            \
     vex::MetricsManager::CreateHistogram(registry, name, __VA_ARGS__) \
    )
