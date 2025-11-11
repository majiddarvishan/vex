#include <expirator/expirator.hpp>
#include <gtest/gtest.h>

TEST(IntegrationTest, VersionInfo) {
    EXPECT_EQ(expirator::version_major, 1);
    EXPECT_EQ(expirator::version_minor, 0);
    EXPECT_EQ(expirator::version_patch, 0);
    EXPECT_STREQ(expirator::version_string(), "1.0.0");
}

TEST(IntegrationTest, AllImplementationsAvailable) {
    boost::asio::io_context io;

    // Should be able to create all three implementations
    auto heap = std::make_shared<expirator::heap_expirator<int, int>>(
        &io, [](int, int) {}
    );

    auto wheel = std::make_shared<expirator::timing_wheel_expirator<
        int, int, boost::fast_pool_allocator<char>
    >>(&io, [](int, int) {});

    auto lockfree = std::make_shared<expirator::lockfree_expirator<int, int>>(
        &io, [](int, int) {}
    );

    EXPECT_NE(heap, nullptr);
    EXPECT_NE(wheel, nullptr);
    EXPECT_NE(lockfree, nullptr);
}

TEST(IntegrationTest, PolymorphicUsage) {
    boost::asio::io_context io;
    std::atomic<int> count{0};

    std::vector<std::shared_ptr<expirator::base_expirator<int, std::string>>> expirators;

    expirators.push_back(std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io, [&count](int, std::string) { count++; }
    ));

    for (auto& exp : expirators) {
        exp->add(1, std::chrono::milliseconds(50), "test");
        EXPECT_EQ(exp->size(), 1);
        EXPECT_TRUE(exp->contains(1));
    }
}
