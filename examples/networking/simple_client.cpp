// Simple client example using basic client API

#include <vex/networking/net/client.hpp>
#include <vex/networking/net/protocol_handler.hpp>

#include <iostream>
#include <memory>

class simple_protocol_handler : public pa::pinex::protocol_handler
{
public:
    void on_request(pa::pinex::request&& req, uint32_t seq) override
    {
        if (auto* stream_req = std::get_if<pa::pinex::stream_request>(&req))
        {
            std::cout << "Received request [seq=" << seq << "]: "
                      << stream_req->message_body << std::endl;
        }
    }

    void on_response(pa::pinex::response&& resp, uint32_t seq, pa::pinex::command_status status) override
    {
        if (auto* stream_resp = std::get_if<pa::pinex::stream_response>(&resp))
        {
            std::cout << "Received response [seq=" << seq << ", status="
                      << static_cast<int>(status) << "]: "
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
            std::cerr << "Example: " << argv[0] << " 127.0.0.1 8080" << std::endl;
            return 1;
        }

        std::string host = argv[1];
        uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));

        boost::asio::io_context io;

        // Prepare bind request
        pa::pinex::bind_request bind_req;
        bind_req.system_id = "simple_client";
        // bind_req.password = "secret";
        bind_req.bind_type = pa::pinex::bind_type::bi_direction;

        // Create client
        auto client = std::make_shared<pa::pinex::client>(
            &io,
            host,
            port,
            60, // inactivity timeout
            bind_req,
            // Bind success handler
            [&io](const pa::pinex::bind_response& resp, pa::pinex::session_ptr session) {
                std::cout << "Connected to server: " << resp.system_id << std::endl;

                // Install protocol handler
                auto handler = std::make_unique<simple_protocol_handler>();
                session->set_protocol_handler(std::move(handler));

                // Set close handler
                session->set_close_handler([](auto, auto reason) {
                    if (reason)
                        std::cout << "Connection closed: " << *reason << std::endl;
                    else
                        std::cout << "Connection closed normally" << std::endl;
                });

                // Send a test message every 5 seconds
                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                auto send_periodic = std::make_shared<std::function<void(boost::system::error_code)>>();

                *send_periodic = [session, timer, send_periodic](boost::system::error_code ec) {
                    if (!ec && session->is_open())
                    {
                        pa::pinex::stream_request req;
                        req.message_body = "Hello from client!";
                        auto seq = session->send_request(req);
                        std::cout << "Sent request [seq=" << seq << "]" << std::endl;

                        timer->expires_after(std::chrono::seconds{5});
                        timer->async_wait(*send_periodic);
                    }
                };

                timer->expires_after(std::chrono::seconds{1});
                timer->async_wait(*send_periodic);
            },
            // Error handler
            [](const std::string& error) {
                std::cerr << "Client error: " << error << std::endl;
            });

        client->start();

        std::cout << "Client starting, connecting to " << host << ":" << port << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl;

        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}