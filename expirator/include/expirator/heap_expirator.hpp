#pragma once

#include <expirator/base_expirator.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <optional>
#include <limits>
#include <memory>

namespace expirator {

/**
 * @brief Heap-based expirator using min-heap for efficient expiration tracking
 *
 * This implementation provides:
 * - O(log n) insertion
 * - O(log n) removal
 * - O(1) access to earliest expiry
 * - Exact timing with no periodic wakeups
 * - Low memory overhead
 *
 * Best for: General-purpose expiration with balanced performance
 *
 * @tparam Tkey Type of the key (must be hashable)
 * @tparam Tinfo Type of the associated information
 */
template<typename Tkey, typename Tinfo>
class heap_expirator : public base_expirator<Tkey, Tinfo>,
                       public std::enable_shared_from_this<heap_expirator<Tkey, Tinfo>>
{
  public:
    using typename base_expirator<Tkey, Tinfo>::expiry_handler;
    using typename base_expirator<Tkey, Tinfo>::error_handler;
    using typename base_expirator<Tkey, Tinfo>::clock_type;
    using typename base_expirator<Tkey, Tinfo>::time_point;
    using typename base_expirator<Tkey, Tinfo>::duration;

  private:
    struct entry_data
    {
        time_point expiry;
        Tinfo info;
        size_t heap_index;
    };

    struct heap_node
    {
        time_point expiry;
        Tkey key;

        bool operator>(const heap_node& other) const {
            return expiry > other.expiry;
        }
    };

    boost::asio::steady_timer timer_;
    std::unordered_map<Tkey, entry_data> entries_;
    std::vector<heap_node> heap_;
    const expiry_handler expiry_handler_;
    const error_handler error_handler_;
    bool running_{false};

  public:
    /**
     * @brief Construct a new heap expirator
     *
     * @param io_context_ptr Pointer to boost::asio::io_context
     * @param handler Callback for expired entries
     * @param err_handler Optional error handler
     */
    heap_expirator(boost::asio::io_context* io_context_ptr,
                   expiry_handler handler,
                   error_handler err_handler = nullptr)
      : timer_(*io_context_ptr)
      , expiry_handler_(std::move(handler))
      , error_handler_(std::move(err_handler))
    {
        if (!expiry_handler_)
            throw std::invalid_argument("expiry_handler cannot be null");

        entries_.reserve(1024);
        heap_.reserve(1024);
    }

    heap_expirator(const heap_expirator&) = delete;
    heap_expirator& operator=(const heap_expirator&) = delete;
    heap_expirator(heap_expirator&&) = delete;
    heap_expirator& operator=(heap_expirator&&) = delete;

    ~heap_expirator() override
    {
        stop();
    }

    /**
     * @brief Reserve space for expected number of entries
     * @param capacity Number of entries to reserve space for
     */
    void reserve(size_t capacity)
    {
        entries_.reserve(capacity);
        heap_.reserve(capacity);
    }

    void start() override
    {
        if (!running_ && !heap_.empty())
        {
            running_ = true;
            schedule_next();
        }
    }

    void stop() override
    {
        if (running_)
        {
            running_ = false;
            boost::system::error_code ec;
            timer_.cancel(ec);
        }
    }

    bool add(Tkey key, duration expiration_duration, Tinfo info = Tinfo{}) override
    {
        if (entries_.find(key) != entries_.end())
            return false;

        time_point expiry = clock_type::now() + expiration_duration;

        size_t heap_index = heap_.size();
        heap_.push_back({expiry, key});

        entries_[key] = {expiry, std::move(info), heap_index};

        heap_bubble_up(heap_index);

        if (heap_index == 0 || heap_[0].key == key)
        {
            if (running_)
            {
                boost::system::error_code ec;
                timer_.cancel(ec);
                schedule_next();
            }
            else
            {
                start();
            }
        }

        return true;
    }

    /**
     * @brief Update expiry time for an existing entry
     * @param key The key to update
     * @param new_expiration_duration New expiration duration from now
     * @return true if updated, false if key not found
     */
    bool update_expiry(const Tkey& key, duration new_expiration_duration)
    {
        auto entry_it = entries_.find(key);
        if (entry_it == entries_.end())
            return false;

        time_point new_expiry = clock_type::now() + new_expiration_duration;
        time_point old_expiry = entry_it->second.expiry;
        size_t heap_idx = entry_it->second.heap_index;

        entry_it->second.expiry = new_expiry;
        heap_[heap_idx].expiry = new_expiry;

        if (new_expiry < old_expiry)
            heap_bubble_up(heap_idx);
        else
            heap_bubble_down(heap_idx);

        if (running_ && heap_idx == 0)
        {
            boost::system::error_code ec;
            timer_.cancel(ec);
            schedule_next();
        }

        return true;
    }

    std::optional<Tinfo> get_info(const Tkey& key) const override
    {
        auto it = entries_.find(key);
        if (it != entries_.end())
            return it->second.info;
        return {};
    }

    std::optional<duration> get_remaining_time(const Tkey& key) const override
    {
        auto it = entries_.find(key);
        if (it != entries_.end())
        {
            auto remaining = it->second.expiry - clock_type::now();
            return remaining > duration::zero() ? remaining : duration::zero();
        }
        return {};
    }

    bool contains(const Tkey& key) const override
    {
        return entries_.find(key) != entries_.end();
    }

    bool remove(const Tkey& key) override
    {
        auto entry_it = entries_.find(key);
        if (entry_it == entries_.end())
            return false;

        size_t heap_idx = entry_it->second.heap_index;
        entries_.erase(entry_it);

        if (heap_idx == heap_.size() - 1)
        {
            heap_.pop_back();
        }
        else
        {
            heap_[heap_idx] = heap_.back();
            heap_.pop_back();

            if (heap_idx < heap_.size())
            {
                auto& swapped_key = heap_[heap_idx].key;
                auto swapped_it = entries_.find(swapped_key);
                if (swapped_it != entries_.end())
                {
                    swapped_it->second.heap_index = heap_idx;

                    if (heap_idx > 0 && heap_[heap_idx].expiry < heap_[parent(heap_idx)].expiry)
                        heap_bubble_up(heap_idx);
                    else
                        heap_bubble_down(heap_idx);
                }
            }
        }

        return true;
    }

    void clear() override
    {
        entries_.clear();
        heap_.clear();

        if (running_)
        {
            boost::system::error_code ec;
            timer_.cancel(ec);
            running_ = false;
        }
    }

    size_t size() const override { return entries_.size(); }
    bool empty() const override { return entries_.empty(); }
    bool is_running() const override { return running_; }

  private:
    static size_t parent(size_t i) { return (i - 1) / 2; }
    static size_t left_child(size_t i) { return 2 * i + 1; }
    static size_t right_child(size_t i) { return 2 * i + 2; }

    void heap_bubble_up(size_t idx)
    {
        while (idx > 0)
        {
            size_t parent_idx = parent(idx);
            if (heap_[idx].expiry >= heap_[parent_idx].expiry)
                break;

            std::swap(heap_[idx], heap_[parent_idx]);
            entries_[heap_[idx].key].heap_index = idx;
            entries_[heap_[parent_idx].key].heap_index = parent_idx;

            idx = parent_idx;
        }
    }

    void heap_bubble_down(size_t idx)
    {
        size_t size = heap_.size();

        while (true)
        {
            size_t left = left_child(idx);
            size_t right = right_child(idx);
            size_t smallest = idx;

            if (left < size && heap_[left].expiry < heap_[smallest].expiry)
                smallest = left;

            if (right < size && heap_[right].expiry < heap_[smallest].expiry)
                smallest = right;

            if (smallest == idx)
                break;

            std::swap(heap_[idx], heap_[smallest]);
            entries_[heap_[idx].key].heap_index = idx;
            entries_[heap_[smallest].key].heap_index = smallest;

            idx = smallest;
        }
    }

    void schedule_next()
    {
        if (heap_.empty())
        {
            running_ = false;
            return;
        }

        time_point next_expiry = heap_[0].expiry;
        timer_.expires_at(next_expiry);

        timer_.async_wait([self = this->shared_from_this()](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
                return;

            if (!self->running_)
                return;

            if (ec)
            {
                if (self->error_handler_)
                    self->error_handler_(ec);
                self->running_ = false;
                return;
            }

            self->process_expired();

            if (self->running_)
                self->schedule_next();
        });
    }

    void process_expired()
    {
        time_point now = clock_type::now();

        while (!heap_.empty() && heap_[0].expiry <= now)
        {
            Tkey key = heap_[0].key;

            auto entry_it = entries_.find(key);
            if (entry_it != entries_.end())
            {
                Tinfo info = std::move(entry_it->second.info);

                heap_[0] = heap_.back();
                heap_.pop_back();

                if (!heap_.empty())
                {
                    entries_[heap_[0].key].heap_index = 0;
                    heap_bubble_down(0);
                }

                entries_.erase(entry_it);
                expiry_handler_(key, std::move(info));
            }
            else
            {
                heap_[0] = heap_.back();
                heap_.pop_back();
                if (!heap_.empty())
                {
                    entries_[heap_[0].key].heap_index = 0;
                    heap_bubble_down(0);
                }
            }
        }
    }
};

}  // namespace expirator