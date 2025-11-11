#include <expirator/timing_wheel_expirator.hpp>
#include <iostream>
#include <string>
#include <iomanip>
#include <random>
#include <thread>

struct SessionData {
    std::string username;
    std::string email;
    int connection_id;
    std::chrono::system_clock::time_point login_time;
    std::string ip_address;
};

void print_session_info(const std::string& session_id, const SessionData& data) {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - data.login_time);

    std::cout << "  Session: " << session_id << std::endl;
    std::cout << "    User: " << data.username << " (" << data.email << ")" << std::endl;
    std::cout << "    Duration: " << duration.count() << "s" << std::endl;
    std::cout << "    IP: " << data.ip_address << std::endl;
}

int main()
{
    std::cout << "==================================================" << std::endl;
    std::cout << "    Timing Wheel Expirator - Session Manager" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "\nThis example demonstrates high-performance session" << std::endl;
    std::cout << "management using the timing wheel expirator." << std::endl;
    std::cout << "Perfect for handling millions of concurrent sessions!" << std::endl;
    std::cout << std::endl;

    boost::asio::io_context io_context;

    // Use timing wheel with memory pool for maximum performance
    auto exp = std::make_shared<expirator::timing_wheel_expirator<
        std::string,
        SessionData,
        boost::fast_pool_allocator<char>
    >>(
        &io_context,
        [](std::string session_id, SessionData data) {
            std::cout << "\n⏰ Session Expired:" << std::endl;
            print_session_info(session_id, data);
        },
        [](const boost::system::error_code& ec) {
            std::cerr << "❌ Error: " << ec.message() << std::endl;
        }
    );

    // Pre-allocate for high performance
    std::cout << "📊 Reserving space for 100,000 sessions..." << std::endl;
    exp->reserve(100000);

    // Simulate multiple users logging in
    std::vector<std::string> session_ids;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> session_time(10, 60);  // 10-60 seconds

    std::cout << "\n👥 Creating user sessions..." << std::endl;

    const std::vector<std::string> usernames = {
        "alice", "bob", "charlie", "diana", "eve",
        "frank", "grace", "henry", "iris", "jack"
    };

    const std::vector<std::string> ips = {
        "192.168.1.100", "192.168.1.101", "192.168.1.102",
        "10.0.0.50", "10.0.0.51", "172.16.0.10"
    };

    for (size_t i = 0; i < usernames.size(); ++i) {
        std::string session_id = "sess_" + std::to_string(i + 1);

        SessionData data{
            usernames[i],
            usernames[i] + "@example.com",
            static_cast<int>(i + 1),
            std::chrono::system_clock::now(),
            ips[i % ips.size()]
        };

        // Random session duration between 10-60 seconds
        int duration = session_time(gen);

        exp->add(session_id, std::chrono::seconds(duration), data);
        session_ids.push_back(session_id);

        std::cout << "  ✅ " << std::setw(10) << std::left << usernames[i]
                  << " → " << session_id
                  << " (expires in " << duration << "s)" << std::endl;
    }

    std::cout << "\n📈 Active sessions: " << exp->size() << std::endl;

    // Simulate some user activity
    std::cout << "\n🔄 Simulating user activity..." << std::endl;
    std::thread activity([&exp, &session_ids]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // User alice extends her session
        if (exp->contains(session_ids[0])) {
            std::cout << "\n⏰ User 'alice' is still active, extending session..." << std::endl;
            auto info = exp->get_info(session_ids[0]);
            if (info) {
                exp->remove(session_ids[0]);
                exp->add(session_ids[0], std::chrono::seconds(30), *info);
                std::cout << "   Session extended by 30 seconds" << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));

        // User bob logs out manually
        if (exp->remove(session_ids[1])) {
            std::cout << "\n👋 User 'bob' logged out manually" << std::endl;
        }
    });

    // Start the expirator
    std::cout << "\n🚀 Starting session manager..." << std::endl;
    std::cout << "---" << std::endl;
    exp->start();

    // Run event loop for 70 seconds to see all sessions expire
    io_context.run_for(std::chrono::seconds(70));

    activity.join();

    std::cout << "\n---" << std::endl;
    std::cout << "📊 Final Statistics:" << std::endl;
    std::cout << "   Remaining sessions: " << exp->size() << std::endl;
    std::cout << "\n✅ Session manager demo completed!" << std::endl;

    // Performance note
    std::cout << "\n💡 Performance Note:" << std::endl;
    std::cout << "   The timing wheel implementation used here can handle" << std::endl;
    std::cout << "   10+ million operations per second with O(1) complexity!" << std::endl;
    std::cout << "   Perfect for high-traffic applications." << std::endl;

    return 0;
}