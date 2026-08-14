#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "engine.hpp"

using namespace dpe;

static int failures = 0;

static void check_close(double actual, double expected, const char* name) {
    if (std::fabs(actual - expected) > 1e-6) {
        printf("FAIL %s: got %f expected %f\n", name, actual, expected);
        ++failures;
    }
}

static void check(bool cond, const char* name) {
    if (!cond) { printf("FAIL %s\n", name); ++failures; }
}

int main() {
    check_close(success_rate(0.9, 0.5), 0.7, "success_rate default");
    check_close(success_rate(1.0, 1.0), 1.0, "success_rate max");
    check_close(success_rate(0.0, 0.0), 0.0, "success_rate zero");
    check_close(success_rate(2.0, -1.0), 0.5, "success_rate clamped");

    check_close(opportunity_score(0.0, 0.3), 0.0, "opp zero sr");
    check_close(opportunity_score(0.9, 0.0), 0.0, "opp zero margin");
    check_close(opportunity_score(0.9, 0.4), 100 * 0.9 * std::sqrt(0.4 / 0.3), "opp example");

    bool threw = false;
    try { (void)opportunity_score(0.5, 0.3, 0.0); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "opp margin_ref=0 throws");

    // resolve_category: empty -> keyword fallback, else unknown
    Product p;
    p.name = "Portable solar charger 100W";
    p.id = "solar-charger";
    std::string dir = ".";
    std::string cat = resolve_category(p, dir);
    printf("category fallback for solar-charger: '%s'\n", cat.c_str());
    check(!cat.empty(), "category non-empty");

    Product p2;
    p2.name = "Mystery widget XYZ";
    p2.id = "mystery-widget";
    std::string cat2 = resolve_category(p2, dir);
    printf("category fallback for mystery widget: '%s'\n", cat2.c_str());
    check(cat2 == "unknown", "unknown fallback");

    if (failures == 0) { printf("All tests passed.\n"); return 0; }
    printf("%d test(s) failed.\n", failures);
    return 1;
}
