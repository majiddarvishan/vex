find_package(GTest QUIET)

if(GTest_FOUND)
    message(STATUS "Google Test found - building unit tests")

    # Test for heap expirator
    add_executable(test_heap_expirator test_heap_expirator.cpp)
    target_link_libraries(test_heap_expirator
        PRIVATE
            expirator::expirator
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME HeapExpirator COMMAND test_heap_expirator)

    # Test for timing wheel
    add_executable(test_timing_wheel test_timing_wheel.cpp)
    target_link_libraries(test_timing_wheel
        PRIVATE
            expirator::expirator
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME TimingWheel COMMAND test_timing_wheel)

    # Test for lock-free
    add_executable(test_lockfree test_lockfree.cpp)
    target_link_libraries(test_lockfree
        PRIVATE
            expirator::expirator
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME LockFree COMMAND test_lockfree)

    # Integration tests
    add_executable(test_integration test_integration.cpp)
    target_link_libraries(test_integration
        PRIVATE
            expirator::expirator
            GTest::gtest
            GTest::gtest_main
    )
    add_test(NAME Integration COMMAND test_integration)

else()
    message(STATUS "Google Test not found - building simple tests")

    # Simple test without Google Test
    add_executable(simple_test simple_test.cpp)
    target_link_libraries(simple_test PRIVATE expirator::expirator)
    add_test(NAME SimpleTest COMMAND simple_test)
endif()