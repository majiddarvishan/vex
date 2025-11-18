// Simple server example using basic server API

#include <vex/networking/net/server.hpp>
#include <vex/networking/net/protocol_handler.hpp>

#include <iostream>
#include <memory>
#include <unordered_map>

class simple_server_handler : public pa::pinex::protocol_handler
{
    pa::pinex::session_ptr session_;
    std::string client_id_;

public:
    simple_server_handler(pa::pinex::session_ptr session, std::string client_id)
        : session_(session), client_id_(std::move(client_id)) {}

    void on_request(pa::pinex::request&& req, uint32_t seq) override
    {
        if (auto* stream_req = std::get_if<pa::pinex::stream_request>(&req))
        {
            std::cout << "Request from " << client_id_ << " [seq=" << seq << "]: "
                      << stream_req->message_body << std::endl;

            // Echo back
            pa::pinex::stream_response resp;
            resp.message_body = "Echo: " + stream_req->message_body;
            session_->send_response(resp, seq, pa::pinex::command_status::rok);
        }
    }

    void on_response(pa::pinex::response&& resp, uint32_t seq, pa::pinex::command_status status) override
    {
        if (auto* stream_resp = std::get_if<pa::pinex::stream_response>(&resp))
        {
            std::cout << "Response from " << client_id_ << " [seq=" << seq << "]: "
                      << stream_resp->message_body << std::endl;
        }
    }
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
            std::cerr << "Example: " << argv[0] << " 0.0.0.0 8080" << std::endl;
            return 1;
        }

        std::string host = argv[1];
        uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));

        boost::asio::io_context io;

        // Track connected clients
        auto active_clients = std::make_shared<std::unordered_map<std::string, pa::pinex::session_ptr>>();

        auto srv = std::make_shared<pa::pinex::server>(
            &io,
            host,
            port,
            "simple_server",
            60, // inactivity timeout
            [active_clients](const pa::pinex::bind_request& req, pa::pinex::session_ptr session) -> bool {
                std::cout << "Bind request from: " << req.system_id << std::endl;

                // Simple authentication
                // if (req.password != "secret")
                // {
                //     std::cout << "Authentication failed for: " << req.system_id << std::endl;
                //     return false;
                // }

                // Check if already connected
                if (active_clients->count(req.system_id) > 0)
                {
                    std::cout << "Client already connected: " << req.system_id << std::endl;
                    return false;
                }

                std::cout << "Bind accepted for: " << req.system_id << std::endl;

                // Install protocol handler
                auto handler = std::make_unique<simple_server_handler>(session, req.system_id);
                session->set_protocol_handler(std::move(handler));

                // Set close handler
                std::string client_id = req.system_id;
                session->set_close_handler([active_clients, client_id](auto, auto reason) {
                    active_clients->erase(client_id);
                    if (reason)
                        std::cout << "Client " << client_id << " disconnected: " << *reason << std::endl;
                    else
                        std::cout << "Client " << client_id << " disconnected normally" << std::endl;
                });

                // Track the client
                (*active_clients)[req.system_id] = session;

                return true;
            });

        srv->start();

        std::cout << "Server listening on " << host << ":" << port << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl;

        // Periodic status report
        auto status_timer = std::make_shared<boost::asio::steady_timer>(io);
        auto print_status = std::make_shared<std::function<void(boost::system::error_code)>>();

        *print_status = [status_timer, active_clients, print_status](boost::system::error_code ec) {
            if (!ec)
            {
                std::cout << "\n=== Status ===" << std::endl;
                std::cout << "Active clients: " << active_clients->size() << std::endl;
                for (const auto& [id, session] : *active_clients)
                {
                    auto& metrics = session->metrics();
                    std::cout << "  " << id << ": "
                              << metrics.messages_received << " msgs received, "
                              << metrics.messages_sent << " msgs sent" << std::endl;
                }
                std::cout << "==============\n" << std::endl;

                status_timer->expires_after(std::chrono::seconds{30});
                status_timer->async_wait(*print_status);
            }
        };

        status_timer->expires_after(std::chrono::seconds{30});
        status_timer->async_wait(*print_status);

        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}