# This script is executed by CTest, not during configuration.
if(NOT VEX_BINARY_DIR OR NOT VEX_SOURCE_DIR OR NOT VEX_GENERATOR)
    message(FATAL_ERROR "Missing Vex package-smoke configuration")
endif()

set(prefix "${VEX_BINARY_DIR}/_package_smoke/install")
set(consumer_build "${VEX_BINARY_DIR}/_package_smoke/consumer")
set(install_cmd "${CMAKE_COMMAND}" --install "${VEX_BINARY_DIR}" --prefix "${prefix}")
if(VEX_CONFIG)
    list(APPEND install_cmd --config "${VEX_CONFIG}")
endif()
execute_process(
    COMMAND ${install_cmd}
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err
)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Vex install failed (${code}):\n${out}\n${err}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${VEX_SOURCE_DIR}/tests/package_consumer"
        -B "${consumer_build}"
        -G "${VEX_GENERATOR}"
        "-DCMAKE_PREFIX_PATH=${prefix}"
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err
)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Consumer configure failed (${code}):\n${out}\n${err}")
endif()

set(build_cmd "${CMAKE_COMMAND}" --build "${consumer_build}")
if(VEX_CONFIG)
    list(APPEND build_cmd --config "${VEX_CONFIG}")
endif()
execute_process(
    COMMAND ${build_cmd}
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err
)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Consumer build failed (${code}):\n${out}\n${err}")
endif()

set(test_cmd "${CMAKE_CTEST_COMMAND}" --test-dir "${consumer_build}" --output-on-failure)
if(VEX_CONFIG)
    list(APPEND test_cmd -C "${VEX_CONFIG}")
endif()
execute_process(
    COMMAND ${test_cmd}
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err
)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "Consumer test failed (${code}):\n${out}\n${err}")
endif()
