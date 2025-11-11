#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <functional>
#include <optional>

namespace expirator {

/**
 * @brief Base interface for all expirator implementations
 */
template<typename Tkey, typename Tinfo>
class base_expirator
{
  public:
    using expiry_handler = std::function<void(Tkey, Tinfo)>;
    using error_handler = std::function<void(const boost::system::error_code&)>;
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;
    using duration = clock_type::duration;

    virtual ~base_expirator() = default;

    // Core operations
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool add(Tkey key, duration expiration_duration, Tinfo info = Tinfo{}) = 0;
    virtual bool remove(const Tkey& key) = 0;
    virtual void clear() = 0;

    // Query operations
    virtual std::optional<Tinfo> get_info(const Tkey& key) const = 0;
    virtual std::optional<duration> get_remaining_time(const Tkey& key) const = 0;
    virtual bool contains(const Tkey& key) const = 0;
    virtual size_t size() const = 0;
    virtual bool empty() const = 0;
    virtual bool is_running() const = 0;
};

}  // namespace expirator
