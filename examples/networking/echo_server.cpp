// Simple echo server - echoes back everything received

#include <vex/networking/p_server.hpp>

#include <iostream>
#include <atomic>
#include <csignal>

std::atomic<bool> running{true};

void signal_handler(int)
{
    running = false;
}

int main(int argc, char* argv[])
{
    try
    {
        std::string bind_addr = "0.0.0.0:8080";
        if (argc > 1)
            bind_addr = argv[1];

        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        boost::asio::io_context io;

        // Statistics
        std::atomic<uint64_t> total_messages{0};
        std::atomic<uint64_t> total_bytes{0};

        // Declare server pointer first
        std::shared_ptr<pa::pinex::p_server> server;

        // Create the server with handlers
        // Use a capturing lambda that will use the server pointer
        server = std::make_shared<pa::pinex::p_server>(
            &io,
            "echo_server",
            bind_addr,
            30,    // timeout
            60,    // inactivity timeout
            // Request handler - echo back by sending response
            [&server, &total_messages, &total_bytes](
                const std::string& client_id, uint32_t seq, const std::string& msg) {

                total_messages++;
                total_bytes += msg.size();

                // Echo back using the captured server reference
                server->send_response(msg, seq, client_id);

                if (total_messages % 100 == 0)
                {
                    std::cout << "Stats: " << total_messages << " messages, "
                              << total_bytes << " bytes echoed" << std::endl;
                }
            },
            // Response handler (shouldn't receive responses as server)
            [](const std::string& client_id, uint32_t seq, const std::string& msg) {
                std::cout << "[" << client_id << "] Unexpected response [seq=" << seq << "]: "
                          << msg << std::endl;
            },
            // Timeout handler
            [](const std::string& client_id, uint32_t seq, const std::string&) {
                std::cerr << "[" << client_id << "] Timeout [seq=" << seq << "]" << std::endl;
            },
            // Session handler
            [&total_messages, &total_bytes](const std::string& client_id, pa::pinex::session_stat stat) {
                if (stat == pa::pinex::session_stat::bind)
                {
                    std::cout << "\n=== Client Connected: " << client_id << " ===" << std::endl;
                    std::cout << "Total messages echoed: " << total_messages << std::endl;
                    std::cout << "Total bytes echoed: " << total_bytes << std::endl;
                    std::cout << "======================================\n" << std::endl;
                }
                else if (stat == pa::pinex::session_stat::close)
                {
                    std::cout << "\n=== Client Disconnected: " << client_id << " ===" << std::endl;
                }
            });

        std::cout << "Echo Server" << std::endl;
        std::cout << "Listening on: " << bind_addr << std::endl;
        std::cout << "Press Ctrl+C to stop\n" << std::endl;

        // Statistics timer
        auto stats_timer = std::make_shared<boost::asio::steady_timer>(io);
        auto print_stats = std::make_shared<std::function<void(boost::system::error_code)>>();

        *print_stats = [&server, stats_timer, print_stats, &total_messages, &total_bytes, &running](boost::system::error_code ec) {
            if (!ec && running)
            {
                std::cout << "\n=== Statistics ===" << std::endl;
                std::cout << "Active clients: " << server->session_count() << std::endl;
                std::cout << "Total messages: " << total_messages << std::endl;
                std::cout << "Total bytes: " << total_bytes << std::endl;

                auto clients = server->get_client_ids();
                std::cout << "Connected clients:" << std::endl;
                for (const auto& id : clients)
                    std::cout << "  - " << id << std::endl;

                std::cout << "==================\n" << std::endl;

                stats_timer->expires_after(std::chrono::seconds{30});
                stats_timer->async_wait(*print_stats);
            }
        };

        stats_timer->expires_after(std::chrono::seconds{30});
        stats_timer->async_wait(*print_stats);

        while (running)
        {
            io.run_for(std::chrono::milliseconds{100});
        }

        std::cout << "\nShutting down..." << std::endl;
        server->stop();

        std::cout << "\nFinal Statistics:" << std::endl;
        std::cout << "Total messages echoed: " << total_messages << std::endl;
        std::cout << "Total bytes echoed: " << total_bytes << std::endl;
        std::cout << "Goodbye!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}