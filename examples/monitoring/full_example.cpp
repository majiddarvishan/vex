#include <vex/monitoring/monitoring.hpp>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

using namespace vex;

class WebServer
{
public:
    WebServer()
    {
        // Initialize metrics
        auto registry = MetricsManager::GetRegistry();

        // Type-safe metric creation (compile-time validated)
        duration_histogram_ = &SAFE_CREATE_HISTOGRAM(
          registry,
          "request_duration_seconds",
          "HTTP request duration",
          {},
          {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0}
        );

        success_counter_ = &SAFE_CREATE_COUNTER(
          registry, "requests_success_total", "Successful requests", {}
        );

        failure_counter_ = &SAFE_CREATE_COUNTER(
          registry, "requests_failure_total", "Failed requests", {}
        );

        // Register health metrics
        HealthCheck::RegisterHealthMetrics(registry);
        HealthCheck::SetHealthy(true);
    }

    void HandleRequest(int request_id)
    {
        TimedResultTracker
          tracker(*duration_histogram_, *success_counter_, *failure_counter_);

        try
        {
            tracker.TrackVoid(
              [this, request_id]() { ProcessRequest(request_id); }
            );
        }
        catch (const std::exception& e)
        {
            std::cout << "  Request " << request_id << " failed: " << e.what()
                      << "\n";
        }
    }

    void UpdateHealth()
    {
        HealthCheck::UpdateUptime();
        HealthCheck::UpdateMemoryUsage(1024 * 1024 * 64); // 64 MB
    }

private:
    void ProcessRequest(int id)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1, 10);

        // Simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen) * 10));

        // 10% failure rate
        if (dis(gen) == 1)
        {
            throw std::runtime_error("Request processing failed");
        }

        std::cout << "  Request " << id << " processed successfully\n";
    }

    prometheus::Histogram* duration_histogram_;
    prometheus::Counter* success_counter_;
    prometheus::Counter* failure_counter_;
};

int main()
{
    std::cout << "=== Full Featured Metrics Example ===\n\n";

    // Initialize (single-threaded by default)
    if (MetricsManager::Init())
    {
        std::cout << "✓ MetricsManager initialized\n";
    }

    std::cout << "✓ MetricsManager initialized (single-threaded)\n";
    std::cout << "✓ Threading enabled: "
              << (MetricsManager::IsThreadingEnabled() ? "Yes" : "No")
              << "\n\n";

    // Create web server with metrics
    WebServer server;

    // Expose metrics
    prometheus::Exposer exposer {"127.0.0.1:8080"};
    MetricsManager::RegisterAllWithExposer(exposer);
    std::cout << "✓ Metrics exposed on http://localhost:8080/metrics\n\n";

    // Simulate traffic
    std::cout << "Simulating web traffic...\n";
    for (int i = 1; i <= 30; ++i)
    {
        server.HandleRequest(i);

        // Update health every 5 requests
        if (i % 5 == 0)
        {
            server.UpdateHealth();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n✓ Simulation complete!\n";
    std::cout << "✓ All metrics tracked:\n";
    std::cout << "  - Request duration (histogram)\n";
    std::cout << "  - Success/failure counters\n";
    std::cout << "  - Health status\n";
    std::cout << "  - Uptime\n";
    std::cout << "  - Memory usage\n\n";

    std::cout << "View metrics at: http://localhost:8080/metrics\n";
    std::cout << "Press Ctrl+C to exit...\n";

    std::this_thread::sleep_for(std::chrono::hours(1));
    return 0;
}