cmake_minimum_required(VERSION 3.16)
project(vex VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_SHARED "Build Vex compiled libraries as shared libraries" OFF)
if(ENABLE_SHARED)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
elseif(NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
endif()

include(GNUInstallDirs)
include(CTest)

# Declare options, resolve module dependencies, and create enabled targets.
include(${CMAKE_CURRENT_SOURCE_DIR}/modules.cmake)

set(VEX_EXPORT_TARGETS "")
foreach(vex_target IN ITEMS logging monitoring object_pool thread_pool)
    if(TARGET ${vex_target})
        list(APPEND VEX_EXPORT_TARGETS ${vex_target})
    endif()
endforeach()

if(VEX_EXPORT_TARGETS)
    install(
        TARGETS ${VEX_EXPORT_TARGETS}
        EXPORT VexTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install only public headers for modules supported by this build.
    install(FILES include/vex/version.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vex)
    if(BUILD_THREADPOOL)
        install(DIRECTORY include/vex/thread/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vex/thread)
    endif()
    if(BUILD_OBJECT_POOL)
        install(DIRECTORY include/vex/object_pool/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vex/object_pool)
    endif()
    if(BUILD_MONITORING)
        install(DIRECTORY include/vex/monitoring/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vex/monitoring)
    endif()

    install(
        EXPORT VexTargets
        FILE VexTargets.cmake
        NAMESPACE Vex::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Vex
    )

    set(VEX_NEEDS_THREADS OFF)
    if(BUILD_THREADPOOL OR BUILD_OBJECT_POOL)
        set(VEX_NEEDS_THREADS ON)
    endif()
    set(VEX_NEEDS_PROMETHEUS OFF)
    if(BUILD_MONITORING OR BUILD_OBJECT_POOL)
        set(VEX_NEEDS_PROMETHEUS ON)
    endif()

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/VexConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/VexConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Vex
    )
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/VexConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(
        FILES
            "${PROJECT_BINARY_DIR}/VexConfig.cmake"
            "${PROJECT_BINARY_DIR}/VexConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Vex
    )
endif()

option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
