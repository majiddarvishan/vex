#include <vex/monitoring/monitoring.hpp>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
template <typename Error, typename Func>
bool throws_as(Func&& func)
{
    try
    {
        func();
    }
    catch (const Error&)
    {
        return true;
    }
    catch (...)
    {
    }
    return false;
}
} // namespace

int main()
{
    using vex::HealthCheck;
    using vex::MetricsManager;

    if (!MetricsManager::Init(true))
        return 1;
    if (!MetricsManager::IsInitialized())
        return 2;
#ifdef METRICS_MULTI_THREADED
    if (!MetricsManager::IsThreadingEnabled())
        return 19;
#else
    if (MetricsManager::IsThreadingEnabled())
        return 20;
#endif

    auto registry = MetricsManager::GetRegistry();
    auto registry_again = MetricsManager::GetRegistry();
    if (!registry || registry != registry_again)
        return 3;
    registry_again.reset();

    auto subsystem = MetricsManager::GetSubsystemRegistry("monitoring_contract");
    if (!subsystem ||
        subsystem != MetricsManager::GetSubsystemRegistry("monitoring_contract"))
        return 4;
    if (!throws_as<std::invalid_argument>([]
            { (void) MetricsManager::GetSubsystemRegistry(""); }))
        return 5;

    MetricsManager::SetDefaultLabels({{"service", "vex"}, {"env", "test"}});
    const auto merged =
      MetricsManager::MergeLabels({{"env", "override"}, {"method", "GET"}});
    if (merged.at("service") != "vex" ||
        merged.at("env") != "override" ||
        merged.at("method") != "GET")
        return 6;

    if (!throws_as<std::invalid_argument>([&]
            {
                (void) MetricsManager::CreateCounter(
                  registry, "invalid-name", "Invalid metric");
            }))
        return 7;

    if (!throws_as<std::invalid_argument>([]
            { MetricsManager::SetDefaultLabels({{"bad-name", "value"}}); }))
        return 8;

    // Prometheus label VALUES support UTF-8 (names still have ASCII rules).
    MetricsManager::SetDefaultLabels(
      {{"service", "vex"}, {"env", "test"}, {"city", u8"تهران"}});
    if (MetricsManager::GetDefaultLabels().at("city") != u8"تهران")
        return 21;

    auto& request_counter = MetricsManager::CreateCounter(
      registry, "vex_monitoring_contract_requests_total", "Contract requests");
    request_counter.Increment();
    if (request_counter.Value() != 1.0)
        return 9;

    // A public type-safe macro must work without 'using namespace vex'.
    auto& type_safe_gauge = SAFE_CREATE_GAUGE(
      registry, "vex_monitoring_contract_gauge", "Contract gauge");
    type_safe_gauge.Set(3.0);
    if (type_safe_gauge.Value() != 3.0)
        return 10;

    HealthCheck::RegisterHealthMetrics(registry);
    if (!HealthCheck::IsRegistered())
        return 11;
    // Re-registering on the same registry must not duplicate metric families.
    HealthCheck::RegisterHealthMetrics(registry);
    HealthCheck::SetHealthy(false);
    HealthCheck::UpdateMemoryUsage(4096);
    HealthCheck::UpdateUptime();

    std::atomic<bool> start{false};
    std::atomic<int> errors{0};
    std::vector<std::thread> workers;
    for (int t = 0; t < 8; ++t)
    {
        workers.emplace_back([&, t]
        {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (int n = 0; n < 2000; ++n)
            {
                try
                {
                    const auto labels = MetricsManager::MergeLabels(
                      {{"iteration", std::to_string(n)}});
                    if (labels.at("service") != "vex")
                        ++errors;
                    if (!MetricsManager::GetRegistry())
                        ++errors;
                    if (!MetricsManager::GetSubsystemRegistry("monitoring_contract"))
                        ++errors;
                    if (MetricsManager::GetUptimeSeconds() < 0)
                        ++errors;
                    HealthCheck::SetHealthy((t & 1) == 0);
                    HealthCheck::UpdateUptime();
                }
                catch (...)
                {
                    ++errors;
                }
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (auto& worker: workers)
        worker.join();
    if (errors.load() != 0)
        return 12;

    // Drop client references so only MetricsManager owns its registry.
    registry.reset();
    subsystem.reset();
    MetricsManager::Reset();
    if (MetricsManager::IsInitialized())
        return 13;
    if (HealthCheck::IsRegistered())
        return 14;
    if (!throws_as<std::runtime_error>([]
            { HealthCheck::SetHealthy(true); }))
        return 15;
    if (!throws_as<std::runtime_error>([]
            { (void) MetricsManager::GetRegistry(); }))
        return 16;

    if (!MetricsManager::Init())
        return 17;
    registry = MetricsManager::GetRegistry();
    HealthCheck::RegisterHealthMetrics(registry);
    if (!HealthCheck::IsRegistered())
        return 18;
    HealthCheck::SetHealthy(true);
    registry.reset();
    MetricsManager::Reset();
    return 0;
}
