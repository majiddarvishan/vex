// ============================================================================
// examples/advanced_patterns.cpp
// ============================================================================
#include <expirator/heap_expirator.hpp>
#include <expirator/timing_wheel_expirator.hpp>
#include <expirator/lockfree_expirator.hpp>
#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <random>
#include <thread>

// ============================================================================
// Example 1: Session Management System
// ============================================================================
namespace session_management {

struct Session {
    std::string user_id;
    std::string ip_address;
    std::chrono::system_clock::time_point created_at;
    std::map<std::string, std::string> metadata;
};

class SessionManager {
private:
    boost::asio::io_context& io_;
    std::shared_ptr<expirator::heap_expirator<std::string, Session>> expirator_;
    std::map<std::string, std::string> user_to_session_;  // user_id -> session_id

public:
    SessionManager(boost::asio::io_context& io, std::chrono::seconds session_timeout)
        : io_(io)
    {
        expirator_ = std::make_shared<expirator::heap_expirator<std::string, Session>>(
            &io_,
            [this](std::string session_id, Session session) {
                std::cout << "🔒 Session expired: " << session_id
                          << " (user: " << session.user_id << ")" << std::endl;
                user_to_session_.erase(session.user_id);
            },
            [](const boost::system::error_code& ec) {
                std::cerr << "❌ Session manager error: " << ec.message() << std::endl;
            }
        );

        expirator_->start();
    }

    std::string create_session(const std::string& user_id, const std::string& ip) {
        // Generate session ID
        std::string session_id = "sess_" + std::to_string(std::rand()) + "_" + user_id;

        // Check if user already has a session
        auto it = user_to_session_.find(user_id);
        if (it != user_to_session_.end()) {
            // Extend existing session
            extend_session(it->second);
            return it->second;
        }

        // Create new session
        Session session{
            user_id,
            ip,
            std::chrono::system_clock::now(),
            {}
        };

        expirator_->add(session_id, std::chrono::seconds(30), session);
        user_to_session_[user_id] = session_id;

        std::cout << "✅ Created session: " << session_id << " for user: " << user_id << std::endl;
        return session_id;
    }

    bool extend_session(const std::string& session_id) {
        if (expirator_->update_expiry(session_id, std::chrono::seconds(30))) {
            std::cout << "⏰ Extended session: " << session_id << std::endl;
            return true;
        }
        return false;
    }

    bool logout(const std::string& session_id) {
        auto session_info = expirator_->get_info(session_id);
        if (expirator_->remove(session_id)) {
            if (session_info) {
                user_to_session_.erase(session_info->user_id);
            }
            std::cout << "👋 Logged out session: " << session_id << std::endl;
            return true;
        }
        return false;
    }

    size_t active_sessions() const {
        return expirator_->size();
    }
};

void run_example() {
    std::cout << "\n=== Session Management Example ===" << std::endl;

    boost::asio::io_context io;
    SessionManager manager(io, std::chrono::seconds(30));

    // Create sessions
    auto sess1 = manager.create_session("alice", "192.168.1.1");
    auto sess2 = manager.create_session("bob", "192.168.1.2");
    auto sess3 = manager.create_session("charlie", "192.168.1.3");

    std::cout << "Active sessions: " << manager.active_sessions() << std::endl;

    // Simulate some activity
    std::thread activity([&manager, sess1]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        manager.extend_session(sess1);
    });

    // Run for a bit
    io.run_for(std::chrono::seconds(2));

    activity.join();
    std::cout << "Remaining sessions: " << manager.active_sessions() << std::endl;
}

}  // namespace session_management

// ============================================================================
// Example 2: Cache with TTL (Time To Live)
// ============================================================================
namespace cache_system {

template<typename TKey, typename TValue>
class TTLCache {
private:
    boost::asio::io_context& io_;
    std::shared_ptr<expirator::timing_wheel_expirator<
        TKey, TValue, boost::fast_pool_allocator<char>
    >> expirator_;
    std::map<TKey, TValue> cache_;
    std::chrono::milliseconds default_ttl_;

    size_t hits_{0};
    size_t misses_{0};

public:
    TTLCache(boost::asio::io_context& io, std::chrono::milliseconds default_ttl)
        : io_(io), default_ttl_(default_ttl)
    {
        expirator_ = std::make_shared<expirator::timing_wheel_expirator<
            TKey, TValue, boost::fast_pool_allocator<char>
        >>(
            &io_,
            [this](TKey key, TValue) {
                cache_.erase(key);
                std::cout << "🗑️  Cache entry expired: " << key << std::endl;
            }
        );

        expirator_->start();
    }

    void put(const TKey& key, const TValue& value,
             std::chrono::milliseconds ttl = std::chrono::milliseconds(0)) {
        if (ttl == std::chrono::milliseconds(0)) {
            ttl = default_ttl_;
        }

        // Remove old entry if exists
        if (cache_.find(key) != cache_.end()) {
            expirator_->remove(key);
        }

        cache_[key] = value;
        expirator_->add(key, ttl, value);

        std::cout << "💾 Cache PUT: " << key << " (TTL: "
                  << ttl.count() << "ms)" << std::endl;
    }

    std::optional<TValue> get(const TKey& key) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            hits_++;
            std::cout << "✅ Cache HIT: " << key << std::endl;
            return it->second;
        }

        misses_++;
        std::cout << "❌ Cache MISS: " << key << std::endl;
        return std::nullopt;
    }

    void touch(const TKey& key) {
        if (cache_.find(key) != cache_.end()) {
            // For timing wheel, we need to remove and re-add
            auto value_opt = expirator_->get_info(key);
            if (value_opt) {
                expirator_->remove(key);
                expirator_->add(key, default_ttl_, *value_opt);
                std::cout << "👆 Cache TOUCH: " << key << std::endl;
            }
        }
    }

    void print_stats() const {
        size_t total = hits_ + misses_;
        double hit_rate = total > 0 ? (100.0 * hits_ / total) : 0.0;

        std::cout << "\n📊 Cache Statistics:" << std::endl;
        std::cout << "   Size: " << cache_.size() << std::endl;
        std::cout << "   Hits: " << hits_ << std::endl;
        std::cout << "   Misses: " << misses_ << std::endl;
        std::cout << "   Hit Rate: " << hit_rate << "%" << std::endl;
    }
};

void run_example() {
    std::cout << "\n=== TTL Cache Example ===" << std::endl;

    boost::asio::io_context io;
    TTLCache<std::string, std::string> cache(io, std::chrono::milliseconds(500));

    // Add some data
    cache.put("user:1", "Alice");
    cache.put("user:2", "Bob");
    cache.put("user:3", "Charlie", std::chrono::milliseconds(200));  // Custom TTL

    // Access data
    cache.get("user:1");
    cache.get("user:2");
    cache.get("user:999");  // Miss

    // Touch to extend TTL
    cache.touch("user:1");

    // Run and let some entries expire
    io.run_for(std::chrono::milliseconds(300));

    cache.get("user:1");  // Should still exist
    cache.get("user:3");  // Should be expired

    cache.print_stats();
}

}  // namespace cache_system

// ============================================================================
// Example 3: Rate Limiter
// ============================================================================
namespace rate_limiter {

class TokenBucket {
private:
    boost::asio::io_context& io_;
    std::shared_ptr<expirator::heap_expirator<int, std::chrono::steady_clock::time_point>> expirator_;
    std::string client_id_;
    size_t max_requests_;
    std::chrono::milliseconds window_;
    size_t current_tokens_{0};
    int token_counter_{0};

public:
    TokenBucket(boost::asio::io_context& io,
                const std::string& client_id,
                size_t max_requests,
                std::chrono::milliseconds window)
        : io_(io), client_id_(client_id), max_requests_(max_requests), window_(window)
    {
        current_tokens_ = max_requests_;

        expirator_ = std::make_shared<expirator::heap_expirator<
            int, std::chrono::steady_clock::time_point
        >>(
            &io_,
            [this](int, std::chrono::steady_clock::time_point) {
                current_tokens_++;
                std::cout << "🪙 Token restored for " << client_id_
                          << " (" << current_tokens_ << "/" << max_requests_ << ")" << std::endl;
            }
        );

        expirator_->start();
    }

    bool try_consume() {
        if (current_tokens_ > 0) {
            current_tokens_--;

            // Add token expiration
            expirator_->add(token_counter_++, window_,
                           std::chrono::steady_clock::now());

            std::cout << "✅ Request allowed for " << client_id_
                      << " (remaining: " << current_tokens_ << ")" << std::endl;
            return true;
        }

        std::cout << "⛔ Rate limit exceeded for " << client_id_ << std::endl;
        return false;
    }

    size_t available_tokens() const {
        return current_tokens_;
    }
};

void run_example() {
    std::cout << "\n=== Rate Limiter Example ===" << std::endl;

    boost::asio::io_context io;

    // Allow 5 requests per second
    TokenBucket limiter(io, "client_123", 5, std::chrono::seconds(1));

    // Simulate requests
    for (int i = 0; i < 7; ++i) {
        limiter.try_consume();
    }

    std::cout << "\nWaiting for tokens to refill..." << std::endl;

    // Run and let tokens refill
    io.run_for(std::chrono::milliseconds(1100));

    std::cout << "\nTrying again after refill:" << std::endl;
    for (int i = 0; i < 3; ++i) {
        limiter.try_consume();
    }
}

}  // namespace rate_limiter

// ============================================================================
// Example 4: Distributed Lock with Auto-Release
// ============================================================================
namespace distributed_lock {

class AutoReleaseLock {
private:
    boost::asio::io_context& io_;
    std::shared_ptr<expirator::heap_expirator<std::string, std::string>> expirator_;
    std::map<std::string, std::string> locks_;  // resource -> owner

public:
    AutoReleaseLock(boost::asio::io_context& io)
        : io_(io)
    {
        expirator_ = std::make_shared<expirator::heap_expirator<std::string, std::string>>(
            &io_,
            [this](std::string resource, std::string owner) {
                locks_.erase(resource);
                std::cout << "🔓 Lock auto-released: " << resource
                          << " (owner: " << owner << ")" << std::endl;
            }
        );

        expirator_->start();
    }

    bool acquire(const std::string& resource, const std::string& owner,
                 std::chrono::seconds timeout = std::chrono::seconds(30)) {
        if (locks_.find(resource) != locks_.end()) {
            std::cout << "⛔ Lock already held: " << resource << std::endl;
            return false;
        }

        locks_[resource] = owner;
        expirator_->add(resource, timeout, owner);

        std::cout << "🔒 Lock acquired: " << resource
                  << " by " << owner
                  << " (timeout: " << timeout.count() << "s)" << std::endl;
        return true;
    }

    bool release(const std::string& resource, const std::string& owner) {
        auto it = locks_.find(resource);
        if (it == locks_.end()) {
            std::cout << "⚠️  Lock not found: " << resource << std::endl;
            return false;
        }

        if (it->second != owner) {
            std::cout << "⛔ Lock owned by different process" << std::endl;
            return false;
        }

        locks_.erase(it);
        expirator_->remove(resource);

        std::cout << "🔓 Lock released: " << resource << " by " << owner << std::endl;
        return true;
    }

    bool extend(const std::string& resource, std::chrono::seconds additional_time) {
        if (locks_.find(resource) == locks_.end()) {
            return false;
        }

        if (expirator_->update_expiry(resource, additional_time)) {
            std::cout << "⏰ Lock extended: " << resource << std::endl;
            return true;
        }
        return false;
    }
};

void run_example() {
    std::cout << "\n=== Distributed Lock Example ===" << std::endl;

    boost::asio::io_context io;
    AutoReleaseLock lock_manager(io);

    // Process 1 acquires lock
    lock_manager.acquire("db:table1", "process_1", std::chrono::seconds(2));

    // Process 2 tries to acquire same lock
    lock_manager.acquire("db:table1", "process_2", std::chrono::seconds(2));

    // Process 1 extends lock
    std::this_thread::sleep_for(std::chrono::seconds(1));
    lock_manager.extend("db:table1", std::chrono::seconds(3));

    // Let the lock expire
    io.run_for(std::chrono::seconds(4));

    // Now process 2 can acquire
    lock_manager.acquire("db:table1", "process_2", std::chrono::seconds(2));
}

}  // namespace distributed_lock

// ============================================================================
// Example 5: Connection Pool with Idle Timeout
// ============================================================================
namespace connection_pool {

struct Connection {
    int id;
    std::string host;
    int port;
    bool in_use{false};
    std::chrono::steady_clock::time_point last_used;

    void execute_query(const std::string& query) {
        std::cout << "🔌 Conn[" << id << "] executing: " << query << std::endl;
        last_used = std::chrono::steady_clock::now();
    }
};

class ConnectionPool {
private:
    boost::asio::io_context& io_;
    std::shared_ptr<expirator::heap_expirator<int, Connection>> expirator_;
    std::vector<std::shared_ptr<Connection>> connections_;
    std::chrono::seconds idle_timeout_;
    int next_id_{0};

public:
    ConnectionPool(boost::asio::io_context& io,
                   size_t pool_size,
                   std::chrono::seconds idle_timeout)
        : io_(io), idle_timeout_(idle_timeout)
    {
        expirator_ = std::make_shared<expirator::heap_expirator<int, Connection>>(
            &io_,
            [this](int conn_id, Connection) {
                std::cout << "⏱️  Connection " << conn_id
                          << " closed due to idle timeout" << std::endl;
                // In real implementation, actually close the connection
            }
        );

        expirator_->start();

        // Create initial connections
        for (size_t i = 0; i < pool_size; ++i) {
            create_connection();
        }
    }

    std::shared_ptr<Connection> acquire() {
        // Find available connection
        for (auto& conn : connections_) {
            if (!conn->in_use) {
                conn->in_use = true;

                // Remove from idle tracker
                expirator_->remove(conn->id);

                std::cout << "📤 Acquired connection: " << conn->id << std::endl;
                return conn;
            }
        }

        // No available connections, create new one
        auto conn = create_connection();
        conn->in_use = true;
        return conn;
    }

    void release(std::shared_ptr<Connection> conn) {
        conn->in_use = false;
        conn->last_used = std::chrono::steady_clock::now();

        // Add to idle timeout tracking
        expirator_->add(conn->id, idle_timeout_, *conn);

        std::cout << "📥 Released connection: " << conn->id << std::endl;
    }

    size_t size() const {
        return connections_.size();
    }

    size_t active_connections() const {
        return std::count_if(connections_.begin(), connections_.end(),
                           [](const auto& conn) { return conn->in_use; });
    }

private:
    std::shared_ptr<Connection> create_connection() {
        auto conn = std::make_shared<Connection>();
        conn->id = next_id_++;
        conn->host = "localhost";
        conn->port = 5432;
        conn->last_used = std::chrono::steady_clock::now();

        connections_.push_back(conn);

        std::cout << "🆕 Created connection: " << conn->id << std::endl;
        return conn;
    }
};

void run_example() {
    std::cout << "\n=== Connection Pool Example ===" << std::endl;

    boost::asio::io_context io;
    ConnectionPool pool(io, 3, std::chrono::seconds(2));

    // Acquire and use connections
    auto conn1 = pool.acquire();
    conn1->execute_query("SELECT * FROM users");

    auto conn2 = pool.acquire();
    conn2->execute_query("SELECT * FROM orders");

    std::cout << "Active connections: " << pool.active_connections() << std::endl;

    // Release connections
    pool.release(conn1);
    pool.release(conn2);

    std::cout << "Waiting for idle timeout..." << std::endl;
    io.run_for(std::chrono::seconds(3));

    std::cout << "Pool size after timeout: " << pool.size() << std::endl;
}

}  // namespace connection_pool

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "🚀 Advanced Expirator Examples\n" << std::endl;

    try {
        session_management::run_example();
        cache_system::run_example();
        rate_limiter::run_example();
        distributed_lock::run_example();
        connection_pool::run_example();

        std::cout << "\n✅ All examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}