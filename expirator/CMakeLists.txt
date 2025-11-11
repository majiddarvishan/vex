cmake_minimum_required(VERSION 3.15)
project(expirator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(EXPIRATOR_BUILD_EXAMPLES "Build examples" ON)
option(EXPIRATOR_BUILD_BENCHMARKS "Build benchmarks" ON)
option(EXPIRATOR_BUILD_TESTS "Build tests" ON)

# Find dependencies
find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# Header-only library
add_library(expirator INTERFACE)
add_library(expirator::expirator ALIAS expirator)

target_include_directories(expirator
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(expirator
    INTERFACE
        Boost::system
        Threads::Threads
)

target_compile_features(expirator INTERFACE cxx_std_17)

# Compiler warnings
if(MSVC)
    target_compile_options(expirator INTERFACE /W4 /WX)
else()
    target_compile_options(expirator INTERFACE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>
    )
endif()

# Examples
if(EXPIRATOR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Benchmarks
if(EXPIRATOR_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Tests
if(EXPIRATOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS expirator
    EXPORT expiratorTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/expirator
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT expiratorTargets
    FILE expiratorTargets.cmake
    NAMESPACE expirator::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/expirator
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/expiratorConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/expiratorConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/expirator
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/expiratorConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/expiratorConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/expiratorConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/expirator
)

# Print summary
message(STATUS "")
message(STATUS "Expirator Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build examples: ${EXPIRATOR_BUILD_EXAMPLES}")
message(STATUS "  Build benchmarks: ${EXPIRATOR_BUILD_BENCHMARKS}")
message(STATUS "  Build tests: ${EXPIRATOR_BUILD_TESTS}")
message(STATUS "")
