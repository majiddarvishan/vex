# Basic usage example
add_executable(basic_usage basic_usage.cpp)
target_link_libraries(basic_usage PRIVATE expirator::expirator)

# Timing wheel example
add_executable(timing_wheel_example timing_wheel_example.cpp)
target_link_libraries(timing_wheel_example PRIVATE expirator::expirator)

# Lock-free example
add_executable(lockfree_example lockfree_example.cpp)
target_link_libraries(lockfree_example PRIVATE expirator::expirator)

# Advanced patterns
add_executable(advanced_patterns advanced_patterns.cpp)
target_link_libraries(advanced_patterns PRIVATE expirator::expirator)

# Production scenarios
add_executable(production_scenarios production_scenarios.cpp)
target_link_libraries(production_scenarios PRIVATE expirator::expirator)
