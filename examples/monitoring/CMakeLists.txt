cmake_minimum_required(VERSION 3.16)
project(vex_example_monitoring)

# Force-enable monitoring for this example
set(ENABLE_MONITORING ON CACHE BOOL "Force enable monitoring for example" FORCE)

add_executable(basic_example basic_example.cpp)
add_executable(raii_example raii_example.cpp)
add_executable(health_example health_example.cpp)
add_executable(decorator_example decorator_example.cpp)
add_executable(full_example full_example.cpp)

# Make sure the monitoring target exists
if(NOT TARGET monitoring)
    add_subdirectory(${CMAKE_SOURCE_DIR}/modules/monitoring ${CMAKE_BINARY_DIR}/modules/monitoring)
endif()

# Link against the monitoring target if available
if(TARGET Vex::monitoring)
    target_link_libraries(basic_example PRIVATE Vex::monitoring)
    target_link_libraries(raii_example PRIVATE Vex::monitoring)
    target_link_libraries(health_example PRIVATE Vex::monitoring)
    target_link_libraries(decorator_example PRIVATE Vex::monitoring)
    target_link_libraries(full_example PRIVATE Vex::monitoring)
else()
    message(WARNING "Vex::monitoring target not found; the example may not link")
endif()

target_include_directories(basic_example PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(raii_example PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(health_example PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(decorator_example PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(full_example PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
