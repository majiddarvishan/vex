#include <vex/monitoring/monitoring.hpp>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

using namespace vex;

// Simulate an operation that might fail
int RiskyDatabaseQuery(int id)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1, 10);

    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(10 + dis(gen) * 5));

    // 20% chance of failure
    if (dis(gen) <= 2)
    {
        throw std::runtime_error("Database connection failed");
    }

    return id * 100;
}

int main()
{
    std::cout << "=== Metric Decorators Example ===\n\n";

    if (MetricsManager::Init())
    {
        std::cout << "✓ MetricsManager initialized\n";
    }

    auto registry = MetricsManager::GetRegistry();

    // Create metrics
    auto& duration_histogram = MetricsManager::CreateHistogram(
      registry,
      "db_query_duration_seconds",
      "Database query duration",
      {},
      {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0}
    );

    auto& success_counter = MetricsManager::
      CreateCounter(registry, "db_queries_success_total", "Successful queries");

    auto& failure_counter = MetricsManager::
      CreateCounter(registry, "db_queries_failure_total", "Failed queries");

    // Create combined tracker
    TimedResultTracker
      tracker(duration_histogram, success_counter, failure_counter);

    // Expose metrics
    prometheus::Exposer exposer {"127.0.0.1:8080"};
    MetricsManager::RegisterAllWithExposer(exposer);
    std::cout << "Metrics exposed on http://localhost:8080/metrics\n\n";

    // Run queries
    std::cout << "Running database queries...\n";
    for (int i = 1; i <= 20; ++i)
    {
        try
        {
            auto result = tracker.Track([i]() { return RiskyDatabaseQuery(i); }
            );
            std::cout << "  Query " << i << ": ✓ Success (result=" << result
                      << ")\n";
        }
        catch (const std::exception& e)
        {
            std::cout << "  Query " << i << ": ✗ Failed (" << e.what() << ")\n";
        }
    }

    std::cout << "\n✓ All queries tracked with timing and success/failure!\n";
    std::cout << "Check metrics at http://localhost:8080/metrics\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}