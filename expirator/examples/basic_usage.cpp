#include <expirator/heap_expirator.hpp>
#include <iostream>
#include <string>

int main()
{
    boost::asio::io_context io_context;

    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [](int key, std::string info) {
            std::cout << "⏱️  Expired key " << key << " with info: " << info << std::endl;
        },
        [](const boost::system::error_code& ec) {
            std::cerr << "❌ Error: " << ec.message() << std::endl;
        }
    );

    std::cout << "Adding entries..." << std::endl;
    exp->add(1, std::chrono::seconds(2), "First entry - 2 seconds");
    exp->add(2, std::chrono::seconds(5), "Second entry - 5 seconds");
    exp->add(3, std::chrono::seconds(1), "Quick entry - 1 second");

    std::cout << "✅ Added " << exp->size() << " entries" << std::endl;
    std::cout << "🚀 Starting expirator..." << std::endl << std::endl;

    exp->start();
    io_context.run();

    std::cout << std::endl << "✅ All entries expired" << std::endl;
    return 0;
}