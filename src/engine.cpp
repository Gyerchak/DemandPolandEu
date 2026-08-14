#include "engine.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace dpe {

double clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

double success_rate(double popularity, double demand, const SuccessWeights& w) {
    return clamp01(w.popularity * clamp01(popularity) + w.demand * clamp01(demand));
}

double freight_cost(const Region& region, const Product& product) {
    return region.freight_per_unit_pln + region.freight_per_kg_pln * product.weight_kg;
}

double duty_cost(const Region& region, double supplier_price_pln, const Product& product) {
    // Region duty can be flat or product-specific (products don't carry one in the
    // current data, but the API mirrors the Python model).
    (void)product;
    return supplier_price_pln * region.duty_rate + region.handling_pln;
}

LandingCost landing_cost(double supplier_price_pln, const Region& region,
                         const Product& product, bool include_vat) {
    LandingCost c;
    c.supplier = supplier_price_pln;
    c.freight = freight_cost(region, product);
    c.duty = duty_cost(region, supplier_price_pln, product);
    double subtotal = c.supplier + c.freight + c.duty;
    if (include_vat) {
        c.vat = subtotal * region.vat_rate;
    }
    c.total = subtotal + c.vat;
    return c;
}

double opportunity_score(double sr, double margin, double margin_ref) {
    sr = clamp01(sr);
    if (margin_ref <= 0.0) {
        throw std::invalid_argument("margin_ref must be > 0");
    }
    double ratio = std::max(0.0, margin / margin_ref);
    return 100.0 * sr * std::sqrt(ratio);
}

static double get_double(const json& j, const std::string& key, double def) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    if (it->is_number()) return it->get<double>();
    return def;
}

std::vector<Product> load_products(const std::string& dir) {
    std::vector<Product> out;
    std::ifstream f(dir + "/data/products.json");
    if (!f.is_open()) throw std::runtime_error("cannot open products.json");
    json j;
    f >> j;
    for (const auto& e : j.at("products")) {
        Product p;
        p.id = e.value("id", "");
        p.name = e.value("name", "");
        p.category = e.value("category", "");
        p.local_price_pln = get_double(e, "local_price_pln", 0.0);
        p.demand = get_double(e, "demand", 0.0);
        p.popularity = get_double(e, "popularity", 0.0);
        p.weight_kg = get_double(e, "weight_kg", 0.0);
        out.push_back(std::move(p));
    }
    return out;
}

std::vector<Region> load_regions(const std::string& dir) {
    std::vector<Region> out;
    std::ifstream f(dir + "/data/regions.json");
    if (!f.is_open()) throw std::runtime_error("cannot open regions.json");
    json j;
    f >> j;
    for (const auto& e : j.at("regions")) {
        Region r;
        r.id = e.value("id", "");
        r.name = e.value("name", "");
        r.currency = e.value("currency", "");
        r.fx_to_pln = get_double(e, "fx_to_pln", 1.0);
        r.freight_per_kg_pln = get_double(e, "freight_per_kg_pln", 0.0);
        r.freight_per_unit_pln = get_double(e, "freight_per_unit_pln", 0.0);
        r.duty_rate = get_double(e, "duty_rate", 0.0);
        r.handling_pln = get_double(e, "handling_pln", 0.0);
        r.vat_rate = get_double(e, "vat_rate", 0.0);
        r.lead_days = e.value("lead_days", 30);
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<Supplier> load_suppliers(const std::string& dir) {
    std::vector<Supplier> out;
    std::ifstream f(dir + "/data/suppliers.json");
    if (!f.is_open()) throw std::runtime_error("cannot open suppliers.json");
    json j;
    f >> j;
    for (const auto& e : j.at("suppliers")) {
        Supplier s;
        s.id = e.value("id", "");
        s.name = e.value("name", "");
        s.region_id = e.value("region_id", "");
        s.lead_days = e.value("lead_days", 30);
        auto p = e.find("prices");
        if (p != e.end() && p->is_object()) {
            for (auto it = p->begin(); it != p->end(); ++it) {
                s.prices.emplace_back(it.key(), it.value().get<double>());
            }
        }
        out.push_back(std::move(s));
    }
    return out;
}

std::vector<ProductRow> build_rows(const std::string& dir, const SuccessWeights& weights,
                                   double margin_ref, bool include_vat) {
    auto products = load_products(dir);
    auto regions = load_regions(dir);
    auto suppliers = load_suppliers(dir);

    std::map<std::string, const Region*> region_by_id;
    for (const auto& r : regions) region_by_id[r.id] = &r;

    std::vector<ProductRow> rows;
    for (const auto& product : products) {
        const double sr = success_rate(product.popularity, product.demand, weights);
        const double local_price = product.local_price_pln;

        std::vector<Offer> offers;
        for (const auto& supplier : suppliers) {
            double unit_price = 0.0;
            bool found = false;
            for (const auto& [pid, price] : supplier.prices) {
                if (pid == product.id) {
                    unit_price = price;
                    found = true;
                    break;
                }
            }
            if (!found) continue;
            auto rit = region_by_id.find(supplier.region_id);
            if (rit == region_by_id.end()) continue;
            const Region& region = *rit->second;

            double unit_price_pln = unit_price * region.fx_to_pln;
            LandingCost cost = landing_cost(unit_price_pln, region, product, include_vat);
            double profit = local_price - cost.total;
            double margin = local_price != 0.0 ? profit / local_price : 0.0;

            Offer offer;
            offer.product_id = product.id;
            offer.supplier_id = supplier.id;
            offer.supplier_name = supplier.name;
            offer.region_id = region.id;
            offer.region_name = region.name;
            offer.currency = region.currency;
            offer.unit_price = unit_price;
            offer.unit_price_pln = unit_price_pln;
            offer.landing_cost_pln = cost.total;
            offer.profit_per_unit = profit;
            offer.profit_margin = margin;
            offer.freight_pln = cost.freight;
            offer.duty_pln = cost.duty;
            offer.vat_pln = cost.vat;
            offer.lead_days = supplier.lead_days != 0 ? supplier.lead_days : region.lead_days;
            offer.success_rate = sr;
            offer.opportunity = opportunity_score(sr, margin, margin_ref);
            offers.push_back(std::move(offer));
        }

        if (offers.empty()) continue;

        auto best = std::min_element(offers.begin(), offers.end(),
                                     [](const Offer& a, const Offer& b) {
                                         return a.landing_cost_pln < b.landing_cost_pln;
                                     });

        std::sort(offers.begin(), offers.end(),
                  [](const Offer& a, const Offer& b) {
                      return a.landing_cost_pln < b.landing_cost_pln;
                  });

        ProductRow row;
        row.id = product.id;
        row.name = product.name;
        row.category = product.category;
        row.local_price_pln = local_price;
        row.demand = product.demand;
        row.popularity = product.popularity;
        row.weight_kg = product.weight_kg;
        row.success_rate = sr;
        row.profit_margin = best->profit_margin;
        row.profit_per_unit = best->profit_per_unit;
        row.opportunity = best->opportunity;
        row.best_offer = *best;
        row.offers = offers;
        rows.push_back(std::move(row));
    }
    return rows;
}

void sort_rows(std::vector<ProductRow>& rows, const std::string& key) {
    auto val = [&](const ProductRow& r) -> double {
        if (key == "demand") return r.demand;
        if (key == "popularity") return r.popularity;
        if (key == "profit_margin") return r.profit_margin;
        if (key == "profit_per_unit") return r.profit_per_unit;
        if (key == "success_rate") return r.success_rate;
        return r.opportunity;
    };
    std::sort(rows.begin(), rows.end(),
              [&](const ProductRow& a, const ProductRow& b) { return val(a) > val(b); });
}

}  // namespace dpe
