#include <vex/routing/route_matcher.hpp>

#include <cassert>
#include <string>
#include <vector>

using namespace vex::routing;

static void test_case_insensitive_route_fields() {
    // from: normalized to lowercase by Route::normalize
    // other fields: source/destination/pdu_type/target normalized to lowercase by Route::normalize
    Route r{
        1,
        10,
        "SENDER",
        "src",
        "dst",
        "PDU",
        "TARGET"
    };

    RouteMatcher matcher({r});

    MessageContext ctx{
        "sender",
        "src",
        "dst",
        "pdu"
    };

    const CompiledRoute* cr = matcher.find(ctx);
    assert(cr != nullptr);
}

static void test_wildcards_and_prefix_are_case_insensitive() {
    // Use prefix patterns for from and exact patterns for addresses/type via wildcards.
    // Wildcard syntax: "*" becomes PatternKind::Wildcard and should match everything.
    Route r{
        2,
        1,
        "SEN*",
        "*",
        "*",
        "pdu*",
        "*"
    };

    RouteMatcher matcher({r});

    MessageContext ctx{
        "SeNdErX",
        "anything",
        "anything2",
        "PDU123"
    };

    const CompiledRoute* cr = matcher.find(ctx);
    assert(cr != nullptr);
}

static void test_literal_star_is_exact() {
    // "a*b" contains '*' not at the end => should be treated as an Exact match (literal '*').
    Route r{
        3,
        5,
        "a*b",
        "*",
        "*",
        "*",
        "*"
    };

    RouteMatcher matcher({r});

    MessageContext ctx1{
        "a*b",
        "anything",
        "anything2",
        "anything3"
    };

    const CompiledRoute* cr1 = matcher.find(ctx1);
    assert(cr1 != nullptr);

    MessageContext ctx2{
        "ab",
        "anything",
        "anything2",
        "anything3"
    };

    const CompiledRoute* cr2 = matcher.find(ctx2);
    assert(cr2 == nullptr);
}

int main() {
    test_case_insensitive_route_fields();
    test_wildcards_and_prefix_are_case_insensitive();
    test_literal_star_is_exact();
    return 0;
}
