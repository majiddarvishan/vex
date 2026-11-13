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
        // Every gauge access locks mutex_, obtains shared ownership from
        // registry_, then dereferences the pointers while ownership is held.
        inline static std::mutex mutex_;
        inline static std::weak_ptr<prometheus::Registry> registry_;
        inline static prometheus::Gauge* health_status_ = nullptr;
        inline static prometheus::Gauge* uptime_gauge_  = nullptr;
        inline static prometheus::Gauge* memory_usage_  = nullptr;
    };

} // namespace vex