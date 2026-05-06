#include "vex/routing/trie.hpp"

namespace routing
{

Trie::Trie()
{
    nodes_.emplace_back();  // root node at index 0
}

void Trie::insert(const Pattern& pat, int route_idx)
{
    if (pat.kind == PatternKind::Wildcard)
    {
        wildcards_.push_back(route_idx);
        return;
    }

    int node = 0;
    for (char c : pat.value)
    {
        auto uc = static_cast<unsigned char>(c);
        if (uc >= 128)
            break;  // non-ASCII: stop traversal

        if (nodes_[node].children[uc] == -1)
        {
            nodes_[node].children[uc] = static_cast<int>(nodes_.size());
            nodes_.emplace_back();
        }
        node = nodes_[node].children[uc];
    }

    nodes_[node].route_indices.push_back(route_idx);
}

void Trie::collect(const std::string& input, std::vector<int>& out) const
{
    // Wildcards always match
    out.insert(out.end(), wildcards_.begin(), wildcards_.end());

    int node = 0;
    for (char c : input)
    {
        auto uc = static_cast<unsigned char>(c);
        if (uc >= 128)
            break;

        int next = nodes_[node].children[uc];
        if (next == -1)
            break;

        node = next;

        // Routes stored at intermediate nodes are prefix routes whose prefix
        // equals the characters consumed so far.  Routes at the final node
        // after full traversal are exact matches (also collected here).
        if (!nodes_[node].route_indices.empty())
            out.insert(out.end(), nodes_[node].route_indices.begin(), nodes_[node].route_indices.end());
    }
}

}  // namespace routing
