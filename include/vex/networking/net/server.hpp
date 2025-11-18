#pragma once

#include <vex/networking/net/session_manager.hpp>
#include <vex/networking/net/session_builder.hpp>
#include <vex/networking/net/protocol_handler.hpp>
#include <vex/networking/net/tcp_utils.hpp>

#include <boost/asio.hpp>

#include <set>
#include <unordered_map>
#include <memory>
#include <functional>
#include <string>

namespace pa::pinex
{

// ============================================================================
// Server Protocol Handler
// ============================================================================

class server_protocol_handler : public protocol_handler
{
public:
    using bind_request_callback = std::function<void(const bind_request&, uint32_t)>;

private:
    bind_request_callback bind_callback_;
    std::function<void(request&&, uint32_t)> request_callback_;
    std::function<void(response&&, uint32_t, command_status)> response_callback_;

public:
    void set_bind_request_callback(bind_request_callback callback)
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
        // Handle bind request specially during binding phase
        if (bind_callback_)
        {
            if (auto* bind_req = std::get_if<bind_request>(&req))
            {
                // CRITICAL: Move the callback to a local variable first
                // This keeps it (and any captured shared_ptrs) alive during the call
                auto callback = std::move(bind_callback_);
                bind_callback_ = {}; // Clear the member now
                callback(*bind_req, sequence_number); // Call with callback still alive
                return;
            }
        }

        // Forward to general request callback
        if (request_callback_)
            request_callback_(std::move(req), sequence_number);
    }

    void on_response(response&& resp, uint32_t sequence_number, command_status status) override
    {
        if (response_callback_)
        {
            // Defensive: copy callback to local variable to ensure it stays alive
            // during execution, even if the callback clears itself
            auto callback = response_callback_;
            callback(std::move(resp), sequence_number, status);
        }
    }
};

// ============================================================================
// Server
// ============================================================================

class server : public std::enable_shared_from_this<server>
{
public:
    using bind_handler_t = std::function<bool(const bind_request&, session_ptr)>;

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::io_context* io_context_;
    std::unordered_map<session_ptr, session_manager::session_id> binding_sessions_;
    session_manager session_mgr_;
    std::string system_id_;
    uint16_t inactivity_timeout_;
    bind_handler_t bind_handler_;
    session_config config_;

public:
    server(boost::asio::io_context* io_context,
           std::string_view ip_address,
           uint16_t port,
           std::string_view system_id,
           uint16_t inactivity_timeout,
           bind_handler_t bind_handler)
      : acceptor_(*io_context)
      , io_context_{io_context}
      , session_mgr_(*io_context)
      , system_id_(system_id)
      , inactivity_timeout_(inactivity_timeout)
      , bind_handler_(std::move(bind_handler))
    {
        try
        {
            auto endpoint = boost::asio::ip::tcp::endpoint{
                boost::asio::ip::make_address(ip_address), port};
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error{
                "Failed to listen on " + std::string{ip_address} +
                ":" + std::to_string(port) + " error:" + std::string{ex.what()}};
        }
    }

    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server(server&&) = delete;
    server& operator=(server&&) = delete;
    ~server() = default;

    void start()
    {
        do_accept();
    }

    void stop()
    {
        boost::system::error_code ec;
        acceptor_.close(ec);
        session_mgr_.close_all();
    }

    void set_session_config(const session_config& config)
    {
        config_ = config;
    }

    size_t binding_session_count() const
    {
        return binding_sessions_.size();
    }

    size_t active_session_count() const
    {
        return session_mgr_.active_count();
    }

    session_manager::aggregate_metrics get_metrics() const
    {
        return session_mgr_.get_metrics();
    }

    const session_manager& get_session_manager() const
    {
        return session_mgr_;
    }

private:
    void do_accept()
    {
        acceptor_.async_accept([this, wptr = weak_from_this()]
                              (boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (wptr.expired())
                return;

            if (ec)
            {
                if (ec != boost::asio::error::operation_aborted)
                    throw std::runtime_error{
                        "async_accept failed, error:" + std::string{ec.message()}};
                return;
            }

            // Apply keep-alive and other TCP settings
            enable_keepalive(socket, inactivity_timeout_);
            enable_no_delay(socket);

            on_accept(std::move(socket));

            do_accept();
        });
    }

    void on_accept(boost::asio::ip::tcp::socket socket)
    {
        // Create protocol handler for binding phase
        auto protocol_handler = std::make_unique<server_protocol_handler>();
        auto* protocol_handler_ptr = protocol_handler.get();

        // Build session with configuration
        auto session = session_builder()
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
            .build(*io_context_, std::move(socket));

        // Set bind request callback
        protocol_handler_ptr->set_bind_request_callback(
            [this, wptr = weak_from_this(), session](const bind_request& req, uint32_t seq) {
                if (wptr.expired())
                    return;
                on_bind_request(session, req, seq);
            });

        // Track binding session temporarily
        auto temp_id = reinterpret_cast<session_manager::session_id>(session.get());
        binding_sessions_[session] = temp_id;

        session->start();
    }

    void on_bind_request(session_ptr session, const bind_request& bind_req, uint32_t sequence_number)
    {
        const auto bind_resp = bind_response{bind_req.bind_type, system_id_};

        // Get remote endpoint info
        std::string remote_info = "unknown";
        if (auto endpoint = session->remote_endpoint())
        {
            auto [ip_address, port] = *endpoint;
            remote_info = ip_address + ":" + std::to_string(port);
        }

        // Call user's bind handler
        bool accept_bind = false;
        try
        {
            accept_bind = bind_handler_(bind_req, session);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception in bind_handler from " << remote_info
                      << ": " << e.what() << std::endl;
            accept_bind = false;
        }

        // Send response
        if (accept_bind)
        {
            session->send_response(bind_resp, sequence_number, command_status::rok);

            // Move session from binding to active sessions
            binding_sessions_.erase(session);

            // Add to session manager (it will now track it)
            // Note: We don't use add_session here as session is already created
            // User's bind_handler should manage the session from now on
        }
        else
        {
            session->send_response(bind_resp, sequence_number, command_status::rfail);

            // Close session after sending rejection
            session->close("Bind rejected");
            binding_sessions_.erase(session);
        }
    }

    void on_binding_session_close(session_ptr session, const std::optional<std::string>& error)
    {
        // Remove from binding sessions
        binding_sessions_.erase(session);

        if (error)
        {
            std::string remote_info = "unknown";
            if (auto endpoint = session->remote_endpoint())
            {
                auto [ip, port] = *endpoint;
                remote_info = ip + ":" + std::to_string(port);
            }

            std::cerr << "Binding session from " << remote_info
                      << " closed during binding: " << *error << std::endl;
        }
    }
};

} // namespace pa::pinex