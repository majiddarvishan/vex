#include <vex/monitoring/health_check.hpp>

namespace vex
{
    void HealthCheck::RegisterHealthMetrics(
      // Keep an owning Registry handle throughout registration so that
      // the metric pointers cannot outlive the passed Registry mid-call.
      // Preserve the established public by-value API.
      // NOLINTNEXTLINE(performance-unnecessary-value-param)
      std::shared_ptr<prometheus::Registry> registry
    )
    {
        if (!registry)
            throw std::invalid_argument("Registry cannot be null");

        std::lock_guard<std::mutex> guard(mutex_);
        if (auto existing = registry_.lock())
        {
            if (existing == registry && health_status_ && uptime_gauge_ &&
                memory_usage_)
            {
                return; // Already registered against this Registry.
            }
        }

        // Publish new metric handles only after all registration steps succeed.
        auto* health = &MetricsManager::CreateGauge(
          registry, "health_status",
          "Service health status (1=healthy, 0=unhealthy)"
        );
        auto* uptime = &MetricsManager::CreateGauge(
          registry, "uptime_seconds", "Service uptime in seconds"
        );
        auto* memory = &MetricsManager::CreateGauge(
          registry, "memory_usage_bytes", "Current memory usage in bytes"
        );

        health->Set(1.0);
        uptime->Set(0.0);
        memory->Set(0.0);

        registry_ = registry; // Weak: do not prevent the Registry from closing.
        health_status_ = health;
        uptime_gauge_ = uptime;
        memory_usage_ = memory;
    }

    void HealthCheck::SetHealthy(bool healthy)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto registry = registry_.lock();
        if (!registry || !health_status_)
        {
            throw std::runtime_error(
              "Health metrics not registered or Registry expired"
            );
        }
        health_status_->Set(healthy ? 1.0 : 0.0);
    }

    void HealthCheck::UpdateUptime()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto registry = registry_.lock();
        if (!registry || !uptime_gauge_)
            throw std::runtime_error("Health metrics not registered or Registry expired");
        uptime_gauge_->Set(MetricsManager::GetUptimeSeconds());
    }

    void HealthCheck::UpdateMemoryUsage(size_t bytes)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto registry = registry_.lock();
        if (!registry || !memory_usage_)
            throw std::runtime_error("Health metrics not registered or Registry expired");
        memory_usage_->Set(static_cast<double>(bytes));
    }

    bool HealthCheck::IsRegistered()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return !registry_.expired() && health_status_ && uptime_gauge_ &&
               memory_usage_;
    }
} // namespace vex
