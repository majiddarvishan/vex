#include <atomic>
#include <thread>
#include <vector>

// Intentionally independent from Vex/Prometheus. If this race-free program
// fails with a TSan mapping fatality, investigate toolchain/kernel/runtime.
int main()
{
    std::atomic<int> total{0};
    std::vector<std::thread> threads;
    threads.reserve(4);
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&total]
        {
            for (int j = 0; j < 1000; ++j)
                total.fetch_add(1, std::memory_order_relaxed);
        });
    }
    for (auto& thread : threads)
        thread.join();
    return total.load(std::memory_order_relaxed) == 4000 ? 0 : 1;
}
