#include <expirator/heap_expirator.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <atomic>

class HeapExpiratorTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context;
    std::atomic<int> expired_count{0};
    std::vector<int> expired_keys;

    void SetUp() override {
        expired_count = 0;
        expired_keys.clear();
    }
};

TEST_F(HeapExpiratorTest, BasicAddAndExpire) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [this](int key, std::string) {
            expired_count++;
            expired_keys.push_back(key);
        }
    );

    exp->add(1, std::chrono::milliseconds(50), "first");
    exp->add(2, std::chrono::milliseconds(100), "second");

    EXPECT_EQ(exp->size(), 2);
    EXPECT_TRUE(exp->contains(1));
    EXPECT_TRUE(exp->contains(2));

    exp->start();
    io_context.run();

    EXPECT_EQ(expired_count, 2);
    EXPECT_EQ(exp->size(), 0);
}

TEST_F(HeapExpiratorTest, ExpirationOrder) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [this](int key, std::string) {
            expired_keys.push_back(key);
        }
    );

    // Add in reverse order, should expire in correct order
    exp->add(3, std::chrono::milliseconds(150), "third");
    exp->add(2, std::chrono::milliseconds(100), "second");
    exp->add(1, std::chrono::milliseconds(50), "first");

    exp->start();
    io_context.run();

    ASSERT_EQ(expired_keys.size(), 3);
    EXPECT_EQ(expired_keys[0], 1);
    EXPECT_EQ(expired_keys[1], 2);
    EXPECT_EQ(expired_keys[2], 3);
}

TEST_F(HeapExpiratorTest, RemoveBeforeExpiry) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [this](int key, std::string) {
            expired_count++;
        }
    );

    exp->add(1, std::chrono::seconds(10), "first");
    exp->add(2, std::chrono::seconds(10), "second");

    EXPECT_TRUE(exp->remove(1));
    EXPECT_FALSE(exp->contains(1));
    EXPECT_TRUE(exp->contains(2));
    EXPECT_EQ(exp->size(), 1);
}

TEST_F(HeapExpiratorTest, DuplicateKey) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [](int, std::string) {}
    );

    EXPECT_TRUE(exp->add(1, std::chrono::seconds(1), "first"));
    EXPECT_FALSE(exp->add(1, std::chrono::seconds(1), "duplicate"));
    EXPECT_EQ(exp->size(), 1);
}

TEST_F(HeapExpiratorTest, GetInfo) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [](int, std::string) {}
    );

    exp->add(1, std::chrono::seconds(10), "test_info");

    auto info = exp->get_info(1);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(*info, "test_info");

    auto missing = exp->get_info(999);
    EXPECT_FALSE(missing.has_value());
}

TEST_F(HeapExpiratorTest, GetRemainingTime) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [](int, std::string) {}
    );

    exp->add(1, std::chrono::seconds(10), "test");

    auto remaining = exp->get_remaining_time(1);
    ASSERT_TRUE(remaining.has_value());
    EXPECT_GT(*remaining, std::chrono::seconds(9));
    EXPECT_LE(*remaining, std::chrono::seconds(10));
}

TEST_F(HeapExpiratorTest, UpdateExpiry) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [this](int key, std::string) {
            expired_keys.push_back(key);
        }
    );

    exp->add(1, std::chrono::milliseconds(50), "first");
    exp->add(2, std::chrono::milliseconds(100), "second");

    // Extend key 1's expiry so it expires after key 2
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    exp->update_expiry(1, std::chrono::milliseconds(100));

    exp->start();
    io_context.run();

    ASSERT_EQ(expired_keys.size(), 2);
    EXPECT_EQ(expired_keys[0], 2);  // Key 2 should expire first now
    EXPECT_EQ(expired_keys[1], 1);
}

TEST_F(HeapExpiratorTest, Clear) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    exp->add(1, std::chrono::seconds(10), "first");
    exp->add(2, std::chrono::seconds(10), "second");
    exp->add(3, std::chrono::seconds(10), "third");

    EXPECT_EQ(exp->size(), 3);
    exp->clear();
    EXPECT_EQ(exp->size(), 0);
    EXPECT_EQ(expired_count, 0);  // Clear doesn't trigger expiration
}

TEST_F(HeapExpiratorTest, StopAndRestart) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    exp->add(1, std::chrono::milliseconds(50), "first");
    exp->start();

    EXPECT_TRUE(exp->is_running());
    exp->stop();
    EXPECT_FALSE(exp->is_running());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(expired_count, 0);  // Shouldn't expire while stopped
}

TEST_F(HeapExpiratorTest, LargeScale) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    const int count = 10000;
    for (int i = 0; i < count; ++i) {
        exp->add(i, std::chrono::milliseconds(100 + (i % 1000)), "data");
    }

    EXPECT_EQ(exp->size(), count);

    exp->start();
    io_context.run();

    EXPECT_EQ(expired_count, count);
    EXPECT_EQ(exp->size(), 0);
}

TEST_F(HeapExpiratorTest, ImmediateExpiry) {
    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io_context,
        [this](int, std::string) {
            expired_count++;
        }
    );

    exp->add(1, std::chrono::milliseconds(0), "immediate");
    exp->add(2, std::chrono::milliseconds(-100), "past");  // Already expired

    exp->start();
    io_context.run();

    EXPECT_EQ(expired_count, 2);
}