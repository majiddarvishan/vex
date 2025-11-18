add_library(object_pool INTERFACE)

target_include_directories(object_pool
    INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(object_pool INTERFACE cxx_std_17)

add_library(Vex::object_pool ALIAS object_pool)
