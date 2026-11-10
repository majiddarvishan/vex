#include <vex/monitoring/monitoring.hpp>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    using vex::HealthCheck;
    using vex::MetricsManager;

    if (!MetricsManager::Init(true))
        return 1;

    // Registry handoff: every HealthCheck call either has an owning
    // registry snapshot or fails with the documented runtime_error.
    std::atomic<bool> stop{false};
    std::atomic<int> unexpected{0};
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i)
    {
        workers.emplace_back([&, i]
        {
            while (!stop.load(std::memory_order_acquire))
            {
                try
                {
                    HealthCheck::SetHealthy(i % 2 == 0);
                    HealthCheck::UpdateMemoryUsage(1024);
                    HealthCheck::UpdateUptime();
                }
                catch (const std::runtime_error&)
                {
                    // No live Registry after a handoff is a permitted
                    // failure; stale pointers and crashes are not.
                }
                catch (...)
                {
                    ++unexpected;
                }
            }
        });
    }

    for (int i = 0; i < 100; ++i)
    {
        {
            auto registry = std::make_shared<prometheus::Registry>();
            HealthCheck::RegisterHealthMetrics(registry);
            HealthCheck::SetHealthy(true);
            HealthCheck::UpdateMemoryUsage(2048);
        } // The only strong owner is dropped; the weak handle expires.
        (void) HealthCheck::IsRegistered();
    }

    // Manager control-plane reinitialization is synchronized. Callers
    // may receive runtime_error while no registry is initialized.
    std::atomic<int> get_failures{0};
    std::thread reader([&]
    {
        for (int i = 0; i < 1500; ++i)
        {
            try
            {
                auto registry = MetricsManager::GetRegistry();
                if (!registry)
                    ++unexpected;
            }
            catch (const std::runtime_error&)
            {
                ++get_failures;
            }
            catch (...)
            {
                ++unexpected;
            }
        }
    });
    for (int i = 0; i < 100; ++i)
    {
        MetricsManager::Reset();
        (void) MetricsManager::Init(true);
        (void) MetricsManager::GetUptimeSeconds();
    }
    reader.join();
    stop.store(true, std::memory_order_release);
    for (auto& worker : workers)
        worker.join();
    if (unexpected != 0)
        return 2;

    auto registry = MetricsManager::GetRegistry();
    HealthCheck::RegisterHealthMetrics(registry);
    if (!HealthCheck::IsRegistered())
        return 3;

    registry.reset();
    MetricsManager::Reset();
    if (HealthCheck::IsRegistered())
        return 4;
    return 0;
}
