#include <expirator/timing_wheel_expirator.hpp>
#include <gtest/gtest.h>

class TimingWheelTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context;
    std::atomic<int> expired_count{0};

    void SetUp() override {
        expired_count = 0;
    }
};

TEST_F(TimingWheelTest, BasicOperation) {
    auto exp = std::make_shared<expirator::timing_wheel_expirator<
        int, std::string, boost::fast_pool_allocator<char>
    >>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    exp->add(1, std::chrono::milliseconds(50), "test");
    EXPECT_EQ(exp->size(), 1);

    exp->start();
    io_context.run_for(std::chrono::milliseconds(100));

    EXPECT_EQ(expired_count, 1);
}

TEST_F(TimingWheelTest, MultipleWheelLevels) {
    auto exp = std::make_shared<expirator::timing_wheel_expirator<
        int, std::string, boost::fast_pool_allocator<char>
    >>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    // Different time ranges to test different wheel levels
    exp->add(1, std::chrono::milliseconds(50), "wheel0");    // Wheel 0: < 256ms
    exp->add(2, std::chrono::seconds(1), "wheel1");          // Wheel 1: < 16s
    exp->add(3, std::chrono::seconds(30), "wheel2");         // Wheel 2: < 17min

    EXPECT_EQ(exp->size(), 3);
}

TEST_F(TimingWheelTest, HighFrequency) {
    auto exp = std::make_shared<expirator::timing_wheel_expirator<
        int, std::string, boost::fast_pool_allocator<char>
    >>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    exp->reserve(100000);

    for (int i = 0; i < 10000; ++i) {
        exp->add(i, std::chrono::milliseconds(100), "data");
    }

    EXPECT_EQ(exp->size(), 10000);
}