#pragma once

#include <vex/monitoring/metrics_manager.hpp>

#include <chrono>
#include <functional>
#include <type_traits>
#include <utility>

namespace vex
{

    // Records one histogram observation on every exit path, including
    // exceptions. A timer cannot be copied/moved: each scope is unique.
    class ScopedTimer
    {
    public:
        explicit ScopedTimer(prometheus::Histogram& histogram);
        ~ScopedTimer();

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
        ScopedTimer(ScopedTimer&&) = delete;
        ScopedTimer& operator=(ScopedTimer&&) = delete;

        [[nodiscard]] double ElapsedSeconds() const;

    private:
        prometheus::Histogram& histogram_;
        std::chrono::steady_clock::time_point start_;
    };

    template <typename Func>
    auto TimeFunction(prometheus::Histogram& histogram, Func&& func)
      -> std::invoke_result_t<Func&&>
    {
        ScopedTimer timer(histogram);
        return std::invoke(std::forward<Func>(func));
    }

    // For a value/reference result, returns pair<result, elapsed_seconds>.
    // For a void result, returns elapsed_seconds. Timer observes even on throw.
    template <typename Func>
    auto TimeFunctionWithDuration(
      prometheus::Histogram& histogram, Func&& func
    )
    {
        using Result = std::invoke_result_t<Func&&>;
        ScopedTimer timer(histogram);
        if constexpr (std::is_void_v<Result>)
        {
            std::invoke(std::forward<Func>(func));
            return timer.ElapsedSeconds();
        }
        else
        {
            decltype(auto) result = std::invoke(std::forward<Func>(func));
            const double elapsed = timer.ElapsedSeconds();
            return std::pair<Result, double>(
              std::forward<Result>(result), elapsed
            );
        }
    }

    class ResultTracker
    {
    public:
        ResultTracker(
          prometheus::Counter& success_counter,
          prometheus::Counter& failure_counter
        );

        template <typename Func>
        auto Track(Func&& func) -> std::invoke_result_t<Func&&>
        {
            using Result = std::invoke_result_t<Func&&>;
            try
            {
                if constexpr (std::is_void_v<Result>)
                {
                    std::invoke(std::forward<Func>(func));
                    success_.Increment();
                    return;
                }
                else
                {
                    decltype(auto) result =
                      std::invoke(std::forward<Func>(func));
                    success_.Increment();
                    return std::forward<Result>(result);
                }
            }
            catch (...)
            {
                failure_.Increment();
                throw;
            }
        }

        template <typename Func>
        void TrackVoid(Func&& func)
        {
            static_assert(std::is_void_v<std::invoke_result_t<Func&&>>,
                          "TrackVoid expects a void-returning callable");
            Track(std::forward<Func>(func));
        }

    private:
        prometheus::Counter& success_;
        prometheus::Counter& failure_;
    };

    class TimedResultTracker
    {
    public:
        TimedResultTracker(
          prometheus::Histogram& duration_histogram,
          prometheus::Counter& success_counter,
          prometheus::Counter& failure_counter
        );

        template <typename Func>
        auto Track(Func&& func) -> std::invoke_result_t<Func&&>
        {
            using Result = std::invoke_result_t<Func&&>;
            // Destruction observes exactly once on both paths, including
            // if the callable throws or if a returned value fails to move.
            ScopedTimer timer(histogram_);
            try
            {
                if constexpr (std::is_void_v<Result>)
                {
                    std::invoke(std::forward<Func>(func));
                    success_.Increment();
                    return;
                }
                else
                {
                    decltype(auto) result =
                      std::invoke(std::forward<Func>(func));
                    success_.Increment();
                    return std::forward<Result>(result);
                }
            }
            catch (...)
            {
                failure_.Increment();
                throw;
            }
        }

        template <typename Func>
        void TrackVoid(Func&& func)
        {
            static_assert(std::is_void_v<std::invoke_result_t<Func&&>>,
                          "TrackVoid expects a void-returning callable");
            Track(std::forward<Func>(func));
        }

    private:
        prometheus::Histogram& histogram_;
        prometheus::Counter& success_;
        prometheus::Counter& failure_;
    };

} // namespace vex
