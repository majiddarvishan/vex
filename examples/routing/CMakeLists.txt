cmake_minimum_required(VERSION 3.16)
project(vex_example_routing)

# Force-enable routing for this example
set(ENABLE_ROUTING ON CACHE BOOL "Force enable routing for example" FORCE)

add_executable(routing_example main.cpp)

# Make sure the routing target exists
if(NOT TARGET routing)
    add_subdirectory(${CMAKE_SOURCE_DIR}/modules/routing ${CMAKE_BINARY_DIR}/modules/routing)
endif()

# Link against the routing target if available
if(TARGET Vex::routing)
    target_link_libraries(routing_example PRIVATE Vex::routing)
else()
    message(WARNING "Vex::routing target not found; the example may not link")
endif()

target_include_directories(routing_example PRIVATE ${CMAKE_SOURCE_DIR}/../../include)