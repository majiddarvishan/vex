# Find Google Benchmark (optional)
find_package(benchmark QUIET)

if(benchmark_FOUND)
    # Comprehensive benchmark
    add_executable(benchmark_all benchmark_all.cpp)
    target_link_libraries(benchmark_all
        PRIVATE
            expirator::expirator
            benchmark::benchmark
    )

    # Individual benchmarks
    add_executable(benchmark_insert benchmark_insert.cpp)
    target_link_libraries(benchmark_insert
        PRIVATE
            expirator::expirator
            benchmark::benchmark
    )

    add_executable(benchmark_comparison benchmark_comparison.cpp)
    target_link_libraries(benchmark_comparison
        PRIVATE
            expirator::expirator
            benchmark::benchmark
    )
else()
    # Simple timing-based benchmarks (no Google Benchmark dependency)
    add_executable(simple_benchmark benchmark_comparison.cpp)
    target_link_libraries(simple_benchmark PRIVATE expirator::expirator)
    target_compile_definitions(simple_benchmark PRIVATE NO_GOOGLE_BENCHMARK)
endif()