#include <vex/monitoring/monitoring.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace vex;

void UpdateHealthMetrics()
{
    // Simulate health monitoring
    static int counter = 0;

    // Toggle health status every 5 iterations
    bool healthy = (counter / 5) % 2 == 0;
    HealthCheck::SetHealthy(healthy);

    // Update uptime
    HealthCheck::UpdateUptime();

    // Simulate memory usage (in reality, you'd get this from system)
    size_t memory_bytes = 1024 * 1024 * (50 + counter % 20); // 50-70 MB
    HealthCheck::UpdateMemoryUsage(memory_bytes);

    std::cout << "Health: " << (healthy ? "✓ Healthy" : "✗ Unhealthy")
              << " | Uptime: " << MetricsManager::GetUptimeSeconds() << "s"
              << " | Memory: " << (memory_bytes / 1024 / 1024) << " MB\n";

    counter++;
}

int main()
{
    std::cout << "=== Health Check Example ===\n\n";

    if (MetricsManager::Init())
    {
        std::cout << "✓ MetricsManager initialized\n";
    }

    auto registry = MetricsManager::GetRegistry();

    // Register health metrics
    HealthCheck::RegisterHealthMetrics(registry);
    std::cout << "✓ Health metrics registered\n\n";

    // Expose metrics
    prometheus::Exposer exposer {"127.0.0.1:8080"};
    MetricsManager::RegisterAllWithExposer(exposer);
    std::cout << "Metrics exposed on http://localhost:8080/metrics\n\n";

    // Simulate health updates
    std::cout << "Monitoring health (updates every second)...\n";
    for (int i = 0; i < 20; ++i)
    {
        UpdateHealthMetrics();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\nDone! Check http://localhost:8080/metrics\n";
    return 0;
}
