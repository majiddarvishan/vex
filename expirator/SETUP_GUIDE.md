# Complete Expirator Library Setup Guide

This guide will walk you through setting up the complete, production-ready expirator library with all tests and examples.

## 📋 Prerequisites

### Required
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15 or higher
- Boost 1.70 or higher (system component)

### Optional (for full functionality)
- Google Test (for unit tests)
- Google Benchmark (for detailed benchmarks)

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libboost-system-dev \
    libgtest-dev \
    libbenchmark-dev
```

#### macOS
```bash
brew install cmake boost googletest google-benchmark
```

#### Fedora/RHEL
```bash
sudo dnf install cmake boost-devel gtest-devel benchmark-devel
```

## 🚀 Quick Start (5 Minutes)

### Step 1: Create Project Structure

```bash
# Download and run the build script
bash build_library.sh
cd expirator
```

### Step 2: Copy All Header Files

Copy the following files from the artifacts:

1. **include/expirator/base_expirator.hpp** - Already created by script
2. **include/expirator/expirator.hpp** - Already created by script
3. **include/expirator/heap_expirator.hpp** - Copy from "heap_expirator_hpp" artifact
4. **include/expirator/timing_wheel_expirator.hpp** - Copy from "All Implementation Files"
5. **include/expirator/lockfree_expirator.hpp** - Copy from "All Implementation Files"

### Step 3: Copy Example Files

Copy these to `examples/`:

1. **basic_usage.cpp** - From "All Implementation Files"
2. **advanced_patterns.cpp** - From "Advanced Examples" artifact
3. **production_scenarios.cpp** - From "Production Scenarios" artifact
4. **timing_wheel_example.cpp** - Create (see below)
5. **lockfree_example.cpp** - Create (see below)

### Step 4: Copy Test Files

Copy these to `tests/`:

1. **test_heap_expirator.cpp** - From "Complete Unit Tests Suite"
2. **test_timing_wheel.cpp** - From "Complete Unit Tests Suite"
3. **test_lockfree.cpp** - From "Complete Unit Tests Suite"
4. **test_integration.cpp** - From "Complete Unit Tests Suite"
5. **simple_test.cpp** - From "Complete Unit Tests Suite"

### Step 5: Copy Benchmark Files

Copy **benchmark_comparison.cpp** to `benchmarks/` from "All Implementation Files"

### Step 6: Build Everything

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

## 📁 Complete File Structure

After setup, your project should look like this:

```
expirator/
├── CMakeLists.txt                     # Main build configuration
├── README.md                          # Project documentation
├── LICENSE                            # MIT License
├── .gitignore                         # Git ignore rules
│
├── cmake/
│   └── expiratorConfig.cmake.in     # CMake package config
│
├── include/expirator/
│   ├── expirator.hpp                 # Main include file
│   ├── base_expirator.hpp           # Base interface
│   ├── heap_expirator.hpp           # Min-heap implementation
│   ├── timing_wheel_expirator.hpp   # Timing wheel implementation
│   └── lockfree_expirator.hpp       # Lock-free implementation
│
├── examples/
│   ├── CMakeLists.txt
│   ├── basic_usage.cpp              # Simple introduction
│   ├── timing_wheel_example.cpp     # High-performance example
│   ├── lockfree_example.cpp         # Multi-threaded example
│   ├── advanced_patterns.cpp        # Advanced use cases
│   └── production_scenarios.cpp     # Real-world scenarios
│
├── benchmarks/
│   ├── CMakeLists.txt
│   └── benchmark_comparison.cpp     # Performance comparison
│
└── tests/
    ├── CMakeLists.txt
    ├── test_heap_expirator.cpp      # Heap tests
    ├── test_timing_wheel.cpp        # Timing wheel tests
    ├── test_lockfree.cpp            # Lock-free tests
    ├── test_integration.cpp         # Integration tests
    └── simple_test.cpp              # No-dependency tests
```

## 🔨 Building Options

### Default Build (Everything)
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Release Build (Optimized)
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Build Without Examples
```bash
cmake -DEXPIRATOR_BUILD_EXAMPLES=OFF ..
cmake --build .
```

### Build Without Tests
```bash
cmake -DEXPIRATOR_BUILD_TESTS=OFF ..
cmake --build .
```

### Build Without Benchmarks
```bash
cmake -DEXPIRATOR_BUILD_BENCHMARKS=OFF ..
cmake --build .
```

## 🧪 Running Tests

### All Tests (with Google Test)
```bash
cd build
ctest --output-on-failure
```

### Specific Test
```bash
./tests/test_heap_expirator
./tests/test_timing_wheel
./tests/test_lockfree
./tests/test_integration
```

### Simple Test (no dependencies)
```bash
./tests/simple_test
```

## 📊 Running Benchmarks

### Simple Benchmark
```bash
./benchmarks/simple_benchmark
```

### With Google Benchmark
```bash
./benchmarks/benchmark_comparison
```

## 🎯 Running Examples

### Basic Usage
```bash
./examples/basic_usage
```

### Advanced Patterns
```bash
./examples/advanced_patterns
```

Output shows:
- Session Management System
- TTL Cache
- Rate Limiter
- Distributed Lock
- Connection Pool

### Production Scenarios
```bash
./examples/production_scenarios
```

Output shows:
- Multi-Tier API Rate Limiting
- Job Queue with Timeout & Retry
- WebSocket Connection Manager

## 📦 Installation

### System-Wide Installation
```bash
cd build
sudo cmake --install .
```

This installs:
- Headers to `/usr/local/include/expirator/`
- CMake config to `/usr/local/lib/cmake/expirator/`

### Using in Your Project

#### Method 1: With CMake (Installed)
```cmake
find_package(expirator REQUIRED)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE expirator::expirator)
```

#### Method 2: With CMake (Subdirectory)
```cmake
add_subdirectory(path/to/expirator)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE expirator::expirator)
```

#### Method 3: Manual
```bash
# Copy include directory
cp -r expirator/include/expirator /usr/local/include/

# In your code
#include <expirator/heap_expirator.hpp>

# Compile with
g++ -std=c++17 myapp.cpp -lboost_system -lpthread
```

## 📖 Quick API Reference

### Creating an Expirator

```cpp
#include <expirator/heap_expirator.hpp>

boost::asio::io_context io;

auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
    &io,
    [](int key, std::string data) {
        // Called when item expires
        std::cout << "Expired: " << key << std::endl;
    },
    [](const boost::system::error_code& ec) {
        // Optional: Called on error
        std::cerr << "Error: " << ec.message() << std::endl;
    }
);
```

### Basic Operations

```cpp
// Add item (expires in 5 seconds)
exp->add(1, std::chrono::seconds(5), "data");

// Check if exists
if (exp->contains(1)) {
    // Get associated data
    auto info = exp->get_info(1);

    // Get time remaining
    auto remaining = exp->get_remaining_time(1);
}

// Remove before expiry
exp->remove(1);

// Start processing
exp->start();
io.run();
```

### Advanced Operations

```cpp
// Update expiry time
exp->update_expiry(1, std::chrono::seconds(10));

// Clear all entries (no callbacks)
exp->clear();

// Stop processing
exp->stop();

// Query state
size_t count = exp->size();
bool empty = exp->empty();
bool running = exp->is_running();
```

## 🎓 Examples by Use Case

### 1. Session Management
```cpp
#include <expirator/heap_expirator.hpp>
// See examples/advanced_patterns.cpp - session_management namespace
```

### 2. Cache with TTL
```cpp
#include <expirator/timing_wheel_expirator.hpp>
// See examples/advanced_patterns.cpp - cache_system namespace
```

### 3. Rate Limiting
```cpp
#include <expirator/heap_expirator.hpp>
// See examples/advanced_patterns.cpp - rate_limiter namespace
// See examples/production_scenarios.cpp - api_rate_limiting namespace
```

### 4. Distributed Lock
```cpp
#include <expirator/heap_expirator.hpp>
// See examples/advanced_patterns.cpp - distributed_lock namespace
```

### 5. Connection Pool
```cpp
#include <expirator/heap_expirator.hpp>
// See examples/advanced_patterns.cpp - connection_pool namespace
```

### 6. Job Scheduler
```cpp
#include <expirator/heap_expirator.hpp>
// See examples/production_scenarios.cpp - job_queue namespace
```

### 7. WebSocket Manager
```cpp
#include <expirator/heap_expirator.hpp>
// See examples/production_scenarios.cpp - websocket_manager namespace
```

## 🐛 Troubleshooting

### Build Fails - Boost Not Found
```bash
# Install Boost
sudo apt-get install libboost-system-dev

# Or specify Boost location
cmake -DBOOST_ROOT=/path/to/boost ..
```

### Build Fails - C++17 Not Supported
```bash
# Update compiler or specify newer one
cmake -DCMAKE_CXX_COMPILER=g++-9 ..
```

### Tests Fail - Google Test Not Found
```bash
# Install Google Test
sudo apt-get install libgtest-dev

# Or build without tests
cmake -DEXPIRATOR_BUILD_TESTS=OFF ..
```

### Runtime Error - Timer Issues
Make sure you're calling `io_context.run()` in a thread:
```cpp
boost::asio::io_context io;
auto exp = std::make_shared<expirator::heap_expirator<int, int>>(...);
exp->start();
io.run();  // This will block until all work is done
```

## 📈 Performance Tips

### 1. Reserve Space
```cpp
exp->reserve(100000);  // Pre-allocate for 100k entries
```

### 2. Use Timing Wheel for High Throughput
```cpp
#include <expirator/timing_wheel_expirator.hpp>
auto exp = std::make_shared<expirator::timing_wheel_expirator<
    int, std::string, boost::fast_pool_allocator<char>
>>(...);
```

### 3. Use Lock-Free for Multi-Threading
```cpp
#include <expirator/lockfree_expirator.hpp>
auto exp = std::make_shared<expirator::lockfree_expirator<int, std::string>>(...);

// Can now call add/remove from any thread safely
```

### 4. Batch Operations
```cpp
// Add many items at once
for (int i = 0; i < 10000; ++i) {
    exp->add(i, std::chrono::seconds(60), data);
}
// Start processing after all adds
exp->start();
```

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Add tests for new features
4. Submit a pull request

## 📄 License

MIT License - See LICENSE file

## 🙏 Support

- 📖 Documentation: See README.md
- 💬 Issues: Open an issue on GitHub
- 📧 Email: [your-email]

## ✅ Verification Checklist

After setup, verify everything works:

- [ ] Project builds without errors
- [ ] All unit tests pass (`ctest`)
- [ ] Examples run successfully
- [ ] Benchmarks execute
- [ ] Can install system-wide
- [ ] Can use in your own project

## 🎉 You're Done!

Your expirator library is now fully set up and ready to use. Start with `examples/basic_usage.cpp` and explore the advanced examples to see what's possible!