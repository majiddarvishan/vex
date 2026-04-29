# router

A high-performance, thread-safe route matching library written in C++20.

## Features

- **Trie-based matching** for prefix/exact patterns on source and destination addresses
- **Priority routing** — higher priority number wins; ties broken by pattern specificity
- **Runtime add/remove** of routes with RCU-style lock-free reads
- **Zero heap allocation** per `find()` call (thread-local buffers)

## Pattern syntax

| Pattern      | Meaning                          |
|--------------|----------------------------------|
| `""`         | Wildcard — matches any value     |
| `"98912*"`   | Prefix — matches values starting with `98912` |
| `"989125305483"` | Exact — full string match    |

Longer matches win over shorter ones. Exact beats prefix of the same length.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/example/router_example
```

To skip the example:
```bash
cmake -B build -DROUTER_BUILD_EXAMPLES=OFF
```

## Integration via CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(router
    GIT_REPOSITORY https://github.com/your-org/router.git
    GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(router)

target_link_libraries(your_target PRIVATE router)
```

## Quick start

```cpp
#include <router.hpp>

router::RouteMatcher matcher({
    {1, 1, "", "",        "982000*", "deliver", "client_0"},
    {2, 2, "", "98911*",  "",        "",        "client_1"},
});

auto* route = matcher.find({"", "989111234", "", ""});
if (route)
    std::cout << route->target << '\n'; // "client_1"

// Runtime mutations
matcher.add({3, 1, "", "98900*", "", "", "client_2"});
matcher.remove(1);
```

## Project layout

```
router/
├── include/
│   ├── router.hpp          # umbrella header (include this)
│   └── router/
│       ├── route.hpp            # Route, MessageContext
│       ├── pattern.hpp          # Pattern (parse + match)
│       ├── trie.hpp             # Trie (prefix index)
│       ├── routing_table.hpp    # Immutable compiled snapshot
│       └── route_matcher.hpp    # Public thread-safe API
├── src/
│   ├── pattern.cpp
│   ├── trie.cpp
│   ├── routing_table.cpp
│   └── route_matcher.cpp
├── example/
│   ├── CMakeLists.txt
│   └── main.cpp
├── cmake/
│   └── routerConfig.cmake.in
└── CMakeLists.txt
```
