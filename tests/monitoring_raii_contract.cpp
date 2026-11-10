#include <vex/monitoring/monitoring.hpp>

#include <atomic>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

int main()
{
    using namespace vex;
    if (!MetricsManager::Init())
        return 1;
    const auto registry = MetricsManager::GetRegistry();
    // Includes both defaults and any dynamic container labels. Guard
    // collision checks must work in real CI/container environments too.
    MetricsManager::SetDefaultLabels({{"service", "vex_scoped_test"}});
    auto& counters = prometheus::BuildCounter()
        .Name("vex_scoped_ephemeral_counter_total")
        .Help("Ephemeral counters")
        .Register(*registry);
    auto& gauges = prometheus::BuildGauge()
        .Name("vex_scoped_ephemeral_gauge")
        .Help("Ephemeral gauges")
        .Register(*registry);

    const prometheus::Labels common{{"case", "shared"}};
    const auto merged_common = MetricsManager::MergeLabels(common);
    {
        ScopedCounter first(counters, common);
        {
            ScopedCounter second(counters, common);
            if (first.get() != second.get())
                return 2;
            second->Increment();
        }
        if (!counters.Has(merged_common))
            return 3;
        first->Increment();
        if (first->Value() != 2.0)
            return 4;
    }
    if (counters.Has(merged_common))
        return 5;

    // Guard creation must never adopt a pre-existing, externally
    // managed series: the guard's destructor would otherwise remove it.
    const prometheus::Labels external_labels{{"case", "persistent"}};
    const auto merged_external = MetricsManager::MergeLabels(external_labels);
    auto& external_counter = counters.Add(merged_external);
    external_counter.Increment();
    bool counter_rejected = false;
    try
    {
        ScopedCounter wrong_owner(counters, external_labels);
    }
    catch (const std::invalid_argument&)
    {
        counter_rejected = true;
    }
    if (!counter_rejected || !counters.Has(merged_external) ||
        external_counter.Value() != 1.0)
        return 14;

    auto& external_gauge = gauges.Add(merged_external);
    external_gauge.Set(7.0);
    bool gauge_rejected = false;
    try
    {
        ScopedGauge wrong_owner(gauges, external_labels);
    }
    catch (const std::invalid_argument&)
    {
        gauge_rejected = true;
    }
    if (!gauge_rejected || !gauges.Has(merged_external) ||
        external_gauge.Value() != 7.0)
        return 15;

    const prometheus::Labels first_labels{{"case", "first"}};
    const prometheus::Labels second_labels{{"case", "second"}};
    const auto merged_first = MetricsManager::MergeLabels(first_labels);
    const auto merged_second = MetricsManager::MergeLabels(second_labels);
    {
        ScopedGauge first(gauges, first_labels);
        ScopedGauge second(gauges, second_labels);
        first = std::move(second);
        if (gauges.Has(merged_first) || !gauges.Has(merged_second))
            return 6;
        if (second.get() || !first.get())
            return 7;
    }
    if (gauges.Has(merged_second))
        return 8;

    // Concurrency: N guards on the same family/labels are all live
    // before any one of them can release the shared metric.
    constexpr int threads = 8;
    std::atomic<int> ready{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    const prometheus::Labels labels{{"case", "parallel"}};
    const auto merged_parallel = MetricsManager::MergeLabels(labels);
    for (int i = 0; i < threads; ++i)
    {
        workers.emplace_back([&]
        {
            ScopedGauge g(gauges, labels);
            ready.fetch_add(1, std::memory_order_acq_rel);
            while (ready.load(std::memory_order_acquire) < threads)
                std::this_thread::yield();
            for (int j = 0; j < 500; ++j)
                g->Increment();
            if (!gauges.Has(merged_parallel))
                ++failures;
        });
    }
    for (auto& worker : workers)
        worker.join();
    if (failures.load() || gauges.Has(merged_parallel))
        return 9;

    // Live default-label reconfiguration must never change a guard's
    // ownership between Family::Has and Family::Add. Pre-create the
    // "west" series outside the guard domain: workers may use the
    // transient "east" series, but must NEVER adopt/delete "west".
    MetricsManager::SetDefaultLabels({
      {"service", "vex_scoped_test"}, {"zone", "west"}
    });
    const prometheus::Labels changing{{"case", "label_switch"}};
    const auto persistent_label_set = MetricsManager::MergeLabels(changing);
    auto& persistent = gauges.Add(persistent_label_set);
    persistent.Set(23.0);

    std::atomic<int> unexpected_label_errors{0};
    std::atomic<int> accepted_guards{0};
    std::atomic<bool> begin_label_race{false};
    std::vector<std::thread> reconfig_workers;
    std::thread reconfigure([&]
    {
        while (!begin_label_race.load(std::memory_order_acquire))
            std::this_thread::yield();
        for (int i = 0; i < 1200; ++i)
        {
            MetricsManager::SetDefaultLabels({
              {"service", "vex_scoped_test"},
              {"zone", (i % 2 == 0) ? "east" : "west"}
            });
        }
    });
    for (int i = 0; i < 4; ++i)
    {
        reconfig_workers.emplace_back([&]
        {
            while (!begin_label_race.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (int j = 0; j < 300; ++j)
            {
                try
                {
                    ScopedGauge scoped(gauges, changing);
                    scoped->Increment();
                    ++accepted_guards;
                }
                catch (const std::invalid_argument&)
                {
                    // The west series is externally owned: rejecting an
                    // attempted scoped adoption is correct and expected.
                }
                catch (...)
                {
                    ++unexpected_label_errors;
                }
            }
        });
    }
    begin_label_race.store(true, std::memory_order_release);
    reconfigure.join();
    for (auto& worker : reconfig_workers)
        worker.join();
    if (unexpected_label_errors != 0 || accepted_guards == 0 ||
        !gauges.Has(persistent_label_set) || persistent.Value() != 23.0)
        return 16;
    MetricsManager::SetDefaultLabels({{"service", "vex_scoped_test"}});

    // The snapshot helper rejects invalid labels instead of passing them
    // directly through prometheus-cpp Family::Add.
    bool invalid_label_rejected = false;
    try
    {
        ScopedGauge invalid(gauges, {{"invalid-label", "value"}});
    }
    catch (const std::invalid_argument&)
    {
        invalid_label_rejected = true;
    }
    if (!invalid_label_rejected)
        return 17;

    auto& live = MetricsManager::CreateGauge(
      registry, "vex_inflight_requests", "In-flight requests"
    );
    {
        ScopedInFlightGauge one(live);
        if (live.Value() != 1.0)
            return 10;
        {
            ScopedInFlightGauge two(live);
            auto three = std::move(two);
            if (live.Value() != 2.0)
                return 11;
        }
        if (live.Value() != 1.0)
            return 12;
    }
    if (live.Value() != 0.0)
        return 13;

    MetricsManager::Reset();
    return 0;
}
