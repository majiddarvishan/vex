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

    // Invalid UTF-8 must be rejected, not silently accepted as a label.
    const std::string invalid_utf8{static_cast<char>(0xC3), '('};
    if (!throws_as<std::invalid_argument>([&]
        {
            MetricsManager::SetDefaultLabels({{"bad", invalid_utf8}});
        }))
        return 22;
    const std::string surrogate{static_cast<char>(0xED),
                                static_cast<char>(0xA0),
                                static_cast<char>(0x80)};
    if (!throws_as<std::invalid_argument>([&]
        {
            MetricsManager::SetDefaultLabels({{"bad", surrogate}});
        }))
        return 23;
    const std::string del_character(1, static_cast<char>(0x7F));
    if (!throws_as<std::invalid_argument>([&]
        {
            MetricsManager::SetDefaultLabels({{"bad", del_character}});
        }))
        return 24;
    MetricsManager::SetDefaultLabels({{"city", u8"تهران"}});

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
    (void) MetricsManager::CreateCounter(
      second_registry, name, "Bounded family", {{"status", "c"}});

    auto& default_hist = MetricsManager::CreateHistogram(
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

    // A failed managed registration must not reserve quota or leave
    // an unusable shadow record that blocks a later valid registration.
    auto strict = std::make_shared<prometheus::Registry>(
      prometheus::Registry::InsertBehavior::Throw);
    auto& collision = prometheus::BuildGauge()
      .Name("vex_quota_registration_failure")
      .Help("External collision").Register(*strict);
    MetricsManager::SetMaxSeriesPerMetric(1);
    for (int retry = 0; retry < 10; ++retry)
    {
        if (!throws_as<std::exception>([&]
            {
                (void) MetricsManager::CreateGauge(
                  strict, "vex_quota_registration_failure",
                  "Managed retry", {{"kind", "one"}});
            }))
            return 25;
    }
    if (!strict->Remove(collision))
        return 26;
    (void) MetricsManager::CreateGauge(
      strict, "vex_quota_registration_failure",
      "Managed retry", {{"kind", "one"}});

    // Quota is consumed only by the successful creation above.
    if (!throws_as<std::length_error>([&]
        {
            (void) MetricsManager::CreateGauge(
              strict, "vex_quota_registration_failure",
              "Managed retry", {{"kind", "two"}});
        }))
        return 27;
    strict.reset();
    MetricsManager::SetMaxSeriesPerMetric(2);

    // prometheus-cpp Merge reuses Family metadata and does not reject
    // inconsistent HELP text; the manager must fail closed.
    auto help_registry = std::make_shared<prometheus::Registry>();
    (void) MetricsManager::CreateCounter(
      help_registry, "vex_consistent_help_total", "Original HELP",
      {{"result", "a"}});
    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateCounter(
              help_registry, "vex_consistent_help_total",
              "Different HELP", {{"result", "a"}});
        }))
        return 28;
    if (!throws_as<std::invalid_argument>([&]
        {
            (void) MetricsManager::CreateCounter(
              help_registry, "vex_consistent_help_total",
              "Different HELP", {{"result", "b"}});
        }))
        return 29;
    // Rejected registrations must not consume a series slot.
    (void) MetricsManager::CreateCounter(
      help_registry, "vex_consistent_help_total", "Original HELP",
      {{"result", "b"}});
    if (!throws_as<std::length_error>([&]
        {
            (void) MetricsManager::CreateCounter(
              help_registry, "vex_consistent_help_total", "Original HELP",
              {{"result", "c"}});
        }))
        return 30;

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

    if (MetricsManager::GetMaxSubsystemRegistries() != 0)
        return 17;
    MetricsManager::SetMaxSubsystemRegistries(1);
    if (!MetricsManager::GetSubsystemRegistry("first"))
        return 18;
    if (MetricsManager::GetSubsystemRegistry("first") !=
        MetricsManager::GetSubsystemRegistry("first"))
        return 19;
    if (!throws_as<std::length_error>([]
        {
            (void) MetricsManager::GetSubsystemRegistry("second");
        }))
        return 20;
    MetricsManager::SetMaxSubsystemRegistries(0);
    if (!MetricsManager::GetSubsystemRegistry("second"))
        return 21;

    // Owner-indexed ledger regression: expired external Registry
    // control blocks must not retain metrics, quota or HELP identity.
    // More than 256 distinct short-lived owners forces the amortized
    // expired-entry collection path to run.
    MetricsManager::SetMaxSeriesPerMetric(1);
    auto persistent_owner = std::make_shared<prometheus::Registry>();
    auto& persistent_series = MetricsManager::CreateCounter(
      persistent_owner, "vex_owner_index_total", "Persistent owner",
      {{"phase", "primary"}});
    persistent_series.Increment();

    for (int i = 0; i < 320; ++i)
    {
        std::weak_ptr<prometheus::Registry> weak_owner;
        {
            auto short_lived = std::make_shared<prometheus::Registry>();
            weak_owner = short_lived;
            auto& sample = MetricsManager::CreateCounter(
              short_lived, "vex_owner_index_total", "Transient owner",
              {{"phase", "primary"}});
            sample.Increment();
            if (sample.Value() != 1.0)
                return 31;
            if (i == 0 &&
                !throws_as<std::length_error>([&]
                    {
                        (void) MetricsManager::CreateCounter(
                          short_lived, "vex_owner_index_total",
                          "Transient owner", {{"phase", "secondary"}});
                    }))
                return 32;
        }
        // Vex's bookkeeping must NOT hold a strong Registry reference.
        if (!weak_owner.expired())
            return 33;
    }

    // The original live owner must not lose its signature or HELP after
    // the periodic sweep and the creation of hundreds of new owners.
    if (persistent_series.Value() != 1.0 ||
        !throws_as<std::length_error>([&]
            {
                (void) MetricsManager::CreateCounter(
                  persistent_owner, "vex_owner_index_total",
                  "Persistent owner", {{"phase", "secondary"}});
            }))
        return 34;

    auto fresh_owner = std::make_shared<prometheus::Registry>();
    // Different Registry owner with same name/labels but different HELP.
    // It must not inherit metadata from any already-expired Registry.
    (void) MetricsManager::CreateCounter(
      fresh_owner, "vex_owner_index_total", "Fresh owner",
      {{"phase", "primary"}});
    if (!throws_as<std::length_error>([&]
        {
            (void) MetricsManager::CreateCounter(
              fresh_owner, "vex_owner_index_total", "Fresh owner",
              {{"phase", "secondary"}});
        }))
        return 35;
    fresh_owner.reset();
    persistent_owner.reset();

    MetricsManager::SetMaxSeriesPerMetric(0);
    (void) MetricsManager::CreateCounter(
      registry, name, "Bounded family", {{"status", "c"}});
    if (MetricsManager::GetMaxSeriesPerMetric() != 0)
        return 15;

    registry.reset();
    second_registry.reset();
    MetricsManager::Reset();
    if (MetricsManager::GetMaxSeriesPerMetric() != 0 ||
        MetricsManager::GetMaxSubsystemRegistries() != 0)
        return 16;
    return 0;
}
