#pragma once

#include <expirator/base_expirator.hpp>
#include <boost/asio.hpp>
#include <atomic>
#include <array>
#include <map>

namespace expirator {

template<typename Tkey, typename Tinfo>
class lockfree_expirator : public base_expirator<Tkey, Tinfo>,
                           public std::enable_shared_from_this<lockfree_expirator<Tkey, Tinfo>>
{
  public:
    using typename base_expirator<Tkey, Tinfo>::expiry_handler;
    using typename base_expirator<Tkey, Tinfo>::error_handler;
    using typename base_expirator<Tkey, Tinfo>::clock_type;
    using typename base_expirator<Tkey, Tinfo>::time_point;
    using typename base_expirator<Tkey, Tinfo>::duration;

  private:
    struct operation {
        enum class op_type : uint8_t { ADD, REMOVE, STOP };
        op_type type;
        Tkey key;
        Tinfo info;
        time_point expiry;

        operation() : type(op_type::STOP) {}
        operation(op_type t, Tkey k, Tinfo i, time_point e)
            : type(t), key(std::move(k)), info(std::move(i)), expiry(e) {}
    };

    static constexpr size_t QUEUE_SIZE = 4096;
    std::array<operation, QUEUE_SIZE> op_queue_;
    std::atomic<size_t> write_index_{0};
    std::atomic<size_t> read_index_{0};

    struct entry_data {
        time_point expiry;
        Tinfo info;
    };

    boost::asio::steady_timer timer_;
    std::unordered_map<Tkey, entry_data> entries_;
    std::multimap<time_point, Tkey> expiry_queue_;
    const expiry_handler expiry_handler_;
    const error_handler error_handler_;
    std::atomic<bool> running_{false};

  public:
    lockfree_expirator(boost::asio::io_context* io_context_ptr,
                       expiry_handler handler,
                       error_handler err_handler = nullptr)
      : timer_(*io_context_ptr)
      , expiry_handler_(std::move(handler))
      , error_handler_(std::move(err_handler))
    {
        if (!expiry_handler_)
            throw std::invalid_argument("expiry_handler cannot be null");
    }

    ~lockfree_expirator() override { stop(); }

    void start() override {
        bool expected = false;
        if (running_.compare_exchange_strong(expected, true, std::memory_order_release)) {
            schedule_next();
        }
    }

    void stop() override {
        bool expected = true;
        if (running_.compare_exchange_strong(expected, false, std::memory_order_release)) {
            boost::system::error_code ec;
            timer_.cancel(ec);
        }
    }

    bool add(Tkey key, duration expiration_duration, Tinfo info = Tinfo{}) override {
        time_point expiry = clock_type::now() + expiration_duration;

        size_t current_write = write_index_.load(std::memory_order_relaxed);
        size_t next_write = (current_write + 1) % QUEUE_SIZE;

        if (next_write == read_index_.load(std::memory_order_acquire))
            return false;

        op_queue_[current_write] = operation(operation::op_type::ADD, std::move(key), std::move(info), expiry);
        write_index_.store(next_write, std::memory_order_release);

        if (!running_.load(std::memory_order_acquire))
            start();

        return true;
    }

    bool remove(const Tkey& key) override {
        size_t current_write = write_index_.load(std::memory_order_relaxed);
        size_t next_write = (current_write + 1) % QUEUE_SIZE;

        if (next_write == read_index_.load(std::memory_order_acquire))
            return false;

        op_queue_[current_write] = operation(operation::op_type::REMOVE, key, Tinfo{}, time_point{});
        write_index_.store(next_write, std::memory_order_release);
        return true;
    }

    std::optional<Tinfo> get_info(const Tkey& key) const override {
        auto it = entries_.find(key);
        return (it != entries_.end()) ? std::optional<Tinfo>(it->second.info) : std::nullopt;
    }

    std::optional<duration> get_remaining_time(const Tkey& key) const override {
        auto it = entries_.find(key);
        if (it != entries_.end()) {
            auto remaining = it->second.expiry - clock_type::now();
            return remaining > duration::zero() ? remaining : duration::zero();
        }
        return {};
    }

    bool contains(const Tkey& key) const override {
        return entries_.find(key) != entries_.end();
    }

    void clear() override {
        entries_.clear();
        expiry_queue_.clear();
        if (running_.load(std::memory_order_acquire)) stop();
    }

    size_t size() const override { return entries_.size(); }
    bool empty() const override { return entries_.empty(); }
    bool is_running() const override { return running_.load(std::memory_order_acquire); }

  private:
    void process_operations() {
        size_t current_read = read_index_.load(std::memory_order_relaxed);
        size_t current_write = write_index_.load(std::memory_order_acquire);

        while (current_read != current_write) {
            const auto& op = op_queue_[current_read];

            if (op.type == operation::op_type::ADD) {
                if (entries_.find(op.key) == entries_.end()) {
                    entries_[op.key] = {op.expiry, op.info};
                    expiry_queue_.emplace(op.expiry, op.key);
                }
            }
            else if (op.type == operation::op_type::REMOVE) {
                auto it = entries_.find(op.key);
                if (it != entries_.end()) {
                    auto range = expiry_queue_.equal_range(it->second.expiry);
                    for (auto eq_it = range.first; eq_it != range.second; ++eq_it) {
                        if (eq_it->second == op.key) {
                            expiry_queue_.erase(eq_it);
                            break;
                        }
                    }
                    entries_.erase(it);
                }
            }

            current_read = (current_read + 1) % QUEUE_SIZE;
        }

        read_index_.store(current_read, std::memory_order_release);
    }

    void schedule_next() {
        process_operations();

        if (expiry_queue_.empty()) {
            running_.store(false, std::memory_order_release);
            return;
        }

        time_point next_expiry = expiry_queue_.begin()->first;
        timer_.expires_at(next_expiry);

        timer_.async_wait([self = this->shared_from_this()](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted || !self->running_.load(std::memory_order_acquire))
                return;
            if (ec) {
                if (self->error_handler_) self->error_handler_(ec);
                self->running_.store(false, std::memory_order_release);
                return;
            }
            self->process_expired();
            self->schedule_next();
        });
    }

    void process_expired() {
        time_point now = clock_type::now();
        while (!expiry_queue_.empty() && expiry_queue_.begin()->first <= now) {
            auto it = expiry_queue_.begin();
            Tkey key = it->second;
            expiry_queue_.erase(it);

            auto entry_it = entries_.find(key);
            if (entry_it != entries_.end()) {
                Tinfo info = std::move(entry_it->second.info);
                entries_.erase(entry_it);
                expiry_handler_(key, std::move(info));
            }
        }
    }
};

}  // namespace expirator