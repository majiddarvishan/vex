#include <vex/monitoring/health_check.hpp>

namespace vex
{

    void HealthCheck::RegisterHealthMetrics(
      std::shared_ptr<prometheus::Registry> registry
    )
    {
        if (!registry)
        {
            throw std::invalid_argument("Registry cannot be null");
        }

        // Create health status gauge
        health_status_ = &MetricsManager::CreateGauge(
          registry,
          "health_status",
          "Service health status (1=healthy, 0=unhealthy)"
        );

        // Create uptime gauge
        uptime_gauge_ = &MetricsManager::CreateGauge(
          registry, "uptime_seconds", "Service uptime in seconds"
        );

        // Create memory usage gauge
        memory_usage_ = &MetricsManager::CreateGauge(
          registry, "memory_usage_bytes", "Current memory usage in bytes"
        );

        // Initialize with default values
        health_status_->Set(1.0); // Healthy by default
        uptime_gauge_->Set(0.0);
        memory_usage_->Set(0.0);
    }

    void HealthCheck::SetHealthy(bool healthy)
    {
        if (!health_status_)
        {
            throw std::runtime_error(
              "Health metrics not registered. Call RegisterHealthMetrics() "
              "first."
            );
        }
        health_status_->Set(healthy ? 1.0 : 0.0);
    }

    void HealthCheck::UpdateUptime()
    {
        if (!uptime_gauge_)
        {
            throw std::runtime_error(
              "Health metrics not registered. Call RegisterHealthMetrics() "
              "first."
            );
        }
        uptime_gauge_->Set(MetricsManager::GetUptimeSeconds());
    }

    void HealthCheck::UpdateMemoryUsage(size_t bytes)
    {
        if (!memory_usage_)
        {
            throw std::runtime_error(
              "Health metrics not registered. Call RegisterHealthMetrics() "
              "first."
            );
        }
        memory_usage_->Set(static_cast<double>(bytes));
    }

    bool HealthCheck::IsRegistered()
    {
        return health_status_ != nullptr && uptime_gauge_ != nullptr &&
               memory_usage_ != nullptr;
    }

} // namespace vex