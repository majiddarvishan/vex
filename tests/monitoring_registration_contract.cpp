#include <vex/monitoring/monitoring.hpp>

#include <stdexcept>

int main()
{
    using vex::HealthCheck;
    using vex::MetricsManager;
    if (!MetricsManager::Init())
        return 1;
    auto registry = std::make_shared<prometheus::Registry>();

    // A conflicting Prometheus family type makes registration fail after
    // health_status was already successfully created.
    auto& conflict = prometheus::BuildCounter()
      .Name("uptime_seconds")
      .Help("Deliberate counter/gauge name conflict")
      .Register(*registry);

    bool rejected = false;
    try
    {
        HealthCheck::RegisterHealthMetrics(registry);
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    if (!rejected || HealthCheck::IsRegistered())
        return 2;

    // After removing the conflict, re-registration must recover without
    // duplicate health_status errors or use of partially published pointers.
    if (!registry->Remove(conflict))
        return 3;
    HealthCheck::RegisterHealthMetrics(registry);
    if (!HealthCheck::IsRegistered())
        return 4;

    HealthCheck::SetHealthy(false);
    HealthCheck::UpdateMemoryUsage(12345);
    HealthCheck::UpdateUptime();
    HealthCheck::RegisterHealthMetrics(registry); // idempotent
    HealthCheck::SetHealthy(true);

    registry.reset();
    if (HealthCheck::IsRegistered())
        return 5;
    MetricsManager::Reset();
    return 0;
}
