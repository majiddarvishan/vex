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
    auto& counters = prometheus::BuildCounter()
        .Name("vex_scoped_ephemeral_counter_total")
        .Help("Ephemeral counters")
        .Register(*registry);
    auto& gauges = prometheus::BuildGauge()
        .Name("vex_scoped_ephemeral_gauge")
        .Help("Ephemeral gauges")
        .Register(*registry);

    const prometheus::Labels common{{"case", "shared"}};
    {
        ScopedCounter first(counters, common);
        {
            ScopedCounter second(counters, common);
            if (first.get() != second.get())
                return 2;
            second->Increment();
        }
        if (!counters.Has(common))
            return 3;
        first->Increment();
        if (first->Value() != 2.0)
            return 4;
    }
    if (counters.Has(common))
        return 5;

    const prometheus::Labels first_labels{{"case", "first"}};
    const prometheus::Labels second_labels{{"case", "second"}};
    {
        ScopedGauge first(gauges, first_labels);
        ScopedGauge second(gauges, second_labels);
        first = std::move(second);
        if (gauges.Has(first_labels) || !gauges.Has(second_labels))
            return 6;
        if (second.get() || !first.get())
            return 7;
    }
    if (gauges.Has(second_labels))
        return 8;

    // Concurrency: N guards on the same family/labels are all live
    // before any one of them can release the shared metric.
    constexpr int threads = 8;
    std::atomic<int> ready{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    const prometheus::Labels labels{{"case", "parallel"}};
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
            if (!gauges.Has(labels))
                ++failures;
        });
    }
    for (auto& worker : workers)
        worker.join();
    if (failures.load() || gauges.Has(labels))
        return 9;

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
