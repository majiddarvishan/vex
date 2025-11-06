#include <prometheus_cpp_wrapper/monitoring.hpp>

#include <iostream>
#include <thread>
#include <chrono>

using namespace vex::metrics;

int main() {
    MetricsManager::Init();
    auto registry = MetricsManager::GetRegistry();

    auto& requests = SAFE_CREATE_COUNTER(
        registry,
        "http_requests_total",
        "Total HTTP requests",
        {}
    );

    auto& duration = SAFE_CREATE_HISTOGRAM(
        registry,
        "http_request_duration_seconds",
        "Request duration",
        {},
        {0.001, 0.01, 0.1, 1.0}
    );

    // Simulate requests
    for (int i = 0; i < 10; ++i) {
        auto result = TimeFunction(duration, [&]() {
            requests.Increment();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i;
        });
        std::cout << "Request " << result << " completed\n";
    }

    std::cout << "Total requests: " << requests.Value() << std::endl;
    return 0;
}