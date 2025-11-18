// Advanced session example with custom configuration and metrics

#include <vex/networking/net/session_builder.hpp>
#include <vex/networking/net/session_manager.hpp>
#include <vex/networking/net/protocol_handler.hpp>
#include <vex/networking/net/error_handler.hpp>
#include <vex/networking/net/tcp_utils.hpp>

#include <iostream>
#include <memory>

// Custom protocol handler with state
class stateful_protocol_handler : public pa::pinex::protocol_handler
{
    std::string peer_id_;
    size_t message_count_{0};

public:
    explicit stateful_protocol_handler(std::string peer_id)
        : peer_id_(std::move(peer_id)) {}

    void on_request(pa::pinex::request&& req, uint32_t seq) override
    {
        ++message_count_;

        if (auto* stream_req = std::get_if<pa::pinex::stream_request>(&req))
        {
            std::cout << "[" << peer_id_ << "] Request #" << message_count_
                      << " [seq=" << seq << "]: " << stream_req->message_body << std::endl;
        }
    }

    void on_response(pa::pinex::response&& resp, uint32_t seq, pa::pinex::command_status status) override
    {
        ++message_count_;

        if (auto* stream_resp = std::get_if<pa::pinex::stream_response>(&resp))
        {
            std::cout << "[" << peer_id_ << "] Response #" << message_count_
                      << " [seq=" << seq << ", status=" << static_cast<int>(status)
                      << "]: " << stream_resp->message_body << std::endl;
        }
    }

    size_t message_count() const { return message_count_; }
};

// Custom error handler with logging
class logging_error_handler : public pa::pinex::error_handler
{
    std::string peer_id_;

public:
    explicit logging_error_handler(std::string peer_id)
        : peer_id_(std::move(peer_id)) {}

    void on_deserialization_error(const std::string& msg, pa::pinex::command_id id,
                                  std::span<const uint8_t> data) override
    {
        std::cerr << "[ERROR][" << peer_id_ << "] Deserialization failed for command "
                  << static_cast<int>(id) << ": " << msg
                  << " (data size: " << data.size() << " bytes)" << std::endl;
    }

    void on_protocol_error(const std::string& msg) override
    {
        std::cerr << "[ERROR][" << peer_id_ << "] Protocol error: " << msg << std::endl;
    }

    void on_network_error(const std::string& msg) override
    {
        std::cerr << "[ERROR][" << peer_id_ << "] Network error: " << msg << std::endl;
    }
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2 || (std::string(argv[1]) != "client" && std::string(argv[1]) != "server"))
        {
            std::cerr << "Usage: " << argv[0] << " <client|server>" << std::endl;
            return 1;
        }

        bool is_server = (std::string(argv[1]) == "server");
        boost::asio::io_context io;

        // Create custom session configuration
        pa::pinex::session_config config;
        config.send_buf_capacity = 2 * 1024 * 1024;       // 2MB
        config.send_buf_threshold = 1 * 1024 * 1024;      // 1MB
        config.receive_buf_size = 2 * 1024 * 1024;        // 2MB
        config.max_command_length = 5 * 1024 * 1024;      // 5MB
        config.unbind_timeout = std::chrono::seconds{15};
        config.backpressure_low_watermark = 512 * 1024;   // 512KB
        config.backpressure_high_watermark = 1 * 1024 * 1024; // 1MB

        if (is_server)
        {
            std::cout << "=== Advanced Server ===" << std::endl;

            // Use shared_ptr to keep acceptor alive
            auto acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(
                io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8080));

            auto manager = std::make_shared<pa::pinex::session_manager>(io);

            // Use shared_ptr for the recursive function to avoid dangling references
            auto do_accept = std::make_shared<std::function<void()>>();

            *do_accept = [&io, acceptor, manager, &config, do_accept]() {
                acceptor->async_accept([&io, acceptor, manager, &config, do_accept](
                    boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                    if (!ec)
                    {
                        std::string peer_addr = socket.remote_endpoint().address().to_string();
                        uint16_t peer_port = socket.remote_endpoint().port();
                        std::string peer_id = peer_addr + ":" + std::to_string(peer_port);

                        std::cout << "New connection from: " << peer_id << std::endl;

                        // Configure socket
                        pa::pinex::enable_keepalive(socket, 60);
                        pa::pinex::enable_no_delay(socket);

                        // Build session with custom configuration
                        auto session = pa::pinex::session_builder()
                            .with_send_capacity(config.send_buf_capacity)
                            .with_send_threshold(config.send_buf_threshold)
                            .with_receive_buffer(config.receive_buf_size)
                            .with_max_message_size(config.max_command_length)
                            .with_unbind_timeout(config.unbind_timeout)
                            .with_backpressure(config.backpressure_low_watermark,
                                             config.backpressure_high_watermark)
                            .with_protocol_handler(
                                std::make_unique<stateful_protocol_handler>(peer_id))
                            .with_error_handler(
                                std::make_unique<logging_error_handler>(peer_id))
                            .with_close_handler([peer_id](auto session, auto reason) {
                                auto& metrics = session->metrics();
                                std::cout << "\n[" << peer_id << "] Session closed" << std::endl;
                                if (reason)
                                    std::cout << "  Reason: " << *reason << std::endl;
                                std::cout << "  Bytes sent: " << metrics.bytes_sent << std::endl;
                                std::cout << "  Bytes received: " << metrics.bytes_received << std::endl;
                                std::cout << "  Messages sent: " << metrics.messages_sent << std::endl;
                                std::cout << "  Messages received: " << metrics.messages_received << std::endl;
                                std::cout << "  Errors: " << metrics.errors << std::endl;
                                std::cout << "  Uptime: " << metrics.uptime().count() << "ms" << std::endl;
                            })
                            .build(io, std::move(socket));

                        session->start();

                        // Send welcome message
                        pa::pinex::stream_request welcome;
                        welcome.message_body = "Welcome to advanced server!";
                        session->send_request(welcome);
                    }

                    // Call recursively
                    (*do_accept)();
                });
            };

            // Start accepting
            (*do_accept)();

            std::cout << "Server listening on port 8080" << std::endl;
            std::cout << "Configuration:" << std::endl;
            std::cout << "  Send buffer: " << config.send_buf_capacity / 1024 << "KB" << std::endl;
            std::cout << "  Max message: " << config.max_command_length / 1024 << "KB" << std::endl;
            std::cout << "  Unbind timeout: " << config.unbind_timeout.count() << "s" << std::endl;

            // Periodic metrics
            auto metrics_timer = std::make_shared<boost::asio::steady_timer>(io);

            // Use shared_ptr for recursive function
            auto print_metrics = std::make_shared<std::function<void(boost::system::error_code)>>();

            *print_metrics = [manager, metrics_timer, print_metrics](boost::system::error_code ec) {
                if (!ec)
                {
                    auto agg = manager->get_metrics();
                    std::cout << "\n=== Server Metrics ===" << std::endl;
                    std::cout << "Active sessions: " << agg.active_sessions << std::endl;
                    std::cout << "Total bytes sent: " << agg.total_bytes_sent << std::endl;
                    std::cout << "Total bytes received: " << agg.total_bytes_received << std::endl;
                    std::cout << "Total messages: "
                              << (agg.total_messages_sent + agg.total_messages_received) << std::endl;
                    std::cout << "Total errors: " << agg.total_errors << std::endl;
                    std::cout << "=====================\n" << std::endl;

                    metrics_timer->expires_after(std::chrono::seconds{10});
                    metrics_timer->async_wait(*print_metrics);
                }
            };

            metrics_timer->expires_after(std::chrono::seconds{10});
            metrics_timer->async_wait(*print_metrics);
        }
        else
        {
            std::cout << "=== Advanced Client ===" << std::endl;

            boost::asio::ip::tcp::socket socket(io);
            boost::asio::ip::tcp::resolver resolver(io);
            auto endpoints = resolver.resolve("127.0.0.1", "8080");
            boost::asio::connect(socket, endpoints);

            std::cout << "Connected to server" << std::endl;

            // Configure socket
            pa::pinex::enable_keepalive(socket, 60);
            pa::pinex::enable_no_delay(socket);

            // Build session
            auto session = pa::pinex::session_builder()
                .with_send_capacity(config.send_buf_capacity)
                .with_send_threshold(config.send_buf_threshold)
                .with_receive_buffer(config.receive_buf_size)
                .with_max_message_size(config.max_command_length)
                .with_unbind_timeout(config.unbind_timeout)
                .with_backpressure(config.backpressure_low_watermark,
                                 config.backpressure_high_watermark)
                .with_protocol_handler(
                    std::make_unique<stateful_protocol_handler>("server"))
                .with_error_handler(
                    std::make_unique<logging_error_handler>("server"))
                .with_close_handler([](auto session, auto reason) {
                    auto& metrics = session->metrics();
                    std::cout << "\nSession closed" << std::endl;
                    if (reason)
                        std::cout << "  Reason: " << *reason << std::endl;
                    std::cout << "  Bytes sent: " << metrics.bytes_sent << std::endl;
                    std::cout << "  Bytes received: " << metrics.bytes_received << std::endl;
                    std::cout << "  Messages sent: " << metrics.messages_sent << std::endl;
                    std::cout << "  Messages received: " << metrics.messages_received << std::endl;
                    std::cout << "  Errors: " << metrics.errors << std::endl;
                    std::cout << "  Uptime: " << metrics.uptime().count() << "ms" << std::endl;
                })
                .build(io, std::move(socket));

            session->start();

            // Send periodic messages
            auto send_timer = std::make_shared<boost::asio::steady_timer>(io);
            auto send_periodic = std::make_shared<std::function<void(boost::system::error_code)>>();
            auto msg_counter = std::make_shared<int>(0);

            *send_periodic = [session, send_timer, send_periodic, msg_counter](boost::system::error_code ec) {
                if (!ec && session->is_open())
                {
                    pa::pinex::stream_request req;
                    req.message_body = "Test message #" + std::to_string(++(*msg_counter));
                    auto seq = session->send_request(req);

                    std::cout << "Sent message #" << *msg_counter << " [seq=" << seq << "]" << std::endl;

                    // Print current metrics
                    auto& m = session->metrics();
                    std::cout << "  Current stats: " << m.messages_sent << " sent, "
                              << m.messages_received << " received, "
                              << m.bytes_sent << " bytes out, "
                              << m.bytes_received << " bytes in" << std::endl;

                    send_timer->expires_after(std::chrono::seconds{2});
                    send_timer->async_wait(*send_periodic);
                }
            };

            send_timer->expires_after(std::chrono::seconds{1});
            send_timer->async_wait(*send_periodic);
        }

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