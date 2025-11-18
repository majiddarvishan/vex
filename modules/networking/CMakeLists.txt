# Threading mode
if(NETWORKING_MULTI_THREADED)
    add_compile_definitions(NETWORKING_MULTI_THREADED)
    message(STATUS "Networking: Multi-threaded mode enabled")
else()
    message(STATUS "Networking: Single-threaded mode enabled (default)")
endif()

add_library(networking INTERFACE)

target_include_directories(networking
    INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(networking INTERFACE cxx_std_20)

add_library(Vex::networking ALIAS networking)
