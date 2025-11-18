#include <vex/networking/p_client.hpp>

#include <fmt/core.h>
#include <future>
#include <chrono>

int req_count = 0;
int resp_count = 0;

std::chrono::time_point<std::chrono::steady_clock> start;

void receive_request(pa::pinex::p_client* client, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    fmt::print("receive request: {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
    // client->send("hello world back!", seq_no);
}

void receive_response(pa::pinex::p_client* client, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    // fmt::print("receive response: {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
    resp_count++;
    if(resp_count == req_count)
    {
        auto end = std::chrono::steady_clock::now();
        fmt::print("{} responses is received in {} seconds\n", resp_count, std::chrono::duration_cast<std::chrono::seconds>(end - start).count());
    }
}

void timeout(pa::pinex::p_client* client, const std::string& client_id, uint32_t seq_no, const std::string& msg_body)
{
    fmt::print("timeout packet {} with sequence: {} from client: {}\n", msg_body, seq_no, client_id);
}

int main(int argc, const char** argv)
{
    boost::asio::io_context io_context;

    pa::pinex::p_client example_client{&io_context,
                                      argv[1] /*"client_1"*/,
                                      "127.0.0.1:57148",
                                      10,
                                      100,
                                      true,
                                      std::bind_front(receive_request, &example_client),
                                      std::bind_front(receive_response, &example_client),
                                      std::bind_front(timeout, &example_client)};

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const std::error_code& ec, int) {
        if (!ec)
        {
            example_client.stop();
            io_context.stop();
        }
    });

    int test_request = std::stoi(argv[2]);

    std::thread t([&io_context, &example_client, &test_request](){
        sleep(5);

        start = std::chrono::steady_clock::now();
        for(; req_count < test_request; req_count++)
        {
            std::packaged_task<void()> task([&] {
                example_client.send_request("hello world!");
            });

            boost::asio::post(io_context, std::move(task));
        }

        fmt::print("{} requests is sent\n", req_count);
    });

    io_context.run();

    t.join();

    return 0;
}