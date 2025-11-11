#ifndef NO_GOOGLE_BENCHMARK
#include <benchmark/benchmark.h>
#endif

#include <expirator/heap_expirator.hpp>
#include <expirator/timing_wheel_expirator.hpp>
#include <expirator/lockfree_expirator.hpp>
#include <iostream>
#include <chrono>

#ifndef NO_GOOGLE_BENCHMARK

// Benchmark heap expirator insertion
static void BM_HeapInsert(benchmark::State& state)
{
    boost::asio::io_context io;
    auto exp = std::make_shared<expirator::heap_expirator<int, int>>(
        &io, [](int, int){}, nullptr
    );

    int key = 0;
    for (auto _ : state) {
        exp->add(key++, std::chrono::milliseconds(100), key);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HeapInsert);

// Benchmark timing wheel insertion
static void BM_TimingWheelInsert(benchmark::State& state)
{
    boost::asio::io_context io;
    auto exp = std::make_shared<expirator::timing_wheel_expirator<
        int, int, boost::fast_pool_allocator<char>
    >>(&io, [](int, int){}, nullptr);

    int key = 0;
    for (auto _ : state) {
        exp->add(key++, std::chrono::milliseconds(100), key);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TimingWheelInsert);

// Benchmark lock-free insertion
static void BM_LockFreeInsert(benchmark::State& state)
{
    boost::asio::io_context io;
    auto exp = std::make_shared<expirator::lockfree_expirator<int, int>>(
        &io, [](int, int){}, nullptr
    );

    int key = 0;
    for (auto _ : state) {
        exp->add(key++, std::chrono::milliseconds(100), key);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LockFreeInsert);

// Benchmark remove operations
static void BM_HeapRemove(benchmark::State& state)
{
    boost::asio::io_context io;
    auto exp = std::make_shared<expirator::heap_expirator<int, int>>(
        &io, [](int, int){}, nullptr
    );

    // Pre-populate
    for (int i = 0; i < 10000; ++i) {
        exp->add(i, std::chrono::seconds(100), i);
    }

    int key = 0;
    for (auto _ : state) {
        exp->remove(key++);
        if (key >= 10000) key = 0;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HeapRemove);

BENCHMARK_MAIN();

#else

// Simple benchmark without Google Benchmark
#include <vector>

template<typename Func>
double measure_time(Func&& func, int iterations)
{
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return static_cast<double>(duration.count()) / iterations;
}

int main()
{
    const int iterations = 100000;

    std::cout << "Benchmark Results (average time per operation in microseconds)\n";
    std::cout << "=================================================================\n\n";

    // Heap insertion benchmark
    {
        boost::asio::io_context io;
        auto exp = std::make_shared<expirator::heap_expirator<int, int>>(
            &io, [](int, int){}, nullptr
        );

        int key = 0;
        auto time = measure_time([&]() {
            exp->add(key++, std::chrono::milliseconds(100), key);
        }, iterations);

        std::cout << "Heap Expirator - Insert: " << time << " μs\n";
    }

    // Timing wheel insertion benchmark
    {
        boost::asio::io_context io;
        auto exp = std::make_shared<expirator::timing_wheel_expirator<
            int, int, boost::fast_pool_allocator<char>
        >>(&io, [](int, int){}, nullptr);

        int key = 0;
        auto time = measure_time([&]() {
            exp->add(key++, std::chrono::milliseconds(100), key);
        }, iterations);

        std::cout << "Timing Wheel - Insert: " << time << " μs\n";
    }

    // Lock-free insertion benchmark
    {
        boost::asio::io_context io;
        auto exp = std::make_shared<expirator::lockfree_expirator<int, int>>(
            &io, [](int, int){}, nullptr
        );

        int key = 0;
        auto time = measure_time([&]() {
            exp->add(key++, std::chrono::milliseconds(100), key);
        }, iterations);

        std::cout << "Lock-Free - Insert: " << time << " μs\n";
    }

    std::cout << "\n";

    // Removal benchmarks
    {
        boost::asio::io_context io;
        auto exp = std::make_shared<expirator::heap_expirator<int, int>>(
            &io, [](int, int){}, nullptr
        );

        for (int i = 0; i < iterations; ++i) {
            exp->add(i, std::chrono::seconds(100), i);
        }

        int key = 0;
        auto time = measure_time([&]() {
            exp->remove(key++);
        }, iterations / 10);

        std::cout << "Heap Expirator - Remove: " << time << " μs\n";
    }

    return 0;
}

#endif