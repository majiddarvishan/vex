// Optional Monitoring-only Release benchmark. No production dependencies.
// Memory fields are process snapshots, not counts of allocations.
#include <vex/monitoring/monitoring.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        Clock::now() - start).count();
}
std::int64_t proc_status_kib(const char* wanted) {
#if defined(__linux__)
    std::ifstream input("/proc/self/status");
    std::string key;
    while (input >> key) {
        if (key == wanted) {
            std::int64_t value = -1;
            std::string unit;
            return (input >> value >> unit && unit == "kB") ? value : -1;
        }
        std::string rest;
        std::getline(input, rest);
    }
#else
    (void)wanted;
#endif
    return -1;
}
std::int64_t rss_kib() {
    return proc_status_kib("VmRSS:");
}
std::int64_t peak_rss_kib() {
#if defined(__linux__)
    // Same /proc accounting source as current RSS. Prior getrusage
    // ru_maxrss and VmRSS disagreed in 32/33 user benchmark rows.
    return proc_status_kib("VmHWM:");
#else
    struct rusage usage {};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) return -1;
#if defined(__APPLE__)
    return static_cast<std::int64_t>(usage.ru_maxrss / 1024);
#else
    return static_cast<std::int64_t>(usage.ru_maxrss);
#endif
#endif
}
std::int64_t heap_live_bytes() {
#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2, 33)
    return static_cast<std::int64_t>(::mallinfo2().uordblks);
#endif
#endif
    return -1;
}
double percentile(std::vector<double> v, double q) {
    if (v.empty()) return -1.0;
    std::sort(v.begin(), v.end());
    const auto i = static_cast<std::size_t>(
        std::ceil(q * static_cast<double>(v.size()))) - 1;
    return v[std::min(i, v.size() - 1)];
}
void emit(const char* phase, const char* impl, int threads, int series,
          std::uint64_t operations, double elapsed,
          const std::vector<double>& microseconds, std::int64_t before) {
    const auto after = rss_kib();
    const auto delta = before < 0 || after < 0 ? -1 : after - before;
    // A peak smaller than current RSS is physically inconsistent.
    // Report unavailable (-1), never silently accept that estimate.
    const auto candidate_peak = peak_rss_kib();
    const auto peak = candidate_peak >= 0 && after >= 0 &&
                      candidate_peak < after ? -1 : candidate_peak;
    std::cout << phase << ',' << impl << ',' << threads << ',' << series
              << ',' << operations << ',' << std::fixed
              << std::setprecision(3) << elapsed << ','
              << (elapsed > 0 ? 1000.0 * operations / elapsed : 0.0)
              << ',' << percentile(microseconds, 0.50)
              << ',' << percentile(microseconds, 0.95)
              << ',' << percentile(microseconds, 0.99)
              << ',' << after << ',' << delta
              << ',' << peak << ',' << heap_live_bytes() << '\n';
}
template <typename Fn>
double parallel(int threads, Fn&& fn) {
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::exception_ptr> errors(static_cast<std::size_t>(threads));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(threads));
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&, t] {
            ready.fetch_add(1, std::memory_order_release);
            while (!go.load(std::memory_order_acquire))
                std::this_thread::yield();
            try { fn(t); }
            catch (...) { errors[static_cast<std::size_t>(t)] =
                              std::current_exception(); }
        });
    }
    while (ready.load(std::memory_order_acquire) != threads)
        std::this_thread::yield();
    const auto start = Clock::now(); // excludes thread creation
    go.store(true, std::memory_order_release);
    for (auto& worker : workers) worker.join();
    const double elapsed = ms_since(start);
    for (const auto& error : errors)
        if (error) std::rethrow_exception(error);
    return elapsed;
}
void registration(int threads, bool managed) {
    constexpr int count = 1000;
    auto registry = std::make_shared<prometheus::Registry>();
    std::atomic<int> next{0};
    const auto before = rss_kib();
    const double elapsed = parallel(threads, [&](int) {
        for (;;) {
            const int i = next.fetch_add(1, std::memory_order_relaxed);
            if (i >= count) break;
            const prometheus::Labels labels{{"id", std::to_string(i)}};
            if (managed) {
                (void)vex::MetricsManager::CreateCounter(
                    registry, "vex_load_reg_total", "Concurrent registrations",
                    labels);
            } else {
                auto& family = prometheus::BuildCounter()
                    .Name("vex_load_reg_total")
                    .Help("Concurrent registrations").Register(*registry);
                (void)family.Add(labels);
            }
        }
    });
    const auto families = registry->Collect();
    if (families.size() != 1 || families.front().metric.size() != count)
        throw std::runtime_error("bad concurrent registration sample count");
    emit("registration", managed ? "managed" : "raw",
         threads, count, count, elapsed, {}, before);
}
void update(int threads) {
    constexpr int per_thread = 25000;
    auto registry = std::make_shared<prometheus::Registry>();
    auto& counter = vex::MetricsManager::CreateCounter(
        registry, "vex_load_updates_total", "Concurrent shared counter");
    const auto before = rss_kib();
    const auto elapsed = parallel(threads, [&](int) {
        for (int i = 0; i < per_thread; ++i) counter.Increment();
    });
    const auto operations = static_cast<std::uint64_t>(threads) * per_thread;
    if (counter.Value() != static_cast<double>(operations))
        throw std::runtime_error("lost shared counter increments");
    emit("counter_update", "managed", threads, 1, operations,
         elapsed, {}, before);
}
// Distinguish a hot shared Counter from one Counter per writer. For both
// arrangements, compare wrapper-created and direct Prometheus Counters.
void counter_control(int threads, bool managed, bool sharded) {
    constexpr int per_thread = 25000;
    auto registry = std::make_shared<prometheus::Registry>();
    prometheus::Family<prometheus::Counter>* raw_family = nullptr;
    if (!managed) {
        raw_family = &prometheus::BuildCounter()
            .Name("vex_load_control_total").Help("Counter control")
            .Register(*registry);
    }

    std::vector<prometheus::Counter*> counters;
    counters.reserve(static_cast<std::size_t>(sharded ? threads : 1));
    for (int i = 0; i < (sharded ? threads : 1); ++i) {
        const prometheus::Labels labels = sharded
            ? prometheus::Labels{{"worker", std::to_string(i)}}
            : prometheus::Labels{};
        if (managed) {
            counters.push_back(&vex::MetricsManager::CreateCounter(
                registry, "vex_load_control_total", "Counter control",
                labels));
        } else {
            counters.push_back(&raw_family->Add(labels));
        }
    }

    const auto before = rss_kib();
    const auto elapsed = parallel(threads, [&](int t) {
        auto& counter = *counters[static_cast<std::size_t>(sharded ? t : 0)];
        for (int i = 0; i < per_thread; ++i) counter.Increment();
    });

    const auto operations = static_cast<std::uint64_t>(threads) * per_thread;
    for (auto* counter : counters) {
        const auto expected = sharded
            ? static_cast<double>(per_thread)
            : static_cast<double>(operations);
        if (counter->Value() != expected)
            throw std::runtime_error("lost controlled Counter increments");
    }
    emit(sharded ? "counter_update_sharded" : "counter_update",
         managed ? "managed" : "raw", threads,
         static_cast<int>(counters.size()), operations,
         elapsed, {}, before);
}
void collect_during_updates() {
    constexpr int count = 1000;
    constexpr int samples = 100;
    auto registry = std::make_shared<prometheus::Registry>();
    std::vector<prometheus::Counter*> counters;
    counters.reserve(count);
    for (int i = 0; i < count; ++i) {
        counters.push_back(&vex::MetricsManager::CreateCounter(
            registry, "vex_load_collect_total", "Concurrent collection",
            {{"id", std::to_string(i)}}));
    }
    std::atomic<bool> start{false}, stop{false};
    std::thread worker([&] {
        while (!start.load(std::memory_order_acquire))
            std::this_thread::yield();
        std::size_t i = 0;
        while (!stop.load(std::memory_order_acquire))
            counters[(i++) % counters.size()]->Increment();
    });
    std::vector<double> latencies;
    latencies.reserve(samples);
    const auto rss_before = rss_kib();
    const auto begin = Clock::now();
    start.store(true, std::memory_order_release);
    bool correct = true;
    for (int i = 0; i < samples; ++i) {
        const auto s = Clock::now();
        const auto families = registry->Collect();
        latencies.push_back(ms_since(s) * 1000.0);
        if (families.size() != 1 ||
            families.front().metric.size() != count) correct = false;
    }
    const auto elapsed = ms_since(begin);
    stop.store(true, std::memory_order_release);
    worker.join();
    if (!correct) throw std::runtime_error("inconsistent Collect samples");
    emit("collect_during_update", "managed", 2, count, samples,
         elapsed, latencies, rss_before);
}
class Socket {
public:
    explicit Socket(int fd) : fd_(fd) {}
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    ~Socket() { if (fd_ >= 0) ::close(fd_); }
    int get() const { return fd_; }
private:
    int fd_;
};
void http_get(int port) {
    Socket sock(::socket(AF_INET, SOCK_STREAM, 0));
    if (sock.get() < 0) throw std::runtime_error("socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        ::connect(sock.get(), reinterpret_cast<sockaddr*>(&address),
                  sizeof(address)) != 0)
        throw std::runtime_error("loopback connect failed");
    timeval timeout{};
    timeout.tv_sec = 2;
    (void)::setsockopt(sock.get(), SOL_SOCKET, SO_RCVTIMEO,
                       &timeout, sizeof(timeout));
    (void)::setsockopt(sock.get(), SOL_SOCKET, SO_SNDTIMEO,
                       &timeout, sizeof(timeout));
    const std::string request =
        "GET /metrics HTTP/1.1\r\nHost: 127.0.0.1\r\n"
        "Connection: close\r\nAccept-Encoding: identity\r\n\r\n";
    std::size_t sent = 0;
    while (sent < request.size()) {
        const auto n = ::send(sock.get(), request.data() + sent,
                              request.size() - sent, 0);
        if (n <= 0) throw std::runtime_error("HTTP send failed");
        sent += static_cast<std::size_t>(n);
    }
    std::string prefix;
    std::size_t total = 0;
    char buffer[8192];
    for (;;) {
        const auto n = ::recv(sock.get(), buffer, sizeof(buffer), 0);
        if (n == 0) break;
        if (n < 0) throw std::runtime_error("HTTP receive failed");
        if (prefix.size() < 16) {
            const auto len = std::min(static_cast<std::size_t>(n),
                                     static_cast<std::size_t>(16 - prefix.size()));
            prefix.append(buffer, len);
        }
        total += static_cast<std::size_t>(n);
        if (total > 4 * 1024 * 1024)
            throw std::runtime_error("unbounded HTTP response");
    }
    if (prefix.rfind("HTTP/1.1 200", 0) != 0 &&
        prefix.rfind("HTTP/1.0 200", 0) != 0)
        throw std::runtime_error("HTTP status not 200");
}
void http_scrape() {
    constexpr int count = 100;
    constexpr int requests = 50;
    auto registry = std::make_shared<prometheus::Registry>();
    for (int i = 0; i < count; ++i) {
        (void)vex::MetricsManager::CreateCounter(
            registry, "vex_load_http_total", "HTTP benchmark",
            {{"id", std::to_string(i)}});
    }
    prometheus::Exposer exposer("127.0.0.1:0", 1);
    const auto ports = exposer.GetListeningPorts();
    if (ports.size() != 1 || ports[0] <= 0)
        throw std::runtime_error("invalid ephemeral Exposer port");
    exposer.RegisterCollectable(registry);
    // Exposer may start accepting asynchronously. Bounded warm-up
    // retries avoid confusing startup scheduling with scrape latency.
    bool ready = false;
    for (int attempt = 0; attempt < 30; ++attempt) {
        try {
            http_get(ports.front());
            ready = true;
            break;
        } catch (const std::runtime_error&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    if (!ready)
        throw std::runtime_error("Exposer failed loopback startup");
    std::vector<double> latencies;
    latencies.reserve(requests);
    const auto before = rss_kib();
    const auto begin = Clock::now();
    for (int i = 0; i < requests; ++i) {
        const auto one = Clock::now();
        http_get(ports.front());
        latencies.push_back(ms_since(one) * 1000.0);
    }
    emit("http_scrape", "managed", 1, count, requests,
         ms_since(begin), latencies, before);
}
} // namespace

int main(int argc, char** argv) {
    int repeats = 3;
    bool controls = false;
    if (argc > 3) {
        std::cerr << "Usage: vex_monitoring_load_benchmark [1|3|5] [--controls]\n";
        return 2;
    }
    if (argc == 3) {
        controls = std::string(argv[2]) == "--controls";
        if (!controls) {
            std::cerr << "Usage: vex_monitoring_load_benchmark [1|3|5] [--controls]\n";
            return 2;
        }
    }
    if (argc >= 2) {
        const std::string n(argv[1]);
        if (n != "1" && n != "3" && n != "5") {
            std::cerr << "Usage: vex_monitoring_load_benchmark [1|3|5] [--controls]\n";
            return 2;
        }
        repeats = std::stoi(n);
    }
    try {
        (void)vex::MetricsManager::Init(true);
        std::cout << "phase,implementation,threads,series,operations,"
                     "elapsed_ms,ops_per_second,p50_us,p95_us,p99_us,"
                     "rss_kib,rss_delta_kib,peak_rss_kib,heap_live_bytes\n";
        for (int run = 0; run < repeats; ++run) {
            for (int n : {1, 4, 8}) {
                if ((run % 2) == 0) {
                    registration(n, true);
                    registration(n, false);
                } else {
                    registration(n, false);
                    registration(n, true);
                }
                update(n);
                if (controls) {
                    // New controls are opt-in: preserve the historical
                    // 34-line CSV for the default benchmark.
                    counter_control(n, false, false); // raw shared
                    counter_control(n, true, true);   // managed sharded
                    counter_control(n, false, true);  // raw sharded
                }
            }
            collect_during_updates();
            http_scrape();
        }
        vex::MetricsManager::Reset();
    } catch (const std::exception& e) {
        std::cerr << "MONITORING_LOAD_BENCHMARK_FAIL " << e.what() << '\n';
        return 1;
    }
    return 0;
}
