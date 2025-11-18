cmake_minimum_required(VERSION 3.16)
project(vex_example_networking)

# Force-enable networking for this example
set(ENABLE_NETWORKING ON CACHE BOOL "Force enable networking for example" FORCE)

add_compile_options(-O0 -g3)

include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

CPMAddPackage(
    NAME fmt
    VERSION 10.2.1
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 10.2.1
    OPTIONS
        "FMT_INSTALL OFF"
        "FMT_TEST OFF"
        "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}"
)

add_executable(example_simple_client simple_client.cpp)
add_executable(example_simple_server simple_server.cpp)
add_executable(example_p_client p_client_example.cpp)
add_executable(example_p_server p_server_example.cpp)
add_executable(example_p_client_list p_client_list_example.cpp)
add_executable(example_advanced_session advanced_session.cpp)
add_executable(echo_server echo_server.cpp)
add_executable(echo_client echo_client.cpp)

# Make sure the networking target exists
if(NOT TARGET networking)
    add_subdirectory(${CMAKE_SOURCE_DIR}/modules/networking ${CMAKE_BINARY_DIR}/modules/networking)
endif()

# Link against the networking target if available
if(TARGET Vex::networking)
    target_link_libraries(example_simple_client PRIVATE Vex::networking fmt)
    target_link_libraries(example_simple_server PRIVATE Vex::networking fmt)
    target_link_libraries(example_p_client PRIVATE Vex::networking fmt)
    target_link_libraries(example_p_server PRIVATE Vex::networking fmt)
    target_link_libraries(example_p_client_list PRIVATE Vex::networking fmt)
    target_link_libraries(example_advanced_session PRIVATE Vex::networking fmt)
    target_link_libraries(echo_server PRIVATE Vex::networking fmt)
    target_link_libraries(echo_client PRIVATE Vex::networking fmt)
else()
    message(WARNING "Vex::networking target not found; the example may not link")
endif()

target_include_directories(example_simple_client PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(example_simple_server PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(example_p_client PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(example_p_server PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(example_p_client_list PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(example_advanced_session PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(echo_server PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_include_directories(echo_client PRIVATE ${CMAKE_SOURCE_DIR}/../../include)