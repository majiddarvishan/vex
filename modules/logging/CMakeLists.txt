# logging is header-only (INTERFACE)
add_library(logging INTERFACE)
target_include_directories(logging INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_compile_features(logging INTERFACE cxx_std_17)

add_library(Vex::logging ALIAS logging)

