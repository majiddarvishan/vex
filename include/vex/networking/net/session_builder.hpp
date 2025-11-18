#pragma once

#include <vex/networking/net/session_impl.hpp>
#include <vex/networking/net/session_config.hpp>
#include <vex/networking/net/error_handler.hpp>
#include <vex/networking/net/protocol_handler.hpp>

#include <boost/asio.hpp>

#include <memory>
#include <functional>
#include <chrono>

namespace pa::pinex
{

// ============================================================================
// Session Builder
// ============================================================================

class session_builder
{
    session_config config_;
    std::unique_ptr<protocol_handler> protocol_handler_;
    std::unique_ptr<error_handler> error_handler_;
    typename session_impl::close_callback close_handler_;
    std::function<void()> send_buf_available_handler_;

public:
    session_builder& with_send_capacity(size_t size)
    {
        config_.send_buf_capacity = size;
        return *this;
    }

    session_builder& with_send_threshold(size_t size)
    {
        config_.send_buf_threshold = size;
        return *this;
    }

    session_builder& with_receive_buffer(size_t size)
    {
        config_.receive_buf_size = size;
        return *this;
    }

    session_builder& with_unbind_timeout(std::chrono::seconds timeout)
    {
        config_.unbind_timeout = timeout;
        return *this;
    }

    session_builder& with_max_message_size(uint32_t size)
    {
        config_.max_command_length = size;
        return *this;
    }

    session_builder& with_small_body_size(size_t size)
    {
        config_.small_body_size = size;
        return *this;
    }

    session_builder& with_backpressure(size_t low, size_t high)
    {
        config_.backpressure_low_watermark = low;
        config_.backpressure_high_watermark = high;
        return *this;
    }

    session_builder& with_protocol_handler(std::unique_ptr<protocol_handler> handler)
    {
        protocol_handler_ = std::move(handler);
        return *this;
    }

    session_builder& with_error_handler(std::unique_ptr<error_handler> handler)
    {
        error_handler_ = std::move(handler);
        return *this;
    }

    session_builder& with_close_handler(typename session_impl::close_callback handler)
    {
        close_handler_ = std::move(handler);
        return *this;
    }

    session_builder& with_send_buf_available_handler(std::function<void()> handler)
    {
        send_buf_available_handler_ = std::move(handler);
        return *this;
    }

    session_ptr build(boost::asio::io_context& io, boost::asio::ip::tcp::socket socket)
    {
        if (!config_.is_valid())
            throw std::invalid_argument{"Invalid session configuration"};

        auto session = std::make_shared<session_impl>(io, std::move(socket), config_);

        if (protocol_handler_)
            session->set_protocol_handler(std::move(protocol_handler_));

        if (error_handler_)
            session->set_error_handler(std::move(error_handler_));
        else
            session->set_error_handler(std::make_unique<logging_error_handler>());

        if (close_handler_)
            session->set_close_handler(std::move(close_handler_));

        if (send_buf_available_handler_)
            session->set_send_buf_available_handler(std::move(send_buf_available_handler_));

        return session;
    }
};

} // namespace pa::pinex