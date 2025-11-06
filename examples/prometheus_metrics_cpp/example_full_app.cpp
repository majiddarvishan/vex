#include <prometheus_cpp_wrapper/monitoring.hpp>

#include <iostream>

using namespace vex::metrics;

int main() {
    // Initialize
    MetricsManager::Init();
    auto registry = MetricsManager::GetRegistry();

    // Health checks
    HealthCheck::RegisterHealthMetrics(registry);
    HealthCheck::SetHealthy(true);

    // Create metrics
    auto& counter = SAFE_CREATE_COUNTER(registry, "app_events_total", "Events", {});
    counter.Increment(100);

    // Update health
    HealthCheck::UpdateUptime();

    std::cout << "Application metrics initialized\n";
    std::cout << "Total events: " << counter.Value() << std::endl;

    // Start exposer
    prometheus::Exposer exposer{"0.0.0.0:8080"};
    MetricsManager::RegisterAllWithExposer(exposer);

    std::cout << "Metrics available at http://localhost:8080/metrics\n";
    std::cout << "Press Ctrl+C to exit\n";

    // Keep alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        HealthCheck::UpdateUptime();
    }

    return 0;
}