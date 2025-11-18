#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <map>
#include <unordered_map>
#include <optional>
#include <functional>

namespace pa::pinex::io
{
template<typename Tkey, typename Tinfo>
class expirator : public std::enable_shared_from_this<expirator<Tkey, Tinfo>>
{
  public:
    using expiry_handler = std::function<void(Tkey, Tinfo)>;
    using error_handler = std::function<void(const boost::system::error_code&)>;
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;
    using duration = clock_type::duration;

  private:
    struct entry_data
    {
        time_point expiry;
        Tinfo info;
        typename std::multimap<time_point, Tkey>::iterator queue_iter;
    };

    boost::asio::steady_timer timer_;
    std::unordered_map<Tkey, entry_data> entries_;
    std::multimap<time_point, Tkey> expiry_queue_;
    const expiry_handler expiry_handler_;
    const error_handler error_handler_;
    bool running_{false};

  public:
    expirator(boost::asio::io_context* io_context_ptr,
              expiry_handler handler,
              error_handler err_handler = nullptr)
      : timer_(*io_context_ptr)
      , expiry_handler_(std::move(handler))
      , error_handler_(std::move(err_handler))
    {
        if (!expiry_handler_)
            throw std::invalid_argument("expiry_handler cannot be null");
    }

    expirator(const expirator&) = delete;
    expirator& operator=(const expirator&) = delete;
    expirator(expirator&&) = delete;
    expirator& operator=(expirator&&) = delete;

    ~expirator()
    {
        stop();
    }

    void start()
    {
        if (!running_ && !expiry_queue_.empty())
        {
            running_ = true;
            schedule_next();
        }
    }

    void stop()
    {
        if (running_)
        {
            running_ = false;
            boost::system::error_code ec;
            timer_.cancel(ec);
        }
    }

    bool add(Tkey key, duration expiration_duration, Tinfo info = Tinfo{})
    {
        // Reject if key already exists
        if (entries_.find(key) != entries_.end())
            return false;

        // Calculate absolute expiry time
        time_point expiry = clock_type::now() + expiration_duration;

        // Insert into expiry queue first
        auto queue_iter = expiry_queue_.emplace(expiry, key);

        // Insert into entries map with iterator to queue entry
        entries_[key] = {expiry, std::move(info), queue_iter};

        // If this is now the earliest expiry, reschedule timer
        if (running_ && queue_iter == expiry_queue_.begin())
        {
            boost::system::error_code ec;
            timer_.cancel(ec);
            schedule_next();
        }
        // If we weren't running but now have items, auto-start
        else if (!running_)
        {
            start();
        }

        return true;
    }

    bool update_expiry(const Tkey& key, duration new_expiration_duration)
    {
        auto entry_it = entries_.find(key);
        if (entry_it == entries_.end())
            return false;

        // Remove old entry from queue using stored iterator (O(1))
        expiry_queue_.erase(entry_it->second.queue_iter);

        // Calculate new expiry time
        time_point new_expiry = clock_type::now() + new_expiration_duration;

        // Insert into queue with new expiry
        auto new_queue_iter = expiry_queue_.emplace(new_expiry, key);

        // Update entry
        entry_it->second.expiry = new_expiry;
        entry_it->second.queue_iter = new_queue_iter;

        // Reschedule if this became the earliest expiry
        if (running_ && new_queue_iter == expiry_queue_.begin())
        {
            boost::system::error_code ec;
            timer_.cancel(ec);
            schedule_next();
        }

        return true;
    }

    bool refresh(const Tkey& key, duration extension_duration)
    {
        auto entry_it = entries_.find(key);
        if (entry_it == entries_.end())
            return false;

        // Calculate new expiry based on current expiry (not now)
        time_point new_expiry = entry_it->second.expiry + extension_duration;

        // Remove old entry from queue
        expiry_queue_.erase(entry_it->second.queue_iter);

        // Insert with new expiry
        auto new_queue_iter = expiry_queue_.emplace(new_expiry, key);

        // Update entry
        entry_it->second.expiry = new_expiry;
        entry_it->second.queue_iter = new_queue_iter;

        // Reschedule if needed
        if (running_ && new_queue_iter == expiry_queue_.begin())
        {
            boost::system::error_code ec;
            timer_.cancel(ec);
            schedule_next();
        }

        return true;
    }

    std::optional<Tinfo> get_info(const Tkey& key) const
    {
        auto it = entries_.find(key);
        if (it != entries_.end())
            return it->second.info;
        return {};
    }

    std::optional<duration> get_remaining_time(const Tkey& key) const
    {
        auto it = entries_.find(key);
        if (it != entries_.end())
        {
            auto remaining = it->second.expiry - clock_type::now();
            return remaining > duration::zero() ? remaining : duration::zero();
        }
        return {};
    }

    bool contains(const Tkey& key) const
    {
        return entries_.find(key) != entries_.end();
    }

    bool remove(const Tkey& key)
    {
        auto entry_it = entries_.find(key);
        if (entry_it == entries_.end())
            return false;

        // Remove from queue using stored iterator (O(1))
        expiry_queue_.erase(entry_it->second.queue_iter);

        // Remove from entries
        entries_.erase(entry_it);

        return true;
    }

    void expire_all()
    {
        // Process all entries
        for (const auto& [key, entry] : entries_)
        {
            expiry_handler_(key, entry.info);
        }

        entries_.clear();
        expiry_queue_.clear();

        // Stop timer if running
        if (running_)
        {
            boost::system::error_code ec;
            timer_.cancel(ec);
            running_ = false;
        }
    }

    void clear()
    {
        entries_.clear();
        expiry_queue_.clear();

        if (running_)
        {
            boost::system::error_code ec;
            timer_.cancel(ec);
            running_ = false;
        }
    }

    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }
    bool is_running() const { return running_; }

  private:
    void schedule_next()
    {
        if (expiry_queue_.empty())
        {
            running_ = false;
            return;
        }

        // Get the earliest expiry time
        time_point next_expiry = expiry_queue_.begin()->first;

        // Set timer to expire at that exact time
        timer_.expires_at(next_expiry);

        timer_.async_wait([self = this->shared_from_this()](const boost::system::error_code& ec) {
            // Timer was cancelled (normal during reschedule or stop)
            if (ec == boost::asio::error::operation_aborted)
                return;

            // Object is being destroyed
            if (!self->running_)
                return;

            // Timer error
            if (ec)
            {
                if (self->error_handler_)
                    self->error_handler_(ec);
                self->running_ = false;
                return;
            }

            // Process expired entries
            self->process_expired();

            // Schedule next expiry if still running
            if (self->running_)
                self->schedule_next();
        });
    }

    void process_expired()
    {
        time_point now = clock_type::now();

        // Process all entries that have expired
        // Note: We iterate and collect keys first to avoid iterator invalidation
        std::vector<Tkey> expired_keys;

        auto it = expiry_queue_.begin();
        while (it != expiry_queue_.end() && it->first <= now)
        {
            expired_keys.push_back(it->second);
            ++it;
        }

        // Now process and remove
        for (const auto& key : expired_keys)
        {
            auto entry_it = entries_.find(key);
            if (entry_it != entries_.end())
            {
                // Call handler with copy of info (in case handler throws)
                Tinfo info_copy = entry_it->second.info;

                // Remove from queue
                expiry_queue_.erase(entry_it->second.queue_iter);

                // Remove from entries
                entries_.erase(entry_it);

                // Call handler (do this last in case it throws)
                expiry_handler_(key, std::move(info_copy));
            }
        }
    }
};
}  // namespace pa::pinex::io