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
endif()
