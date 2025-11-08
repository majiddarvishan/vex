#include <vex/monitoring/monitoring.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace vex;

int main()
{
    std::cout << "=== Basic Metrics Example ===\n\n";

    // Initialize metrics manager
    if (MetricsManager::Init())
    {
        std::cout << "✓ MetricsManager initialized\n";
    }

    // Get main registry
    auto registry = MetricsManager::GetRegistry();

    // Create metrics
    auto& request_counter = MetricsManager::CreateCounter(
      registry,
      "http_requests_total",
      "Total HTTP requests",
      {
        {"method", "GET"}
    }
    );

    auto& active_connections = MetricsManager::CreateGauge(
      registry, "active_connections", "Number of active connections"
    );

    // Use metrics
    std::cout << "\nSimulating traffic...\n";
    for (int i = 0; i < 10; ++i)
    {
        request_counter.Increment();
        active_connections.Set(i % 5 + 1);
        std::cout << "  Request " << (i + 1) << " processed\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Expose metrics
    std::cout << "\n✓ Metrics exposed on http://localhost:8080/metrics\n";
    prometheus::Exposer exposer {"127.0.0.1:8080"};
    MetricsManager::RegisterAllWithExposer(exposer);

    std::cout << "\nPress Ctrl+C to exit...\n";
    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}
