# routing module
file(GLOB ROUTING_SRC ${CMAKE_SOURCE_DIR}/src/routing/*.cpp)
file(GLOB ROUTING_HDR
    ${CMAKE_SOURCE_DIR}/include/vex/routing/*.h
    ${CMAKE_SOURCE_DIR}/include/vex/routing/*.hpp
    )

add_library(routing ${ROUTING_SRC} ${ROUTING_HDR})
add_library(Vex::routing ALIAS routing)

target_include_directories(routing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(routing PUBLIC cxx_std_17)

target_link_libraries(routing
    PUBLIC
        Threads::Threads
)

set_target_properties(routing PROPERTIES OUTPUT_NAME "vex_routing")
