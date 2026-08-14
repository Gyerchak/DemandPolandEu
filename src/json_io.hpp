#pragma once

#include "engine.hpp"

#include <nlohmann/json.hpp>

namespace dpe {

nlohmann::json trade_to_json(const Trade& t);
nlohmann::json trades_to_json(const std::vector<Trade>& trades);
nlohmann::json market_to_json(const Market& m);
nlohmann::json markets_to_json(const std::vector<Market>& markets);

}  // namespace dpe
