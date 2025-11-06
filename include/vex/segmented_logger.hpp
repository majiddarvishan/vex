#pragma once

#include "logging.hpp"
#include <pa/config.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace segmented_logging {

// ============================================================================
// Configuration and Types
// ============================================================================

enum class FileMode { Text, Binary };

enum class BackpressureStrategy {
    Block,      // Block caller until space available
    DropOldest, // Drop oldest records when full
    DropNewest, // Drop new records when full
    Reject      // Return false immediately when full
};

struct LoggerConfig {
    bool enabled = true;
    FileMode file_mode = FileMode::Text;
    std::string file_name_format;
    std::string create_path;
    std::string close_path;
    uint32_t buffer_size = 10000;
    uint32_t records_threshold = 100000;
    std::chrono::seconds time_threshold{3600};
    uint32_t queue_capacity = 50000;
    BackpressureStrategy backpressure = BackpressureStrategy::Block;
    std::chrono::milliseconds flush_interval{100};
    uint32_t num_shards = 4;  // For MPSC queue sharding
};

struct LoggerStats {
    std::atomic<uint64_t> records_written{0};
    std::atomic<uint64_t> records_dropped{0};
    std::atomic<uint64_t> files_created{0};
    std::atomic<uint64_t> write_errors{0};
    std::atomic<uint64_t> queue_size{0};
    std::atomic<uint64_t> contention_events{0};

    // Copy constructor for atomics
    LoggerStats() = default;
    LoggerStats(const LoggerStats& other)
        : records_written(other.records_written.load(std::memory_order_relaxed))
        , records_dropped(other.records_dropped.load(std::memory_order_relaxed))
        , files_created(other.files_created.load(std::memory_order_relaxed))
        , write_errors(other.write_errors.load(std::memory_order_relaxed))
        , queue_size(other.queue_size.load(std::memory_order_relaxed))
        , contention_events(other.contention_events.load(std::memory_order_relaxed))
    {}

    LoggerStats& operator=(const LoggerStats&) = delete;
};

// ============================================================================
// Time Formatting Utilities
// ============================================================================

namespace detail {

struct TimeComponents {
    int year_4;
    int year_2;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int millisecond;
};

inline TimeComponents decompose_time(const std::chrono::system_clock::time_point& tp) {
    using namespace std::chrono;

    auto time_t_val = system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_r(&time_t_val, &tm);

    auto since_epoch = tp.time_since_epoch();
    auto millis = duration_cast<milliseconds>(since_epoch) % 1000;

    return {
        tm.tm_year + 1900,
        tm.tm_year % 100,
        tm.tm_mon + 1,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec,
        static_cast<int>(millis.count())
    };
}

class FileNameFormatter {
public:
    explicit FileNameFormatter(std::string_view format) {
        parse_format(format);
    }

    std::string format(const TimeComponents& open_time,
                      const TimeComponents& close_time,
                      uint32_t sequence) const {
        std::string result;
        result.reserve(128);

        for (const auto& part : parts_) {
            if (std::holds_alternative<std::string>(part)) {
                result += std::get<std::string>(part);
            } else {
                const auto& field = std::get<FormatField>(part);
                append_field(result, field, open_time, close_time, sequence);
            }
        }

        return result;
    }

private:
    enum class FieldType {
        OpenYear4, OpenYear2, OpenMonth, OpenDay, OpenHour, OpenMinute, OpenSecond, OpenMillis,
        CloseYear4, CloseYear2, CloseMonth, CloseDay, CloseHour, CloseMinute, CloseSecond, CloseMillis,
        Sequence
    };

    struct FormatField {
        FieldType type;
        int width;
    };

    using Part = std::variant<std::string, FormatField>;
    std::vector<Part> parts_;

    void parse_format(std::string_view format) {
        bool begin_time = true;
        std::string literal;

        for (size_t i = 0; i < format.size(); ++i) {
            if (format[i] == '%' && i + 1 < format.size()) {
                if (!literal.empty()) {
                    parts_.emplace_back(std::move(literal));
                    literal.clear();
                }

                char spec = format[++i];
                switch (spec) {
                    case 'Y': parts_.emplace_back(FormatField{begin_time ? FieldType::OpenYear4 : FieldType::CloseYear4, 4}); break;
                    case 'y': parts_.emplace_back(FormatField{begin_time ? FieldType::OpenYear2 : FieldType::CloseYear2, 2}); break;
                    case 'M': parts_.emplace_back(FormatField{begin_time ? FieldType::OpenMonth : FieldType::CloseMonth, 2}); break;
                    case 'd': parts_.emplace_back(FormatField{begin_time ? FieldType::OpenDay : FieldType::CloseDay, 2}); break;
                    case 'h': parts_.emplace_back(FormatField{begin_time ? FieldType::OpenHour : FieldType::CloseHour, 2}); break;
                    case 'm': parts_.emplace_back(FormatField{begin_time ? FieldType::OpenMinute : FieldType::CloseMinute, 2}); break;
                    case 's': parts_.emplace_back(FormatField{begin_time ? FieldType::OpenSecond : FieldType::CloseSecond, 2}); break;
                    case 'S': parts_.emplace_back(FormatField{begin_time ? FieldType::OpenMillis : FieldType::CloseMillis, 3}); break;
                    case 'n': parts_.emplace_back(FormatField{FieldType::Sequence, 4}); break;
                    case 'b': begin_time = true; break;
                    case 'e': begin_time = false; break;
                    default: literal += '%'; literal += spec; break;
                }
            } else {
                literal += format[i];
            }
        }

        if (!literal.empty()) {
            parts_.emplace_back(std::move(literal));
        }
    }

    void append_field(std::string& out, const FormatField& field,
                     const TimeComponents& open, const TimeComponents& close,
                     uint32_t seq) const {
        char buf[16];
        int value = 0;

        switch (field.type) {
            case FieldType::OpenYear4: value = open.year_4; break;
            case FieldType::OpenYear2: value = open.year_2; break;
            case FieldType::OpenMonth: value = open.month; break;
            case FieldType::OpenDay: value = open.day; break;
            case FieldType::OpenHour: value = open.hour; break;
            case FieldType::OpenMinute: value = open.minute; break;
            case FieldType::OpenSecond: value = open.second; break;
            case FieldType::OpenMillis: value = open.millisecond; break;
            case FieldType::CloseYear4: value = close.year_4; break;
            case FieldType::CloseYear2: value = close.year_2; break;
            case FieldType::CloseMonth: value = close.month; break;
            case FieldType::CloseDay: value = close.day; break;
            case FieldType::CloseHour: value = close.hour; break;
            case FieldType::CloseMinute: value = close.minute; break;
            case FieldType::CloseSecond: value = close.second; break;
            case FieldType::CloseMillis: value = close.millisecond; break;
            case FieldType::Sequence: value = seq; break;
        }

        snprintf(buf, sizeof(buf), "%0*d", field.width, value);
        out += buf;
    }
};

// ============================================================================
// Multi-Producer Single-Consumer Queue (MPSC)
// Uses sharded lock-free approach for high concurrency
// ============================================================================

template<typename T>
class MPSCQueue {
public:
    explicit MPSCQueue(size_t capacity, size_t num_shards = 4)
        : num_shards_(num_shards)
        , shard_capacity_(capacity / num_shards)
    {
        shards_.reserve(num_shards_);
        for (size_t i = 0; i < num_shards_; ++i) {
            shards_.emplace_back(std::make_unique<Shard>(shard_capacity_));
        }
    }

    bool try_push(T&& item) {
        // Use thread ID for shard selection to reduce contention
        size_t shard_idx = get_thread_shard();

        // Try local shard first
        if (shards_[shard_idx]->try_push(std::move(item))) {
            return true;
        }

        // If local shard is full, try others (load balancing)
        for (size_t i = 1; i < num_shards_; ++i) {
            size_t idx = (shard_idx + i) % num_shards_;
            if (shards_[idx]->try_push(std::move(item))) {
                return true;
            }
        }

        return false;
    }

    bool try_pop(T& item) {
        // Consumer tries each shard in round-robin fashion
        for (size_t i = 0; i < num_shards_; ++i) {
            size_t idx = (pop_shard_idx_++ % num_shards_);
            if (shards_[idx]->try_pop(item)) {
                return true;
            }
        }
        return false;
    }

    size_t size() const {
        size_t total = 0;
        for (const auto& shard : shards_) {
            total += shard->size();
        }
        return total;
    }

    bool empty() const {
        for (const auto& shard : shards_) {
            if (!shard->empty()) {
                return false;
            }
        }
        return true;
    }

    size_t capacity() const {
        return shard_capacity_ * num_shards_;
    }

private:
    // Single shard - lock-free ring buffer
    class Shard {
    public:
        explicit Shard(size_t capacity)
            : buffer_(capacity)
            , capacity_(capacity)
            , write_idx_(0)
            , read_idx_(0)
        {}

        bool try_push(T&& item) {
            const size_t current_write = write_idx_.load(std::memory_order_relaxed);
            const size_t next_write = (current_write + 1) % capacity_;

            if (next_write == read_idx_.load(std::memory_order_acquire)) {
                return false; // Full
            }

            buffer_[current_write] = std::move(item);
            write_idx_.store(next_write, std::memory_order_release);
            return true;
        }

        bool try_pop(T& item) {
            const size_t current_read = read_idx_.load(std::memory_order_relaxed);

            if (current_read == write_idx_.load(std::memory_order_acquire)) {
                return false; // Empty
            }

            item = std::move(buffer_[current_read]);
            read_idx_.store((current_read + 1) % capacity_, std::memory_order_release);
            return true;
        }

        size_t size() const {
            const size_t write = write_idx_.load(std::memory_order_acquire);
            const size_t read = read_idx_.load(std::memory_order_acquire);
            return write >= read ? (write - read) : (capacity_ - read + write);
        }

        bool empty() const {
            return read_idx_.load(std::memory_order_acquire) ==
                   write_idx_.load(std::memory_order_acquire);
        }

    private:
        std::vector<T> buffer_;
        const size_t capacity_;

        // Align to cache line to prevent false sharing
        alignas(64) std::atomic<size_t> write_idx_;
        alignas(64) std::atomic<size_t> read_idx_;
    };

    size_t get_thread_shard() const {
        // Hash thread ID for shard selection
        static thread_local size_t cached_shard =
            std::hash<std::thread::id>{}(std::this_thread::get_id()) % num_shards_;
        return cached_shard;
    }

    const size_t num_shards_;
    const size_t shard_capacity_;
    std::vector<std::unique_ptr<Shard>> shards_;

    // Consumer-side round-robin index
    alignas(64) std::atomic<size_t> pop_shard_idx_{0};
};

} // namespace detail

// ============================================================================
// Main Logger Implementation
// ============================================================================

class SegmentedLogger {
public:
    SegmentedLogger(pa::config::manager* config_manager,
                    const std::shared_ptr<pa::config::node>& config)
        : config_manager_(config_manager)
        , config_(config)
        , should_stop_(false)
        , file_open_(false)
        , records_in_file_(0)
        , sequence_number_(1)
    {
        auto cfg = load_config();
        queue_ = std::make_unique<detail::MPSCQueue<std::string>>(
            cfg.queue_capacity,
            cfg.num_shards
        );

        setup_config_observers();
        apply_config(cfg);
        prepare_directories();
        cleanup_incomplete_files();

        worker_thread_ = std::thread(&SegmentedLogger::worker_loop, this);
    }

    ~SegmentedLogger() {
        shutdown();
    }

    // Non-copyable, non-movable
    SegmentedLogger(const SegmentedLogger&) = delete;
    SegmentedLogger& operator=(const SegmentedLogger&) = delete;
    SegmentedLogger(SegmentedLogger&&) = delete;
    SegmentedLogger& operator=(SegmentedLogger&&) = delete;

    // Thread-safe: Can be called from multiple threads
    bool record(std::string log) {
        if (!enabled_.load(std::memory_order_relaxed)) {
            return true;
        }

        auto strategy = backpressure_.load(std::memory_order_relaxed);
        uint32_t retry_count = 0;
        constexpr uint32_t MAX_RETRIES = 3;

        while (retry_count < MAX_RETRIES) {
            if (queue_->try_push(std::move(log))) {
                stats_.queue_size.fetch_add(1, std::memory_order_relaxed);
                cv_.notify_one();
                return true;
            }

            // Queue full - handle based on strategy
            switch (strategy) {
                case BackpressureStrategy::Block:
                    if (retry_count++ < MAX_RETRIES) {
                        std::this_thread::yield();
                        continue;
                    }
                    stats_.contention_events.fetch_add(1, std::memory_order_relaxed);
                    return false;

                case BackpressureStrategy::DropNewest:
                    stats_.records_dropped.fetch_add(1, std::memory_order_relaxed);
                    return false;

                case BackpressureStrategy::DropOldest: {
                    std::string dummy;
                    if (queue_->try_pop(dummy)) {
                        stats_.queue_size.fetch_sub(1, std::memory_order_relaxed);
                        stats_.records_dropped.fetch_add(1, std::memory_order_relaxed);
                    }
                    retry_count++;
                    continue;
                }

                case BackpressureStrategy::Reject:
                    stats_.records_dropped.fetch_add(1, std::memory_order_relaxed);
                    return false;
            }
        }

        return false;
    }

    void set_header(std::string header) {
        std::lock_guard lock(config_mutex_);
        header_ = std::move(header);
    }

    void set_footer(std::string footer) {
        std::lock_guard lock(config_mutex_);
        footer_ = std::move(footer);
    }

    bool is_enabled() const {
        return enabled_.load(std::memory_order_relaxed);
    }

    LoggerStats get_stats() const {
        return stats_;
    }

    void flush() {
        flush_requested_.store(true, std::memory_order_release);
        cv_.notify_one();
    }

private:
    // Configuration
    LoggerConfig load_config() {
        LoggerConfig cfg;
        cfg.enabled = config_->at("enabled")->get<bool>();
        cfg.buffer_size = config_->at("buffer_size")->get<uint32_t>();
        cfg.records_threshold = config_->at("records_threshold")->get<uint32_t>();
        cfg.time_threshold = std::chrono::seconds(config_->at("time_threshold")->get<uint32_t>());
        cfg.file_name_format = config_->at("file_name_format")->get<std::string>();
        cfg.create_path = config_->at("create_path")->get<std::string>();
        cfg.close_path = config_->at("close_path")->get<std::string>();

        auto mode = config_->at("file_mode")->get<std::string>();
        cfg.file_mode = (mode == "binary") ? FileMode::Binary : FileMode::Text;

        // Optional: num_shards for MPSC queue
        if (config_->contains("num_shards")) {
            cfg.num_shards = config_->at("num_shards")->get<uint32_t>();
        }

        return cfg;
    }

    void apply_config(const LoggerConfig& cfg) {
        enabled_.store(cfg.enabled, std::memory_order_relaxed);

        std::lock_guard lock(config_mutex_);
        config_data_ = cfg;

        // Ensure paths end with '/'
        if (!config_data_.create_path.empty() && config_data_.create_path.back() != '/') {
            config_data_.create_path += '/';
        }
        if (!config_data_.close_path.empty() && config_data_.close_path.back() != '/') {
            config_data_.close_path += '/';
        }

        formatter_ = std::make_unique<detail::FileNameFormatter>(cfg.file_name_format);
    }

    void setup_config_observers() {
        config_obs_enabled_ = config_manager_->on_replace(
            config_->at("enabled"),
            [this](const auto& node) { enabled_.store(node->template get<bool>(), std::memory_order_relaxed); }
        );

        config_obs_buffer_ = config_manager_->on_replace(
            config_->at("buffer_size"),
            [this](const auto& node) {
                std::lock_guard lock(config_mutex_);
                config_data_.buffer_size = node->template get<uint32_t>();
            }
        );

        config_obs_threshold_ = config_manager_->on_replace(
            config_->at("records_threshold"),
            [this](const auto& node) {
                std::lock_guard lock(config_mutex_);
                config_data_.records_threshold = node->template get<uint32_t>();
            }
        );

        config_obs_time_ = config_manager_->on_replace(
            config_->at("time_threshold"),
            [this](const auto& node) {
                std::lock_guard lock(config_mutex_);
                config_data_.time_threshold = std::chrono::seconds(node->template get<uint32_t>());
            }
        );
    }

    // Directory management
    void prepare_directories() {
        std::lock_guard lock(config_mutex_);

        if (!fs::exists(config_data_.create_path)) {
            if (!fs::create_directories(config_data_.create_path)) {
                LOG_ERROR("Failed to create path: {}", config_data_.create_path);
                config_data_.create_path = "./open/";
                fs::create_directories(config_data_.create_path);
            }
        }

        if (!config_data_.close_path.empty() && !fs::exists(config_data_.close_path)) {
            if (!fs::create_directories(config_data_.close_path)) {
                LOG_ERROR("Failed to create path: {}", config_data_.close_path);
                config_data_.close_path = "./close/";
                fs::create_directories(config_data_.close_path);
            }
        }
    }

    void cleanup_incomplete_files() {
        std::lock_guard lock(config_mutex_);

        for (const auto& entry : fs::directory_iterator(config_data_.create_path)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".incomp") {
                continue;
            }

            try {
                auto stem = entry.path().stem().string();
                auto micros = std::stoull(stem);
                auto open_tp = std::chrono::system_clock::time_point(std::chrono::microseconds(micros));
                auto close_tp = std::chrono::system_clock::now();

                auto open_time = detail::decompose_time(open_tp);
                auto close_time = detail::decompose_time(close_tp);

                auto final_name = formatter_->format(open_time, close_time, sequence_number_++);
                auto dest = config_data_.close_path + final_name;

                fs::rename(entry.path(), dest);
                LOG_INFO("Recovered incomplete file: {}", final_name);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to cleanup incomplete file {}: {}", entry.path().string(), e.what());
            }
        }
    }

    // File operations
    void open_new_file() {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto micros = duration_cast<microseconds>(now.time_since_epoch()).count();

        std::lock_guard lock(config_mutex_);

        temp_filename_ = std::to_string(micros) + ".incomp";
        std::string full_path = config_data_.create_path + temp_filename_;

        auto mode = std::ios::out;
        if (config_data_.file_mode == FileMode::Binary) {
            mode |= std::ios::binary;
        }

        file_.open(full_path, mode);
        if (!file_) {
            LOG_CRITICAL("Failed to open file: {}", full_path);
            stats_.write_errors.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        file_open_ = true;
        file_open_time_ = now;
        records_in_file_ = 0;
        stats_.files_created.fetch_add(1, std::memory_order_relaxed);

        // Write header for text files
        if (config_data_.file_mode == FileMode::Text && !header_.empty()) {
            file_ << header_ << '\n';
        }

        LOG_INFO("Opened new log file: {}", full_path);
    }

    void close_current_file() {
        if (!file_open_) {
            return;
        }

        std::lock_guard lock(config_mutex_);

        // Write footer for text files
        if (config_data_.file_mode == FileMode::Text && !footer_.empty()) {
            file_ << footer_;
        }

        file_.close();
        file_open_ = false;

        // Rename to final name
        try {
            auto close_time = std::chrono::system_clock::now();
            auto open_time = detail::decompose_time(file_open_time_);
            auto close_time_components = detail::decompose_time(close_time);

            auto final_name = formatter_->format(open_time, close_time_components, sequence_number_);

            std::string old_path = config_data_.create_path + temp_filename_;
            std::string new_path = config_data_.close_path + final_name;

            fs::rename(old_path, new_path);
            LOG_INFO("Closed and renamed log file: {}", final_name);

            // Increment sequence, reset daily
            auto open_day = open_time.day;
            auto close_day = close_time_components.day;
            if (open_day != close_day) {
                sequence_number_ = 1;
            } else {
                sequence_number_ = (sequence_number_ % 9999) + 1;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to rename file: {}", e.what());
            stats_.write_errors.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void write_batch() {
        if (queue_->empty()) {
            return;
        }

        if (!file_open_) {
            open_new_file();
            if (!file_open_) {
                return;
            }
        }

        std::string record;
        size_t batch_size = 0;

        std::lock_guard lock(config_mutex_);
        const size_t max_batch = std::min(
            config_data_.buffer_size,
            config_data_.records_threshold - records_in_file_
        );

        while (batch_size < max_batch && queue_->try_pop(record)) {
            file_ << record;
            if (config_data_.file_mode == FileMode::Text) {
                file_ << '\n';
            }

            ++batch_size;
            ++records_in_file_;
            stats_.records_written.fetch_add(1, std::memory_order_relaxed);
            stats_.queue_size.fetch_sub(1, std::memory_order_relaxed);
        }

        if (batch_size > 0) {
            file_.flush();
        }
    }

    bool should_rotate_file() {
        if (!file_open_) {
            return false;
        }

        std::lock_guard lock(config_mutex_);

        // Check record threshold
        if (records_in_file_ >= config_data_.records_threshold) {
            return true;
        }

        // Check time threshold
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - file_open_time_);
        if (elapsed >= config_data_.time_threshold) {
            return true;
        }

        // Check day boundary
        auto open_components = detail::decompose_time(file_open_time_);
        auto now_components = detail::decompose_time(now);
        if (open_components.day != now_components.day) {
            return true;
        }

        return false;
    }

    // Worker thread
    void worker_loop() {
        while (!should_stop_.load(std::memory_order_acquire)) {
            std::unique_lock lock(cv_mutex_);

            cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return should_stop_.load(std::memory_order_acquire) ||
                       !queue_->empty() ||
                       flush_requested_.load(std::memory_order_acquire);
            });

            lock.unlock();

            // Process records
            write_batch();

            // Check for rotation
            if (should_rotate_file()) {
                close_current_file();
            }

            // Handle flush request
            if (flush_requested_.exchange(false, std::memory_order_acq_rel)) {
                std::lock_guard file_lock(config_mutex_);
                if (file_open_) {
                    file_.flush();
                }
            }
        }
    }

    void shutdown() {
        should_stop_.store(true, std::memory_order_release);
        cv_.notify_one();

        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }

        // Drain remaining records
        std::string record;
        while (queue_->try_pop(record)) {
            if (!file_open_) {
                open_new_file();
            }
            if (file_open_) {
                std::lock_guard lock(config_mutex_);
                file_ << record;
                if (config_data_.file_mode == FileMode::Text) {
                    file_ << '\n';
                }
            }
        }

        close_current_file();
    }

    // Member variables
    pa::config::manager* config_manager_;
    std::shared_ptr<pa::config::node> config_;

    pa::config::manager::observer config_obs_enabled_;
    pa::config::manager::observer config_obs_buffer_;
    pa::config::manager::observer config_obs_threshold_;
    pa::config::manager::observer config_obs_time_;

    std::atomic<bool> enabled_;
    std::atomic<BackpressureStrategy> backpressure_;

    std::unique_ptr<detail::MPSCQueue<std::string>> queue_;
    LoggerStats stats_;

    std::thread worker_thread_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> flush_requested_{false};
    std::mutex cv_mutex_;
    std::condition_variable cv_;

    std::mutex config_mutex_;
    LoggerConfig config_data_;
    std::unique_ptr<detail::FileNameFormatter> formatter_;

    std::ofstream file_;
    bool file_open_;
    std::string temp_filename_;
    std::chrono::system_clock::time_point file_open_time_;
    uint32_t records_in_file_;
    uint32_t sequence_number_;

    std::string header_;
    std::string footer_;
};

} // namespace segmented_logging