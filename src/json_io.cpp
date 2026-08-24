#include "json_io.hpp"

namespace dpe {

using json = nlohmann::json;

json trade_to_json(const Trade& t) {
    return json{
        {"kind", t.kind},
        {"product_id", t.product_id},
        {"product_name", t.product_name},
        {"brand", t.brand},
        {"model", t.model},
        {"ean", t.ean},
        {"live", t.live},
        {"discovered", t.discovered},
        {"official", t.official},
        {"supplier_url", t.supplier_url},
        {"price_source", t.price_source},
        {"link_checked", t.link_checked},
        {"channel", t.channel},
        {"shops", t.shops},
        {"category", t.category},
        {"from_market_id", t.from_market_id},
        {"from_market", t.from_market},
        {"to_market_id", t.to_market_id},
        {"to_market", t.to_market},
        {"buy_eur", t.buy_eur},
        {"sell_eur", t.sell_eur},
        {"sell_est", t.sell_est},
        {"sell_low", t.sell_low},
        {"sell_high", t.sell_high},
        {"freight_eur", t.freight_eur},
        {"duty_eur", t.duty_eur},
        {"handling_eur", t.handling_eur},
        {"vat_eur", t.vat_eur},
        {"cost_eur", t.cost_eur},
        {"total_eur", t.total_eur},
        {"profit_eur", t.profit_eur},
        {"margin", t.margin},
        {"margin_pct", t.margin * 100.0},
        {"success_rate", t.success_rate},
        {"opportunity", t.opportunity},
        {"lead_days", t.lead_days},
    };
}

json trades_to_json(const std::vector<Trade>& trades) {
    json arr = json::array();
    for (const auto& t : trades) arr.push_back(trade_to_json(t));
    return arr;
}

json market_to_json(const Market& m) {
    return json{
        {"id", m.id},
        {"name", m.name},
        {"role", m.role},
        {"watch", m.watch},
        {"currency", m.currency},
        {"fx_to_eur", m.fx_to_eur},
        {"freight_per_kg_eur", m.freight_per_kg_eur},
        {"freight_per_unit_eur", m.freight_per_unit_eur},
        {"duty_rate", m.duty_rate},
        {"handling_eur", m.handling_eur},
        {"vat_rate", m.vat_rate},
        {"lead_days", m.lead_days},
        {"note", m.note},
        {"members", m.members},
        {"search", m.search},
        {"shops", m.shops},
        {"deep", m.deep},
    };
}

json markets_to_json(const std::vector<Market>& markets) {
    json arr = json::array();
    for (const auto& m : markets) arr.push_back(market_to_json(m));
    return arr;
}

}  // namespace dpe
