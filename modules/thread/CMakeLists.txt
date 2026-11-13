add_library(thread_pool INTERFACE)
add_library(Vex::thread_pool ALIAS thread_pool)
target_include_directories(thread_pool INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(thread_pool INTERFACE cxx_std_17)
target_link_libraries(thread_pool INTERFACE Threads::Threads)
