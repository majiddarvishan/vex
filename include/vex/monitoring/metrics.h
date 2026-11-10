#pragma once

#include <vex/monitoring/metrics_manager.hpp>

#include <string>

namespace vex::monitoring
{
    // Compatibility helper for a single scalar Gauge.
    // Requires MetricsManager::Init() and fails on invalid/conflicting names.
    // Unlike the former no-op, this publishes an actual metric.
    inline void record_metric(const std::string& name, double value)
    {
        auto registry = vex::MetricsManager::GetRegistry();
        auto& gauge = vex::MetricsManager::CreateGauge(
          registry, name, "Metric recorded through record_metric"
        );
        gauge.Set(value);
    }
} // namespace vex::monitoring
