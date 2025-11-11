#pragma once

#include <expirator/base_expirator.hpp>
#include <boost/asio.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <chrono>
#include <array>
#include <list>
#include <unordered_map>

namespace expirator {

template<typename Tkey, typename Tinfo, typename Allocator = std::allocator<char>>
class timing_wheel_expirator : public base_expirator<Tkey, Tinfo>,
                                public std::enable_shared_from_this<timing_wheel_expirator<Tkey, Tinfo, Allocator>>
{
  public:
    using typename base_expirator<Tkey, Tinfo>::expiry_handler;
    using typename base_expirator<Tkey, Tinfo>::error_handler;
    using typename base_expirator<Tkey, Tinfo>::clock_type;
    using typename base_expirator<Tkey, Tinfo>::time_point;
    using typename base_expirator<Tkey, Tinfo>::duration;

  private:
    static constexpr size_t WHEEL_0_SIZE = 256;
    static constexpr size_t WHEEL_1_SIZE = 64;
    static constexpr size_t WHEEL_2_SIZE = 64;
    static constexpr size_t WHEEL_3_SIZE = 64;

    static constexpr auto TICK_DURATION = std::chrono::milliseconds(1);

    struct entry_node {
        Tkey key;
        Tinfo info;
        time_point expiry;
        uint8_t wheel_level;
        size_t slot_index;
    };

    using list_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<entry_node>;
    using entry_list = std::list<entry_node, list_allocator>;
    using entry_iterator = typename entry_list::iterator;

    struct slot_data {
        entry_list entries;
    };

    boost::asio::steady_timer timer_;
    time_point start_time_;
    uint64_t current_tick_{0};

    std::array<slot_data, WHEEL_0_SIZE> wheel_0_;
    std::array<slot_data, WHEEL_1_SIZE> wheel_1_;
    std::array<slot_data, WHEEL_2_SIZE> wheel_2_;
    std::array<slot_data, WHEEL_3_SIZE> wheel_3_;

    std::unordered_map<Tkey, entry_iterator> key_to_entry_;

    const expiry_handler expiry_handler_;
    const error_handler error_handler_;
    bool running_{false};

  public:
    timing_wheel_expirator(boost::asio::io_context* io_context_ptr,
                           expiry_handler handler,
                           error_handler err_handler = nullptr)
      : timer_(*io_context_ptr)
      , start_time_(clock_type::now())
      , expiry_handler_(std::move(handler))
      , error_handler_(std::move(err_handler))
    {
        if (!expiry_handler_)
            throw std::invalid_argument("expiry_handler cannot be null");
    }

    ~timing_wheel_expirator() override { stop(); }

    void reserve(size_t capacity) { key_to_entry_.reserve(capacity); }

    void start() override {
        if (!running_) {
            running_ = true;
            start_time_ = clock_type::now();
            current_tick_ = 0;
            schedule_tick();
        }
    }

    void stop() override {
        if (running_) {
            running_ = false;
            boost::system::error_code ec;
            timer_.cancel(ec);
        }
    }

    bool add(Tkey key, duration expiration_duration, Tinfo info = Tinfo{}) override {
        if (key_to_entry_.find(key) != key_to_entry_.end())
            return false;

        time_point expiry = clock_type::now() + expiration_duration;
        insert_entry(std::move(key), std::move(info), expiry);

        if (!running_) start();
        return true;
    }

    std::optional<Tinfo> get_info(const Tkey& key) const override {
        auto it = key_to_entry_.find(key);
        return (it != key_to_entry_.end()) ? std::optional<Tinfo>(it->second->info) : std::nullopt;
    }

    std::optional<duration> get_remaining_time(const Tkey& key) const override {
        auto it = key_to_entry_.find(key);
        if (it != key_to_entry_.end()) {
            auto remaining = it->second->expiry - clock_type::now();
            return remaining > duration::zero() ? remaining : duration::zero();
        }
        return {};
    }

    bool contains(const Tkey& key) const override {
        return key_to_entry_.find(key) != key_to_entry_.end();
    }

    bool remove(const Tkey& key) override {
        auto it = key_to_entry_.find(key);
        if (it == key_to_entry_.end()) return false;

        auto entry_iter = it->second;
        get_wheel(entry_iter->wheel_level, entry_iter->slot_index).entries.erase(entry_iter);
        key_to_entry_.erase(it);
        return true;
    }

    void clear() override {
        for (auto& slot : wheel_0_) slot.entries.clear();
        for (auto& slot : wheel_1_) slot.entries.clear();
        for (auto& slot : wheel_2_) slot.entries.clear();
        for (auto& slot : wheel_3_) slot.entries.clear();
        key_to_entry_.clear();

        if (running_) {
            boost::system::error_code ec;
            timer_.cancel(ec);
            running_ = false;
        }
    }

    size_t size() const override { return key_to_entry_.size(); }
    bool empty() const override { return key_to_entry_.empty(); }
    bool is_running() const override { return running_; }

  private:
    slot_data& get_wheel(uint8_t level, size_t index) {
        switch (level) {
            case 0: return wheel_0_[index];
            case 1: return wheel_1_[index];
            case 2: return wheel_2_[index];
            default: return wheel_3_[index];
        }
    }

    void insert_entry(Tkey key, Tinfo info, time_point expiry) {
        auto ticks_from_now = std::chrono::duration_cast<std::chrono::milliseconds>(
            expiry - clock_type::now()).count();

        if (ticks_from_now < 0) ticks_from_now = 0;

        uint64_t target_tick = current_tick_ + ticks_from_now;
        uint8_t wheel_level;
        size_t slot_index;

        if (ticks_from_now < WHEEL_0_SIZE) {
            wheel_level = 0;
            slot_index = target_tick % WHEEL_0_SIZE;
        }
        else if (ticks_from_now < WHEEL_0_SIZE * WHEEL_1_SIZE) {
            wheel_level = 1;
            slot_index = (target_tick / WHEEL_0_SIZE) % WHEEL_1_SIZE;
        }
        else if (ticks_from_now < WHEEL_0_SIZE * WHEEL_1_SIZE * WHEEL_2_SIZE) {
            wheel_level = 2;
            slot_index = (target_tick / (WHEEL_0_SIZE * WHEEL_1_SIZE)) % WHEEL_2_SIZE;
        }
        else {
            wheel_level = 3;
            slot_index = (target_tick / (WHEEL_0_SIZE * WHEEL_1_SIZE * WHEEL_2_SIZE)) % WHEEL_3_SIZE;
        }

        auto& slot = get_wheel(wheel_level, slot_index);
        slot.entries.push_back({key, std::move(info), expiry, wheel_level, slot_index});
        key_to_entry_[key] = std::prev(slot.entries.end());
    }

    void schedule_tick() {
        timer_.expires_after(TICK_DURATION);
        timer_.async_wait([self = this->shared_from_this()](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted || !self->running_) return;
            if (ec) {
                if (self->error_handler_) self->error_handler_(ec);
                self->running_ = false;
                return;
            }
            self->process_tick();
            if (self->running_) self->schedule_tick();
        });
    }

    void process_tick() {
        current_tick_++;

        size_t slot_0 = current_tick_ % WHEEL_0_SIZE;
        process_slot(wheel_0_[slot_0]);

        if (current_tick_ % WHEEL_0_SIZE == 0) {
            size_t slot_1 = (current_tick_ / WHEEL_0_SIZE) % WHEEL_1_SIZE;
            cascade_wheel(wheel_1_[slot_1]);

            if (current_tick_ % (WHEEL_0_SIZE * WHEEL_1_SIZE) == 0) {
                size_t slot_2 = (current_tick_ / (WHEEL_0_SIZE * WHEEL_1_SIZE)) % WHEEL_2_SIZE;
                cascade_wheel(wheel_2_[slot_2]);

                if (current_tick_ % (WHEEL_0_SIZE * WHEEL_1_SIZE * WHEEL_2_SIZE) == 0) {
                    size_t slot_3 = (current_tick_ / (WHEEL_0_SIZE * WHEEL_1_SIZE * WHEEL_2_SIZE)) % WHEEL_3_SIZE;
                    cascade_wheel(wheel_3_[slot_3]);
                }
            }
        }
    }

    void process_slot(slot_data& slot) {
        time_point now = clock_type::now();
        auto it = slot.entries.begin();
        while (it != slot.entries.end()) {
            if (it->expiry <= now) {
                Tkey key = std::move(it->key);
                Tinfo info = std::move(it->info);
                key_to_entry_.erase(key);
                it = slot.entries.erase(it);
                expiry_handler_(key, std::move(info));
            } else {
                ++it;
            }
        }
    }

    void cascade_wheel(slot_data& slot) {
        auto entries_copy = std::move(slot.entries);
        slot.entries.clear();
        for (auto& entry : entries_copy) {
            key_to_entry_.erase(entry.key);
            insert_entry(std::move(entry.key), std::move(entry.info), entry.expiry);
        }
    }
};

}  // namespace expirator