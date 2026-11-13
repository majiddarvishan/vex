#include <vex/monitoring/monitoring.hpp>

int main()
{
    if (!vex::MetricsManager::Init())
        return 1;
    auto registry = vex::MetricsManager::GetRegistry();
    if (!registry)
        return 2;
    auto& gauge = vex::MetricsManager::CreateGauge(
        registry, "vex_smoke_gauge", "Smoke test gauge");
    gauge.Set(2);
    if (gauge.Value() != 2)
        return 3;
    vex::HealthCheck::RegisterHealthMetrics(registry);
    vex::HealthCheck::SetHealthy(true);
    vex::HealthCheck::UpdateUptime();
    return vex::HealthCheck::IsRegistered() ? 0 : 4;
}
