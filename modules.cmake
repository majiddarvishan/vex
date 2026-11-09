# This is the single source of truth for Vex module options.
# The legacy BUILD_LOGGER option was silently ignored. It remains reserved,
# but the missing logging.hpp / pa::config.hpp integration is not ready.
option(BUILD_LOGGER "Build SegmentedLogger (currently unsupported)" OFF)
option(BUILD_THREADPOOL "Build ThreadPool module" ON)
option(BUILD_OBJECT_POOL "Build ObjectPool module" ON)
option(BUILD_MONITORING "Build Monitoring module" ON)
option(ENABLE_METRICS_THREADING "Compile monitoring with internal mutex guards" OFF)
option(VEX_ALLOW_DEPENDENCY_FETCH
    "Allow CMake to fetch prometheus-cpp if no installed package is found" ON)

if(BUILD_LOGGER)
    message(FATAL_ERROR
        "BUILD_LOGGER is not supported yet: SegmentedLogger requires "
        "missing logging.hpp and pa/config.hpp integration. "
        "Use BUILD_LOGGER=OFF until that module is repaired.")
endif()

if(BUILD_OBJECT_POOL OR BUILD_MONITORING)
    include(${PROJECT_SOURCE_DIR}/cmake/VexDependencies.cmake)
endif()

if(BUILD_THREADPOOL OR BUILD_OBJECT_POOL)
    find_package(Threads REQUIRED)
endif()

if(BUILD_OBJECT_POOL)
    add_subdirectory(modules/object_pool)
endif()

if(BUILD_MONITORING)
    add_subdirectory(modules/monitoring)
endif()

if(BUILD_THREADPOOL)
    add_subdirectory(modules/thread)
endif()
