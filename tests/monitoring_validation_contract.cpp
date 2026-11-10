#include <vex/monitoring/monitoring.hpp>

#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
template <typename E, typename F>
bool throws_as(F&& f)
{
    try { f(); }
    catch (const E&) { return true; }
    catch (...) {}
    return false;
}
}

int main()
{
    using vex::MetricsManager;
    if (!MetricsManager::Init())
        return 1;
    auto registry = MetricsManager::GetRegistry();

    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateCounter(
              {}, "valid_name", "null registry");
        }))
        return 2;
    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateGauge(
              registry, "__invalid", "reserved");
        }))
        return 3;
    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateGauge(
              registry, "valid_name", "labels",
              {{"bad-label", "value"}});
        }))
        return 4;
    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateHistogram(
              registry, "vex_invalid_hist", "buckets",
              {}, {0.5, 0.25});
        }))
        return 5;
    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateHistogram(
              registry, "vex_invalid_hist", "buckets",
              {}, {0.5, std::numeric_limits<double>::infinity()});
        }))
        return 6;
    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateHistogram(
              registry, "vex_reserved_hist", "le label",
              {{"le", "0.5"}}, {0.1, 0.5});
        }))
        return 7;

    // The default is unlimited for full backward compatibility.
    if (MetricsManager::GetMaxSeriesPerMetric() != 0)
        return 8;
    MetricsManager::SetMaxSeriesPerMetric(2);
    const std::string name = "vex_bounded_requests_total";
    auto& a = MetricsManager::CreateCounter(
      registry, name, "Bounded family", {{"status", "a"}});
    auto& b = MetricsManager::CreateCounter(
      registry, name, "Bounded family", {{"status", "b"}});
    auto& again = MetricsManager::CreateCounter(
      registry, name, "Bounded family", {{"status", "a"}});
    if (&a != &again || &a == &b)
        return 9;
    if (!throws_as<std::length_error>([&]
        {
            (void) MetricsManager::CreateCounter(
              registry, name, "Bounded family", {{"status", "c"}});
        }))
        return 10;
    // Each Registry has an independent limit.
    auto second_registry = std::make_shared<prometheus::Registry>();
    if (!&MetricsManager::CreateCounter(
          second_registry, name, "Bounded family", {{"status", "c"}}))
        return 11;

    const auto& default_hist = MetricsManager::CreateHistogram(
      registry, "vex_default_histogram", "Default histogram");
    if (default_hist.Collect().histogram.bucket.size() < 5)
        return 12;

    (void) MetricsManager::CreateHistogram(
      registry, "vex_bucket_immutable", "Immutable buckets",
      {}, {0.1, 0.5});
    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateHistogram(
              registry, "vex_bucket_immutable", "Immutable buckets",
              {}, {0.1, 0.75});
        }))
        return 13;

    // Eight callers cannot exceed the configured two-series limit.
    std::atomic<int> admitted{0};
    std::atomic<int> rejected{0};
    std::atomic<int> other{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i)
    {
        threads.emplace_back([&, i]
        {
            try
            {
                (void) MetricsManager::CreateGauge(
                  registry, "vex_parallel_limit", "Concurrent quota",
                  {{"worker", std::to_string(i)}});
                ++admitted;
            }
            catch (const std::length_error&)
            {
                ++rejected;
            }
            catch (...)
            {
                ++other;
            }
        });
    }
    for (auto& thread : threads)
        thread.join();
    if (admitted != 2 || rejected != 6 || other != 0)
        return 14;

    MetricsManager::SetMaxSeriesPerMetric(0);
    (void) MetricsManager::CreateCounter(
      registry, name, "Bounded family", {{"status", "c"}});
    if (MetricsManager::GetMaxSeriesPerMetric() != 0)
        return 15;

    registry.reset();
    second_registry.reset();
    MetricsManager::Reset();
    if (MetricsManager::GetMaxSeriesPerMetric() != 0)
        return 16;
    return 0;
}
