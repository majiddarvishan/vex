#include <vex/thread/threadpool.hpp>

#include <chrono>
#include <iostream>

int main()
{
    std::cout << "Starting vex example\n";

    vex::ThreadPool pool(4);

    for (int i = 0; i < 10; ++i)
    {
        pool.enqueue(
          [i]
          {
              std::cout << "Task " << i << " running\n";
              std::this_thread::sleep_for(std::chrono::milliseconds(100));
              std::cout << "Task " << i << " finished\n";
          }
        );
    }

    std::cout << "Main thread done, waiting for tasks...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2)); // wait tasks finish
    return 0;
}
