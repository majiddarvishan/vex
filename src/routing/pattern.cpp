#include "vex/routing/pattern.hpp"

#include <cstring>

namespace routing
{

Pattern Pattern::parse(const std::string& s)
{
    if (s.empty())
        return {PatternKind::Wildcard, "", 0};

    if (s.back() == '*')
    {
        std::string prefix = s.substr(0, s.size() - 1);
        return {PatternKind::Prefix, prefix, static_cast<int>(prefix.size())};
    }

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
