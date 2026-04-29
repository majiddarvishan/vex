#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace routing
{
enum class PatternKind : uint8_t
{
    Wildcard,  // empty string — matches everything, specificity = 0
    Prefix,    // "98912*"      — prefix match
    Exact      // "989125305483" — full string match
};

struct Pattern
{
    PatternKind kind = PatternKind::Wildcard;
    std::string value;    // prefix (without '*') or exact string
    int specificity = 0;  // length of value; 0 for wildcard

    /// Parse a raw pattern string into a Pattern.
    static Pattern parse(const std::string& s);

    /// Returns match specificity (>= 0) on success, -1 on no match.
    int match(const std::string& input) const;
};
}  // namespace routing
