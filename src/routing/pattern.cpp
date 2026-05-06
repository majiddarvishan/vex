#include "vex/routing/pattern.hpp"

#include <cstring>

namespace vex
{
namespace routing
{
Pattern Pattern::parse(const std::string& s)
{
    // Wildcard matches everything.
    // By convention: empty string or "*" represent a wildcard.
    if (s.empty() || s == "*")
        return {PatternKind::Wildcard, "", 0};

    // Prefix patterns must end with a single '*' and contain no other '*'.
    if (!s.empty() && s.back() == '*')
    {
        const auto star_pos = s.find('*');
        const bool single_star_at_end = (star_pos != std::string::npos) && (star_pos == s.size() - 1);

        if (single_star_at_end)
        {
            std::string prefix = s.substr(0, s.size() - 1);
            return {PatternKind::Prefix, prefix, static_cast<int>(prefix.size())};
        }
    }

    // Fallback: treat as an exact string (including any literal '*').
    return {PatternKind::Exact, s, static_cast<int>(s.size())};
}

int Pattern::match(const std::string& input) const
{
    switch (kind)
    {
        case PatternKind::Wildcard:
            return 0;

        case PatternKind::Exact:
            return (input == value) ? specificity : -1;

        case PatternKind::Prefix:
            if (input.size() >= static_cast<size_t>(specificity) && std::memcmp(input.data(), value.data(), specificity) == 0)
                return specificity;
            return -1;
    }
    return -1;  // unreachable
}
}  // namespace routing
}