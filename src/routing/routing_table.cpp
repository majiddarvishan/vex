#include "router/routing_table.hpp"

#include <algorithm>
#include <climits>
#include <vector>

namespace routing
{

// ── CompiledRoute ─────────────────────────────────────────────────────────────

CompiledRoute CompiledRoute::compile(const Route& r)
{
    CompiledRoute cr;
    cr.id = r.id;
    cr.priority = r.priority;
    cr.from_pat = Pattern::parse(r.from);
    cr.src_pat = Pattern::parse(r.source_address);
    cr.dst_pat = Pattern::parse(r.destination_address);
    cr.pdu_pat = Pattern::parse(r.pdu_type);
    cr.total_specificity = cr.from_pat.specificity + cr.src_pat.specificity + cr.dst_pat.specificity + cr.pdu_pat.specificity;
    cr.target = r.target;
    return cr;
}

// ── RoutingTable ──────────────────────────────────────────────────────────────

std::shared_ptr<RoutingTable> RoutingTable::build(const std::vector<Route>& routes)
{
    auto tbl = std::make_shared<RoutingTable>();

    tbl->routes_.reserve(routes.size());
    for (const auto& r : routes)
        tbl->routes_.push_back(CompiledRoute::compile(r));

    // Sort order:
    //   1. Higher priority number wins (descending).
    //   2. More specific pattern wins (descending total_specificity).
    //   3. Lower route id wins (ascending) — stable tiebreaker.
    std::sort(tbl->routes_.begin(), tbl->routes_.end(), [](const CompiledRoute& a, const CompiledRoute& b) {
        if (a.priority != b.priority)
            return a.priority > b.priority;
        if (a.total_specificity != b.total_specificity)
            return a.total_specificity > b.total_specificity;
        return a.id < b.id;
    });

    for (int i = 0; i < static_cast<int>(tbl->routes_.size()); ++i)
    {
        tbl->src_trie_.insert(tbl->routes_[i].src_pat, i);
        tbl->dst_trie_.insert(tbl->routes_[i].dst_pat, i);
    }

    return tbl;
}

const CompiledRoute* RoutingTable::find(const MessageContext& ctx) const
{
    // Thread-local buffers avoid heap allocation on every call.
    thread_local std::vector<int> src_cands;
    thread_local std::vector<int> dst_cands;
    thread_local std::vector<bool> in_src;

    src_cands.clear();
    dst_cands.clear();

    src_trie_.collect(ctx.source_address, src_cands);
    dst_trie_.collect(ctx.destination_address, dst_cands);

    // Mark which route indices appeared in the src results.
    if (in_src.size() < routes_.size())
        in_src.assign(routes_.size(), false);
    else
        std::fill(in_src.begin(), in_src.end(), false);

    for (int idx : src_cands)
        in_src[idx] = true;

    // Iterate dst candidates. Because routes_ is pre-sorted, the first index
    // that also appears in src_cands and passes the remaining field checks
    // is the winner — but we still need the lowest index overall, so we track
    // best_rank instead of returning on first hit.
    const CompiledRoute* best = nullptr;
    int best_rank = INT32_MAX;

    for (int idx : dst_cands)
    {
        if (!in_src[idx])
            continue;
        if (idx >= best_rank)
            continue;  // already have a better-ranked candidate

        const CompiledRoute& cr = routes_[idx];
        if (cr.from_pat.match(ctx.from) < 0)
            continue;
        if (cr.pdu_pat.match(ctx.pdu_type) < 0)
            continue;

        best = &cr;
        best_rank = idx;
    }

    return best;
}

}  // namespace routing
