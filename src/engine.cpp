#include "engine.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace dpe {

double clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

double success_rate(double popularity, double demand) {
    return clamp01(0.5 * clamp01(popularity) + 0.5 * clamp01(demand));
}

double opportunity_score(double sr, double margin, double margin_ref) {
    sr = clamp01(sr);
    if (margin_ref <= 0.0) throw std::invalid_argument("margin_ref must be > 0");
    double ratio = std::max(0.0, margin / margin_ref);
    return 100.0 * sr * std::sqrt(ratio);
}

static double get_double(const json& j, const std::string& key, double def) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    if (it->is_number()) return it->get<double>();
    return def;
}

static std::string lowercase(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

static json read_json(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return json::parse(ss.str());
}

std::vector<Market> load_markets(const std::string& dir) {
    std::vector<Market> out;
    json j = read_json(dir + "/data/markets.json");
    for (const auto& e : j.at("markets")) {
        Market m;
        m.id = e.value("id", "");
        m.name = e.value("name", "");
        m.role = e.value("role", "other");
        m.watch = e.value("watch", false);
        m.currency = e.value("currency", "EUR");
        m.fx_to_eur = get_double(e, "fx_to_eur", 1.0);
        m.freight_per_kg_eur = get_double(e, "freight_per_kg_eur", 0.0);
        m.freight_per_unit_eur = get_double(e, "freight_per_unit_eur", 0.0);
        m.duty_rate = get_double(e, "duty_rate", 0.0);
        m.handling_eur = get_double(e, "handling_eur", 0.0);
        m.vat_rate = get_double(e, "vat_rate", 0.0);
        m.lead_days = e.value("lead_days", 30);
        m.note = e.value("note", "");
        m.members = e.value("members", "");
        auto sh = e.find("shops");
        if (sh != e.end() && sh->is_array())
            for (const auto& s : *sh) if (s.is_string()) m.shops.push_back(s.get<std::string>());
        m.search = e.value("search", "");
        auto dp = e.find("deep");
        if (dp != e.end() && dp->is_array()) {
            for (const auto& pair : *dp) {
                if (pair.is_array() && pair.size() >= 2 &&
                    pair[0].is_string() && pair[1].is_string()) {
                    m.deep.push_back({pair[0].get<std::string>(), pair[1].get<std::string>()});
                }
            }
        }
        out.push_back(std::move(m));
    }
    return out;
}

std::vector<Product> load_products(const std::string& dir) {
    std::vector<Product> out;
    json j = read_json(dir + "/data/products.json");
    for (const auto& e : j.at("products")) {
        Product p;
        p.id = e.value("id", "");
        p.name = e.value("name", "");
        p.brand = e.value("brand", "");
        p.model = e.value("model", "");
        p.ean = e.value("ean", "");
        p.category = e.value("category", "");
        p.weight_kg = get_double(e, "weight_kg", 0.0);
        p.supplier_url = e.value("supplier_url", "");
        auto mk = e.find("markets");
        if (mk != e.end() && mk->is_object()) {
            for (auto it = mk->begin(); it != mk->end(); ++it) {
                ProductMarket pm;
                pm.buy = get_double(*it, "buy", 0.0);
                pm.sell = get_double(*it, "sell", 0.0);
                pm.demand = get_double(*it, "demand", 0.0);
                pm.popularity = get_double(*it, "popularity", 0.0);
                pm.supplier_url = it->value("supplier_url", "");
                pm.price_source = it->value("price_source", "est");
                pm.link_checked = it->value("link_checked", "");
                auto w = it->find("whole");
                if (w != it->end() && w->is_object()) {
                    pm.whole_sell = get_double(*w, "sell", 0.0);
                    pm.whole_demand = get_double(*w, "demand", 0.0);
                    pm.whole_popularity = get_double(*w, "popularity", 0.0);
                }
                p.markets[it.key()] = pm;
            }
        }
        out.push_back(std::move(p));
    }
    return out;
}

std::string resolve_category(const Product& p, const std::string& dir) {
    if (!p.category.empty()) return p.category;
    json cats = json::object();
    try {
        cats = read_json(dir + "/data/categories.json");
    } catch (...) { cats = json::object(); }
    std::string hay = lowercase(p.name + " " + p.id);
    for (auto it = cats.begin(); it != cats.end(); ++it) {
        const std::string& cat = it.key();
        const json& c = it.value();
        auto kw = c.find("keywords");
        if (kw == c.end() || !kw->is_array()) continue;
        for (const auto& k : *kw) {
            if (k.is_string() && hay.find(lowercase(k.get<std::string>())) != std::string::npos) {
                return cat;
            }
        }
    }
    return "unknown";
}

static double local_to_eur(double local, const Market& m) {
    return local * m.fx_to_eur;
}

std::vector<Trade> build_trades(const std::string& dir,
                                const std::string& home_market_id,
                                const std::vector<std::string>& watch_markets,
                                bool include_sale_vat,
                                double margin_ref,
                                const std::string& channel) {
    const bool whole = (channel == "whole");
    auto markets = load_markets(dir);
    auto products = load_products(dir);

    std::map<std::string, const Market*> market_by_id;
    for (const auto& m : markets) market_by_id[m.id] = &m;

    const Market* home = nullptr;
    auto hit = market_by_id.find(home_market_id);
    if (hit != market_by_id.end()) home = hit->second;
    if (home == nullptr) {
        for (const auto& m : markets) if (m.role == "home") { home = &m; break; }
    }
    if (home == nullptr) throw std::runtime_error("no home market");

    std::set<std::string> watch;
    for (const auto& id : watch_markets) if (market_by_id.count(id)) watch.insert(id);

    std::vector<Trade> out;
    for (const auto& product : products) {
        auto ph = product.markets.find(home->id);
        if (ph == product.markets.end()) continue;

        for (const auto& entry : product.markets) {
            const std::string& mid = entry.first;
            const ProductMarket& pm = entry.second;
            if (mid == home->id) continue;
            if (!watch.empty() && !watch.count(mid)) continue;
            auto mit = market_by_id.find(mid);
            if (mit == market_by_id.end()) continue;
            const Market& m = *mit->second;
            if (pm.buy <= 0.0 || pm.sell <= 0.0) continue;
            if (ph->second.sell <= 0.0 || ph->second.buy <= 0.0) continue;

            const std::string cat = resolve_category(product, dir);

            {
                const ProductMarket& hp = ph->second;
                double sell_ref = whole && hp.whole_sell > 0 ? hp.whole_sell : hp.sell;
                double dem_ref  = whole ? hp.whole_demand  : hp.demand;
                double pop_ref  = whole ? hp.whole_popularity : hp.popularity;
                double buy_eur = local_to_eur(pm.buy, m);
                double sell_eur = local_to_eur(sell_ref, *home);
                double freight = m.freight_per_unit_eur + m.freight_per_kg_eur * product.weight_kg;
                double duty = buy_eur * m.duty_rate;
                double handling = m.handling_eur;
                double cost = buy_eur + freight + duty + handling;
                double vat = include_sale_vat ? sell_eur * home->vat_rate : 0.0;
                double total = cost + vat;
                double profit = sell_eur - total;
                double margin = sell_eur != 0.0 ? profit / sell_eur : 0.0;
                double sr = success_rate(pop_ref, dem_ref);

                Trade t;
                t.kind = "import";
                t.product_id = product.id;
                t.product_name = product.name;
                t.brand = product.brand;
                t.model = product.model;
                t.ean = product.ean;
                t.supplier_url = pm.supplier_url;      // buy market = from_market
                t.price_source = pm.price_source;
                t.link_checked = pm.link_checked;
                t.channel = channel;
                t.shops = whole ? home->shops : std::vector<std::string>{};
                t.category = cat;
                t.from_market_id = m.id;
                t.from_market = m.name;
                t.to_market_id = home->id;
                t.to_market = home->name;
                t.buy_eur = buy_eur;
                t.sell_eur = sell_eur;
                t.freight_eur = freight;
                t.duty_eur = duty;
                t.handling_eur = handling;
                t.vat_eur = vat;
                t.cost_eur = cost;
                t.total_eur = total;
                t.profit_eur = profit;
                t.margin = margin;
                t.success_rate = sr;
                t.opportunity = opportunity_score(sr, margin, margin_ref);
                t.lead_days = m.lead_days;
                out.push_back(std::move(t));
            }

            {
                const ProductMarket& hp = ph->second;
                double sell_ref = whole && pm.whole_sell > 0 ? pm.whole_sell : pm.sell;
                double dem_ref  = whole ? pm.whole_demand  : pm.demand;
                double pop_ref  = whole ? pm.whole_popularity : pm.popularity;
                double buy_eur = local_to_eur(hp.buy, *home);
                double sell_eur = local_to_eur(sell_ref, m);
                double freight = home->freight_per_unit_eur + home->freight_per_kg_eur * product.weight_kg;
                double duty = buy_eur * m.duty_rate;
                double handling = m.handling_eur;
                double cost = buy_eur + freight + duty + handling;
                double vat = include_sale_vat ? sell_eur * m.vat_rate : 0.0;
                double total = cost + vat;
                double profit = sell_eur - total;
                double margin = sell_eur != 0.0 ? profit / sell_eur : 0.0;
                double sr = success_rate(pop_ref, dem_ref);

                Trade t;
                t.kind = "export";
                t.product_id = product.id;
                t.product_name = product.name;
                t.brand = product.brand;
                t.model = product.model;
                t.ean = product.ean;
                t.supplier_url = hp.supplier_url;   // buy market = home
                t.price_source = hp.price_source;
                t.link_checked = hp.link_checked;
                t.channel = channel;
                t.shops = whole ? m.shops : std::vector<std::string>{};
                t.category = cat;
                t.from_market_id = home->id;
                t.from_market = home->name;
                t.to_market_id = m.id;
                t.to_market = m.name;
                t.buy_eur = buy_eur;
                t.sell_eur = sell_eur;
                t.freight_eur = freight;
                t.duty_eur = duty;
                t.handling_eur = handling;
                t.vat_eur = vat;
                t.cost_eur = cost;
                t.total_eur = total;
                t.profit_eur = profit;
                t.margin = margin;
                t.success_rate = sr;
                t.opportunity = opportunity_score(sr, margin, margin_ref);
                t.lead_days = home->lead_days + m.lead_days;
                out.push_back(std::move(t));
            }
        }
    }
    return out;
}

void sort_trades(std::vector<Trade>& trades, const std::string& key) {
    auto val = [&](const Trade& t) -> double {
        if (key == "profit") return t.profit_eur;
        if (key == "margin") return t.margin;
        if (key == "success_rate") return t.success_rate;
        if (key == "sell") return t.sell_eur;
        return t.opportunity;
    };
    std::sort(trades.begin(), trades.end(),
              [&](const Trade& a, const Trade& b) { return val(a) > val(b); });
}

std::vector<std::string> list_categories(const std::string& dir) {
    std::set<std::string> cats;
    for (const auto& p : load_products(dir)) cats.insert(resolve_category(p, dir));
    std::vector<std::string> out(cats.begin(), cats.end());
    return out;
}

}  // namespace dpe
