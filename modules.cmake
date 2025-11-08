# ──────────────────────────────────────────────────────────────
# Options
# ──────────────────────────────────────────────────────────────
option(BUILD_LOGGER "Build Logger module" ON)
option(BUILD_THREADPOOL "Build ThreadPool module" ON)
option(BUILD_OBJECT_POOL "Build  ObjectPool" ON)
option(BUILD_MONITORING "Build Monitoring module" ON)
option(ENABLE_METRICS_THREADING "Enable multi-threaded metrics support" OFF)

if(BUILD_OBJECT_POOL)
    add_subdirectory(modules/object_pool)
endif()

if(ENABLE_MONITORING)
    add_subdirectory(modules/monitoring)
endif()

if(BUILD_THREADPOOL)
    add_subdirectory(modules/thread)
endif()

