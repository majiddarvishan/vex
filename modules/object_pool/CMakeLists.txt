# ObjectPool is header-only but its public API currently uses Prometheus types.
add_library(object_pool INTERFACE)
add_library(Vex::object_pool ALIAS object_pool)
target_include_directories(object_pool INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(object_pool INTERFACE cxx_std_17)
target_link_libraries(object_pool INTERFACE
    Threads::Threads
    prometheus-cpp::core
    prometheus-cpp::pull
)
