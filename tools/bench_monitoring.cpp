// Monitoring registration/update/collection baseline. No network, no
// externally hosted services, no hardcoded performance conclusions.
#include <vex/monitoring/monitoring.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
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

void measure(std::size_t series, bool many_families, bool managed)
{
    using vex::MetricsManager;

    auto registry = std::make_shared<prometheus::Registry>();
    std::vector<prometheus::Counter*> counters;
    counters.reserve(series);
    const auto begin = Clock::now();

    for (std::size_t i = 0; i < series; ++i)
    {
        const auto name = many_families
          ? "vex_bench_family_" + std::to_string(i) + "_total"
          : "vex_bench_requests_total";
        const std::map<std::string, std::string> labels = many_families
          ? std::map<std::string, std::string> {}
          : std::map<std::string, std::string> {
              {"status", std::to_string(i)}
            };

        prometheus::Counter* counter = nullptr;
        if (managed)
        {
            counter = &MetricsManager::CreateCounter(
              registry, name, "Synthetic benchmark counter", labels
            );
        }
        else
        {
            // Same Registry, HELP, labels and Counter type, but bypass
            // MetricsManager's registry/series tracking. This isolates
            // Vex wrapper overhead from upstream Registry::Add.
            auto& family = prometheus::BuildCounter()
              .Name(name).Help("Synthetic benchmark counter")
              .Register(*registry);
            counter = &family.Add(labels);
        }
        counters.push_back(counter);
    }
    const double registration_ms = milliseconds(begin);

    constexpr std::size_t updates_per_series = 100;
    const auto update_begin = Clock::now();
    for (std::size_t n = 0; n < updates_per_series; ++n)
        for (auto* counter: counters)
            counter->Increment();
    const double update_ms = milliseconds(update_begin);

    const auto collect_begin = Clock::now();
    const auto families = registry->Collect();
    const double collect_ms = milliseconds(collect_begin);

    std::size_t samples = 0;
    for (const auto& family: families)
        samples += family.metric.size();

    const std::size_t expected_families = many_families ? series : 1;
    if (families.size() != expected_families || samples != series)
        throw std::runtime_error(
          "Benchmark shapes are not equivalent across implementations"
        );

    std::cout << (managed ? "" : "raw_")
              << (many_families ? "many_families" : "many_series")
              << "," << series << ","
              << std::fixed << std::setprecision(3)
              << registration_ms << ","
              << (series * updates_per_series) << ","
              << update_ms << "," << collect_ms << ","
              << families.size() << "," << samples << "\n";
}
} // namespace

int main(int argc, char** argv)
{
    std::size_t repeats = 3;
    if (argc > 2)
    {
        std::cerr << "Usage: vex_monitoring_benchmark [1|3|5]\n";
        return 2;
    }
    if (argc == 2)
    {
        const std::string argument = argv[1];
        if (argument != "1" && argument != "3" && argument != "5")
        {
            std::cerr << "Usage: vex_monitoring_benchmark [1|3|5]\n";
            return 2;
        }
        repeats = static_cast<std::size_t>(std::stoi(argument));
    }

    (void) vex::MetricsManager::Init(true);
    std::cout << "shape,series,registration_ms,update_operations,"
              << "update_ms,collect_ms,families,samples\n";
    for (std::size_t run = 0; run < repeats; ++run)
    {
        for (std::size_t series : {10U, 100U, 1000U})
        {
            // Paired workloads share the exact same metric schemas.
            // Both paths use fresh Registry objects in each measurement.
            measure(series, false, true);
            measure(series, false, false);
            measure(series, true, true);
            measure(series, true, false);
        }
    }
    vex::MetricsManager::Reset();
    return 0;
}
