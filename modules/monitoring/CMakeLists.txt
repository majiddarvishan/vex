# Monitoring is a compiled module and depends on prometheus-cpp.
set(MONITORING_SRC
    ${PROJECT_SOURCE_DIR}/src/monitoring/decorators.cpp
    ${PROJECT_SOURCE_DIR}/src/monitoring/health_check.cpp
    ${PROJECT_SOURCE_DIR}/src/monitoring/metrics.cpp
    ${PROJECT_SOURCE_DIR}/src/monitoring/metrics_manager.cpp
    ${PROJECT_SOURCE_DIR}/src/monitoring/raii_guards.cpp
)
add_library(monitoring ${MONITORING_SRC})
add_library(Vex::monitoring ALIAS monitoring)

target_include_directories(monitoring PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(monitoring PUBLIC cxx_std_17)
target_link_libraries(monitoring PUBLIC
    Threads::Threads
    prometheus-cpp::core
    prometheus-cpp::pull
)
if(ENABLE_METRICS_THREADING)
    target_compile_definitions(monitoring PUBLIC METRICS_MULTI_THREADED=1)
endif()

set_target_properties(monitoring PROPERTIES OUTPUT_NAME vex_monitoring)

# On ELF platforms a packaged shared Monitoring library must find its
# colocated prometheus-cpp shared dependencies without requiring the
# consumer to export LD_LIBRARY_PATH or modify the system loader paths.
# This is deliberately scoped to Monitoring; the other Vex modules stay
# unchanged. Installed dependencies still need to be deployed alongside
# the library (the CPM dependency's install rules do this for fetched
# shared builds).
if(UNIX AND NOT APPLE AND BUILD_SHARED_LIBS)
    set_property(TARGET monitoring APPEND PROPERTY INSTALL_RPATH "$ORIGIN")

    # When prometheus-cpp is built as a subproject (not an imported
    # system package), pull itself depends on core. DT_RUNPATH is not
    # transitive on Linux, so give pull its own sibling-library lookup.
    if(TARGET pull)
        set_property(TARGET pull APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
endif()

# Optional executable benchmark, not a correctness gate or CI assertion.
# Enable with -DVEX_BUILD_MONITORING_BENCHMARK=ON.
option(VEX_BUILD_MONITORING_BENCHMARK
    "Build repeatable Monitoring registration/scrape microbenchmark" OFF)
if(VEX_BUILD_MONITORING_BENCHMARK)
    add_executable(vex_monitoring_benchmark
        ${PROJECT_SOURCE_DIR}/tools/bench_monitoring.cpp)
    target_link_libraries(vex_monitoring_benchmark PRIVATE Vex::monitoring)
    if(UNIX)
        add_executable(vex_monitoring_load_benchmark
            ${PROJECT_SOURCE_DIR}/tools/bench_monitoring_load.cpp)
        target_link_libraries(vex_monitoring_load_benchmark PRIVATE Vex::monitoring)
    endif()
endif()
