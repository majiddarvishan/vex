#include <expirator/lockfree_expirator.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

class LockFreeTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context;
    std::atomic<int> expired_count{0};

    void SetUp() override {
        expired_count = 0;
    }
};

TEST_F(LockFreeTest, BasicThreadSafety) {
    auto exp = std::make_shared<expirator::lockfree_expirator<int, std::string>>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    exp->start();

    // Multiple threads adding entries
    std::vector<std::thread> threads;
    const int threads_count = 4;
    const int entries_per_thread = 1000;

    for (int t = 0; t < threads_count; ++t) {
        threads.emplace_back([exp, t, entries_per_thread]() {
            for (int i = 0; i < entries_per_thread; ++i) {
                int key = t * entries_per_thread + i;
                exp->add(key, std::chrono::milliseconds(100), "data");
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have all entries
    EXPECT_EQ(exp->size(), threads_count * entries_per_thread);
}

TEST_F(LockFreeTest, ConcurrentAddRemove) {
    auto exp = std::make_shared<expirator::lockfree_expirator<int, std::string>>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    exp->start();

    std::thread adder([exp]() {
        for (int i = 0; i < 5000; ++i) {
            exp->add(i, std::chrono::seconds(10), "data");
        }
    });

    std::thread remover([exp]() {
        for (int i = 0; i < 2500; ++i) {
            exp->remove(i * 2);
        }
    });

    adder.join();
    remover.join();

    // Should have approximately half the entries
    EXPECT_GT(exp->size(), 2000);
    EXPECT_LT(exp->size(), 3000);
}