#include "json_io.hpp"

namespace dpe {

using json = nlohmann::json;

json offer_to_json(const Offer& o) {
    return json{
        {"product_id", o.product_id},
        {"supplier_id", o.supplier_id},
        {"supplier_name", o.supplier_name},
        {"region_id", o.region_id},
        {"region_name", o.region_name},
        {"currency", o.currency},
        {"unit_price", o.unit_price},
        {"unit_price_pln", o.unit_price_pln},
        {"landing_cost_pln", o.landing_cost_pln},
        {"profit_per_unit", o.profit_per_unit},
        {"profit_margin", o.profit_margin},
        {"profit_margin_pct", o.profit_margin * 100.0},
        {"freight_pln", o.freight_pln},
        {"duty_pln", o.duty_pln},
        {"vat_pln", o.vat_pln},
        {"lead_days", o.lead_days},
        {"success_rate", o.success_rate},
        {"opportunity", o.opportunity},
    };
}

json row_to_json(const ProductRow& r) {
    return json{
        {"id", r.id},
        {"name", r.name},
        {"category", r.category},
        {"local_price_pln", r.local_price_pln},
        {"demand", r.demand},
        {"popularity", r.popularity},
        {"weight_kg", r.weight_kg},
        {"success_rate", r.success_rate},
        {"profit_margin", r.profit_margin},
        {"profit_per_unit", r.profit_per_unit},
        {"opportunity", r.opportunity},
        {"best_offer", offer_to_json(r.best_offer)},
    };
}

json rows_to_json(const std::vector<ProductRow>& rows) {
    json arr = json::array();
    for (const auto& r : rows) {
        json j = row_to_json(r);
        json offers = json::array();
        for (const auto& o : r.offers) offers.push_back(offer_to_json(o));
        j["offers"] = offers;
        arr.push_back(std::move(j));
    }
    return arr;
}

}  // namespace dpe
