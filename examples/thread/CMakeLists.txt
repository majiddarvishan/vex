cmake_minimum_required(VERSION 3.16)
project(vex_example_thread_pool)

add_executable(example_thread_pool example_thread_pool.cpp)

# if(NOT TARGET monitoring)
#     add_subdirectory(${CMAKE_SOURCE_DIR}/modules/monitoring ${CMAKE_BINARY_DIR}/modules/monitoring)
# endif()

# Link against the monitoring target if available
# if(TARGET Vex::monitoring)
#     target_link_libraries(basic_example PRIVATE Vex::monitoring)
# else()
#     message(WARNING "Vex::monitoring target not found; the example may not link")
# endif()

target_include_directories(example_thread_pool PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
