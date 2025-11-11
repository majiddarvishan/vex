// ============================================================================
// No Google Test
// ============================================================================

#include <expirator/heap_expirator.hpp>
#include <expirator/timing_wheel_expirator.hpp>
#include <expirator/lockfree_expirator.hpp>
#include <iostream>
#include <cassert>

void test_basic_functionality() {
    std::cout << "Testing basic functionality..." << std::endl;

    boost::asio::io_context io;
    int expired = 0;

    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io,
        [&expired](int, std::string) { expired++; }
    );

    exp->add(1, std::chrono::milliseconds(50), "test");
    assert(exp->size() == 1);
    assert(exp->contains(1));

    exp->start();
    io.run();

    assert(expired == 1);
    assert(exp->size() == 0);

    std::cout << "✅ Basic functionality test passed" << std::endl;
}

void test_removal() {
    std::cout << "Testing removal..." << std::endl;

    boost::asio::io_context io;

    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io,
        [](int, std::string) {}
    );

    exp->add(1, std::chrono::seconds(10), "test");
    assert(exp->size() == 1);

    assert(exp->remove(1));
    assert(exp->size() == 0);
    assert(!exp->contains(1));

    std::cout << "✅ Removal test passed" << std::endl;
}

void test_multiple_implementations() {
    std::cout << "Testing multiple implementations..." << std::endl;

    boost::asio::io_context io;

    auto heap = std::make_shared<expirator::heap_expirator<int, int>>(
        &io, [](int, int) {}
    );

    auto wheel = std::make_shared<expirator::timing_wheel_expirator<
        int, int, boost::fast_pool_allocator<char>
    >>(&io, [](int, int) {});

    auto lockfree = std::make_shared<expirator::lockfree_expirator<int, int>>(
        &io, [](int, int) {}
    );

    assert(heap != nullptr);
    assert(wheel != nullptr);
    assert(lockfree != nullptr);

    std::cout << "✅ Multiple implementations test passed" << std::endl;
}

int main() {
    std::cout << "Running simple tests..." << std::endl << std::endl;

    try {
        test_basic_functionality();
        test_removal();
        test_multiple_implementations();

        std::cout << std::endl << "✅ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}