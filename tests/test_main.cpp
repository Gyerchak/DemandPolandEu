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
    if (!cond) {
        printf("FAIL %s\n", name);
        ++failures;
    }
}

int main() {
    // success_rate
    check_close(success_rate(0.9, 0.5), 0.7, "success_rate default");
    check_close(success_rate(1.0, 1.0), 1.0, "success_rate max");
    check_close(success_rate(0.0, 0.0), 0.0, "success_rate zero");

    SuccessWeights w{0.3, 0.7};
    check_close(success_rate(1.0, 0.0, w), 0.3, "success_rate custom pop");
    check_close(success_rate(0.0, 1.0, w), 0.7, "success_rate custom dem");

    // clamping
    check_close(success_rate(2.0, -1.0), 0.5, "success_rate clamped");
    check_close(success_rate(-5.0, 5.0), 0.5, "success_rate clamped2");

    // opportunity_score
    check_close(opportunity_score(0.0, 0.3), 0.0, "opp zero sr");
    check_close(opportunity_score(0.9, 0.0), 0.0, "opp zero margin");
    check_close(opportunity_score(0.9, 0.4), 100 * 0.9 * std::sqrt(0.4 / 0.3), "opp example");
    check_close(opportunity_score(0.3, 0.6), 100 * 0.3 * std::sqrt(0.6 / 0.3), "opp example2");

    // monotonic
    double base = opportunity_score(0.5, 0.3);
    check(opportunity_score(0.9, 0.3) > base, "opp monotonic sr");
    check(opportunity_score(0.5, 0.6) > base, "opp monotonic margin");

    // margin_ref validation
    bool threw = false;
    try {
        (void)opportunity_score(0.5, 0.3, 0.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "opp margin_ref=0 throws");

    // landing_cost sanity: no VAT
    Region r;
    r.freight_per_unit_pln = 5.0;
    r.freight_per_kg_pln = 2.0;
    r.duty_rate = 0.1;
    r.handling_pln = 8.0;
    r.vat_rate = 0.23;
    Product p;
    p.weight_kg = 3.0;
    auto cost = landing_cost(100.0, r, p, false);
    // freight = 5 + 2*3 = 11; duty = 100*0.1 + 8 = 18; subtotal=129; total=129
    check_close(cost.freight, 11.0, "freight");
    check_close(cost.duty, 18.0, "duty");
    check_close(cost.total, 129.0, "total no vat");
    auto cost_vat = landing_cost(100.0, r, p, true);
    check_close(cost_vat.total, 129.0 * 1.23, "total with vat");

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    printf("%d test(s) failed.\n", failures);
    return 1;
}
