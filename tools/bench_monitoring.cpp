// Monitoring-only baseline benchmark. No external service or network.
// Output is empirical; no hardcoded performance claims.
#include <vex/monitoring/monitoring.hpp>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

double milliseconds(Clock::time_point begin)
{
    return std::chrono::duration<double, std::milli>(
      Clock::now() - begin).count();
}

void measure(std::size_t series)
{
    using vex::MetricsManager;
    auto registry = std::make_shared<prometheus::Registry>();
    const auto begin = Clock::now();

    for (std::size_t i = 0; i < series; ++i)
    {
        auto& counter = MetricsManager::CreateCounter(
          registry, "vex_benchmark_requests_total",
          "Synthetic benchmark requests",
          {{"status", std::to_string(i)}}
        );
        counter.Increment();
    }
    const double register_ms = milliseconds(begin);
    const auto scrape_begin = Clock::now();
    const auto families = registry->Collect();
    const double scrape_ms = milliseconds(scrape_begin);

    std::size_t samples = 0;
    for (const auto& family: families)
        samples += family.metric.size();

    std::cout << series << "," << std::fixed
              << std::setprecision(3) << register_ms << ","
              << scrape_ms << "," << families.size() << ","
              << samples << "\n";
}
}

int main(int argc, char** argv)
{
    std::size_t runs = 1;
    if (argc > 1)
    {
        const std::string arg = argv[1];
        if (arg != "1" && arg != "3" && arg != "5")
        {
            std::cerr << "Usage: vex_monitoring_benchmark [1|3|5]\n";
            return 2;
        }
        runs = static_cast<std::size_t>(std::stoi(arg));
    }
    (void) vex::MetricsManager::Init(true);
    std::cout << "series,registration_ms,collect_ms,families,samples\n";
    for (std::size_t r = 0; r < runs; ++r)
        for (const auto count : {10U, 100U, 1000U})
            measure(count);
    vex::MetricsManager::Reset();
    return 0;
}
