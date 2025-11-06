#include <prometheus_cpp_wrapper/monitoring.hpp>

#include <iostream>
#include <thread>

using namespace vex::metrics;

int main() {
    MetricsManager::Init();
    auto db_registry = MetricsManager::GetSubsystemRegistry("database");

    auto& query_duration = SAFE_CREATE_HISTOGRAM(
        db_registry,
        "query_duration_seconds",
        "Query duration",
        {},
        {0.001, 0.01, 0.1, 1.0}
    );

    auto& query_success = SAFE_CREATE_COUNTER(
        db_registry,
        "queries_success_total",
        "Successful queries",
        {}
    );

    auto& query_failure = SAFE_CREATE_COUNTER(
        db_registry,
        "queries_failure_total",
        "Failed queries",
        {}
    );

    // Simulate queries
    TimedResultTracker tracker(query_duration, query_success, query_failure);

    for (int i = 0; i < 20; ++i) {
        try {
            tracker.Track([i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                if (i % 10 == 0) throw std::runtime_error("Query failed");
            });
        } catch (...) {
            std::cout << "Query " << i << " failed\n";
        }
    }

    std::cout << "Success: " << query_success.Value() << "\n";
    std::cout << "Failure: " << query_failure.Value() << std::endl;

    return 0;
}