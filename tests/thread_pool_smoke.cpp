#include <vex/thread/threadpool.hpp>

#include <atomic>

int main()
{
    std::atomic<int> completed{0};
    {
        vex::ThreadPool pool(4);
        for (int i = 0; i < 128; ++i)
            pool.enqueue([&completed] { completed.fetch_add(1); });
    } // Existing destructor drains ordinary non-throwing tasks.
    return completed.load() == 128 ? 0 : 1;
}
