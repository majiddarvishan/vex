cmake_minimum_required(VERSION 3.16)
project(vex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Option to control building shared vs static libraries.
# If ENABLE_SHARED is ON, we force BUILD_SHARED_LIBS to ON so add_library()
# follow that default. If OFF, libraries will be static (unless BUILD_SHARED_LIBS
# already set by user).
option(ENABLE_SHARED "Build libraries as shared (sets BUILD_SHARED_LIBS)" OFF)

if(ENABLE_SHARED)
    # Force BUILD_SHARED_LIBS on, as a cache variable so it propagates to subdirs
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
else()
    # If user explicitly turned ENABLE_SHARED off, ensure BUILD_SHARED_LIBS is off
    # only if it wasn't set by user on the command line.
    if(NOT DEFINED BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
    endif()
endif()


include(GNUInstallDirs)


# include(FetchContent)
# FetchContent_Declare(
#     CPM
#     GIT_REPOSITORY https://github.com/cpm-cmake/CPM.cmake.git
#     GIT_TAG origin/master
# )
# FetchContent_MakeAvailable(CPM)

include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

# Headers available to consumers
include_directories(${CMAKE_SOURCE_DIR}/include)

# Load module options and add_subdirectory calls
include(${CMAKE_SOURCE_DIR}/modules.cmake)

# Collect targets for install/export
set(VEX_EXPORT_TARGETS "")

if(TARGET logging)
    list(APPEND VEX_EXPORT_TARGETS logging)
endif()
if(TARGET monitoring)
    list(APPEND VEX_EXPORT_TARGETS monitoring)
endif()
if(TARGET object_pool)
    list(APPEND VEX_EXPORT_TARGETS object_pool)
endif()
if(TARGET thread_pool)
    list(APPEND VEX_EXPORT_TARGETS thread_pool)
endif()

# Install targets and headers
if(VEX_EXPORT_TARGETS)
    install(
        TARGETS ${VEX_EXPORT_TARGETS}
        EXPORT VexTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Export the targets for find_package
install(
    EXPORT VexTargets
    FILE VexTargets.cmake
    NAMESPACE Vex::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vex
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/VexConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/VexConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vex
)

install(
    FILES "${CMAKE_BINARY_DIR}/VexConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vex
)

option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
