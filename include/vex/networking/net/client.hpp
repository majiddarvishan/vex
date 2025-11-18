#pragma once

#include <vex/networking/net/session_builder.hpp>
#include <vex/networking/net/protocol_handler.hpp>
#include <vex/networking/net/tcp_utils.hpp>

#include <boost/asio.hpp>

#include <memory>
#include <functional>
#include <string>
#include <optional>

namespace pa::pinex
{

// ============================================================================
// Client Protocol Handler
// ============================================================================

class client_protocol_handler : public protocol_handler
{
public:
    using bind_response_callback = std::function<void(const bind_response&)>;

private:
    bind_response_callback bind_callback_;
    std::function<void(request&&, uint32_t)> request_callback_;
    std::function<void(response&&, uint32_t, command_status)> response_callback_;

public:
    void set_bind_response_callback(bind_response_callback callback)
    {
        bind_callback_ = std::move(callback);
    }

    void set_request_callback(std::function<void(request&&, uint32_t)> callback)
    {
        request_callback_ = std::move(callback);
    }

    void set_response_callback(std::function<void(response&&, uint32_t, command_status)> callback)
    {
        response_callback_ = std::move(callback);
    }

    void on_request(request&& req, uint32_t sequence_number) override
    {
        if (request_callback_)
            request_callback_(std::move(req), sequence_number);
    }

    void on_response(response&& resp, uint32_t sequence_number, command_status status) override
    {
        // Handle bind response specially during binding phase
        if (bind_callback_)
        {
            if (auto* bind_resp = std::get_if<bind_response>(&resp))
            {
                if (status == command_status::rok)
                {
                    bind_callback_(*bind_resp);
                    bind_callback_ = {}; // Clear bind callback after successful bind
                    return;
                }
            }
        }

        // Forward to general response callback
        if (response_callback_)
            response_callback_(std::move(resp), sequence_number, status);
    }
};

// ============================================================================
// Client
// ============================================================================

class client : public std::enable_shared_from_this<client>
{
public:
    using bind_handler_t = std::function<void(const bind_response&, session_ptr)>;
    using error_handler_t = std::function<void(const std::string&)>;

private:
    boost::asio::ip::tcp::endpoint endpoint_;
    boost::asio::io_context* io_context_;
    uint16_t inactivity_timeout_;
    bind_request bind_request_;
    boost::asio::steady_timer reconnect_timer_;
    boost::asio::ip::tcp::socket socket_;
    session_ptr binding_session_;
    bind_handler_t bind_handler_;
    error_handler_t error_handler_;
    session_config config_;
    bool auto_reconnect_{true};

public:
    client(boost::asio::io_context* io_context,
           std::string_view ip_address,
           uint16_t port,
           uint16_t inactivity_timeout,
           bind_request bind_request,
           bind_handler_t bind_handler,
           error_handler_t error_handler)
      : endpoint_(boost::asio::ip::make_address(ip_address), port)
      , io_context_{io_context}
      , inactivity_timeout_{inactivity_timeout}
      , bind_request_(std::move(bind_request))
      , reconnect_timer_(*io_context)
      , socket_(*io_context)
      , bind_handler_(std::move(bind_handler))
      , error_handler_(std::move(error_handler))
    {
    }

    client(const client&) = delete;
    client& operator=(const client&) = delete;
    client(client&&) = delete;
    client& operator=(client&&) = delete;
    ~client() = default;

    void start()
    {
        do_connect();
    }

    void stop()
    {
        auto_reconnect_ = false;
        reconnect_timer_.cancel();
        if (binding_session_)
            binding_session_->close("Client stopped");
        binding_session_.reset();

        boost::system::error_code ec;
        socket_.close(ec);
    }

    void set_session_config(const session_config& config)
    {
        config_ = config;
    }

    const boost::asio::ip::tcp::endpoint& endpoint() const
    {
        return endpoint_;
    }

private:
    void do_set_retry_timer()
    {
        if (!auto_reconnect_)
            return;

        reconnect_timer_.expires_after(std::chrono::seconds{5});
        reconnect_timer_.async_wait([this, wptr = weak_from_this()](boost::system::error_code ec) {
            if (wptr.expired())
                return;

            if (ec)
            {
                if (ec != boost::asio::error::operation_aborted)
                    on_error("Retry timer failed, error:" + std::string{ec.message()});
                return;
            }

            do_connect();
        });
    }

    void do_connect()
    {
        socket_.async_connect(endpoint_, [this, wptr = weak_from_this()](boost::system::error_code ec) {
            if (wptr.expired())
                return;

            if (ec)
            {
                socket_.close(ec);
                do_set_retry_timer();
                return;
            }

            on_connect();
        });
    }

    void on_connect()
    {
        enable_keepalive(socket_, inactivity_timeout_);

        // Create protocol handler for binding phase
        auto protocol_handler = std::make_unique<client_protocol_handler>();
        auto* protocol_handler_ptr = protocol_handler.get();

        // Build session with configuration
        binding_session_ = session_builder()
            .with_send_capacity(config_.send_buf_capacity)
            .with_send_threshold(config_.send_buf_threshold)
            .with_receive_buffer(config_.receive_buf_size)
            .with_max_message_size(config_.max_command_length)
            .with_unbind_timeout(config_.unbind_timeout)
            .with_backpressure(config_.backpressure_low_watermark, config_.backpressure_high_watermark)
            .with_protocol_handler(std::move(protocol_handler))
            .with_close_handler([this, wptr = weak_from_this()](auto session, auto error) {
                if (wptr.expired())
                    return;
                on_binding_session_close(session, error);
            })
            .build(*io_context_, std::move(socket_));

        // Set bind response callback
        protocol_handler_ptr->set_bind_response_callback(
            [this, wptr = weak_from_this()](const bind_response& resp) {
                if (wptr.expired())
                    return;
                on_bind_response(resp);
            });

        binding_session_->start();
        binding_session_->send_request(bind_request_);
    }

    void on_bind_response(const bind_response& bind_resp)
    {
        // Pause receiving before handing over session
        binding_session_->pause_receiving();

        // Defer handler for execution, so it would be safe to destroy this object inside the handler
        boost::asio::defer(binding_session_->remote_endpoint() ?
                          io_context_->get_executor() : io_context_->get_executor(),
                          [wptr = weak_from_this(),
                           bind_resp,
                           session = binding_session_,
                           handler = bind_handler_] {
            if (wptr.expired())
                return;

            // Hand over the session to the user
            handler(bind_resp, session);

            // Resume receiving (user is now responsible for the session)
            session->resume_receiving();
        });

        // Release our reference (user now owns the session)
        binding_session_.reset();
    }

    void on_binding_session_close(session_ptr session, const std::optional<std::string>& error)
    {
        on_error("Session has been closed during binding, error:" + error.value_or("none"));
        binding_session_.reset();

        // Auto-reconnect if enabled
        if (auto_reconnect_)
            do_set_retry_timer();
    }

    void on_error(const std::string& error)
    {
        // Defer handler for execution, so it would be safe to destroy this object inside the handler
        boost::asio::post(io_context_->get_executor(),
                         [wptr = weak_from_this(), error, handler = error_handler_] {
            if (wptr.expired())
                return;

            handler(error);
        });
    }
};

} // namespace pa::pinex