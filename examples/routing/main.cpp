#include <vex/routing/router.hpp>

#include <cstring>
#include <iostream>

// ── Helpers ───────────────────────────────────────────────────────────────────

static void check(const vex::routing::RouteMatcher& matcher, const vex::routing::MessageContext& ctx, const char* expected)
{
    const auto* r = matcher.find(ctx);
    const char* got = r ? r->target.c_str() : nullptr;

    bool ok = (got == expected) || (got && expected && std::strcmp(got, expected) == 0);

    std::cout << (ok ? "PASS" : "FAIL") << "  from=\"" << ctx.from << "\" src=\"" << ctx.source_address << "\" dst=\""
              << ctx.destination_address << "\" pdu=\"" << ctx.pdu_type << "\""
              << "  ->  " << (got ? got : "nullptr");

    if (!ok)
        std::cout << "  (expected: " << (expected ? expected : "nullptr") << ")";

    std::cout << '\n';
}

static void section(const char* title)
{
    std::cout << "\n=== " << title << " ===\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main()
{
    // ── Initial route table ───────────────────────────────────────────────
    std::vector<vex::routing::Route> initial = {
        // id  pri  from        src              dst         pdu        target
        {1, 1, "", "", "982000*", "deliver", "smpp_client_0"},
        {2, 1, "", "", "983000*", "deliver", "smpp_client_1"},
        {3, 1, "", "98912*", "", "deliver", "smpp_client_2"},
        {4, 1, "client_1", "", "", "", "smpp_client_3"},
        {5, 1, "", "989125305483", "", "", "smpp_client_4"},
        {6, 1, "", "98911*", "", "", "smpp_client_5"},
        {7, 2, "", "98911*", "", "", "smpp_client_6"},
    };

    vex::routing::RouteMatcher matcher(initial);

    section("Initial routes");
    check(matcher, {"", "", "9820001234", "deliver"}, "smpp_client_0");
    check(matcher, {"", "", "9830001234", "deliver"}, "smpp_client_1");
    check(matcher, {"", "989121234", "", "deliver"}, "smpp_client_2");
    check(matcher, {"client_1", "", "", ""}, "smpp_client_3");
    check(matcher, {"", "989125305483", "", ""}, "smpp_client_4");  // exact beats prefix
    check(matcher, {"", "989111234", "", ""}, "smpp_client_6");     // priority 2 > priority 1
    check(matcher, {"", "NOMATCH", "", ""}, nullptr);

    // ── Remove route 5 (exact source match) ──────────────────────────────
    section("After remove(5) — exact src route gone");
    matcher.remove(5);
    // Route 5 removed. "989125305483" now matches nothing because
    // route 3 (98912*) requires pdu_type="deliver".
    check(matcher, {"", "989125305483", "", ""}, nullptr);
    check(matcher, {"", "989125305483", "", "deliver"}, "smpp_client_2");  // falls through to prefix

    // ── Add a catch-all route ─────────────────────────────────────────────
    section("After add(99) — catch-all");
    matcher.add({99, 0, "", "", "", "", "smpp_catch_all"});
    check(matcher, {"", "ANYTHING", "", ""}, "smpp_catch_all");
    check(matcher, {"", "", "9820001234", "deliver"}, "smpp_client_0");  // higher priority still wins

    // ── Replace route 1 with a new target ────────────────────────────────
    section("After replacing route 1 with new target");
    matcher.add({1, 1, "", "", "982000*", "deliver", "smpp_client_NEW"});
    check(matcher, {"", "", "9820001234", "deliver"}, "smpp_client_NEW");

    // ── Remove catch-all ──────────────────────────────────────────────────
    section("After remove(99) — catch-all gone");
    matcher.remove(99);
    check(matcher, {"", "ANYTHING", "", ""}, nullptr);

    std::cout << "\nFinal route count: " << matcher.size() << '\n';
}
