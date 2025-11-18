#include <vex/networking/p_client.hpp>
#include <vex/networking/net/definitions.hpp>

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " <host:port> <client_id>" << std::endl;
            std::cerr << "Example: " << argv[0] << " 127.0.0.1:8080 client1" << std::endl;
            return 1;
        }

        std::string uri = argv[1];
        std::string client_id = argv[2];

        boost::asio::io_context io;

        auto client = std::make_shared<pa::pinex::p_client>(
            &io,
            client_id,
            uri,
            30,    // timeout seconds
            60,    // inactivity timeout
            true,  // auto-reconnect
            // Request handler
            [](const std::string& server_id, uint32_t seq, const std::string& msg) {
                std::cout << "Request from server [seq=" << seq << "]: " << msg << std::endl;
            },
            // Response handler
            [](const std::string& server_id, uint32_t seq, const std::string& msg) {
                std::cout << "Response from server [seq=" << seq << "]: " << msg << std::endl;
            },
            // Timeout handler
            [](const std::string& server_id, uint32_t seq, const std::string& msg) {
                std::cerr << "Request timeout [seq=" << seq << "]: " << msg << std::endl;
            },
            // Bind handler
            [](const std::string& server_id, std::shared_ptr<pa::pinex::p_client> client) {
                std::cout << "Successfully connected to server: " << server_id << std::endl;

                // Start sending messages
                client->send_request("Hello from " + client->client_id());
            },
            // Close handler
            [](const std::string& server_id) {
                std::cout << "Disconnected from server: " << server_id << std::endl;
            },
            // Session handler
            [](const std::string& server_id, pa::pinex::session_stat stat) {
                if (stat == pa::pinex::session_stat::bind)
                    std::cout << "Session bound to: " << server_id << std::endl;
                else if (stat == pa::pinex::session_stat::close)
                    std::cout << "Session closed from: " << server_id << std::endl;
            });

        client->start();

        std::cout << "Client '" << client_id << "' started" << std::endl;
        std::cout << "Connecting to " << uri << std::endl;
        std::cout << "Type messages and press Enter (or 'quit' to exit)" << std::endl;

        // Command line input
        std::thread input_thread([client, &io]() {
            std::string line;
            while (std::getline(std::cin, line))
            {
                if (line == "quit" || line == "exit")
                {
                    std::cout << "Shutting down..." << std::endl;
                    boost::asio::post(io, [client]() {
                        client->stop();
                    });
                    io.stop();
                    break;
                }

                if (!line.empty())
                {
                    boost::asio::post(io, [client, line]() {
                        if (client->is_connected())
                        {
                            auto seq = client->send_request(line);
                            std::cout << "Sent message [seq=" << seq << "]" << std::endl;
                        }
                        else
                        {
                            std::cout << "Not connected to server" << std::endl;
                        }
                    });
                }
            }
        });

        io.run();

        if (input_thread.joinable())
            input_thread.join();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}