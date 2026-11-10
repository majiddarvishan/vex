#include <vex/monitoring/monitoring.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace vex;

void SimulateRequest(
  prometheus::Counter& total_requests,
  prometheus::Gauge& in_flight
)
{
    // A persistent counter is correct for requests_total. Do NOT remove
    // a cumulative counter's series on each scope exit.
    total_requests.Increment();
    ScopedInFlightGauge active(in_flight);

    std::cout << "Processing request...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // In-flight gauge decrements here; total_requests retains its value.
}

int main()
{
    if (MetricsManager::Init())
        std::cout << "MetricsManager initialized\n";

    auto registry = MetricsManager::GetRegistry();
    auto& total = MetricsManager::CreateCounter(
      registry, "requests_total", "Cumulative requests processed"
    );
    auto& active = MetricsManager::CreateGauge(
      registry, "active_requests", "Current active requests"
    );
    for (int i = 0; i < 5; ++i)
        SimulateRequest(total, active);

    std::cout << "Total requests: " << total.Value()
              << ", active: " << active.Value() << "\n";

    prometheus::Exposer exposer {"127.0.0.1:8080"};
    MetricsManager::RegisterAllWithExposer(exposer);
    std::cout << "Metrics exposed on http://localhost:8080/metrics\n";
    std::this_thread::sleep_for(std::chrono::hours(1));
    return 0;
}
