# object_pool is header-only (INTERFACE)
add_library(object_pool INTERFACE)
target_include_directories(object_pool INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_compile_features(object_pool INTERFACE cxx_std_17)

add_library(Vex::object_pool ALIAS object_pool)