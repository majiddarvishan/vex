#pragma once

#include <vex/monitoring/metrics_manager.hpp>

#include <chrono>
#include <functional>
#include <utility>

namespace vex
{

    class ScopedTimer
    {
    public:
        explicit ScopedTimer(prometheus::Histogram& histogram);
        ~ScopedTimer();

        [[nodiscard]] double ElapsedSeconds() const;

    private:
        prometheus::Histogram& histogram_;
        std::chrono::steady_clock::time_point start_;
    };

    template <typename Func>
    auto TimeFunction(prometheus::Histogram& histogram, Func&& func)
      -> decltype(func())
    {
        ScopedTimer timer(histogram);
        return func();
    }

    template <typename Func>
    auto TimeFunctionWithDuration(prometheus::Histogram& histogram, Func&& func)
      -> std::pair<decltype(func()), double>
    {
        auto start     = std::chrono::steady_clock::now();
        auto result    = func();
        auto duration  = std::chrono::steady_clock::now() - start;
        double seconds = std::chrono::duration<double>(duration).count();
        histogram.Observe(seconds);
        return {std::move(result), seconds};
    }

    class ResultTracker
    {
    public:
        ResultTracker(
          prometheus::Counter& success_counter,
          prometheus::Counter& failure_counter
        );

        template <typename Func>
        auto Track(Func&& func) -> decltype(func())
        {
            try
            {
                auto result = func();
                success_.Increment();
                return result;
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
            try
            {
                func();
                success_.Increment();
            }
            catch (...)
            {
                failure_.Increment();
                throw;
            }
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
        auto Track(Func&& func) -> decltype(func())
        {
            auto start = std::chrono::steady_clock::now();
            try
            {
                auto result   = func();
                auto duration = std::chrono::steady_clock::now() - start;
                histogram_.Observe(
                  std::chrono::duration<double>(duration).count()
                );
                success_.Increment();
                return result;
            }
            catch (...)
            {
                auto duration = std::chrono::steady_clock::now() - start;
                histogram_.Observe(
                  std::chrono::duration<double>(duration).count()
                );
                failure_.Increment();
                throw;
            }
        }

        /// Execute void function with timing and success/failure tracking
        template <typename Func>
        auto TrackVoid(Func&& func) -> typename std::enable_if<
          std::is_void<decltype(func())>::value, void>::type
        {
            auto start = std::chrono::steady_clock::now();
            try
            {
                func(); // No return value to capture
                auto duration = std::chrono::steady_clock::now() - start;
                histogram_.Observe(
                  std::chrono::duration<double>(duration).count()
                );
                success_.Increment();
            }
            catch (...)
            {
                auto duration = std::chrono::steady_clock::now() - start;
                histogram_.Observe(
                  std::chrono::duration<double>(duration).count()
                );
                failure_.Increment();
                throw;
            }
        }

    private:
        prometheus::Histogram& histogram_;
        prometheus::Counter& success_;
        prometheus::Counter& failure_;
    };

} // namespace vex