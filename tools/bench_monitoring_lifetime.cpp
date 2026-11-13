// Optional Linux Monitoring Registry lifetime / memory trend benchmark.
// RSS and glibc heap samples are not leak-freedom or allocation counts.
#include <vex/monitoring/monitoring.hpp>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;

std::int64_t proc_kib(const char* field) {
    std::ifstream proc("/proc/self/status");
    std::string key;
    while (proc >> key) {
        if (key == field) {
            std::int64_t n = -1;
            std::string unit;
            return (proc >> n >> unit && unit == "kB") ? n : -1;
        }
        std::string rest;
        std::getline(proc, rest);
    }
    return -1;
}
std::int64_t heap_bytes() {
#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2, 33)
    return static_cast<std::int64_t>(::mallinfo2().uordblks);
#endif
#endif
    return -1;
}
void check_quota(const std::shared_ptr<prometheus::Registry>& registry,
                 const std::string& metric, const std::string& help) {
    bool rejected = false;
    try {
        (void)vex::MetricsManager::CreateCounter(
            registry, metric, help, {{"id", "second"}});
    } catch (const std::length_error&) {
        rejected = true;
    }
    if (!rejected)
        throw std::runtime_error("live Registry quota was not enforced");
}
void run(int rounds, int batch) {
    using vex::MetricsManager;
    if (!MetricsManager::Init())
        throw std::runtime_error("manager initialization failed");
    MetricsManager::SetMaxSeriesPerMetric(1);
    auto persistent = std::make_shared<prometheus::Registry>();
    auto& live = MetricsManager::CreateCounter(
        persistent, "vex_lifetime_persistent_total",
        "Persistent owner", {{"id", "first"}});
    live.Increment();

    const auto start_rss = proc_kib("VmRSS:");
    const auto start_heap = heap_bytes();
    std::uint64_t destroyed = 0;
    std::cout
       << "round,registries_per_round,expired_registry_checks,elapsed_ms,"
          "rss_kib,rss_delta_kib,peak_rss_kib,heap_live_bytes,heap_delta_bytes\n";

    for (int round = 1; round <= rounds; ++round) {
        const auto start = Clock::now();
        for (int i = 0; i < batch; ++i) {
            std::weak_ptr<prometheus::Registry> weak;
            {
                auto registry = std::make_shared<prometheus::Registry>();
                weak = registry;
                auto& counter = MetricsManager::CreateCounter(
                    registry, "vex_lifetime_transient_total",
                    "Transient owner", {{"id", "first"}});
                counter.Increment();
                if (counter.Value() != 1.0)
                    throw std::runtime_error("transient metric invalid");
            }
            if (!weak.expired())
                throw std::runtime_error("manager pinned expired Registry");
            ++destroyed;
        }

        // Control block address reuse and weak-ledger sweeps must not
        // transfer quota, HELP or label history to a new Registry owner.
        {
            auto fresh = std::make_shared<prometheus::Registry>();
            (void)MetricsManager::CreateCounter(
                fresh, "vex_lifetime_transient_total",
                "Fresh HELP", {{"id", "first"}});
            check_quota(fresh, "vex_lifetime_transient_total", "Fresh HELP");
        }
        if (live.Value() != 1.0)
            throw std::runtime_error("persistent metric changed");
        check_quota(
            persistent, "vex_lifetime_persistent_total", "Persistent owner");

        const double elapsed = std::chrono::duration<double, std::milli>(
            Clock::now() - start).count();
        const auto rss = proc_kib("VmRSS:");
        const auto peak = proc_kib("VmHWM:");
        const auto heap = heap_bytes();
        if (rss >= 0 && peak >= 0 && peak < rss)
            throw std::runtime_error("peak RSS below current RSS");
        const auto rss_delta = rss < 0 || start_rss < 0
            ? -1 : rss - start_rss;
        const auto heap_delta = heap < 0 || start_heap < 0
            ? -1 : heap - start_heap;
        std::cout << round << ',' << batch << ',' << destroyed << ','
                  << std::fixed << std::setprecision(3) << elapsed << ','
                  << rss << ',' << rss_delta << ',' << peak << ','
                  << heap << ',' << heap_delta << '\n';
    }
    persistent.reset();
    MetricsManager::Reset();
}
}

int main(int argc, char** argv) {
    try {
        if (argc > 3) throw std::invalid_argument("too many arguments");
        const int rounds = argc >= 2 ? std::stoi(argv[1]) : 8;
        const int batch = argc >= 3 ? std::stoi(argv[2]) : 512;
        if (rounds < 1 || rounds > 20 || batch < 256 || batch > 2048)
            throw std::invalid_argument(
                "Usage: vex_monitoring_lifetime_benchmark [1..20] [256..2048]");
        run(rounds, batch);
    } catch (const std::exception& e) {
        std::cerr << "MONITORING_LIFETIME_BENCHMARK_FAIL "
                  << e.what() << '\n';
        return 1;
    }
    return 0;
}
