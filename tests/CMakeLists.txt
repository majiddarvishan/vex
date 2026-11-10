# Executable smoke tests with real CTest registration and no GTest dependency.
# Legacy pool_tsan_test.cpp and prometheus_metrics_cpp/ are intentionally not
# built: they refer to removed headers/targets and are tracked as VEX-018.
if(BUILD_THREADPOOL)
    add_executable(vex_thread_pool_smoke thread_pool_smoke.cpp)
    target_link_libraries(vex_thread_pool_smoke PRIVATE Vex::thread_pool)
    add_test(NAME vex.thread_pool.smoke COMMAND vex_thread_pool_smoke)
endif()

if(BUILD_OBJECT_POOL)
    add_executable(vex_object_pool_smoke object_pool_smoke.cpp)
    target_link_libraries(vex_object_pool_smoke PRIVATE Vex::object_pool)
    add_test(NAME vex.object_pool.smoke COMMAND vex_object_pool_smoke)
endif()

if(BUILD_MONITORING)
    add_executable(vex_monitoring_smoke monitoring_smoke.cpp)
    target_link_libraries(vex_monitoring_smoke PRIVATE Vex::monitoring)
    add_test(NAME vex.monitoring.smoke COMMAND vex_monitoring_smoke)

    add_executable(vex_monitoring_contract monitoring_contract.cpp)
    target_link_libraries(vex_monitoring_contract PRIVATE Vex::monitoring)
    # A prior Clang 20 TSan run exposed -Wunused-value in SAFE_* macros.
    # Fail this regression at compile time with both GCC and Clang.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vex_monitoring_contract PRIVATE
            -Werror=unused-value)
    endif()
    add_test(NAME vex.monitoring.contract COMMAND vex_monitoring_contract)

    add_executable(vex_monitoring_decorators_contract
        monitoring_decorators_contract.cpp)
    target_link_libraries(vex_monitoring_decorators_contract
        PRIVATE Vex::monitoring)
    add_test(NAME vex.monitoring.decorators
        COMMAND vex_monitoring_decorators_contract)

    if(UNIX)
        find_program(VEX_BASH_EXECUTABLE bash)
        if(VEX_BASH_EXECUTABLE)
            add_test(NAME vex.monitoring.compiler-selection
                COMMAND "${VEX_BASH_EXECUTABLE}"
                    "${PROJECT_SOURCE_DIR}/tests/compiler_discovery_test.sh"
            )
        endif()
    endif()
endif()

# Ensure the installed CMake package can be consumed from outside this tree.
# This test runs only after the normal build, and must not use source includes.
if(VEX_EXPORT_TARGETS)
    add_test(NAME vex.package.consumer
        COMMAND ${CMAKE_COMMAND}
            "-DVEX_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DVEX_BINARY_DIR=${PROJECT_BINARY_DIR}"
            "-DVEX_GENERATOR=${CMAKE_GENERATOR}"
            "-DVEX_CONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_package_smoke.cmake"
    )
endif()
