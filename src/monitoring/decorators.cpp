#include <vex/monitoring/decorators.hpp>

namespace vex
{

    // ============================================================================
    // ScopedTimer Implementation
    // ============================================================================

    ScopedTimer::ScopedTimer(prometheus::Histogram& histogram)
        : histogram_(histogram), start_(std::chrono::steady_clock::now())
    {
    }

    ScopedTimer::~ScopedTimer()
    {
        auto duration  = std::chrono::steady_clock::now() - start_;
        double seconds = std::chrono::duration<double>(duration).count();
        histogram_.Observe(seconds);
    }

    double ScopedTimer::ElapsedSeconds() const
    {
        auto duration = std::chrono::steady_clock::now() - start_;
        return std::chrono::duration<double>(duration).count();
    }

    // ============================================================================
    // ResultTracker Implementation
    // ============================================================================

    ResultTracker::ResultTracker(
      // Public positional constructor contract: callers select a success
      // Counter followed by a failure Counter. A strong wrapper here would
      // break the established API without adding meaningful type safety.
      // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
      prometheus::Counter& success_counter, prometheus::Counter& failure_counter
    )
        : success_(success_counter), failure_(failure_counter)
    {
    }

    // ============================================================================
    // TimedResultTracker Implementation
    // ============================================================================

    TimedResultTracker::TimedResultTracker(
      prometheus::Histogram& duration_histogram,
      // Preserve the public success-then-failure positional contract.
      // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
      prometheus::Counter& success_counter, prometheus::Counter& failure_counter
    )
        : histogram_(duration_histogram),
          success_(success_counter),
          failure_(failure_counter)
    {
    }

} // namespace vex