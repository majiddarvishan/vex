cmake_minimum_required(VERSION 3.16)
project(vex
        VERSION 0.0.1
        DESCRIPTION "Fast. Simple. Professional C++ utility library"
        LANGUAGES CXX
)

set(VEX_VERSION_MAJOR ${vex_VERSION_MAJOR})
set(VEX_VERSION_MINOR ${vex_VERSION_MINOR})
set(VEX_VERSION_PATCH ${vex_VERSION_PATCH})
set(VEX_VERSION_TAG   "")

# autimatially generate version.hpp
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in
    ${CMAKE_CURRENT_SOURCE_DIR}/include/vex/version.hpp
    @ONLY
)

# ──────────────────────────────────────────────────────────────
# Options
# ──────────────────────────────────────────────────────────────
option(VEX_VEX_BUILD_TESTS    "Build tests" ON)
option(VEX_VEX_BUILD_EXAMPLES "Build examples" ON)
option(VEX_ENABLE_ASAN    "Enable AddressSanitizer" OFF)
option(VEX_ENABLE_UBSAN   "Enable UndefinedBehaviorSanitizer" OFF)

# ──────────────────────────────────────────────────────────────
# C++ Standard
# ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ──────────────────────────────────────────────────────────────
# Compiler warnings (GCC/Clang/MSVC)
# ──────────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion
        -Wformat=2
    )
elseif(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# ──────────────────────────────────────────────────────────────
# Sanitizers
# ──────────────────────────────────────────────────────────────
if(VEX_ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()
if(VEX_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

# ──────────────────────────────────────────────────────────────
# Module build options
# ──────────────────────────────────────────────────────────────
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_LOGGER "Build Logger module" ON)
option(BUILD_THREADPOOL "Build ThreadPool module" ON)
option(BUILD_NETWORKING "Build Networking module" ON)
option(BUILD_OBJECT_POOL "Build ObjectPool module" ON)
option(VEX_BUILD_TESTS "Build tests" ON)
option(VEX_BUILD_EXAMPLES "Build examples" ON)

# ──────────────────────────────────────────────────────────────
# Collect sources
# ──────────────────────────────────────────────────────────────
set(VEX_SOURCES "")

if(BUILD_LOGGER)
    list(APPEND VEX_SOURCES src/logger.cpp)
endif()

if(BUILD_THREADPOOL)
    list(APPEND VEX_SOURCES src/threadpool.cpp)
endif()

if(BUILD_NETWORKING)
    list(APPEND VEX_SOURCES src/networking.cpp)
endif()

# ──────────────────────────────────────────────────────────────
# Build vex library
# ──────────────────────────────────────────────────────────────
if(NOT VEX_SOURCES)
# ──────────────────────────────────────────────────────────────
# Main library (header-only where possible)
# ──────────────────────────────────────────────────────────────
    # Create interface library if no sources (header-only mode)
    message(STATUS "Building vax as header-only library")
    add_library(vex INTERFACE)
    target_include_directories(vex INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Define macros for conditional compilation
    if(BUILD_LOGGER)
        target_compile_definitions(vex INTERFACE BUILD_LOGGER)
    endif()
    if(BUILD_THREADPOOL)
        target_compile_definitions(vex INTERFACE BUILD_THREADPOOL)
    endif()
    if(BUILD_NETWORKING)
        target_compile_definitions(vex INTERFACE BUILD_NETWORKING)
    endif()
    if(BUILD_OBJECT_POOL)
        target_compile_definitions(vex INTERFACE BUILD_OBJECT_POOL)
    endif()
else()
    # Build normal library
    add_library(vex ${VEX_SOURCES})

    target_include_directories(vex PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Define macros for conditional compilation
    if(BUILD_LOGGER)
        target_compile_definitions(vex PUBLIC BUILD_LOGGER)
    endif()
    if(BUILD_THREADPOOL)
        target_compile_definitions(vex PUBLIC BUILD_THREADPOOL)
    endif()
    if(BUILD_NETWORKING)
        target_compile_definitions(vex PUBLIC BUILD_NETWORKING)
    endif()
    if(BUILD_OBJECT_POOL)
        target_compile_definitions(vex PUBLIC BUILD_OBJECT_POOL)
    endif()

    # Platform-specific linking
    if(BUILD_NETWORKING)
        if(WIN32)
            target_link_libraries(vex PRIVATE ws2_32 wsock32)
        elseif(UNIX AND NOT APPLE)
            target_link_libraries(vex PRIVATE pthread)
        endif()
    endif()

    if(BUILD_THREADPOOL)
        if(UNIX)
            target_link_libraries(vex PRIVATE pthread)
        endif()
    endif()

    # Compiler warnings
    if(MSVC)
        target_compile_options(vex PRIVATE /W4 /permissive-)
    else()
        target_compile_options(vex PRIVATE
            -Wall -Wextra -Wpedantic
            $<$<CONFIG:Debug>:-g -O0>
            $<$<CONFIG:Release>:-O3>
        )
    endif()
endif()

# Add namespace alias for better practice
add_library(vex::vex ALIAS vex)

# Set library properties
set_target_properties(vex PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME vex
)

# ──────────────────────────────────────────────────────────────
# Examples
# ──────────────────────────────────────────────────────────────
if(VEX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()


# ──────────────────────────────────────────────────────────────
# Tests
# ──────────────────────────────────────────────────────────────
if(VEX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ──────────────────────────────────────────────────────────────
# Installation
# ──────────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library
install(TARGETS vex
    EXPORT vex-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install CMake targets
install(EXPORT vex-targets
    FILE vex-targets.cmake
    NAMESPACE vex::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vex
)

# Create package config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vexConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/vexConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vex
)

# Create package version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/vexConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Install package config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/vexConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/vexConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vex
)

# Export from build tree (optional, for developers)
export(EXPORT vex-targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/vex-targets.cmake"
    NAMESPACE vex::
)

# Print configuration summary
message(STATUS "")
message(STATUS "vex Configuration:")
message(STATUS "  Version:         ${PROJECT_VERSION}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libs:     ${BUILD_SHARED_LIBS}")
message(STATUS "  Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Modules:")
message(STATUS "  Logger:          ${BUILD_LOGGER}")
message(STATUS "  ThreadPool:      ${BUILD_THREADPOOL}")
message(STATUS "  Networking:      ${BUILD_NETWORKING}")
message(STATUS "  ObjectPool:      ${BUILD_OBJECT_POOL}")
message(STATUS "")
message(STATUS "Build options:")
message(STATUS "  Tests:           ${VEX_BUILD_TESTS}")
message(STATUS "  Examples:        ${VEX_BUILD_EXAMPLES}")
message(STATUS "")

# ──────────────────────────────────────────────────────────────
# CPack (for packaging)
# ──────────────────────────────────────────────────────────────
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "vex")
set(CPACK_PACKAGE_VENDOR "Majid Darvishan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern C++ utility library")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/vex-logo.png")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "TGZ;ZIP")
include(CPack)