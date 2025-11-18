#pragma once

#include <boost/asio.hpp>

// Compile-time threading mode selection
// Define NETWORKING_MULTI_THREADED for multi-threaded mode with strand protection
// Otherwise defaults to single-threaded mode (no strand overhead)
#ifdef NETWORKING_MULTI_THREADED
    #define PINEX_THREADING_MODE MultiThreaded
#else
    #define PINEX_THREADING_MODE SingleThreaded
#endif

namespace pa::pinex
{

// ============================================================================
// Threading Policy
// ============================================================================

struct SingleThreaded
{
    using executor_type = boost::asio::any_io_executor;

    template<typename Executor>
    static executor_type make_executor(Executor&& ex)
    {
        return std::forward<Executor>(ex);
    }

    template<typename F>
    static void dispatch(executor_type& ex, F&& f)
    {
        // Direct call in single-threaded mode (no synchronization needed)
        std::forward<F>(f)();
    }
};

struct MultiThreaded
{
    using executor_type = boost::asio::strand<boost::asio::any_io_executor>;

    template<typename Executor>
    static executor_type make_executor(Executor&& ex)
    {
        return boost::asio::make_strand(std::forward<Executor>(ex));
    }

    template<typename F>
    static void dispatch(executor_type& ex, F&& f)
    {
        boost::asio::dispatch(ex, std::forward<F>(f));
    }
};

} // namespace pa::pinex