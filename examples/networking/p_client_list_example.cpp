// Example using p_client_list for multiple connections

#include <vex/networking/p_client_list.hpp>

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 3)
        {
            std::cerr << "Usage: " << argv[0] << " <client_id> <server1:port> [server2:port] ..." << std::endl;
            std::cerr << "Example: " << argv[0] << " client1 127.0.0.1:8080 127.0.0.1:8081" << std::endl;
            return 1;
        }

        std::string client_id = argv[1];
        std::vector<std::string> servers;

        for (int i = 2; i < argc; ++i)
        {
            servers.push_back(argv[i]);
        }

        boost::asio::io_context io;

        auto clients = std::make_shared<pa::pinex::p_client_list>(
            &io,
            client_id,
            servers,
            30,    // timeout seconds
            60,    // inactivity timeout
            true,  // auto-reconnect
            // Request handler
            [](const std::string& server_id, uint32_t seq, const std::string& msg) {
                std::cout << "Request from " << server_id << " [seq=" << seq << "]: " << msg << std::endl;
            },
            // Response handler
            [](const std::string& server_id, uint32_t seq, const std::string& msg) {
                std::cout << "Response from " << server_id << " [seq=" << seq << "]: " << msg << std::endl;
            },
            // Timeout handler
            [](const std::string& server_id, uint32_t seq, const std::string& msg) {
                std::cerr << "Request timeout from " << server_id << " [seq=" << seq << "]" << std::endl;
            },
            // Session handler
            [](const std::string& server_id, pa::pinex::session_stat stat) {
                if (stat == pa::pinex::session_stat::bind)
                    std::cout << "Connected to server: " << server_id << std::endl;
                else if (stat == pa::pinex::session_stat::close)
                    std::cout << "Disconnected from server: " << server_id << std::endl;
            });

        std::cout << "Client '" << client_id << "' started" << std::endl;
        std::cout << "Connecting to " << servers.size() << " servers" << std::endl;
        std::cout << "\nCommands:" << std::endl;
        std::cout << "  broadcast <msg>       - Send to all servers" << std::endl;
        std::cout << "  send <server_id> <msg> - Send to specific server" << std::endl;
        std::cout << "  roundrobin <msg>      - Send using round-robin" << std::endl;
        std::cout << "  status                - Show connection status" << std::endl;
        std::cout << "  quit                  - Exit" << std::endl;

        // Periodic status
        auto status_timer = std::make_shared<boost::asio::steady_timer>(io);
        auto print_status = std::make_shared<std::function<void(boost::system::error_code)>>();

        *print_status = [status_timer, clients, print_status](boost::system::error_code ec) {
            if (!ec)
            {
                std::cout << "\n=== Connection Status ===" << std::endl;
                std::cout << "Bound clients: " << clients->binded_count()
                          << "/" << clients->total_count() << std::endl;

                auto ids = clients->get_binded_ids();
                for (const auto& id : ids)
                    std::cout << "  - " << id << " (connected)" << std::endl;
                std::cout << "=======================\n" << std::endl;

                status_timer->expires_after(std::chrono::seconds{30});
                status_timer->async_wait(*print_status);
            }
        };

        status_timer->expires_after(std::chrono::seconds{30});
        status_timer->async_wait(*print_status);

        // Command line input
        std::thread input_thread([clients, &io]() {
            std::string line;

            while (std::getline(std::cin, line))
            {
                if (line.empty()) continue;

                boost::asio::post(io, [clients, &io, line]() {
                    std::istringstream iss(line);
                    std::string cmd;
                    iss >> cmd;

                    if (cmd == "broadcast")
                    {
                        std::string msg;
                        std::getline(iss, msg);
                        if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);

                        auto count = clients->broadcast(msg);
                        std::cout << "Broadcast sent to " << count << " servers" << std::endl;
                    }
                    else if (cmd == "send")
                    {
                        std::string server_id, msg;
                        iss >> server_id;
                        std::getline(iss, msg);
                        if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);

                        auto seq = clients->send_request(msg, server_id);
                        if (seq > 0)
                            std::cout << "Sent to " << server_id << " [seq=" << seq << "]" << std::endl;
                        else
                            std::cout << "Failed to send (server not connected)" << std::endl;
                    }
                    else if (cmd == "roundrobin")
                    {
                        std::string msg;
                        std::getline(iss, msg);
                        if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);

                        auto [seq, server_id] = clients->send_request(msg);
                        if (seq > 0)
                            std::cout << "Sent to " << server_id << " [seq=" << seq << "]" << std::endl;
                        else
                            std::cout << "Failed to send (no servers connected)" << std::endl;
                    }
                    else if (cmd == "status")
                    {
                        std::cout << "Bound: " << clients->binded_count()
                                  << "/" << clients->total_count() << std::endl;

                        auto ids = clients->get_binded_ids();
                        std::cout << "Connected servers:" << std::endl;
                        for (const auto& id : ids)
                            std::cout << "  - " << id << std::endl;
                    }
                    else if (cmd == "quit" || cmd == "exit")
                    {
                        std::cout << "Shutting down..." << std::endl;
                        clients->stop();
                        io.stop();
                    }
                    else
                    {
                        std::cout << "Unknown command: " << cmd << std::endl;
                    }
                });
            }
        });

        io.run();

        if (input_thread.joinable())
            input_thread.join();

        std::cout << "Client list stopped" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}