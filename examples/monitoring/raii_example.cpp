#include <vex/monitoring/monitoring.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace vex;

void SimulateRequest(prometheus::Family<prometheus::Counter>& counter_family)
{
    // Create scoped counter - automatically removed when function exits
    ScopedCounter counter(
      counter_family,
      {
        {"endpoint", "/api/users"}
    }
    );

    std::cout << "Processing request...\n";
    counter->Increment();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Counter automatically removed here when scope ends
}

int main()
{
    std::cout << "=== RAII Metrics Example ===\n\n";

    if (MetricsManager::Init())
    {
        std::cout << "✓ MetricsManager initialized\n";
    }

    auto registry = MetricsManager::GetRegistry();

    // Create counter family
    auto& counter_family = prometheus::BuildCounter()
                             .Name("requests_total")
                             .Help("Total requests by endpoint")
                             .Register(*registry);

    std::cout << "Simulating 5 requests with RAII counters...\n";
    for (int i = 0; i < 5; ++i)
    {
        SimulateRequest(counter_family);
    }

    std::cout << "\n✓ All metrics automatically cleaned up!\n";

    // Expose metrics
    prometheus::Exposer exposer {"127.0.0.1:8080"};
    MetricsManager::RegisterAllWithExposer(exposer);

    std::cout << "Metrics exposed on http://localhost:8080/metrics\n";
    std::cout << "Press Ctrl+C to exit...\n";
    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}
