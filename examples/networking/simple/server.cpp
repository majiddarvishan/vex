#include <vex/networking/p_server.hpp>

#include <boost/asio/io_context.hpp>
#include <fmt/core.h>
#include <iostream>

void receive_request(pa::pinex::p_server* server, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    // fmt::print("receive request: {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
    server->send_response("hello world back!", seq_no, client_id);
}

void receive_response(pa::pinex::p_server* server, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    fmt::print("receive response: {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
}

void timeout(pa::pinex::p_server* client, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    fmt::print("timeout packet {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
}

int main(int argc, const char** argv)
{
    boost::asio::io_context io_context;

    pa::pinex::p_server example_server{&io_context,
                                      argv[1] /*"server_1"*/,
                                      "0.0.0.0:" + std::string(argv[2]),
                                      5,
                                      100,
                                      std::bind_front(receive_request, &example_server),
                                      std::bind_front(receive_response, &example_server),
                                      std::bind_front(timeout, &example_server)};

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const std::error_code& ec, int) {
        if (!ec)
        {
            example_server.stop();
            io_context.stop();
        }
    });

    io_context.run();

    // std::thread t(std::bind(&boost::asio::io_context::run, &io_context));
    std::thread t([&io_context](){
        io_context.run();
    });

    sleep(5);

    // example_server.broad_cast("hello world!");
    example_server.send_request("hello world!");

    t.join();

    return 0;
}
