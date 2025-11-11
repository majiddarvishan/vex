// ============================================================================
// Production-Ready Examples
// ============================================================================
#include <expirator/expirator.hpp>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <random>

// ============================================================================
// Example 1: API Rate Limiting with Multiple Tiers
// ============================================================================
namespace api_rate_limiting {

enum class Tier { FREE, BASIC, PREMIUM, ENTERPRISE };

struct RateLimitConfig {
    size_t requests_per_minute;
    size_t requests_per_hour;
    size_t requests_per_day;
};

class MultiTierRateLimiter {
private:
    struct RequestToken {
        std::chrono::steady_clock::time_point timestamp;
        std::string endpoint;
    };

    boost::asio::io_context& io_;
    std::map<Tier, RateLimitConfig> configs_;
    std::map<std::string, Tier> user_tiers_;

    // Separate expirators for different time windows
    std::shared_ptr<expirator::lockfree_expirator<std::string, RequestToken>> minute_limiter_;
    std::shared_ptr<expirator::lockfree_expirator<std::string, RequestToken>> hour_limiter_;
    std::shared_ptr<expirator::lockfree_expirator<std::string, RequestToken>> day_limiter_;

    std::atomic<size_t> minute_requests_{0};
    std::atomic<size_t> hour_requests_{0};
    std::atomic<size_t> day_requests_{0};

public:
    MultiTierRateLimiter(boost::asio::io_context& io)
        : io_(io)
    {
        // Configure rate limits for each tier
        configs_[Tier::FREE] = {60, 1000, 10000};
        configs_[Tier::BASIC] = {300, 10000, 100000};
        configs_[Tier::PREMIUM] = {1000, 50000, 500000};
        configs_[Tier::ENTERPRISE] = {10000, 500000, 5000000};

        // Initialize expirators
        minute_limiter_ = std::make_shared<expirator::lockfree_expirator<std::string, RequestToken>>(
            &io_,
            [this](std::string, RequestToken) { minute_requests_--; }
        );

        hour_limiter_ = std::make_shared<expirator::lockfree_expirator<std::string, RequestToken>>(
            &io_,
            [this](std::string, RequestToken) { hour_requests_--; }
        );

        day_limiter_ = std::make_shared<expirator::lockfree_expirator<std::string, RequestToken>>(
            &io_,
            [this](std::string, RequestToken) { day_requests_--; }
        );

        minute_limiter_->start();
        hour_limiter_->start();
        day_limiter_->start();
    }

    void set_user_tier(const std::string& user_id, Tier tier) {
        user_tiers_[user_id] = tier;
    }

    struct RateLimitResult {
        bool allowed;
        std::string reason;
        size_t remaining_minute;
        size_t remaining_hour;
        size_t remaining_day;
        std::chrono::seconds retry_after{0};
    };

    RateLimitResult check_rate_limit(const std::string& user_id, const std::string& endpoint) {
        auto tier_it = user_tiers_.find(user_id);
        if (tier_it == user_tiers_.end()) {
            return {false, "Unknown user", 0, 0, 0, std::chrono::seconds(60)};
        }

        const auto& config = configs_[tier_it->second];
        std::string key_base = user_id + ":" + endpoint;

        // Check minute limit
        size_t minute_count = minute_requests_.load();
        if (minute_count >= config.requests_per_minute) {
            return {
                false,
                "Minute rate limit exceeded",
                0,
                config.requests_per_hour - hour_requests_.load(),
                config.requests_per_day - day_requests_.load(),
                std::chrono::seconds(60)
            };
        }

        // Check hour limit
        size_t hour_count = hour_requests_.load();
        if (hour_count >= config.requests_per_hour) {
            return {
                false,
                "Hour rate limit exceeded",
                config.requests_per_minute - minute_count,
                0,
                config.requests_per_day - day_requests_.load(),
                std::chrono::seconds(3600)
            };
        }

        // Check day limit
        size_t day_count = day_requests_.load();
        if (day_count >= config.requests_per_day) {
            return {
                false,
                "Daily rate limit exceeded",
                config.requests_per_minute - minute_count,
                config.requests_per_hour - hour_count,
                0,
                std::chrono::seconds(86400)
            };
        }

        // Record the request
        RequestToken token{std::chrono::steady_clock::now(), endpoint};

        std::string minute_key = key_base + ":m:" + std::to_string(std::rand());
        std::string hour_key = key_base + ":h:" + std::to_string(std::rand());
        std::string day_key = key_base + ":d:" + std::to_string(std::rand());

        minute_limiter_->add(minute_key, std::chrono::minutes(1), token);
        hour_limiter_->add(hour_key, std::chrono::hours(1), token);
        day_limiter_->add(day_key, std::chrono::hours(24), token);

        minute_requests_++;
        hour_requests_++;
        day_requests_++;

        return {
            true,
            "OK",
            config.requests_per_minute - minute_count - 1,
            config.requests_per_hour - hour_count - 1,
            config.requests_per_day - day_count - 1,
            std::chrono::seconds(0)
        };
    }

    void print_stats() const {
        std::cout << "\n📊 Rate Limiter Stats:" << std::endl;
        std::cout << "   Minute requests: " << minute_requests_.load() << std::endl;
        std::cout << "   Hour requests: " << hour_requests_.load() << std::endl;
        std::cout << "   Day requests: " << day_requests_.load() << std::endl;
    }
};

void run_example() {
    std::cout << "\n=== Multi-Tier API Rate Limiting ===" << std::endl;

    boost::asio::io_context io;
    MultiTierRateLimiter limiter(io);

    // Set up users
    limiter.set_user_tier("user_free", Tier::FREE);
    limiter.set_user_tier("user_premium", Tier::PREMIUM);

    std::thread io_thread([&io]() { io.run(); });

    // Simulate API requests
    std::cout << "\n--- FREE tier user (60 req/min) ---" << std::endl;
    for (int i = 0; i < 65; ++i) {
        auto result = limiter.check_rate_limit("user_free", "/api/data");
        if (!result.allowed) {
            std::cout << "❌ Request " << i << " denied: " << result.reason
                      << " (retry after " << result.retry_after.count() << "s)" << std::endl;
            break;
        }
        if (i % 10 == 0) {
            std::cout << "✅ Request " << i << " allowed (remaining: "
                      << result.remaining_minute << ")" << std::endl;
        }
    }

    std::cout << "\n--- PREMIUM tier user (1000 req/min) ---" << std::endl;
    for (int i = 0; i < 100; ++i) {
        auto result = limiter.check_rate_limit("user_premium", "/api/data");
        if (i % 20 == 0 && result.allowed) {
            std::cout << "✅ Request " << i << " allowed (remaining: "
                      << result.remaining_minute << ")" << std::endl;
        }
    }

    limiter.print_stats();

    io.stop();
    io_thread.join();
}

}  // namespace api_rate_limiting

// ============================================================================
// Example 2: Job Queue with Timeout and Retry
// ============================================================================
namespace job_queue {

enum class JobStatus { PENDING, RUNNING, COMPLETED, FAILED, TIMEOUT };

struct Job {
    std::string id;
    std::string type;
    std::function<bool()> execute;
    int retry_count{0};
    int max_retries{3};
    std::chrono::seconds timeout;
    JobStatus status{JobStatus::PENDING};
    std::chrono::steady_clock::time_point created_at;
};

class JobScheduler {
private:
    boost::asio::io_context& io_;
    std::shared_ptr<expirator::heap_expirator<std::string, Job>> timeout_tracker_;
    std::queue<std::shared_ptr<Job>> pending_jobs_;
    std::map<std::string, std::shared_ptr<Job>> active_jobs_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{true};
    std::atomic<size_t> completed_jobs_{0};
    std::atomic<size_t> failed_jobs_{0};

public:
    JobScheduler(boost::asio::io_context& io, size_t worker_count)
        : io_(io)
    {
        timeout_tracker_ = std::make_shared<expirator::heap_expirator<std::string, Job>>(
            &io_,
            [this](std::string job_id, Job job) {
                std::cout << "⏱️  Job timeout: " << job_id << std::endl;
                handle_timeout(job_id, job);
            }
        );

        timeout_tracker_->start();

        // Start worker threads
        for (size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this, i]() { worker_loop(i); });
        }
    }

    ~JobScheduler() {
        running_ = false;
        queue_cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    std::string submit_job(const std::string& type,
                          std::function<bool()> execute,
                          std::chrono::seconds timeout = std::chrono::seconds(30),
                          int max_retries = 3) {
        auto job = std::make_shared<Job>();
        job->id = "job_" + std::to_string(std::rand());
        job->type = type;
        job->execute = std::move(execute);
        job->timeout = timeout;
        job->max_retries = max_retries;
        job->created_at = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            pending_jobs_.push(job);
        }

        queue_cv_.notify_one();

        std::cout << "📝 Job submitted: " << job->id << " (type: " << type << ")" << std::endl;
        return job->id;
    }

    void print_stats() const {
        std::cout << "\n📊 Job Scheduler Stats:" << std::endl;
        std::cout << "   Completed: " << completed_jobs_.load() << std::endl;
        std::cout << "   Failed: " << failed_jobs_.load() << std::endl;
        std::cout << "   Pending: " << pending_jobs_.size() << std::endl;
        std::cout << "   Active: " << active_jobs_.size() << std::endl;
    }

private:
    void worker_loop(size_t worker_id) {
        while (running_) {
            std::shared_ptr<Job> job;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this]() {
                    return !pending_jobs_.empty() || !running_;
                });

                if (!running_) break;

                if (!pending_jobs_.empty()) {
                    job = pending_jobs_.front();
                    pending_jobs_.pop();
                }
            }

            if (job) {
                execute_job(worker_id, job);
            }
        }
    }

    void execute_job(size_t worker_id, std::shared_ptr<Job> job) {
        job->status = JobStatus::RUNNING;
        active_jobs_[job->id] = job;

        // Start timeout tracking
        timeout_tracker_->add(job->id, job->timeout, *job);

        std::cout << "⚙️  Worker " << worker_id << " executing: " << job->id << std::endl;

        bool success = false;
        try {
            success = job->execute();
        } catch (const std::exception& e) {
            std::cout << "❌ Job " << job->id << " threw exception: " << e.what() << std::endl;
        }

        // Remove from timeout tracking
        timeout_tracker_->remove(job->id);
        active_jobs_.erase(job->id);

        if (success) {
            job->status = JobStatus::COMPLETED;
            completed_jobs_++;
            std::cout << "✅ Job completed: " << job->id << std::endl;
        } else {
            handle_failure(job);
        }
    }

    void handle_failure(std::shared_ptr<Job> job) {
        job->retry_count++;

        if (job->retry_count < job->max_retries) {
            std::cout << "🔄 Job retry " << job->retry_count << "/" << job->max_retries
                      << ": " << job->id << std::endl;

            // Requeue with exponential backoff
            std::this_thread::sleep_for(
                std::chrono::seconds(1) * (1 << job->retry_count)
            );

            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                pending_jobs_.push(job);
            }
            queue_cv_.notify_one();
        } else {
            job->status = JobStatus::FAILED;
            failed_jobs_++;
            std::cout << "❌ Job failed permanently: " << job->id << std::endl;
        }
    }

    void handle_timeout(const std::string& job_id, Job job) {
        auto it = active_jobs_.find(job_id);
        if (it != active_jobs_.end()) {
            auto job_ptr = it->second;
            job_ptr->status = JobStatus::TIMEOUT;
            active_jobs_.erase(it);

            // Retry if possible
            handle_failure(job_ptr);
        }
    }
};

void run_example() {
    std::cout << "\n=== Job Queue with Timeout & Retry ===" << std::endl;

    boost::asio::io_context io;
    JobScheduler scheduler(io, 3);  // 3 worker threads

    std::thread io_thread([&io]() { io.run(); });

    // Submit various jobs
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 100);

    // Fast jobs
    for (int i = 0; i < 5; ++i) {
        scheduler.submit_job("fast_job", [i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return true;
        });
    }

    // Some failing jobs
    for (int i = 0; i < 3; ++i) {
        scheduler.submit_job("flaky_job", [&dis, &gen, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return dis(gen) > 70;  // 30% success rate
        }, std::chrono::seconds(5), 5);
    }

    // One job that times out
    scheduler.submit_job("slow_job", []() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return true;
    }, std::chrono::seconds(2), 2);

    // Wait for jobs to complete
    std::this_thread::sleep_for(std::chrono::seconds(5));

    scheduler.print_stats();

    io.stop();
    io_thread.join();
}

}  // namespace job_queue

// ============================================================================
// Example 3: Websocket Connection Manager with Heartbeat
// ============================================================================
namespace websocket_manager {

struct WebSocketConnection {
    std::string connection_id;
    std::string client_ip;
    std::chrono::steady_clock::time_point last_heartbeat;
    std::chrono::steady_clock::time_point connected_at;
    size_t messages_sent{0};
    size_t messages_received{0};
};

class ConnectionManager {
private:
    boost::asio::io_context& io_;
    std::shared_ptr<expirator::heap_expirator<std::string, WebSocketConnection>> heartbeat_tracker_;
    std::map<std::string, std::shared_ptr<WebSocketConnection>> connections_;
    std::chrono::seconds heartbeat_timeout_;

public:
    ConnectionManager(boost::asio::io_context& io,
                     std::chrono::seconds heartbeat_timeout = std::chrono::seconds(30))
        : io_(io), heartbeat_timeout_(heartbeat_timeout)
    {
        heartbeat_tracker_ = std::make_shared<expirator::heap_expirator<
            std::string, WebSocketConnection
        >>(
            &io_,
            [this](std::string conn_id, WebSocketConnection conn) {
                std::cout << "💔 Connection lost (no heartbeat): " << conn_id
                          << " (last seen: "
                          << std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - conn.last_heartbeat
                             ).count() << "s ago)" << std::endl;

                disconnect(conn_id);
            }
        );

        heartbeat_tracker_->start();
    }

    std::string connect(const std::string& client_ip) {
        auto conn = std::make_shared<WebSocketConnection>();
        conn->connection_id = "ws_" + std::to_string(std::rand());
        conn->client_ip = client_ip;
        conn->connected_at = std::chrono::steady_clock::now();
        conn->last_heartbeat = conn->connected_at;

        connections_[conn->connection_id] = conn;

        // Start heartbeat tracking
        heartbeat_tracker_->add(conn->connection_id, heartbeat_timeout_, *conn);

        std::cout << "🔌 Connection established: " << conn->connection_id
                  << " from " << client_ip << std::endl;

        return conn->connection_id;
    }

    void heartbeat(const std::string& connection_id) {
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            it->second->last_heartbeat = std::chrono::steady_clock::now();

            // Reset heartbeat timeout
            heartbeat_tracker_->update_expiry(connection_id, heartbeat_timeout_);

            std::cout << "💓 Heartbeat: " << connection_id << std::endl;
        }
    }

    void send_message(const std::string& connection_id, const std::string& message) {
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            it->second->messages_sent++;
            std::cout << "📤 Sent message to " << connection_id
                      << ": " << message << std::endl;
        }
    }

    void receive_message(const std::string& connection_id, const std::string& message) {
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            it->second->messages_received++;
            it->second->last_heartbeat = std::chrono::steady_clock::now();

            // Reset timeout on message activity
            heartbeat_tracker_->update_expiry(connection_id, heartbeat_timeout_);

            std::cout << "📥 Received message from " << connection_id
                      << ": " << message << std::endl;
        }
    }

    void disconnect(const std::string& connection_id) {
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            auto conn = it->second;
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - conn->connected_at
            );

            std::cout << "🔌 Connection closed: " << connection_id
                      << " (duration: " << duration.count() << "s, "
                      << "sent: " << conn->messages_sent << ", "
                      << "received: " << conn->messages_received << ")" << std::endl;

            heartbeat_tracker_->remove(connection_id);
            connections_.erase(it);
        }
    }

    size_t active_connections() const {
        return connections_.size();
    }

    void print_stats() const {
        std::cout << "\n📊 WebSocket Manager Stats:" << std::endl;
        std::cout << "   Active connections: " << active_connections() << std::endl;

        size_t total_sent = 0;
        size_t total_received = 0;

        for (const auto& [id, conn] : connections_) {
            total_sent += conn->messages_sent;
            total_received += conn->messages_received;
        }

        std::cout << "   Total messages sent: " << total_sent << std::endl;
        std::cout << "   Total messages received: " << total_received << std::endl;
    }
};

void run_example() {
    std::cout << "\n=== WebSocket Connection Manager ===" << std::endl;

    boost::asio::io_context io;
    ConnectionManager manager(io, std::chrono::seconds(5));

    std::thread io_thread([&io]() { io.run(); });

    // Simulate connections
    auto conn1 = manager.connect("192.168.1.100");
    auto conn2 = manager.connect("192.168.1.101");
    auto conn3 = manager.connect("192.168.1.102");

    std::cout << "\nActive connections: " << manager.active_connections() << std::endl;

    // Simulate activity
    std::thread activity([&manager, conn1, conn2, conn3]() {
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // conn1 sends heartbeats
            if (i % 2 == 0) {
                manager.heartbeat(conn1);
            }

            // conn2 sends messages (which also count as activity)
            if (i % 3 == 0) {
                manager.receive_message(conn2, "Hello from client");
                manager.send_message(conn2, "Hello from server");
            }

            // conn3 goes silent (will timeout)
        }
    });

    activity.join();

    manager.print_stats();

    io.stop();
    io_thread.join();
}

}  // namespace websocket_manager

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "🚀 Production Scenario Examples\n" << std::endl;

    try {
        api_rate_limiting::run_example();
        job_queue::run_example();
        websocket_manager::run_example();

        std::cout << "\n✅ All production examples completed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}