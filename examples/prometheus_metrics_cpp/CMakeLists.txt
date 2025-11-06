# HTTP Server Example
add_executable(example_http_server example_http_server.cpp)
target_link_libraries(example_http_server PRIVATE metrics-cpp)

# Database Example
add_executable(example_database example_database.cpp)
target_link_libraries(example_database PRIVATE metrics-cpp)

# Cache Example
add_executable(example_cache example_cache.cpp)
target_link_libraries(example_cache PRIVATE
 metrics-cpp
 prometheus-cpp::pull
 )

# Worker Pool Example
add_executable(example_worker_pool example_worker_pool.cpp)
target_link_libraries(example_worker_pool PRIVATE metrics-cpp)

# Full Application Example
add_executable(example_full_app example_full_app.cpp)
target_link_libraries(example_full_app PRIVATE metrics-cpp)

# Message Queue Example
add_executable(example_message_queue example_message_queue.cpp)
target_link_libraries(example_message_queue PRIVATE metrics-cpp)