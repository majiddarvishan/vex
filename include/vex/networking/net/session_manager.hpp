#pragma once

#include <vex/networking/net/session_impl.hpp>
#include <vex/networking/net/session_builder.hpp>

#include <boost/asio.hpp>

#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

namespace pa::pinex
{

// ============================================================================
// Session Manager
// ============================================================================

class session_manager
{
public:
    using session_id = uint64_t;

private:
    boost::asio::io_context& io_context_;
    std::unordered_map<session_id, session_ptr> sessions_;
    std::atomic<session_id> next_id_{1};
    mutable std::mutex mutex_; // For thread-safe access to sessions map

public:
    explicit session_manager(boost::asio::io_context& io)
        : io_context_(io) {}

    // Add session with automatic configuration
    session_id add_session(boost::asio::ip::tcp::socket socket,
                          std::function<void(session_ptr)> configure = {})
    {
        auto id = next_id_++;

        auto builder = session_builder();

        auto session = builder
            .with_close_handler([this, id](session_ptr, std::optional<std::string>) {
                this->remove_session(id);
            })
            .build(io_context_, std::move(socket));

        if (configure)
            configure(session);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_[id] = session;
        }

        return id;
    }

    // Add pre-built session
    session_id add_session(session_ptr session)
    {
        auto id = next_id_++;

        // Wrap existing close handler
        auto existing_handler = [session]() -> typename session_impl::close_callback {
            // This is a hack to get the handler, you'd need to expose it or handle differently
            return {};
        }();

        session->set_close_handler([this, id, existing_handler](auto s, auto reason) {
            if (existing_handler)
                existing_handler(s, reason);
            this->remove_session(id);
        });

        {
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_[id] = session;
        }

        return id;
    }

    // Retrieve session by ID
    session_ptr get_session(session_id id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(id);
        return it != sessions_.end() ? it->second : nullptr;
    }

    // Remove session
    void remove_session(session_id id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(id);
    }

    // Close all sessions gracefully
    void close_all()
    {
        std::vector<session_ptr> to_close;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            to_close.reserve(sessions_.size());
            for (auto& [id, session] : sessions_)
                to_close.push_back(session);
        }

        for (auto& session : to_close)
            session->unbind();
    }

    // Close all sessions immediately
    void close_all_immediate()
    {
        std::vector<session_ptr> to_close;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            to_close.reserve(sessions_.size());
            for (auto& [id, session] : sessions_)
                to_close.push_back(session);
        }

        for (auto& session : to_close)
            session->close("Manager shutdown");
    }

    // Get active session count
    size_t active_count() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

    // Get all session IDs
    std::vector<session_id> get_all_ids() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<session_id> ids;
        ids.reserve(sessions_.size());
        for (auto& [id, _] : sessions_)
            ids.push_back(id);
        return ids;
    }

    // Apply function to all sessions
    void for_each(std::function<void(session_id, session_ptr)> func) const
    {
        std::vector<std::pair<session_id, session_ptr>> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot.reserve(sessions_.size());
            for (auto& [id, session] : sessions_)
                snapshot.emplace_back(id, session);
        }

        for (auto& [id, session] : snapshot)
            func(id, session);
    }

    // Aggregate metrics
    struct aggregate_metrics
    {
        uint64_t total_bytes_sent{0};
        uint64_t total_bytes_received{0};
        uint64_t total_messages_sent{0};
        uint64_t total_messages_received{0};
        uint64_t total_errors{0};
        size_t active_sessions{0};
        size_t open_sessions{0};
        size_t closed_sessions{0};
    };

    aggregate_metrics get_metrics() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        aggregate_metrics agg;
        agg.active_sessions = sessions_.size();

        for (auto& [id, session] : sessions_)
        {
            auto& m = session->metrics();
            agg.total_bytes_sent += m.bytes_sent.load();
            agg.total_bytes_received += m.bytes_received.load();
            agg.total_messages_sent += m.messages_sent.load();
            agg.total_messages_received += m.messages_received.load();
            agg.total_errors += m.errors.load();

            if (session->is_open())
                agg.open_sessions++;
            if (m.is_closed.load())
                agg.closed_sessions++;
        }

        return agg;
    }

    // Clear all closed sessions from the map
    size_t cleanup_closed_sessions()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t removed = 0;

        for (auto it = sessions_.begin(); it != sessions_.end();)
        {
            if (it->second->metrics().is_closed.load())
            {
                it = sessions_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }

        return removed;
    }
};

} // namespace pa::pinex