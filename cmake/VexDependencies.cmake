# Find an installed dependency first; fetch only when expressly enabled.
# ObjectPool's public headers currently include Prometheus unconditionally,
# even if the compiled Monitoring module is disabled.
find_package(prometheus-cpp 1.3 CONFIG QUIET)

if(TARGET prometheus-cpp::core AND NOT TARGET prometheus-cpp::pull)
    message(FATAL_ERROR
        "Installed prometheus-cpp lacks the pull target; install pull support.")
endif()

if(NOT TARGET prometheus-cpp::core)
    if(NOT VEX_ALLOW_DEPENDENCY_FETCH)
        message(FATAL_ERROR
            "Vex ObjectPool/Monitoring require prometheus-cpp >= 1.3. "
            "Install it and set CMAKE_PREFIX_PATH or enable "
            "VEX_ALLOW_DEPENDENCY_FETCH.")
    endif()

    include(${PROJECT_SOURCE_DIR}/cmake/CPM.cmake)
    CPMAddPackage(
        NAME prometheus-cpp
        GITHUB_REPOSITORY jupp0r/prometheus-cpp
        VERSION 1.3.0
        OPTIONS
            "ENABLE_TESTING OFF"
            "OVERRIDE_CXX_STANDARD_FLAGS OFF"
            "GENERATE_PKGCONFIG OFF"
            "USE_THIRDPARTY_LIBRARIES ON"
            "ENABLE_PUSH OFF"
            "ENABLE_COMPRESSION OFF"
            "BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS}"
    )
endif()

if(NOT TARGET prometheus-cpp::core OR NOT TARGET prometheus-cpp::pull)
    message(FATAL_ERROR
        "prometheus-cpp dependency did not provide core and pull targets.")
endif()
