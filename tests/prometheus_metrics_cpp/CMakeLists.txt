# Find GTest
find_package(GTest REQUIRED)
include(GoogleTest)

# Test executable
add_executable(metrics_tests
    test_main.cpp
    test_metrics_manager.cpp
    test_raii_guards.cpp
    test_decorators.cpp
    test_health_check.cpp
)

target_link_libraries(metrics_tests PRIVATE
    metrics-cpp
    GTest::GTest
    GTest::Main
)

# Discover tests
gtest_discover_tests(metrics_tests)