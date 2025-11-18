// Echo client - sends messages and receives echoes

#include <vex/networking/p_client.hpp>

#include <iostream>
#include <string>
#include <random>
#include <atomic>
#include <iomanip>

int main(int argc, char* argv[])
{
    try
    {
        std::string server_addr = "127.0.0.1:8080";
        std::string client_id = "echo_client";
        int message_rate = 1; // messages per second

        if (argc > 1) server_addr = argv[1];
        if (argc > 2) client_id = argv[2];
        if (argc > 3) message_rate = std::stoi(argv[3]);

        boost::asio::io_context io;

        // Statistics
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};

        auto start_time = std::chrono::steady_clock::now();

        auto client = std::make_shared<pa::pinex::p_client>(
            &io,
            client_id,
            server_addr,
            30,    // timeout
            60,    // inactivity timeout
            true,  // auto-reconnect
            // Request handler (shouldn't receive requests as client)
            [](const std::string&, uint32_t, const std::string& msg) {
                std::cout << "Unexpected request: " << msg << std::endl;
            },
            // Response handler (echoes)
            [&messages_received, &bytes_received, &start_time](
                const std::string&, uint32_t seq, const std::string& msg) {

                messages_received++;
                bytes_received += msg.size();

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - start_time).count();

                if (messages_received % 10 == 0)
                {
                    double msg_rate = elapsed > 0 ? messages_received / (double)elapsed : 0;
                    double byte_rate = elapsed > 0 ? bytes_received / (double)elapsed : 0;

                    std::cout << "Received echo [seq=" << seq << "]: " << msg
                              << " (" << messages_received << " total, "
                              << std::fixed << std::setprecision(2)
                              << msg_rate << " msg/s, "
                              << byte_rate / 1024.0 << " KB/s)" << std::endl;
                }
            },
            // Timeout handler
            [](const std::string&, uint32_t seq, const std::string&) {
                std::cerr << "Request timeout [seq=" << seq << "]" << std::endl;
            },
            // Bind handler
            [](const std::string& server_id, std::shared_ptr<pa::pinex::p_client>) {
                std::cout << "Connected to server: " << server_id << std::endl;
            },
            // Close handler
            [](const std::string& server_id) {
                std::cout << "Disconnected from server: " << server_id << std::endl;
            });

        client->start();

        std::cout << "Echo Client: " << client_id << std::endl;
        std::cout << "Connecting to: " << server_addr << std::endl;
        std::cout << "Message rate: " << message_rate << " msg/s" << std::endl;
        std::cout << "Press Ctrl+C to stop\n" << std::endl;

        // Message generator
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> size_dist(10, 100);

        auto send_timer = std::make_shared<boost::asio::steady_timer>(io);
        auto send_messages = std::make_shared<std::function<void(boost::system::error_code)>>();

        *send_messages = [client, send_timer, send_messages, &messages_sent, &bytes_sent, &gen, &size_dist, message_rate](boost::system::error_code ec) {
            if (!ec && client->is_connected())
            {
                // Generate random message
                int msg_size = size_dist(gen);
                std::string msg(msg_size, 'A' + (messages_sent % 26));
                msg = "Message #" + std::to_string(messages_sent + 1) + ": " + msg;

                messages_sent++;
                bytes_sent += msg.size();

                auto seq = client->send_request(msg);

                if (messages_sent % 10 == 0)
                {
                    std::cout << "Sent message #" << messages_sent
                              << " [seq=" << seq << "] - " << msg.size() << " bytes" << std::endl;
                }

                // Schedule next send
                auto delay = std::chrono::milliseconds(1000 / message_rate);
                send_timer->expires_after(delay);
                send_timer->async_wait(*send_messages);
            }
            else if (!client->is_connected())
            {
                std::cout << "Waiting for connection..." << std::endl;
                send_timer->expires_after(std::chrono::seconds{1});
                send_timer->async_wait(*send_messages);
            }
        };

        send_timer->expires_after(std::chrono::seconds{1});
        send_timer->async_wait(*send_messages);

        // Statistics timer
        auto stats_timer = std::make_shared<boost::asio::steady_timer>(io);
        auto print_stats = std::make_shared<std::function<void(boost::system::error_code)>>();

        *print_stats = [stats_timer, print_stats, &messages_sent, &messages_received, &bytes_sent, &bytes_received, &start_time](boost::system::error_code ec) {
            if (!ec)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - start_time).count();

                double send_rate = elapsed > 0 ? messages_sent / (double)elapsed : 0;
                double recv_rate = elapsed > 0 ? messages_received / (double)elapsed : 0;
                double send_bandwidth = elapsed > 0 ? bytes_sent / (double)elapsed / 1024.0 : 0;
                double recv_bandwidth = elapsed > 0 ? bytes_received / (double)elapsed / 1024.0 : 0;

                std::cout << "\n=== Statistics ===" << std::endl;
                std::cout << "Uptime: " << elapsed << " seconds" << std::endl;
                std::cout << "Messages sent: " << messages_sent
                          << " (" << std::fixed << std::setprecision(2) << send_rate << " msg/s)" << std::endl;
                std::cout << "Messages received: " << messages_received
                          << " (" << recv_rate << " msg/s)" << std::endl;
                std::cout << "Bytes sent: " << bytes_sent
                          << " (" << send_bandwidth << " KB/s)" << std::endl;
                std::cout << "Bytes received: " << bytes_received
                          << " (" << recv_bandwidth << " KB/s)" << std::endl;

                if (messages_sent > 0)
                {
                    double success_rate = (messages_received / (double)messages_sent) * 100.0;
                    std::cout << "Success rate: " << success_rate << "%" << std::endl;
                }

                std::cout << "==================\n" << std::endl;

                stats_timer->expires_after(std::chrono::seconds{10});
                stats_timer->async_wait(*print_stats);
            }
        };

        stats_timer->expires_after(std::chrono::seconds{10});
        stats_timer->async_wait(*print_stats);

        io.run();

        std::cout << "\nFinal Statistics:" << std::endl;
        std::cout << "Messages sent: " << messages_sent << std::endl;
        std::cout << "Messages received: " << messages_received << std::endl;
        std::cout << "Bytes sent: " << bytes_sent << std::endl;
        std::cout << "Bytes received: " << bytes_received << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}