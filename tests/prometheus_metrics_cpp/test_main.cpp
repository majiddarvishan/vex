#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
EOFTEST

cat > $PROJECT_NAME / tests / test_metrics.cpp << 'EOFTEST2'
#include <gtest/gtest.h>
#include <metrics/monitoring.hpp>

        using namespace metrics;

TEST(MetricsManager, Initialization)
{
    MetricsManager::Reset();
    EXPECT_FALSE(MetricsManager::IsInitialized());

    bool result = MetricsManager::Init();
    EXPECT_TRUE(result);
    EXPECT_TRUE(MetricsManager::IsInitialized());

    // Second init should return false
    result = MetricsManager::Init();
    EXPECT_FALSE(result);
}

TEST(MetricsManager, CreateCounter)
{
    MetricsManager::Reset();
    MetricsManager::Init();

    auto registry = MetricsManager::GetRegistry();
    auto& counter = MetricsManager::
      CreateCounter(registry, "test_counter", "Test", {});

    counter.Increment();
    EXPECT_EQ(counter.Value(), 1.0);
}