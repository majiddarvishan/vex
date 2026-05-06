#include "vex/routing/route_matcher.hpp"

#include <algorithm>

namespace vex
{
namespace routing
{
RouteMatcher::RouteMatcher(const std::vector<Route>& initial)
    : raw_routes_(initial)
{
    // table_.store(RoutingTable::build(raw_routes_));//updateed version
    std::atomic_store(&table_, RoutingTable::build(raw_routes_));
}

// ── Read path ─────────────────────────────────────────────────────────────────

const CompiledRoute* RouteMatcher::find(const MessageContext& ctx) const
{
    // Atomically load the current table snapshot.  This increments the
    // shared_ptr refcount without acquiring any mutex, so concurrent find()
    // calls never block each other or a concurrent add()/remove().
    // auto tbl = table_.load(); //updated veresion
    auto tbl = std::atomic_load(&table_);
    return tbl->find(ctx);
}

// ── Write path ────────────────────────────────────────────────────────────────

void RouteMatcher::add(const Route& route)
{
    std::lock_guard<std::mutex> lk(write_mutex_);

    auto it = std::find_if(raw_routes_.begin(), raw_routes_.end(), [&](const Route& r) { return r.id == route.id; });
    if (it != raw_routes_.end())
        *it = route;  // replace existing route with same id
    else
        raw_routes_.push_back(route);

    publish();
}

bool RouteMatcher::remove(int id)
{
    std::lock_guard<std::mutex> lk(write_mutex_);

    auto it = std::find_if(raw_routes_.begin(), raw_routes_.end(), [&](const Route& r) { return r.id == id; });
    if (it == raw_routes_.end())
        return false;

    raw_routes_.erase(it);
    publish();
    return true;
}

// ── Introspection ─────────────────────────────────────────────────────────────

std::size_t RouteMatcher::size() const
{
    std::lock_guard<std::mutex> lk(write_mutex_);
    return raw_routes_.size();
}

std::vector<Route> RouteMatcher::routes() const
{
    std::lock_guard<std::mutex> lk(write_mutex_);
    return raw_routes_;
}

// ── Private ───────────────────────────────────────────────────────────────────

void RouteMatcher::publish()
{
    // Rebuild and atomically swap the immutable snapshot.
    // Called with write_mutex_ held, so only one rebuild runs at a time.
    // table_.store(RoutingTable::build(raw_routes_)); //update ed version
    std::atomic_store(&table_, RoutingTable::build(raw_routes_));
}
}  // namespace routing
}