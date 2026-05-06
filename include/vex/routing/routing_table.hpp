#pragma once

#include <memory>
#include <vector>

#include "pattern.hpp"
#include "route.hpp"
#include "trie.hpp"

namespace vex
{
namespace routing
{
/// A compiled, immutable snapshot of the routing table.
///
/// Built once from a list of raw Route entries; after construction it is
/// never mutated, so it can be read concurrently without any locking.
struct CompiledRoute
{
    int id;
    int priority;
    Pattern from_pat;
    Pattern src_pat;
    Pattern dst_pat;
    Pattern pdu_pat;
    int total_specificity;  // sum of all pattern specificities
    std::string target;

    static CompiledRoute compile(const Route& r);
};

class RoutingTable
{
  public:
    /// Build a sorted, indexed routing table from a list of raw routes.
    /// Higher priority number wins; ties broken by total specificity (longer
    /// match wins), then by route id (lower id wins).
    static std::shared_ptr<RoutingTable> build(const std::vector<Route>& routes);

    /// Find the best matching compiled route, or nullptr if none matches.
    /// This method is thread-safe for concurrent reads.
    const CompiledRoute* find(const MessageContext& ctx) const;

    std::size_t size() const
    {
        return routes_.size();
    }

  private:
    std::vector<CompiledRoute> routes_;  // sorted by (priority DESC, specificity DESC, id ASC)
    Trie src_trie_;
    Trie dst_trie_;
};
}  // namespace routing
}