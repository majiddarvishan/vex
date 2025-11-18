#pragma once

#include <vex/networking/p_client.hpp>

#include <fmt/core.h>

#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace pa::pinex
{

// ============================================================================
// P_Client_List - Manages Multiple P_Clients
// ============================================================================

class p_client_list
{
public:
    using packet_handler_t = std::function<void(const std::string&, uint32_t, const std::string&)>;
    using session_handler_t = std::function<void(const std::string&, session_stat)>;

private:
    std::map<std::string, std::shared_ptr<p_client>> binded_clients_;
    std::set<std::shared_ptr<p_client>> p_client_list_; // Keeps clients alive
    int last_index_{0};

public:
    explicit p_client_list(boost::asio::io_context* io_context,
                          const std::string& id,
                          const std::vector<std::string>& uri_addresses,
                          uint64_t timeout_sec,
                          uint16_t inactivity_timeout,
                          bool auto_reconnect,
                          packet_handler_t request_handler,
                          packet_handler_t response_handler,
                          packet_handler_t timeout_handler,
                          session_handler_t session_handler = nullptr)
    {
        for (const auto& uri : uri_addresses)
        {
            auto clnt = std::make_shared<p_client>(
                io_context,
                id,
                uri,
                timeout_sec,
                inactivity_timeout,
                auto_reconnect,
                request_handler,
                response_handler,
                timeout_handler,
                [this](const auto& server_id, auto client) { on_bind(server_id, client); },
                [this](const auto& server_id) { on_session_close(server_id); },
                session_handler);

            p_client_list_.insert(clnt);
        }
    }

    p_client_list(const p_client_list&) = delete;
    p_client_list& operator=(const p_client_list&) = delete;
    p_client_list(p_client_list&&) = default;
    p_client_list& operator=(p_client_list&&) = default;
    ~p_client_list() = default;

    void stop()
    {
        for (auto& [id, client] : binded_clients_)
        {
            client->stop();
        }
    }

    size_t binded_count() const
    {
        return binded_clients_.size();
    }

    size_t total_count() const
    {
        return p_client_list_.size();
    }

    std::vector<std::string> get_binded_ids() const
    {
        std::vector<std::string> ids;
        ids.reserve(binded_clients_.size());
        for (const auto& [id, _] : binded_clients_)
            ids.push_back(id);
        return ids;
    }

    uint32_t send_response(const std::string& msg, uint32_t seq_no, const std::string& id)
    {
        try
        {
            auto clnt = binded_clients_.at(id);
            return clnt->send_response(msg, seq_no);
        }
        catch (const std::out_of_range&)
        {
            fmt::print("could not find connection with id {}\n", id);
            return 0;
        }
    }

    uint32_t send_request(const std::string& msg, const std::string& id)
    {
        try
        {
            auto clnt = binded_clients_.at(id);
            return clnt->send_request(msg);
        }
        catch (const std::out_of_range&)
        {
            fmt::print("could not find connection with id {}\n", id);
            return 0;
        }
    }

    std::tuple<uint32_t, std::string> send_request(const std::string& msg)
    {
        if (binded_clients_.empty())
            return {0, ""};

        auto it = binded_clients_.begin();
        std::advance(it, last_index_);

        auto seq_no = it->second->send_request(msg);
        auto server_id = it->first;

        last_index_ = (last_index_ + 1) % static_cast<int>(binded_clients_.size());

        return {seq_no, server_id};
    }

    uint32_t broadcast(const std::string& msg)
    {
        for (auto& [id, client] : binded_clients_)
        {
            client->send_info(msg);
        }
        return static_cast<uint32_t>(binded_clients_.size());
    }

private:
    void on_bind(const std::string& id, std::shared_ptr<p_client> clnt)
    {
        binded_clients_[id] = clnt;
    }

    void on_session_close(const std::string& id)
    {
        if (!binded_clients_.erase(id))
        {
            fmt::print("could not find client {}\n", id);
        }
    }
};

} // namespace pa::pinex