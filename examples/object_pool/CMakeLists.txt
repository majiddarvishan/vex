cmake_minimum_required(VERSION 3.16)
project(vex_example_object_pool)

include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

CPMAddPackage(
    NAME prometheus-cpp
    GITHUB_REPOSITORY jupp0r/prometheus-cpp
    VERSION 1.3.0
    OPTIONS
        "ENABLE_TESTING OFF"
        "OVERRIDE_CXX_STANDARD_FLAGS OFF"
        "GENERATE_PKGCONFIG OFF"
        "USE_THIRDPARTY_LIBRARIES ON"
        "ENABLE_PUSH=OFF"
        "ENABLE_COMPRESSION=OFF"
        "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}"
)
if(TARGET Vex::object_pool)
    add_executable(example_object_pool example_object_pool.cpp)
    target_link_libraries(example_object_pool
            PRIVATE
                Vex::object_pool

                prometheus-cpp::core
                prometheus-cpp::pull
    )
    target_include_directories(example_object_pool PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
else()
    message(STATUS "Vex::object_pool target not found; skipping object_pool example")
endif()
