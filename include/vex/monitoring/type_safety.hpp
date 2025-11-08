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

/// Macro for compile-time validated metric creation
/// Note: These macros perform compile-time validation of metric names
#define SAFE_CREATE_COUNTER(registry, name, help, ...)                         \
    (detail::MetricNameChecker<vex::detail::ValidateName(name)> {},            \
     vex::MetricsManager::CreateCounter(registry, name, help, ##__VA_ARGS__))

#define SAFE_CREATE_GAUGE(registry, name, help, ...)                           \
    (detail::MetricNameChecker<vex::detail::ValidateName(name)> {},            \
     vex::MetricsManager::CreateGauge(registry, name, help, ##__VA_ARGS__))

#define SAFE_CREATE_HISTOGRAM(registry, name, help, ...)                       \
    (detail::MetricNameChecker<vex::detail::ValidateName(name)> {},            \
     vex::MetricsManager::CreateHistogram(registry, name, help, ##__VA_ARGS__) \
    )


// #define SAFE_CREATE_COUNTER(registry, name, help, ...) \
//     [&]() { \
//         static_assert(vex::detail::ValidateName(name), \
//                      "Invalid metric name: " name); \
//         return vex::MetricsManager::CreateCounter(registry, name, help, ##__VA_ARGS__); \
//     }()

// #define SAFE_CREATE_GAUGE(registry, name, help, ...) \
//     [&]() { \
//         static_assert(vex::detail::ValidateName(name), \
//                      "Invalid metric name: " name); \
//         return vex::MetricsManager::CreateGauge(registry, name, help, ##__VA_ARGS__); \
//     }()

// #define SAFE_CREATE_HISTOGRAM(registry, name, help, ...) \
//     [&]() { \
//         static_assert(vex::detail::ValidateName(name), \
//                      "Invalid metric name: " name); \
//         return vex::MetricsManager::CreateHistogram(registry, name, help, ##__VA_ARGS__); \
//     }()


// // #define SAFE_CREATE_COUNTER(registry, name, help, labels) \
// //     []() { \
// //         static_assert(vex::detail::ValidateName(name), \
// //                      "Invalid metric name: " name); \
// //         return vex::MetricsManager::CreateCounter(registry, name, help, labels); \
// //     }()

// // #define SAFE_CREATE_GAUGE(registry, name, help, labels) \
// //     []() { \
// //         static_assert(vex::detail::ValidateName(name), \
// //                      "Invalid metric name: " name); \
// //         return vex::MetricsManager::CreateGauge(registry, name, help, labels); \
// //     }()

// // #define SAFE_CREATE_HISTOGRAM(registry, name, help, labels, buckets) \
// //     []() { \
// //         static_assert(vex::detail::ValidateName(name), \
// //                      "Invalid metric name: " name); \
// //         return vex::MetricsManager::CreateHistogram(registry, name, help, labels, buckets); \
// //     }()
