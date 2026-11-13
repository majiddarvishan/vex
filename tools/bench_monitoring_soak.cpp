// Linux-only, Monitoring-only steady-clock Registry soak. Not a leak proof.
#include <vex/monitoring/monitoring.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
using Registry = prometheus::Registry;
constexpr std::uint64_t kSweep = 256;

std::int64_t proc_kib(const char* field) {
    std::ifstream file("/proc/self/status");
    std::string key;
    while (file >> key) {
        if (key == field) {
            std::int64_t number = -1;
            std::string unit;
            return (file >> number >> unit && unit == "kB") ? number : -1;
        }
        std::string rest;
        std::getline(file, rest);
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
std::string from_env(const char* key) {
    const char* value = std::getenv(key);
    return value && *value ? value : "unknown";
}
std::string csv_string(const std::string& input) {
    std::string output = "\"";
    for (char c : input) {
        if (c == '"') output += "\"\"";
        else if (c == '\n' || c == '\r') output += ' ';
        else output += c;
    }
    return output + '"';
}
int parse_number(const char* value) {
    const std::string input(value);
    if (input.empty() || input.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("expected unsigned decimal number");
    return std::stoi(input);
}
struct Config {
    int duration = 300, interval = 10, warmup = 30, rate = 2000;
    bool raw = false;
};
prometheus::Counter& counter(
    bool raw, const std::shared_ptr<Registry>& registry,
    const std::string& name, const std::string& help,
    const prometheus::Labels& labels) {
    if (raw) {
        auto& family = prometheus::BuildCounter().Name(name).Help(help)
                           .Register(*registry);
        return family.Add(labels);
    }
    return vex::MetricsManager::CreateCounter(registry, name, help, labels);
}

class Soak {
public:
    explicit Soak(Config config) : cfg_(config) {}
    void run() {
        if (!vex::MetricsManager::Init())
            throw std::runtime_error("MetricsManager::Init failed");
        vex::MetricsManager::SetMaxSeriesPerMetric(1);
        anchor_registry_ = std::make_shared<Registry>();
        anchor_ = &counter(cfg_.raw, anchor_registry_, "vex_soak_anchor_total",
                           "Persistent soak owner", {{"id", "first"}});
        ++attempts_;
        anchor_->Increment();
        started_ = Clock::now();
        last_post_ = started_;
        std::cout
            << "run_id,started_utc,git_sha,kernel,compiler,build_type,"
               "mode,configured_seconds,sample_seconds,warmup_seconds,"
               "target_ops_per_second,stage,warmup,elapsed_seconds,"
               "churn_ops,expired_checks,quota_checks,registration_attempts_equivalent,"
               "alignment_calls,phase_mod_256,modeled_sweeps,"
               "window_ops_per_second,average_ops_per_second,"
               "rss_kib,peak_rss_kib,glibc_uordblks_bytes\n";
        sample("baseline");
        const auto deadline = started_ + std::chrono::seconds(cfg_.duration);
        auto next_sample = started_ + std::chrono::seconds(cfg_.interval);
        while (Clock::now() < deadline) {
            const auto now = Clock::now();
            if (now >= next_sample) {
                sample("interval");
                do { next_sample += std::chrono::seconds(cfg_.interval); }
                while (next_sample <= Clock::now());
                continue;
            }
            const auto allowed = started_ + std::chrono::duration_cast<
                Clock::duration>(std::chrono::duration<double>(
                    static_cast<double>(churn_ + 1) / cfg_.rate));
            if (now < allowed) {
                std::this_thread::sleep_until(
                    std::min({allowed, next_sample, deadline}));
                continue;
            }
            churn_once();
        }
        sample("final");
        anchor_registry_.reset();
        anchor_ = nullptr;
        vex::MetricsManager::Reset();
        emit("after_reset");
    }

private:
    void quota(const std::shared_ptr<Registry>& registry,
               const std::string& name, const std::string& help) {
        // Failed quota registrations also advance RegisterTracked's counter.
        ++attempts_;
        ++quota_checks_;
        bool rejected = false;
        try {
            (void)vex::MetricsManager::CreateCounter(
                registry, name, help, {{"id", "second"}});
        } catch (const std::length_error&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("managed series quota not enforced");
    }
    void churn_once() {
        std::weak_ptr<Registry> weak;
        {
            auto ephemeral = std::make_shared<Registry>();
            weak = ephemeral;
            auto& value = counter(cfg_.raw, ephemeral,
                "vex_soak_transient_total", "Transient soak owner",
                {{"id", "first"}});
            ++attempts_;
            value.Increment();
            if (value.Value() != 1.0)
                throw std::runtime_error("transient Counter invalid");
            if (!cfg_.raw && ((churn_ + 1) % kSweep == 0))
                quota(ephemeral, "vex_soak_transient_total",
                      "Transient soak owner");
        }
        if (!weak.expired())
            throw std::runtime_error("Registry weak ownership retained");
        ++churn_;
        ++expired_;
    }
    void anchor_registration() {
        ++attempts_;
        ++alignment_calls_;
        (void)counter(cfg_.raw, anchor_registry_, "vex_soak_anchor_total",
                      "Persistent soak owner", {{"id", "first"}});
    }
    void sample(const std::string& stage) {
        if (anchor_->Value() != 1.0)
            throw std::runtime_error("persistent Counter invalid");
        if (!cfg_.raw)
            quota(anchor_registry_, "vex_soak_anchor_total",
                  "Persistent soak owner");
        // Pre at phase=255, next tracked registration triggers the sweep.
        // Raw phase is only a modeled equivalent, not an actual sweep.
        while (attempts_ % kSweep != kSweep - 1)
            anchor_registration();
        emit(stage + "_pre_sweep");
        anchor_registration();
        emit(stage + "_post_sweep");
        last_post_ = Clock::now();
        last_post_churn_ = churn_;
    }
    void emit(const std::string& stage) {
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(
            now - started_).count();
        const double window = std::chrono::duration<double>(
            now - last_post_).count();
        const auto rss = proc_kib("VmRSS:");
        const auto peak = proc_kib("VmHWM:");
        if (rss < 0 || peak < rss)
            throw std::runtime_error("invalid Linux RSS/HWM sample");
        const double window_rate = window > 0.0
            ? static_cast<double>(churn_ - last_post_churn_) / window : 0.0;
        const double avg_rate = elapsed > 0.0
            ? static_cast<double>(churn_) / elapsed : 0.0;
        std::cout
            << csv_string(from_env("VEX_SOAK_RUN_ID")) << ','
            << csv_string(from_env("VEX_SOAK_STARTED_UTC")) << ','
            << csv_string(from_env("VEX_SOAK_GIT_SHA")) << ','
            << csv_string(from_env("VEX_SOAK_KERNEL")) << ','
            << csv_string(from_env("VEX_SOAK_COMPILER")) << ','
            << csv_string(from_env("VEX_SOAK_BUILD_TYPE")) << ','
            << (cfg_.raw ? "raw" : "managed") << ','
            << cfg_.duration << ',' << cfg_.interval << ','
            << cfg_.warmup << ',' << cfg_.rate << ','
            << csv_string(stage) << ','
            << (elapsed < cfg_.warmup ? 1 : 0) << ','
            << std::fixed << std::setprecision(3) << elapsed << ','
            << churn_ << ',' << expired_ << ',' << quota_checks_ << ','
            << attempts_ << ',' << alignment_calls_ << ','
            << (attempts_ % kSweep) << ','
            << (cfg_.raw ? 0 : attempts_ / kSweep) << ','
            << window_rate << ',' << avg_rate << ','
            << rss << ',' << peak << ',' << heap_bytes() << '\n'
            << std::flush;
    }
    Config cfg_;
    std::shared_ptr<Registry> anchor_registry_;
    prometheus::Counter* anchor_ = nullptr;
    Clock::time_point started_{}, last_post_{};
    std::uint64_t churn_ = 0, expired_ = 0, quota_checks_ = 0;
    std::uint64_t attempts_ = 0, alignment_calls_ = 0, last_post_churn_ = 0;
};
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 6) throw std::invalid_argument("too many arguments");
        Config c;
        if (argc > 1) c.duration = parse_number(argv[1]);
        if (argc > 2) {
            const std::string mode(argv[2]);
            if (mode != "managed" && mode != "raw")
                throw std::invalid_argument("mode must be managed or raw");
            c.raw = mode == "raw";
        }
        if (argc > 3) c.interval = parse_number(argv[3]);
        if (argc > 4) c.warmup = parse_number(argv[4]);
        if (argc > 5) c.rate = parse_number(argv[5]);
        if (c.duration < 2 || c.duration > 3600 ||
            c.interval < 1 || c.interval > c.duration ||
            c.warmup < 0 || c.warmup >= c.duration ||
            c.rate < 1 || c.rate > 20000)
            throw std::invalid_argument(
                "Usage: soak [duration=2..3600] [managed|raw] "
                "[interval=1..duration] [warmup=0..duration-1] "
                "[target_ops_per_second=1..20000]");
        Soak(c).run();
    } catch (const std::exception& e) {
        std::cerr << "MONITORING_SOAK_FAIL " << e.what() << '\n';
        return 1;
    }
    return 0;
}
