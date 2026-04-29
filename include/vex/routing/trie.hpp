#pragma once

#include <array>
#include <string>
#include <vector>

#include "pattern.hpp"

namespace routing
{

/// A character trie used to efficiently match prefix and exact patterns
/// against an input string.
///
/// Each node stores the indices (into the sorted route table) of routes
/// whose pattern terminates at that node.  Wildcard patterns (empty string)
/// are kept in a flat side-list and are always included in every collect().
class Trie
{
  public:
    Trie();

    /// Insert a route index for the given pattern.
    void insert(const Pattern& pat, int route_idx);

    /// Walk the trie along `input` and append all matching route indices
    /// (wildcards + every prefix/exact that matches) to `out`.
    void collect(const std::string& input, std::vector<int>& out) const;

  private:
    struct Node
    {
        std::array<int, 128> children;
        std::vector<int> route_indices;
        Node()
        {
            children.fill(-1);
        }
    };

    std::vector<Node> nodes_;
    std::vector<int> wildcards_;
};

}  // namespace routing
