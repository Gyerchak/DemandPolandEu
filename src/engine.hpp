#pragma once

#include <string>
#include <vector>

namespace dpe {

struct SuccessWeights {
    double popularity = 0.5;
    double demand = 0.5;
};

struct Region {
    std::string id;
    std::string name;
    std::string currency;
    double fx_to_pln = 1.0;
    double freight_per_kg_pln = 0.0;
    double freight_per_unit_pln = 0.0;
    double duty_rate = 0.0;
    double handling_pln = 0.0;
    double vat_rate = 0.0;
    int lead_days = 30;
};

struct Product {
    std::string id;
    std::string name;
    std::string category;
    double local_price_pln = 0.0;
    double demand = 0.0;
    double popularity = 0.0;
    double weight_kg = 0.0;
};

struct Supplier {
    std::string id;
    std::string name;
    std::string region_id;
    int lead_days = 30;
    // prices: product_id -> unit price in region currency
    std::vector<std::pair<std::string, double>> prices;
};

struct LandingCost {
    double supplier = 0.0;
    double freight = 0.0;
    double duty = 0.0;
    double vat = 0.0;
    double total = 0.0;
};

struct Offer {
    std::string product_id;
    std::string supplier_id;
    std::string supplier_name;
    std::string region_id;
    std::string region_name;
    std::string currency;
    double unit_price = 0.0;      // in region currency
    double unit_price_pln = 0.0;  // converted to PLN
    double landing_cost_pln = 0.0;
    double profit_per_unit = 0.0;
    double profit_margin = 0.0;
    double freight_pln = 0.0;
    double duty_pln = 0.0;
    double vat_pln = 0.0;
    int lead_days = 0;
    double success_rate = 0.0;
    double opportunity = 0.0;
};

struct ProductRow {
    std::string id;
    std::string name;
    std::string category;
    double local_price_pln = 0.0;
    double demand = 0.0;
    double popularity = 0.0;
    double weight_kg = 0.0;
    double success_rate = 0.0;
    double profit_margin = 0.0;
    double profit_per_unit = 0.0;
    double opportunity = 0.0;
    Offer best_offer;
    std::vector<Offer> offers;  // sorted by landing cost
};

double clamp01(double v);
double success_rate(double popularity, double demand, const SuccessWeights& w = SuccessWeights{});
double freight_cost(const Region& region, const Product& product);
double duty_cost(const Region& region, double supplier_price_pln, const Product& product);
LandingCost landing_cost(double supplier_price_pln, const Region& region,
                         const Product& product, bool include_vat);
double opportunity_score(double sr, double margin, double margin_ref = 0.3);

// Data loading (JSON via nlohmann)
std::vector<Product> load_products(const std::string& dir);
std::vector<Region> load_regions(const std::string& dir);
std::vector<Supplier> load_suppliers(const std::string& dir);

// Analyzer
std::vector<ProductRow> build_rows(const std::string& dir,
                                   const SuccessWeights& weights = SuccessWeights{},
                                   double margin_ref = 0.3,
                                   bool include_vat = false);
void sort_rows(std::vector<ProductRow>& rows, const std::string& key);

}  // namespace dpe
