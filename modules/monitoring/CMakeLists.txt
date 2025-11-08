# monitoring module (depends on prometheus-cpp)
include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

CPMAddPackage(
    NAME prometheus-cpp
    GITHUB_REPOSITORY jupp0r/prometheus-cpp
    VERSION 1.3.0
    OPTIONS
        "ENABLE_TESTING OFF"
        "OVERRIDE_CXX_STANDARD_FLAGS OFF"
        "GENERATE_PKGCONFIG OFF"
        "USE_THIRDPARTY_LIBRARIES ON"
        "ENABLE_PUSH=OFF"
        "ENABLE_COMPRESSION=OFF"
        "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}"
)

file(GLOB MONITORING_SRC ${CMAKE_SOURCE_DIR}/src/monitoring/*.cpp)
file(GLOB MONITORING_HDR ${CMAKE_SOURCE_DIR}/include/vex/monitoring/*.h ${CMAKE_SOURCE_DIR}/include/vex/monitoring/*.hpp)

add_library(monitoring ${MONITORING_SRC} ${MONITORING_HDR})
add_library(Vex::monitoring ALIAS monitoring)

target_include_directories(monitoring PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_features(monitoring PUBLIC cxx_std_17)

target_link_libraries(monitoring
    PUBLIC
        prometheus-cpp::core
        prometheus-cpp::pull
)

set_target_properties(monitoring PROPERTIES OUTPUT_NAME "vex_monitoring")
