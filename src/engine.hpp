#pragma once

#include <map>
#include <string>
#include <vector>

namespace dpe {

struct Market {
    std::string id;
    std::string name;
    std::string role;          // home | main | other
    bool watch = false;        // main markets to watch
    std::string currency;
    double fx_to_eur = 1.0;
    double freight_per_kg_eur = 0.0;
    double freight_per_unit_eur = 0.0;
    double duty_rate = 0.0;    // import duty into this market
    double handling_eur = 0.0;
    double vat_rate = 0.0;     // VAT charged on a sale in this market
    int lead_days = 30;
    std::string note;
    std::string members;   // country list for legend (e.g. "PL, CZ, SK, HU")
    std::vector<std::string> shops;  // marketplaces/porównywarki-listed shops for Whole Market view
    std::string search;               // search template for this market's own platform ("{q}" = product)
    std::vector<std::vector<std::string>> deep;  // [platform,url-template] pairs for whole-market deep search
};

struct ProductMarket {
    double buy = 0.0;          // what you pay to acquire (local currency)
    double sell = 0.0;         // what a buyer pays you (local currency)
    double demand = 0.0;
    double popularity = 0.0;
    std::string supplier_url;  // where THIS market's buy price was found
    std::string price_source;  // "real" (verified listing) | "est" (sample estimate)
    std::string link_checked;  // date the supplier URL was last verified alive
    // Whole Market view: main platforms + all shops from price-comparison pages.
    double whole_sell = 0.0;
    double whole_demand = 0.0;
    double whole_popularity = 0.0;
};

struct Product {
    std::string id;
    std::string name;
    std::string brand;
    std::string model;
    std::string ean;           // the global SKU fingerprint — same product everywhere
    std::string category;      // broad, English; may be empty -> fallback
    std::string supplier_url;  // optional: direct supplier/product page
    double weight_kg = 0.0;
    std::map<std::string, ProductMarket> markets;  // market_id -> data
};

struct Trade {
    std::string kind;          // "import" | "export"
    std::string product_id;
    std::string product_name;
    std::string brand;
    std::string model;
    std::string ean;
    std::string category;
    std::string from_market_id;
    std::string from_market;
    std::string to_market_id;
    std::string to_market;
    std::string supplier_url;   // link = the BUY market's source (import: from-market; export: home)
    std::string price_source;   // "real" | "est"
    std::string link_checked;   // date last verified
    std::string channel;        // "main" | "whole"
    std::vector<std::string> shops;  // shops considered for this trade's sell side
    double buy_eur = 0.0;
    double sell_eur = 0.0;
    double freight_eur = 0.0;
    double duty_eur = 0.0;
    double handling_eur = 0.0;
    double vat_eur = 0.0;
    double cost_eur = 0.0;     // buy + freight + duty + handling
    double total_eur = 0.0;    // cost + vat (all payments)
    double profit_eur = 0.0;
    double margin = 0.0;       // profit / sell
    double success_rate = 0.0;
    double opportunity = 0.0;
    int lead_days = 0;
};

double clamp01(double v);
double success_rate(double popularity, double demand);
double opportunity_score(double sr, double margin, double margin_ref = 0.3);
std::string resolve_category(const Product& p, const std::string& dir);

std::vector<Market> load_markets(const std::string& dir);
std::vector<Product> load_products(const std::string& dir);

// Build import/export trades. home_market_id anchors the trade; watch_markets
// restricts the counterparty markets (empty = all markets).
std::vector<Trade> build_trades(const std::string& dir,
                                const std::string& home_market_id,
                                const std::vector<std::string>& watch_markets,
                                bool include_sale_vat,
                                double margin_ref = 0.3,
                                const std::string& channel = "main");
void sort_trades(std::vector<Trade>& trades, const std::string& key);
std::vector<std::string> list_categories(const std::string& dir);

}  // namespace dpe
