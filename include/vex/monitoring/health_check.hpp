#pragma once

#include <vex/monitoring/metrics_manager.hpp>

namespace vex
{

    class HealthCheck
    {
    public:
        static void
          RegisterHealthMetrics(std::shared_ptr<prometheus::Registry> registry);
        static void SetHealthy(bool healthy);
        static void UpdateUptime();
        static void UpdateMemoryUsage(size_t bytes);
        [[nodiscard]] static bool IsRegistered();

    private:
        inline static prometheus::Gauge* health_status_ = nullptr;
        inline static prometheus::Gauge* uptime_gauge_  = nullptr;
        inline static prometheus::Gauge* memory_usage_  = nullptr;
    };

} // namespace vex