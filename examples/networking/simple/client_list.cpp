#include <vex/networking/p_client_list.hpp>

#include <fmt/core.h>
#include <iostream>
#include <future>

void receive_request(pa::pinex::p_client_list* client_list, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    fmt::print("receive request: {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
    client_list->send_response("hello world back!", seq_no, client_id);
}

void receive_response(pa::pinex::p_client_list* client_list, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    fmt::print("receive response: {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
}

void timeout(pa::pinex::p_client_list* client, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    fmt::print("timeout packet {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
}

int main(int argc, const char** argv)
{
    boost::asio::io_context io_context;

    std::vector<std::string> uries {"127.0.0.1:57148"};

    pa::pinex::p_client_list example_client_list{ &io_context,
                             argv[1] /*"client_1"*/,
                             uries,
                             10,
                             100,
                             true,
                             std::bind_front(receive_request, &example_client_list),
                             std::bind_front(receive_response, &example_client_list),
                             std::bind_front(timeout, &example_client_list) };

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const std::error_code& ec, int) {
        if (!ec)
        {
            example_client_list.stop();
            io_context.stop();
        }
    });

    std::thread t([&io_context, &example_client_list](){
        sleep(5);

        for(auto i = 0; i <10; i++)
        {
            std::packaged_task<void()> task([&] {
                example_client_list.send_request("hello world!", "server_1");
            });

            boost::asio::post(io_context, std::move(task));
        }
    });

    io_context.run();

    t.join();

    return 0;
}