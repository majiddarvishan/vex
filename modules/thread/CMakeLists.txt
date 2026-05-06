# thread_pool is header-only (INTERFACE)
add_library(thread_pool INTERFACE)
target_include_directories(thread_pool INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(thread_pool INTERFACE cxx_std_17)

add_library(Vex::thread_pool ALIAS thread_pool)
