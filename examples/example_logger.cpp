#ifdef BUILD_LOGGER

#include "vex/vex.h"
#include <chrono>

int main() {
    auto& logger = vex::Logger::instance();
    logger.log(vex::LogLevel::Info, "Starting vex example");

    vex::ThreadPool pool(4);

    for(int i = 0; i < 10; ++i) {
        pool.enqueue([i, &logger]{
            logger.log(vex::LogLevel::Info, "Task " + std::to_string(i) + " running");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            logger.log(vex::LogLevel::Info, "Task " + std::to_string(i) + " finished");
        });
    }

    logger.log(vex::LogLevel::Info, "Main thread done, waiting for tasks...");
    std::this_thread::sleep_for(std::chrono::seconds(2)); // wait tasks finish
    return 0;
}
#endif
