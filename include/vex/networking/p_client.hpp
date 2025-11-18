#pragma once

#include <vex/networking/net/client.hpp>
#include <vex/networking/net/protocol_handler.hpp>
#include <vex/networking/common/helpers.hpp>
#include <vex/networking/io/expirator.hpp>
#include <vex/networking/net/definitions.hpp>

#include <fmt/core.h>

#include <any>
#include <map>
#include <memory>
#include <functional>
#include <string>

namespace pa::pinex
{

// ============================================================================
// P_Client Protocol Handler
// ============================================================================

class p_client_protocol_handler : public protocol_handler
{
public:
    std::function<void(request&&, uint32_t)> request_callback;
    std::function<void(response&&, uint32_t, command_status)> response_callback;

    void on_request(request&& req, uint32_t sequence_number) override
    {
        if (request_callback)
            request_callback(std::move(req), sequence_number);
    }

    void on_response(response&& resp, uint32_t sequence_number, command_status status) override
    {
        if (response_callback)
            response_callback(std::move(resp), sequence_number, status);
    }
};

// ============================================================================
// P_Client - Protocol Client Wrapper
// ============================================================================

class p_client : public std::enable_shared_from_this<p_client>
{
public:
    using packet_handler_t = std::function<void(const std::string&, uint32_t, const std::string&)>;
    using bind_handler_t = std::function<void(const std::string&, std::shared_ptr<p_client>)>;
    using close_handler_t = std::function<void(const std::string&)>;
    using session_handler_t = std::function<void(const std::string&, session_stat)>;

private:
    boost::asio::io_context* io_context_;
    std::shared_ptr<client> tcp_client_;
    session_ptr binded_session_;
    std::string client_id_;
    std::string server_id_;
    std::chrono::seconds timeout_sec_;
    bool auto_reconnect_;

    packet_handler_t request_handler_;
    packet_handler_t response_handler_;
    packet_handler_t timeout_handler_;
    bind_handler_t bind_handler_;
    close_handler_t close_handler_;
    session_handler_t session_handler_;

    std::shared_ptr<io::expirator<uint32_t, std::any>> packet_expirator_;
    p_client_protocol_handler* protocol_handler_ptr_{nullptr};

public:
    explicit p_client(boost::asio::io_context* io_context,
                      const std::string& id,
                      const std::string& uri_address,
                      uint64_t timeout_sec,
                      uint16_t inactivity_timeout,
                      bool auto_reconnect,
                      packet_handler_t request_handler,
                      packet_handler_t response_handler,
                      packet_handler_t timeout_handler,
                      bind_handler_t bind_handler = nullptr,
                      close_handler_t close_handler = nullptr,
                      session_handler_t session_handler = nullptr)
      : io_context_(io_context)
      , client_id_(id)
      , timeout_sec_{std::chrono::seconds(timeout_sec)}
      , auto_reconnect_{auto_reconnect}
      , request_handler_(std::move(request_handler))
      , response_handler_(std::move(response_handler))
      , timeout_handler_(std::move(timeout_handler))
      , bind_handler_(std::move(bind_handler))
      , close_handler_(std::move(close_handler))
      , session_handler_(std::move(session_handler))
    {
        auto uri = split(uri_address, ':');

        tcp_client_ = std::make_shared<client>(
            io_context,
            uri[0],
            static_cast<uint16_t>(std::stoi(uri[1])),
            inactivity_timeout,
            bind_request{.bind_type = bind_type::bi_direction, .system_id = id},
            [this](const auto& resp, auto session) { on_bind(resp, session); },
            [this](const auto& error) { on_error(error); });

        packet_expirator_ = std::make_shared<io::expirator<uint32_t, std::any>>(
            io_context,
            [this](auto seq, auto data) { on_session_timeout(seq, data); });

        packet_expirator_->start();
    }

    p_client(const p_client&) = delete;
    p_client& operator=(const p_client&) = delete;
    p_client(p_client&&) = delete;
    p_client& operator=(p_client&&) = delete;
    ~p_client() = default;

    void start()
    {
        tcp_client_->start();
    }

    void stop()
    {
        tcp_client_->stop();

        if (binded_session_)
        {
            fmt::print("unbinding binded session...\n");
            binded_session_->unbind();
            binded_session_.reset();
        }
    }

    uint32_t send_response(const std::string& msg, uint32_t seq_no)
    {
        if (!binded_session_)
            return 0;

        binded_session_->send_response(
            stream_response{.message_body = msg},
            seq_no,
            command_status::rok);

        return seq_no;
    }

    uint32_t send_request(const std::string& msg)
    {
        if (!binded_session_)
            return 0;

        stream_request pdu{.message_body = msg};
        auto seq_no = binded_session_->send_request(pdu);

        packet_expirator_->add(seq_no, timeout_sec_, pdu);

        return seq_no;
    }

    uint32_t send_info(const std::string& msg)
    {
        if (!binded_session_)
            return 0;

        stream_request pdu{.message_body = msg};
        return binded_session_->send_request(pdu);
    }

    bool is_connected() const
    {
        return binded_session_ && binded_session_->is_open();
    }

    const std::string& client_id() const { return client_id_; }
    const std::string& server_id() const { return server_id_; }

private:
    void on_bind(const bind_response& bind_resp, session_ptr session)
    {
        fmt::print("client: {} has been successfully binded, server system_id:{}\n",
                   client_id_, bind_resp.system_id);

        server_id_ = bind_resp.system_id;

        // Create and install protocol handler
        auto handler = std::make_unique<p_client_protocol_handler>();
        protocol_handler_ptr_ = handler.get();

        protocol_handler_ptr_->request_callback =
            [this](auto&& req, auto seq) { on_session_request(std::move(req), seq); };

        protocol_handler_ptr_->response_callback =
            [this](auto&& resp, auto seq, auto status) {
                on_session_response(std::move(resp), seq, status);
            };

        session->set_protocol_handler(std::move(handler));

        // Set other handlers
        session->set_close_handler(
            [this](auto session, auto error) {
                on_session_close(session, error);
            });

        binded_session_ = session;

        if (session_handler_)
            session_handler_(server_id_, session_stat::bind);

        if (bind_handler_)
            bind_handler_(bind_resp.system_id, shared_from_this());
    }

    void on_error(const std::string& error)
    {
        fmt::print("bind failed, error:{}\n", error);
    }

    void on_session_close(session_ptr, std::optional<std::string> error)
    {
        if (error)
            fmt::print("session has been closed on error:{}\n", error.value());
        else
            fmt::print("session has been closed gracefully\n");

        binded_session_.reset();
        packet_expirator_->expire_all();

        if (close_handler_)
            close_handler_(server_id_);

        if (session_handler_)
            session_handler_(server_id_, session_stat::close);

        if (auto_reconnect_)
            start();
    }

    void on_session_request(request&& req_packet, uint32_t sequence_number)
    {
        std::visit(
            [this, sequence_number](auto&& req) {
                using request_type = std::decay_t<decltype(req)>;
                if constexpr (std::is_same_v<request_type, stream_request>)
                {
                    stream_request sm = std::move(req);
                    request_handler_(server_id_, sequence_number, sm.message_body);
                }
                else if constexpr (!std::is_same_v<request_type, std::monostate>)
                {
                    fmt::print("invalid packet type\n");
                }
            },
            req_packet);
    }

    void on_session_response(response&& resp_packet,
                            uint32_t sequence_number,
                            command_status /*command_status*/)
    {
        std::visit(
            [this, sequence_number](auto&& resp) {
                using response_type = std::decay_t<decltype(resp)>;
                if constexpr (std::is_same_v<response_type, stream_response>)
                {
                    stream_response sm = std::move(resp);
                    packet_expirator_->remove(sequence_number);
                    response_handler_(server_id_, sequence_number, sm.message_body);
                }
                else if constexpr (!std::is_same_v<response_type, std::monostate>)
                {
                    fmt::print("invalid packet type\n");
                }
            },
            resp_packet);
    }

    void on_session_timeout(uint32_t sequence_number, std::any user_data)
    {
        auto sm = std::any_cast<stream_request>(user_data);
        timeout_handler_(server_id_, sequence_number, sm.message_body);
    }
};

} // namespace pa::pinex