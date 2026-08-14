#pragma once

#include "engine.hpp"

#include <nlohmann/json.hpp>

namespace dpe {

nlohmann::json offer_to_json(const Offer& o);
nlohmann::json row_to_json(const ProductRow& r);
nlohmann::json rows_to_json(const std::vector<ProductRow>& rows);

}  // namespace dpe
