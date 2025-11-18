#pragma once

#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <functional>

class event : public std::enable_shared_from_this<event>
{
  public:
    using event_handler = std::function<bool()>;

  private:
    boost::asio::steady_timer timer_;
    const std::chrono::nanoseconds period_{};
    const event_handler event_handler_{};

  public:
    event(boost::asio::io_context* io_context, const std::chrono::nanoseconds& period, const event_handler& event_handler)
        : timer_(*io_context)
        , period_(period)
        , event_handler_(event_handler)
    {
    }

    event(const event&) = delete;
    event& operator=(const event&) = delete;
    event(event&&) = delete;
    event& operator=(event&&) = delete;
    ~event() = default;

    void start()
    {
        do_set_timer();
    }

  private:
    void do_set_timer()
    {
        timer_.expires_after(period_);
        timer_.async_wait([this, wptr = weak_from_this()](std::error_code ec) {
            if (!wptr.expired())
            {
                if (ec)
                    throw std::runtime_error("io::event::async_wait() " + ec.message());

                if (event_handler_())
                    do_set_timer();
            }
        });
    }
};