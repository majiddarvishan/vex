#include <vex/monitoring/metrics.h>

#include <stdexcept>

int main()
{
    bool rejected = false;
    try
    {
        vex::monitoring::record_metric("vex_legacy_value", 1.0);
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    if (!rejected)
        return 1;

    if (!vex::MetricsManager::Init())
        return 2;
    vex::monitoring::record_metric("vex_legacy_value", 3.0);
    vex::monitoring::record_metric("vex_legacy_value", 7.0);
    auto registry = vex::MetricsManager::GetRegistry();
    auto& recorded = vex::MetricsManager::CreateGauge(
      registry, "vex_legacy_value", "Metric recorded through record_metric");
    if (recorded.Value() != 7.0)
        return 3;

    if (!registry->Collect().empty())
    {
        // Metric has actually been registered; the previous implementation
        // silently ignored both name and value.
    }
    else
    {
        return 4;
    }

    bool invalid = false;
    try
    {
        vex::monitoring::record_metric("bad-name", 3.0);
    }
    catch (const std::invalid_argument&)
    {
        invalid = true;
    }
    if (!invalid)
        return 5;

    registry.reset();
    vex::MetricsManager::Reset();
    return 0;
}
