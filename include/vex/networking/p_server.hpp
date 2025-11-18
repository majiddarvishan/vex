#pragma once

#include <vex/networking/net/server.hpp>
#include <vex/networking/net/protocol_handler.hpp>
#include <vex/networking/common/helpers.hpp>
#include <vex/networking/io/expirator.hpp>
#include <vex/networking/net/definitions.hpp>

#include <fmt/core.h>

#include <map>
#include <memory>
#include <functional>
#include <string>
#include <any>

namespace pa::pinex
{

// ============================================================================
// P_Server Protocol Handler
// ============================================================================

class p_server_protocol_handler : public protocol_handler
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
// P_Server - Protocol Server Wrapper
// ============================================================================

class p_server
{
public:
    using packet_handler_t = std::function<void(const std::string&, uint32_t, const std::string&)>;
    using session_handler_t = std::function<void(const std::string&, session_stat)>;

private:
    boost::asio::io_context* io_context_;
    std::shared_ptr<server> tcp_server_;
    std::map<std::string, session_ptr> binded_sessions_;
    std::map<std::string, std::shared_ptr<io::expirator<uint32_t, std::any>>> packet_expirators_;
    std::chrono::seconds timeout_sec_;
    int last_index_{0};

    packet_handler_t request_handler_;
    packet_handler_t response_handler_;
    packet_handler_t timeout_handler_;
    session_handler_t session_handler_;

public:
    explicit p_server(boost::asio::io_context* io_context,
                     std::string_view system_id,
                     const std::string& uri_address,
                     uint64_t timeout_sec,
                     uint16_t inactivity_timeout,
                     packet_handler_t request_handler,
                     packet_handler_t response_handler,
                     packet_handler_t timeout_handler,
                     session_handler_t session_handler = nullptr)
      : io_context_(io_context)
      , timeout_sec_{std::chrono::seconds(timeout_sec)}
      , request_handler_(std::move(request_handler))
      , response_handler_(std::move(response_handler))
      , timeout_handler_(std::move(timeout_handler))
      , session_handler_(std::move(session_handler))
    {
        auto uri = split(uri_address, ':');

        tcp_server_ = std::make_shared<server>(
            io_context,
            uri[0],
            static_cast<uint16_t>(std::stoi(uri[1])),
            system_id,
            inactivity_timeout,
            [this](const auto& req, auto session) { return on_bind(req, session); });

        tcp_server_->start();
    }

    p_server(const p_server&) = delete;
    p_server& operator=(const p_server&) = delete;
    p_server(p_server&&) = delete;
    p_server& operator=(p_server&&) = delete;
    ~p_server() = default;

    void stop()
    {
        tcp_server_->stop();

        if (!binded_sessions_.empty())
        {
            fmt::print("unbinding binded sessions...\n");
            for (auto& [id, session] : binded_sessions_)
                session->unbind();
        }
    }

    size_t session_count() const
    {
        return binded_sessions_.size();
    }

    std::vector<std::string> get_client_ids() const
    {
        std::vector<std::string> ids;
        ids.reserve(binded_sessions_.size());
        for (const auto& [id, _] : binded_sessions_)
            ids.push_back(id);
        return ids;
    }

    uint32_t send_response(const std::string& msg, uint32_t seq_no, const std::string& client_id)
    {
        try
        {
            auto session = binded_sessions_.at(client_id);
            session->send_response(
                stream_response{.message_body = msg},
                seq_no,
                command_status::rok);
            return seq_no;
        }
        catch (const std::out_of_range&)
        {
            fmt::print("could not find client {}\n", client_id);
            return 0;
        }
    }

    uint32_t send_request(const std::string& msg, const std::string& client_id)
    {
        try
        {
            stream_request pdu{.message_body = msg};

            auto session = binded_sessions_.at(client_id);
            auto seq_no = session->send_request(pdu);

            packet_expirators_.at(client_id)->add(seq_no, timeout_sec_, pdu);

            return seq_no;
        }
        catch (const std::out_of_range&)
        {
            fmt::print("could not find client {}\n", client_id);
            return 0;
        }
    }

    std::tuple<uint32_t, std::string> send_request(const std::string& msg)
    {
        if (binded_sessions_.empty())
            return {0, ""};

        stream_request pdu{.message_body = msg};

        auto it = binded_sessions_.begin();
        std::advance(it, last_index_);

        auto session = it->second;
        auto seq_no = session->send_request(pdu);
        auto client_id = it->first;

        packet_expirators_.at(client_id)->add(seq_no, timeout_sec_, pdu);

        last_index_ = (last_index_ + 1) % static_cast<int>(binded_sessions_.size());

        return {seq_no, client_id};
    }

    uint32_t broadcast(const std::string& msg)
    {
        stream_request pdu{.message_body = msg};

        for (auto& [id, session] : binded_sessions_)
        {
            session->send_request(pdu);
        }

        return static_cast<uint32_t>(binded_sessions_.size());
    }

private:
    bool on_bind(const bind_request& bind_request, session_ptr session)
    {
        fmt::print("new session has been binded system_id:{}\n", bind_request.system_id);

        if (binded_sessions_.contains(bind_request.system_id))
        {
            fmt::print("session with id {} already exists\n", bind_request.system_id);
            return false;
        }

        // Create protocol handler
        auto handler = std::make_unique<p_server_protocol_handler>();
        auto* handler_ptr = handler.get();

        const std::string client_id = bind_request.system_id;

        handler_ptr->request_callback =
            [this, client_id](auto&& req, auto seq) {
                on_session_request(client_id, std::move(req), seq);
            };

        handler_ptr->response_callback =
            [this, client_id](auto&& resp, auto seq, auto status) {
                on_session_response(client_id, std::move(resp), seq, status);
            };

        session->set_protocol_handler(std::move(handler));

        // Set close handler
        session->set_close_handler(
            [this, client_id](auto session, auto error) {
                on_session_close(client_id, session, error);
            });

        // Create packet expirator for this client
        auto packet_expirator = std::make_shared<io::expirator<uint32_t, std::any>>(
            io_context_,
            [this, client_id](auto seq, auto data) {
                on_session_timeout(client_id, seq, data);
            });

        packet_expirator->start();
        packet_expirators_[client_id] = packet_expirator;

        // Store session
        binded_sessions_[client_id] = session;

        if (session_handler_)
            session_handler_(client_id, session_stat::bind);

        return true;
    }

    void on_session_close(const std::string& client_id,
                         session_ptr,
                         std::optional<std::string> error)
    {
        if (!binded_sessions_.erase(client_id))
        {
            fmt::print("could not find client {}\n", client_id);
            return;
        }

        if (error)
            fmt::print("session has been closed on error:{}\n", error.value());
        else
            fmt::print("session has been closed gracefully\n");

        if (auto it = packet_expirators_.find(client_id); it != packet_expirators_.end())
        {
            it->second->expire_all();
            packet_expirators_.erase(it);
        }

        if (session_handler_)
            session_handler_(client_id, session_stat::close);
    }

    void on_session_request(const std::string& client_id,
                           request&& req,
                           uint32_t sequence_number)
    {
        std::visit(
            [this, &client_id, sequence_number](auto&& r) {
                using request_type = std::decay_t<decltype(r)>;
                if constexpr (std::is_same_v<request_type, stream_request>)
                {
                    stream_request sm = std::move(r);
                    request_handler_(client_id, sequence_number, sm.message_body);
                }
                else if constexpr (!std::is_same_v<request_type, std::monostate>)
                {
                    fmt::print("invalid packet type\n");
                }
            },
            req);
    }

    void on_session_response(const std::string& client_id,
                            response&& resp,
                            uint32_t sequence_number,
                            command_status /*status*/)
    {
        std::visit(
            [this, &client_id, sequence_number](auto&& r) {
                using response_type = std::decay_t<decltype(r)>;
                if constexpr (std::is_same_v<response_type, stream_response>)
                {
                    stream_response sm = std::move(r);

                    if (auto it = packet_expirators_.find(client_id); it != packet_expirators_.end())
                        it->second->remove(sequence_number);

                    response_handler_(client_id, sequence_number, sm.message_body);
                }
                else if constexpr (!std::is_same_v<response_type, std::monostate>)
                {
                    fmt::print("invalid packet type\n");
                }
            },
            resp);
    }

    void on_session_timeout(const std::string& client_id,
                           uint32_t seq_no,
                           std::any user_data)
    {
        auto sm = std::any_cast<stream_request>(user_data);
        timeout_handler_(client_id, seq_no, sm.message_body);
    }
};

} // namespace pa::pinex