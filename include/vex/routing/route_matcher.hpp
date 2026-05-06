#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "route.hpp"
#include "routing_table.hpp"

namespace vex
{
namespace routing
{

/// Thread-safe route matcher with runtime add/remove support.
///
/// Threading model (RCU-style):
///   - find()          Lock-free on the read path. Callers take an atomic
///                     shared_ptr snapshot and read from an immutable table.
///   - add() / remove() Serialised by an internal write mutex. After mutating
///                     the canonical route list the table is rebuilt and
///                     published atomically. In-flight find() calls finish
///                     against the previous snapshot; the old table is freed
///                     automatically when the last reader drops its reference.
///
/// This means concurrent find() calls never block each other and are never
/// blocked by an add() or remove().
class RouteMatcher
{
  public:
    explicit RouteMatcher(const std::vector<Route>& initial = {});

    // ── Read path ─────────────────────────────────────────────────────────

    /// Find the best route for a message.  Returns nullptr if no route matches.
    /// Safe to call from multiple threads simultaneously without locking.
    const CompiledRoute* find(const MessageContext& ctx) const;

    // ── Write path ────────────────────────────────────────────────────────

    /// Add a new route, or replace the existing route with the same id.
    void add(const Route& route);

    /// Remove the route with the given id.
    /// Returns true if a route was found and removed, false otherwise.
    bool remove(int id);

    // ── Introspection ─────────────────────────────────────────────────────

    std::size_t size() const;

    /// Return a copy of the current raw route list (for inspection/debugging).
    std::vector<Route> routes() const;

  private:
    void publish();  // must be called with write_mutex_ held

    std::vector<Route> raw_routes_;  // write-path source of truth
    mutable std::mutex write_mutex_;

    // updated gcc version
    // std::atomic<std::shared_ptr<RoutingTable>> table_;       // atomically published snapshot
    std::shared_ptr<RoutingTable> table_;
};
}  // namespace routing
}