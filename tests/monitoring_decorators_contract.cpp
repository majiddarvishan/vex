#include <vex/monitoring/monitoring.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
template <typename Fn>
bool is_runtime_error(Fn&& fn)
{
    try
    {
        fn();
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
    return false;
}
} // namespace

int main()
{
    using namespace vex;
    static_assert(!std::is_copy_constructible_v<ScopedTimer>);
    static_assert(!std::is_move_constructible_v<ScopedTimer>);

    if (!MetricsManager::Init())
        return 1;
    const auto registry = MetricsManager::GetRegistry();
    auto& histogram = MetricsManager::CreateHistogram(
      registry, "vex_decorators_duration_seconds",
      "Duration samples", {}, {0.001, 0.01, 0.1, 1.0}
    );
    auto& successes = MetricsManager::CreateCounter(
      registry, "vex_decorators_success_total", "Success count"
    );
    auto& failures = MetricsManager::CreateCounter(
      registry, "vex_decorators_failure_total", "Failure count"
    );

    const auto start = histogram.Collect().histogram.sample_count;
    TimeFunction(histogram, [] {});
    if (histogram.Collect().histogram.sample_count != start + 1)
        return 2;

    auto value = TimeFunction(histogram, []
    {
        return std::make_unique<int>(42);
    });
    if (!value || *value != 42)
        return 3;

    int original = 8;
    auto& reference = TimeFunction(histogram, [&]() -> int&
    {
        return original;
    });
    reference = 9;
    if (original != 9)
        return 4;

    if (!is_runtime_error([&]
        {
            TimeFunction(histogram, []() -> void
            {
                throw std::runtime_error("timed failure");
            });
        }))
        return 5;

    const auto [moved, duration] =
      TimeFunctionWithDuration(histogram, []
      {
          return std::make_unique<int>(11);
      });
    if (!moved || *moved != 11 || duration < 0)
        return 6;

    const double void_duration =
      TimeFunctionWithDuration(histogram, [] {});
    if (void_duration < 0)
        return 7;

    auto [return_reference, ref_duration] =
      TimeFunctionWithDuration(histogram, [&]() -> int& { return original; });
    static_assert(std::is_same_v<
        decltype(TimeFunctionWithDuration(
            std::declval<prometheus::Histogram&>(),
            std::declval<int& (*)()>()
        )), std::pair<int&, double>>);
    return_reference = 15;
    if (original != 15 || ref_duration < 0)
        return 8;

    if (!is_runtime_error([&]
        {
            (void) TimeFunctionWithDuration(histogram, []() -> int
            {
                throw std::runtime_error("with duration failed");
            });
        }))
        return 9;

    ResultTracker plain(successes, failures);
    plain.Track([] {});
    plain.TrackVoid([] {});
    if (*plain.Track([] { return std::make_unique<int>(8); }) != 8)
        return 10;
    int& tracked_ref = plain.Track([&]() -> int& { return original; });
    tracked_ref = 16;
    if (original != 16)
        return 11;
    if (!is_runtime_error([&]
        {
            plain.Track([]() -> int
            {
                throw std::runtime_error("failure");
            });
        }))
        return 12;

    TimedResultTracker timed(histogram, successes, failures);
    timed.Track([] {});
    timed.TrackVoid([] {});
    if (*timed.Track([] { return std::make_unique<int>(19); }) != 19)
        return 13;
    int& timed_ref = timed.Track([&]() -> int& { return original; });
    timed_ref = 17;
    if (original != 17)
        return 14;
    if (!is_runtime_error([&]
        {
            timed.Track([]() -> int
            {
                throw std::runtime_error("timed fail");
            });
        }))
        return 15;

    // All timed operations (including exceptions) must be observed once.
    if (histogram.Collect().histogram.sample_count != start + 13)
        return 16;
    if (successes.Value() != 8 || failures.Value() != 2)
        return 17;

    MetricsManager::Reset();
    return 0;
}
