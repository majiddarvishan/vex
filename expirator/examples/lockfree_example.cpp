#include <expirator/lockfree_expirator.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <iomanip>

struct RequestData {
    std::string client_id;
    std::string request_path;
    std::chrono::steady_clock::time_point timestamp;
    size_t bytes_transferred;
};

// Thread-safe statistics
struct Statistics {
    std::atomic<size_t> total_requests{0};
    std::atomic<size_t> expired_requests{0};
    std::atomic<size_t> cancelled_requests{0};
    std::atomic<size_t> bytes_total{0};
};

int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << "  Lock-Free Expirator - Multi-Threaded Server" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "\nThis example demonstrates thread-safe request tracking" << std::endl;
    std::cout << "across multiple threads without any locks!" << std::endl;
    std::cout << std::endl;

    boost::asio::io_context io_context;
    Statistics stats;

    // Create lock-free expirator for multi-threaded access
    auto exp = std::make_shared<expirator::lockfree_expirator<int, RequestData>>(
        &io_context,
        [&stats](int request_id, RequestData data) {
            stats.expired_requests++;
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - data.timestamp
            );

            std::cout << "⏱️  Request " << request_id
                      << " expired after " << duration.count() << "ms "
                      << "(client: " << data.client_id << ", "
                      << "path: " << data.request_path << ")" << std::endl;
        },
        [](const boost::system::error_code& ec) {
            std::cerr << "❌ Error: " << ec.message() << std::endl;
        }
    );

    exp->start();

    std::cout << "🚀 Starting multi-threaded server simulation...\n" << std::endl;

    // I/O thread processes expirations
    std::thread io_thread([&io_context]() {
        io_context.run();
    });

    // Configuration
    const int num_request_threads = 4;
    const int num_cancel_threads = 2;
    const int requests_per_thread = 5000;

    std::atomic<int> request_id_counter{0};
    std::atomic<bool> stop_simulation{false};

    // Multiple threads generating requests (producers)
    std::vector<std::thread> request_threads;
    std::cout << "🌐 Starting " << num_request_threads << " request handler threads..." << std::endl;

    for (int t = 0; t < num_request_threads; ++t) {
        request_threads.emplace_back([&, thread_id = t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> timeout_dist(50, 500);  // 50-500ms timeout
            std::uniform_int_distribution<> bytes_dist(100, 10000);

            const std::vector<std::string> paths = {
                "/api/users", "/api/products", "/api/orders",
                "/api/search", "/api/analytics"
            };

            for (int i = 0; i < requests_per_thread; ++i) {
                if (stop_simulation.load()) break;

                int req_id = request_id_counter++;

                RequestData data{
                    "client_" + std::to_string(thread_id),
                    paths[i % paths.size()],
                    std::chrono::steady_clock::now(),
                    static_cast<size_t>(bytes_dist(gen))
                };

                // Add request with random timeout (thread-safe!)
                if (exp->add(req_id, std::chrono::milliseconds(timeout_dist(gen)), data)) {
                    stats.total_requests++;
                    stats.bytes_total += data.bytes_transferred;
                }

                // Simulate processing time
                if (i % 100 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }

    // Multiple threads cancelling requests (producers)
    std::vector<std::thread> cancel_threads;
    std::cout << "🚫 Starting " << num_cancel_threads << " request canceller threads..." << std::endl;

    for (int t = 0; t < num_cancel_threads; ++t) {
        cancel_threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> cancel_dist(0, requests_per_thread * num_request_threads);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Let some requests accumulate

            for (int i = 0; i < requests_per_thread / 10; ++i) {
                if (stop_simulation.load()) break;

                int req_id = cancel_dist(gen);

                // Cancel request (thread-safe!)
                if (exp->remove(req_id)) {
                    stats.cancelled_requests++;
                }

                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        });
    }

    // Monitor thread
    std::thread monitor([&]() {
        for (int i = 0; i < 10 && !stop_simulation.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            std::cout << "\n📊 Stats @ " << (i + 1) * 0.5 << "s:" << std::endl;
            std::cout << "   Active requests: " << exp->size() << std::endl;
            std::cout << "   Total created: " << stats.total_requests.load() << std::endl;
            std::cout << "   Expired: " << stats.expired_requests.load() << std::endl;
            std::cout << "   Cancelled: " << stats.cancelled_requests.load() << std::endl;
        }
    });

    std::cout << "\n⏳ Running simulation..." << std::endl;
    std::cout << "---" << std::endl;

    // Wait for all request threads
    for (auto& thread : request_threads) {
        thread.join();
    }

    std::cout << "\n✅ All request threads completed" << std::endl;

    // Wait for cancel threads
    for (auto& thread : cancel_threads) {
        thread.join();
    }

    std::cout << "✅ All cancel threads completed" << std::endl;

    // Wait a bit for remaining requests to expire
    std::cout << "\n⏳ Waiting for remaining requests to expire..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    stop_simulation = true;
    monitor.join();

    std::cout << "\n---" << std::endl;
    std::cout << "📊 Final Statistics:" << std::endl;
    std::cout << "   Total requests created: " << stats.total_requests.load() << std::endl;
    std::cout << "   Requests expired: " << stats.expired_requests.load() << std::endl;
    std::cout << "   Requests cancelled: " << stats.cancelled_requests.load() << std::endl;
    std::cout << "   Still active: " << exp->size() << std::endl;
    std::cout << "   Total data transferred: "
              << (stats.bytes_total.load() / 1024.0 / 1024.0) << " MB" << std::endl;

    double success_rate = 100.0 * (stats.expired_requests.load() + stats.cancelled_requests.load())
                         / stats.total_requests.load();
    std::cout << "   Success rate: " << std::fixed << std::setprecision(2)
              << success_rate << "%" << std::endl;

    // Cleanup
    exp->stop();
    io_context.stop();
    io_thread.join();

    std::cout << "\n💡 Key Takeaway:" << std::endl;
    std::cout << "   This example processed " << stats.total_requests.load()
              << " requests across" << std::endl;
    std::cout << "   " << (num_request_threads + num_cancel_threads)
              << " threads WITHOUT ANY LOCKS!" << std::endl;
    std::cout << "   Perfect for high-concurrency servers." << std::endl;
    std::cout << "\n✅ Lock-free expirator demo completed!" << std::endl;

    return 0;
}