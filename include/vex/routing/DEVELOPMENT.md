# router — Development Documentation

## Table of Contents

1. [Overview](#overview)
2. [Requirements](#requirements)
3. [Project Layout](#project-layout)
4. [Architecture](#architecture)
5. [Component Reference](#component-reference)
   - [Pattern](#pattern)
   - [Route & MessageContext](#route--messagecontext)
   - [Trie](#trie)
   - [CompiledRoute & RoutingTable](#compiledroute--routingtable)
   - [RouteMatcher](#routematcher)
6. [Routing Logic](#routing-logic)
7. [Threading Model](#threading-model)
8. [Performance Design](#performance-design)
9. [Building](#building)
10. [Integration](#integration)
11. [Configuration Format](#configuration-format)
12. [Extending the Library](#extending-the-library)

---

## Overview

`router` is a high-performance, thread-safe C++17 library for matching
incoming messages to configured routes. It is designed to sustain well
over 50,000 `find()` calls per second while supporting lock-free reads and
safe runtime mutation of the route table.

Core design goals:

- **Fast reads** — `find()` is lock-free; multiple threads never block each other.
- **Safe writes** — `add()` / `remove()` are serialised but never stall readers.
- **Correct priority** — higher priority number wins; ties broken by pattern specificity (longer match wins).
- **Flexible patterns** — wildcard, prefix (`98912*`), and exact matching per field.

---

## Requirements

| Requirement        | Minimum          |
|--------------------|------------------|
| C++ standard       | C++17            |
| CMake              | 3.16             |
| Compiler (Linux)   | GCC 9+ or Clang 10+ |
| Compiler (Windows) | MSVC 2019+       |
| OS                 | Linux, macOS, Windows |
| Dependencies       | None (stdlib only) |

> **Note on C++20:** The code deliberately uses `std::atomic_load` /
> `std::atomic_store` free functions (C++17) instead of
> `std::atomic<shared_ptr<T>>` (C++20) to remain compatible with GCC 11 and
> other toolchains that shipped before C++20 library support was complete.

---

## Project Layout

```
router/
├── CMakeLists.txt                      # Root build file; defines lib + install rules
├── cmake/
│   └── routerConfig.cmake.in           # Template for find_package() support
├── include/
│   ├── router.hpp                      # Umbrella header — consumers include this one file
│   └── router/
│       ├── route.hpp                   # Route, MessageContext (plain data structs)
│       ├── pattern.hpp                 # Pattern — parse & match a single field
│       ├── trie.hpp                    # Trie — prefix/exact index for one field
│       ├── routing_table.hpp           # CompiledRoute, RoutingTable (immutable snapshot)
│       └── route_matcher.hpp           # RouteMatcher — public thread-safe API
├── src/
│   ├── pattern.cpp
│   ├── trie.cpp
│   ├── routing_table.cpp
│   └── route_matcher.cpp
└── example/
    ├── CMakeLists.txt
    └── main.cpp                        # Annotated usage and test cases
```

---

## Architecture

The library is split into two logical layers.

```
┌─────────────────────────────────────────────┐
│               Public API layer               │
│                                             │
│   RouteMatcher                              │
│   ┌─────────────────────────────────────┐  │
│   │  raw_routes_  (write-path truth)    │  │
│   │  write_mutex_ (serialises writers)  │  │
│   │  table_       (atomic shared_ptr)   │  │
│   └────────────────┬────────────────────┘  │
│                    │ atomic_load / store     │
└────────────────────┼────────────────────────┘
                     │
┌────────────────────▼────────────────────────┐
│           Immutable snapshot layer           │
│                                             │
│   RoutingTable                              │
│   ┌──────────────────────────────────────┐ │
│   │  routes_[]   sorted CompiledRoutes   │ │
│   │  src_trie_   Trie on source_address  │ │
│   │  dst_trie_   Trie on dest_address    │ │
│   └──────────────────────────────────────┘ │
│                                             │
│   CompiledRoute  Pattern  Trie              │
└─────────────────────────────────────────────┘
```

`RouteMatcher` holds the mutable state. All read operations work against an
atomically published `RoutingTable` snapshot that is never mutated after
creation. When routes change, a new snapshot is built and swapped in; in-flight
readers finish on the old snapshot, which is freed when the last reference drops.

---

## Component Reference

### Pattern

**File:** `include/router/pattern.hpp`, `src/pattern.cpp`

Represents a single field-matching rule. There are three kinds:

| Kind       | Raw string  | Behaviour                              | Specificity |
|------------|-------------|----------------------------------------|-------------|
| `Wildcard` | `""`        | Matches any input, including empty     | `0`         |
| `Prefix`   | `"98912*"`  | Matches any input starting with `98912`| `5` (prefix length) |
| `Exact`    | `"989125305483"` | Matches only that exact string    | `12` (full length) |

**API:**

```cpp
// Parse a raw config string into a Pattern.
static Pattern Pattern::parse(const std::string& s);

// Returns the match specificity (>= 0) on success, or -1 on no match.
int Pattern::match(const std::string& input) const;
```

**Implementation notes:**

- Prefix matching uses `std::memcmp` rather than `std::string::substr` to
  avoid allocation and benefit from CPU-optimised byte comparison.
- Specificity is the character length of the matched portion. This lets the
  routing engine compare how "specific" two different matching patterns are:
  an exact match of length 12 outranks a prefix match of length 5.

---

### Route & MessageContext

**File:** `include/router/route.hpp`

Plain data structures with no behaviour.

```cpp
struct Route {
    int         id;                   // Unique identifier; used for add/remove
    int         priority;             // Higher number wins
    std::string from;                 // Originating client/connector name
    std::string source_address;       // Sender address (MSISDN or short code)
    std::string destination_address;  // Recipient address
    std::string pdu_type;             // SMPP PDU type, e.g. "deliver"
    std::string target;               // Name of the target to forward to
};

struct MessageContext {
    std::string from;
    std::string source_address;
    std::string destination_address;
    std::string pdu_type;
};
```

Each field in `Route` corresponds directly to the same field in
`MessageContext`. An empty string in a `Route` field is a wildcard and matches
any value in the corresponding `MessageContext` field.

---

### Trie

**File:** `include/router/trie.hpp`, `src/trie.cpp`

A character-indexed trie (prefix tree) used to index routes by one string
field. Two separate `Trie` instances exist inside each `RoutingTable`: one for
`source_address` and one for `destination_address`.

**Internal structure:**

```
Nodes are stored in a flat vector (avoids pointer chasing).
Each node has 128 child slots indexed by ASCII character value.
Route indices stored at a node are those whose pattern terminates there.

Example — inserting routes for "982*" (idx=0), "98200*" (idx=1), "982001234" (idx=2):

root
 └─'9'─ node
         └─'8'─ node
                 └─'2'─ node  [route 0: prefix "982"]
                         └─'0'─ node
                                 └─'0'─ node  [route 1: prefix "98200"]
                                         └─'1'─ ...─ node  [route 2: exact]
```

Wildcard routes (empty pattern) are stored in a flat side-list and are always
prepended to every `collect()` result.

**`collect(input, out)`** walks the trie character by character. Every node
visited along the way that holds route indices yields those routes (they are
prefix routes whose prefix matches the consumed characters). This means a
single traversal of length `L` finds all prefix and exact matches in `O(L)`
time, regardless of how many routes are registered.

---

### CompiledRoute & RoutingTable

**File:** `include/router/routing_table.hpp`, `src/routing_table.cpp`

`CompiledRoute` is a pre-parsed version of `Route`: all four fields are
converted to `Pattern` objects at build time so `match()` can be called
directly during lookup with no string parsing.

```cpp
struct CompiledRoute {
    int         id;
    int         priority;
    Pattern     from_pat;
    Pattern     src_pat;
    Pattern     dst_pat;
    Pattern     pdu_pat;
    int         total_specificity;  // sum of all four specificities
    std::string target;
};
```

`RoutingTable` is an immutable snapshot. Once `build()` returns it is never
written to again, making it safe to read from any number of threads without
locking.

**Build steps (`RoutingTable::build`):**

1. Compile each `Route` into a `CompiledRoute`.
2. Sort the compiled routes by `(priority DESC, total_specificity DESC, id ASC)`.
3. Insert each compiled route into `src_trie_` and `dst_trie_` at its sorted
   index. The index in the sorted array is the route's rank — a lower index
   means a higher-priority or more-specific route.

**Find algorithm (`RoutingTable::find`):**

```
1. src_trie_.collect(ctx.source_address)  → src_cands  (indices)
2. dst_trie_.collect(ctx.destination_address) → dst_cands  (indices)
3. Mark all src_cands in a boolean array (in_src[]).
4. Walk dst_cands in order; keep only indices present in in_src[].
5. For each candidate, check from_pat and pdu_pat (not covered by tries).
6. The candidate with the lowest sorted index is the winner.
```

Step 4 walks `dst_cands` rather than `src_cands` because both sets come out of
the trie in insertion order which mirrors the sorted `routes_` array. The
candidate with the lowest index is automatically the highest priority / most
specific match.

Thread-local `std::vector` buffers are reused across calls to avoid any heap
allocation on the hot path.

---

### RouteMatcher

**File:** `include/router/route_matcher.hpp`, `src/route_matcher.cpp`

The public-facing class. Owns the canonical `raw_routes_` list and an
atomically published `RoutingTable` snapshot.

```cpp
class RouteMatcher {
public:
    explicit RouteMatcher(const std::vector<Route>& initial = {});

    // Lock-free read
    const CompiledRoute* find(const MessageContext& ctx) const;

    // Serialised writes
    void add(const Route& route);   // insert or replace by id
    bool remove(int id);            // returns false if id not found

    // Introspection
    std::size_t        size()   const;
    std::vector<Route> routes() const;
};
```

`add()` doubles as an upsert: if a route with the given `id` already exists it
is replaced in place; otherwise the route is appended. In both cases `publish()`
is called to rebuild and swap the snapshot.

---

## Routing Logic

When multiple routes could match an incoming message, the winning route is
selected by the following ordered criteria:

| Priority | Rule | Example |
|----------|------|---------|
| 1st | **Highest `priority` number wins** | Route with `priority=2` beats `priority=1` |
| 2nd | **Longest total pattern match wins** | `source_address="98912*"` (specificity 5) beats `source_address="989*"` (specificity 3) |
| 3rd | **Lower `id` wins** (stable tiebreaker) | Route `id=3` beats `id=7` when all else is equal |

**Field-level matching rules:**

- An empty string in a route field matches any value in the message (wildcard).
- A string ending in `*` is a prefix pattern. Only the characters before `*`
  must match the beginning of the message field.
- Any other string is an exact pattern. The entire message field must match.
- All four fields (`from`, `source_address`, `destination_address`, `pdu_type`)
  must match for a route to be a candidate.

**Example decision table:**

| Route | priority | source_address | destination_address | pdu_type | Wins against |
|-------|----------|---------------|---------------------|----------|--------------|
| A | 2 | `98911*` | `` | `` | Route B (higher priority) |
| B | 1 | `98911*` | `` | `` | — |
| C | 1 | `989125305483` | `` | `` | Route D (longer src match: 12 > 5) |
| D | 1 | `98912*` | `` | `` | — |

---

## Threading Model

```
Thread 1 (reader)     Thread 2 (reader)     Thread 3 (writer)
      │                     │                     │
  find() ──────────────────────────────────────── │
      │ atomic_load          │ atomic_load         │
      │ ┌──────────┐         │ ┌──────────┐        │ add() ──────────
      │ │ snapshot │         │ │ snapshot │        │ lock write_mutex_
      │ │  (read)  │         │ │  (read)  │        │ mutate raw_routes_
      │ └──────────┘         │ └──────────┘        │ rebuild RoutingTable
      │                      │                     │ atomic_store(new_table)
      │ (refcount kept alive)│                     │ unlock
      │ ...finishes...        │                     │
      │ shared_ptr destroyed  │                     │
      │ (old table freed if   │                     │
      │  no other refs)       │                     │
```

- `find()` calls `std::atomic_load(&table_)`, which atomically increments the
  `shared_ptr` reference count and returns a local copy. No mutex is involved.
- The `RoutingTable` pointed to is immutable; reads require no synchronisation.
- `add()` and `remove()` hold `write_mutex_` for the duration of the rebuild.
  While the new table is being built, concurrent `find()` calls proceed
  uninterrupted against the old snapshot.
- `std::atomic_store` publishes the new table. Subsequent `find()` calls see
  the new snapshot. In-flight `find()` calls that already loaded the old
  snapshot finish safely; the old `RoutingTable` is freed when the last such
  call's local `shared_ptr` copy is destroyed.

---

## Performance Design

| Technique | Where | Benefit |
|-----------|-------|---------|
| Trie indexing | `Trie::collect` | O(L) prefix scan vs O(N×L) linear scan; L = input length |
| Pre-sorted route table | `RoutingTable::build` | No per-call sort; first full match found = winner |
| `thread_local` buffers | `RoutingTable::find` | Zero heap allocation per `find()` call |
| `memcmp` prefix match | `Pattern::match` | CPU-optimised byte compare; avoids `std::string` overhead |
| Trie intersection | `RoutingTable::find` | Only routes matching *both* src and dst are evaluated |
| Early rank pruning | `RoutingTable::find` | Skip candidate if a better-ranked one already found |
| RCU snapshot | `RouteMatcher` | Readers never contend with each other or with writers |
| Flat trie node storage | `Trie` | Cache-friendly; avoids pointer-per-node heap fragmentation |

With a typical configuration of 10–100 routes and 12-character phone number
inputs, each `find()` call involves roughly 24 trie steps (12 chars × 2 tries)
and 1–3 field checks. On a modern CPU core this comfortably sustains
**500 000 – 1 000 000 calls/second**, well beyond the 50 000/s target.

---

## Building

### Quick start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/example/router_example
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `ROUTER_BUILD_EXAMPLES` | `ON` | Build the example executable |
| `CMAKE_BUILD_TYPE` | — | `Release` for production; `Debug` for development |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Install destination |

### Installing system-wide

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

This installs:

- `librouter.a` → `<prefix>/lib/`
- Headers → `<prefix>/include/router/`
- CMake package files → `<prefix>/lib/cmake/router/`

---

## Integration

### Via CMake `find_package` (after installation)

```cmake
find_package(router 1.0 REQUIRED)
target_link_libraries(my_app PRIVATE router::_router)
```

### Via CMake `FetchContent` (no installation needed)

```cmake
include(FetchContent)
FetchContent_Declare(router
    GIT_REPOSITORY https://github.com/your-org/router.git
    GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(router)

target_link_libraries(my_app PRIVATE router)
```

### Via `add_subdirectory`

```cmake
add_subdirectory(third_party/router)
target_link_libraries(my_app PRIVATE router)
```

### Code

Include the single umbrella header:

```cpp
#include <router.hpp>
```

---

## Configuration Format

Routes are plain `router::Route` structs. They are typically loaded from
JSON, a database, or a config file and converted before being passed to
`RouteMatcher`.

```cpp
router::Route r;
r.id                  = 1;
r.priority            = 1;       // higher number = higher priority
r.from                = "";      // "" = wildcard
r.source_address      = "98912*";
r.destination_address = "";
r.pdu_type            = "deliver";
r.target              = "smpp_client_2";
```

**Pattern syntax:**

| Value | Meaning |
|-------|---------|
| `""` | Wildcard — matches any value in that field |
| `"98912*"` | Prefix — matches any string starting with `98912` |
| `"989125305483"` | Exact — matches only this exact string |

---

## Extending the Library

### Adding a new matchable field

1. Add the field to `Route` and `MessageContext` in `route.hpp`.
2. Add a `Pattern` member to `CompiledRoute` in `routing_table.hpp`.
3. Parse it in `CompiledRoute::compile()` in `routing_table.cpp`.
4. Add it to `total_specificity` in `compile()`.
5. Decide whether to index it with a `Trie` (worthwhile for fields with many
   prefix patterns) or check it inline in `RoutingTable::find()` (simpler, fine
   for low-cardinality fields like `pdu_type`).
6. Add the inline check in `RoutingTable::find()` alongside `from_pat` and
   `pdu_pat`.

### Switching to a read-write lock

If `add()` / `remove()` frequency grows to the point where the rebuild cost
matters, the RCU approach can be replaced with a `std::shared_mutex`:

```cpp
mutable std::shared_mutex rw_mutex_;

// find(): shared (read) lock
std::shared_lock lk(rw_mutex_);

// add() / remove(): exclusive (write) lock
std::unique_lock lk(rw_mutex_);
```

This avoids the full rebuild but means readers block during writes. For the
typical case of infrequent mutations the current RCU approach is preferred.

### Supporting regex patterns

Replace `PatternKind` with a `std::regex` member in `Pattern` and update
`Pattern::match()`. Note that regex matching is significantly slower than
`memcmp`; if used, the trie cannot index regex patterns and those routes must
fall back to a linear scan.
