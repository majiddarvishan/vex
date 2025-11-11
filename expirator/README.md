# Expirator - High-Performance Expiration Management Library

A modern C++17 library providing multiple high-performance implementations for managing time-based expiration of key-value pairs.

## Features

- **Multiple Implementations**: Choose the right algorithm for your use case
  - **Heap-based**: Balanced performance, exact timing, O(log n) operations
  - **Timing Wheel**: Ultra-fast O(1) operations, predictable performance
  - **Lock-Free**: Thread-safe, wait-free operations for multi-threaded environments

- **Header-Only**: Easy integration, no linking required
- **Modern C++17**: Clean API, move semantics, RAII
- **Boost.Asio Integration**: Seamless async I/O
- **Memory Pool Support**: Reduced allocation overhead
- **Comprehensive Examples**: Learn by example
- **Benchmarks Included**: Measure performance for your workload

## Requirements

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Boost >= 1.70 (system component)
- CMake >= 3.15

## Quick Start

### Building

```bash
git clone <repository>
cd expirator
mkdir build && cd build
cmake ..
cmake --build .

# Run examples
./examples/basic_usage
./examples/timing_wheel_example

# Run benchmarks
./benchmarks/simple_benchmark
```

### Installation

```bash
cd build
sudo cmake --install .
```

### Using in Your Project

#### With CMake

```cmake
find_package(expirator REQUIRED)
target_link_libraries(your_target PRIVATE expirator::expirator)
```

#### Manual Integration

Simply copy the `include/expirator` directory to your project and add to include path.

## Basic Usage

```cpp
#include <expirator/heap_expirator.hpp>
#include <iostream>

int main()
{
    boost::asio::io_context io;

    auto exp = std::make_shared<expirator::heap_expirator<int, std::string>>(
        &io,
        [](int key, std::string data) {
            std::cout << "Expired: " << key << " -> " << data << std::endl;
        }
    );

    // Add entries with different expiration times
    exp->add(1, std::chrono::seconds(2), "First");
    exp->add(2, std::chrono::seconds(5), "Second");
    exp->add(3, std::chrono::seconds(1), "Quick");

    // Start processing
    exp->start();
    io.run();

    return 0;
}
```

## Performance Comparison

| Feature           | Heap        | Timing Wheel | Lock-Free   |
|-------------------|-------------|--------------|-------------|
| Insert            | O(log n)    | **O(1)**     | O(log n)    |
| Remove            | O(log n)    | O(n)         | O(log n)    |
| Get Earliest      | **O(1)**    | **O(1)**     | **O(1)**    |
| Thread-Safe       | No          | No           | **Yes**     |
| Memory Overhead   | **Low**     | High         | Medium      |
| Throughput        | 500k/s      | **10M+/s**   | 2-5M/s      |
| Precision         | **Exact**   | 1ms          | **Exact**   |
| Predictability    | Medium      | **High**     | Medium      |

## Choosing an Implementation

### heap_expirator
**Best for**: General-purpose expiration management

- Balanced performance
- Exact timing
- Low memory overhead
- Good default choice

```cpp
#include <expirator/heap_expirator.hpp>
auto exp = std::make_shared<expirator::heap_expirator<Key, Info>>(...);
```

### timing_wheel_expirator
**Best for**: High-frequency operations, games, simulations

- Maximum throughput (10M+ ops/sec)
- O(1) insertion
- Predictable performance
- Higher memory usage

```cpp
#include <expirator/timing_wheel_expirator.hpp>
auto exp = std::make_shared<expirator::timing_wheel_expirator<
    Key, Info, boost::fast_pool_allocator<char>
>>(...);
```

### lockfree_expirator
**Best for**: Multi-threaded environments

- Thread-safe add/remove
- Wait-free operations
- Good for producer-consumer patterns
- Network servers

```cpp
#include <expirator/lockfree_expirator.hpp>
auto exp = std::make_shared<expirator::lockfree_expirator<Key, Info>>(...);
```

## Examples

See the `examples/` directory for:
- `basic_usage.cpp` - Simple introduction
- `timing_wheel_example.cpp` - High-performance session management
- `lockfree_example.cpp` - Multi-threaded usage
- `advanced_patterns.cpp` - Complex use cases

## Benchmarks

Run benchmarks to measure performance on your hardware:

```bash
./benchmarks/simple_benchmark
```

## API Reference

### Common Operations

```cpp
// Start/Stop
void start();
void stop();
bool is_running() const;

// Add/Remove
bool add(Tkey key, duration expiration, Tinfo info = {});
bool remove(const Tkey& key);
void clear();

// Query
std::optional<Tinfo> get_info(const Tkey& key) const;
std::optional<duration> get_remaining_time(const Tkey& key) const;
bool contains(const Tkey& key) const;
size_t size() const;
bool empty() const;

// Update (heap_expirator only)
bool update_expiry(const Tkey& key, duration new_expiration);
```

## Contributing

Contributions welcome! Please submit issues and pull requests.

## License

MIT License - See LICENSE file for details

## Authors

Original implementation and design.

## Acknowledgments

- Boost.Asio for async I/O
- Inspired by various timing wheel implementations
