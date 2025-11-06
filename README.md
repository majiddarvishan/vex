<!-- ![Vex Logo](assets/logo.jpg) -->
<p align="center">
  <img src="assets/vex-logo.png" alt="Vex Logo" width="180"/>
</p>

# Vex — Fast. Simple. Professional.

A modern C++ utility library focused on **performance**, **simplicity**, and **professional-grade tooling**.

**Vex** is a modern C++ utility library.
It provides **thread-safe logging, thread pool management, and networking modules** out of the box.
Vex is modular, so you can enable only the features you need.

## Features
- 🚀 Zero-overhead
- ⚡ High-performance
- 🛠 Minimal, header-only

- **Logger**: Thread-safe singleton logging system with `Info`, `Warning`, and `Error` levels.
- **ThreadPool**: Flexible thread pool for parallel task execution.
- **Networking**: TCP server and client for asynchronous communication.
- **Modular CMake**: Select which modules to build: Logger, ThreadPool, Networking.
- **Expandable**: Easily add new modules (database, serialization, etc.) without breaking existing code.

## Directory Structure

```bash
vex/
├── CMakeLists.txt
├── include/vex/ # Public headers
├── src/ # Source files
├── examples/ # Usage examples
└── tests/ # Unit tests
```

## Build Instructions

1. **Clone the repository**

```bash
git clone <your-repo-url> vex
cd vex
```

2. **Configure the build**

- Build all modules (default):

```bash
cmake -S . -B build
```

- Build specific modules only:

```bash
cmake -S . -B build \
  -DBUILD_LOGGER=ON \
  -DBUILD_THREADPOOL=OFF \
  -DBUILD_NETWORKING=ON
```

3. **Build**

```bash
cmake --build build
```

4. **Run examples**

```bash
./build/examples/example_logger
./build/examples/example_network
```

## Install (Optional)

To install headers and library system-wide:

```bash
cmake --install build --prefix /usr/local
```

This will install:

- libvex.a → /usr/local/lib
- include/vex/* → /usr/local/include/vex

## Quick Start

```cpp
#include "vex/vex.h"

int main() {
#ifdef BUILD_LOGGER
    auto &logger = ::vexLogger::instance();
    logger.log(::vexLogLevel::Info, "Vex Logger is working!");
#endif

#ifdef BUILD_THREADPOOL
    ::vexThreadPool pool(4);
    pool.enqueue([]{ /* your task */ });
#endif

#ifdef BUILD_NETWORKING
    ::vexTCPClient client("127.0.0.1", 8080);
    client.connectToServer();
#endif

    return 0;
}
```

## Dependencies

- C++17 or newer
- POSIX-compliant system (Linux/macOS) for Networking module (Windows requires Winsock adjustments)
- CMake >= 3.16
