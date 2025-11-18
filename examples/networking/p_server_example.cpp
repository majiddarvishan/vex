// Example using p_server wrapper

#include <vex/networking/p_server.hpp>

#include <iostream>
#include <string>
#include <csignal>

std::atomic<bool> running{true};

void signal_handler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: " << argv[0] << " <host:port>" << std::endl;
            std::cerr << "Example: " << argv[0] << " 0.0.0.0:8080" << std::endl;
            return 1;
        }

        std::string uri = argv[1];

        // Setup signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        boost::asio::io_context io;

        std::shared_ptr<pa::pinex::p_server> server;
        server = std::make_shared<pa::pinex::p_server>(
            &io,
            "example_server",
            uri,
            30,    // timeout seconds
            60,    // inactivity timeout
            // Request handler
            [&server](const std::string& client_id, uint32_t seq, const std::string& msg) {
                std::cout << "Request from " << client_id << " [seq=" << seq << "]: " << msg << std::endl;

                // Echo back the message
                std::string response = "Echo: " + msg;
                server->send_response(response, seq, client_id);
            },
            // Response handler
            [](const std::string& client_id, uint32_t seq, const std::string& msg) {
                std::cout << "Response from " << client_id << " [seq=" << seq << "]: " << msg << std::endl;
            },
            // Timeout handler
            [](const std::string& client_id, uint32_t seq, const std::string& msg) {
                std::cerr << "Request timeout from " << client_id << " [seq=" << seq << "]" << std::endl;
            },
            // Session handler
            [](const std::string& client_id, pa::pinex::session_stat stat) {
                if (stat == pa::pinex::session_stat::bind)
                    std::cout << "Client connected: " << client_id << std::endl;
                else if (stat == pa::pinex::session_stat::close)
                    std::cout << "Client disconnected: " << client_id << std::endl;
        });

        std::cout << "Server listening on " << uri << std::endl;
        std::cout << "Press Ctrl+C to shutdown" << std::endl;

        // Periodic status report
        boost::asio::steady_timer status_timer(io);
        std::function<void(boost::system::error_code)> print_status;

        print_status = [&status_timer, server, &print_status](boost::system::error_code ec) {
            if (!ec && running)
            {
                std::cout << "\n=== Server Status ===" << std::endl;
                std::cout << "Active sessions: " << server->session_count() << std::endl;

                auto client_ids = server->get_client_ids();
                for (const auto& id : client_ids)
                {
                    std::cout << "  - " << id << std::endl;
                }
                std::cout << "===================\n" << std::endl;

                status_timer.expires_after(std::chrono::seconds{30});
                status_timer.async_wait(print_status);
            }
        };

        status_timer.expires_after(std::chrono::seconds{30});
        status_timer.async_wait(print_status);

        // Command line input for server commands
        std::thread input_thread([server, &io]() {
            std::string line;
            std::cout << "\nCommands: broadcast <msg>, send <client_id> <msg>, list, quit" << std::endl;

            while (running && std::getline(std::cin, line))
            {
                if (line.empty()) continue;

                boost::asio::post(io, [&io, server, line]() {
                    std::istringstream iss(line);
                    std::string cmd;
                    iss >> cmd;

                    if (cmd == "broadcast")
                    {
                        std::string msg;
                        std::getline(iss, msg);
                        if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);

                        auto count = server->broadcast(msg);
                        std::cout << "Broadcast sent to " << count << " clients" << std::endl;
                    }
                    else if (cmd == "send")
                    {
                        std::string client_id, msg;
                        iss >> client_id;
                        std::getline(iss, msg);
                        if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);

                        auto seq = server->send_request(msg, client_id);
                        if (seq > 0)
                            std::cout << "Sent to " << client_id << " [seq=" << seq << "]" << std::endl;
                        else
                            std::cout << "Failed to send (client not found)" << std::endl;
                    }
                    else if (cmd == "list")
                    {
                        auto clients = server->get_client_ids();
                        std::cout << "Connected clients (" << clients.size() << "):" << std::endl;
                        for (const auto& id : clients)
                            std::cout << "  - " << id << std::endl;
                    }
                    else if (cmd == "quit" || cmd == "exit")
                    {
                        running = false;
                        server->stop();
                        io.stop();
                    }
                    else
                    {
                        std::cout << "Unknown command: " << cmd << std::endl;
                    }
                });
            }
        });

        while (running)
        {
            io.run_for(std::chrono::milliseconds{100});
        }

        std::cout << "Stopping server..." << std::endl;
        server->stop();

        if (input_thread.joinable())
            input_thread.join();

        std::cout << "Server stopped" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}