"""Core math for the DemandPolandEu monitor.

Landing cost, profit margin, success rate and the combined opportunity score.
All money values are PLN (PLN / zloty).

Terminology
-----------
* landing_cost   — full cost of getting one unit onto the Polish market:
                   supplier price (converted to PLN) + freight + customs/duties
                   + handling + optional VAT.
* profit_per_unit — local_price - landing_cost.
* profit_margin   — profit_per_unit / local_price (0..1, shown as %).
* success_rate    — probability (0..1) a product actually sells in Poland,
                   derived from popularity and demand signals. Default formula:
                   weighted average of popularity and demand.
* opportunity_score — the default sort key. Combines success_rate with
                   profit_margin into a single, mathematically safe and
                   intuitive "how good is this deal" value (0..100).
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Any

DEFAULT_SUCCESS_WEIGHTS = {"popularity": 0.5, "demand": 0.5}


@dataclass
class SuccessWeights:
    """Weights used in the success-rate formula (must sum to 1)."""

    popularity: float = 0.5
    demand: float = 0.5

    def as_dict(self) -> dict[str, float]:
        return {"popularity": self.popularity, "demand": self.demand}


def clamp(value: float, low: float = 0.0, high: float = 1.0) -> float:
    return max(low, min(high, value))


def success_rate(
    popularity: float,
    demand: float,
    weights: SuccessWeights | None = None,
) -> float:
    """Probability of actually selling the product (0..1).

    Default formula: 0.5 * popularity + 0.5 * demand.
    Both inputs are expected on a 0..1 scale.
    """
    w = weights or SuccessWeights()
    return clamp(w.popularity * clamp(popularity) + w.demand * clamp(demand))


def freight_cost(region: dict[str, Any], product: dict[str, Any]) -> float:
    """Freight per unit from the region to Poland (PLN).

    Freight can be flat per unit or per kg (or both).
    """
    per_unit = float(region.get("freight_per_unit_pln", 0.0))
    per_kg = float(region.get("freight_per_kg_pln", 0.0))
    weight = float(product.get("weight_kg", 0.0))
    return per_unit + per_kg * weight


def duty_cost(region: dict[str, Any], supplier_price_pln: float, product: dict[str, Any]) -> float:
    """Import duty (PLN). Region duty can be flat or product-specific."""
    rate = float(region.get("duty_rate", 0.0))
    rate = float(product.get("duty_rate", rate))
    handling = float(region.get("handling_pln", 0.0))
    return supplier_price_pln * rate + handling


def landing_cost(
    supplier_price_pln: float,
    region: dict[str, Any],
    product: dict[str, Any],
    include_vat: bool = False,
) -> dict[str, float]:
    """Full per-unit cost of landing the product in Poland (PLN)."""
    freight = freight_cost(region, product)
    duty = duty_cost(region, supplier_price_pln, product)
    subtotal = supplier_price_pln + freight + duty
    vat = 0.0
    if include_vat:
        vat_rate = float(region.get("vat_rate", 0.0))
        vat = subtotal * vat_rate
    return {
        "supplier": supplier_price_pln,
        "freight": freight,
        "duty": duty,
        "vat": vat,
        "total": subtotal + vat,
    }


def profit_metrics(
    local_price: float,
    supplier_price_pln: float,
    region: dict[str, Any],
    product: dict[str, Any],
    include_vat: bool = False,
) -> dict[str, float]:
    cost = landing_cost(supplier_price_pln, region, product, include_vat)
    profit = local_price - cost["total"]
    margin = profit / local_price if local_price else 0.0
    return {
        "landing_cost": cost,
        "profit_per_unit": profit,
        "profit_margin": margin,
    }


def opportunity_score(
    sr: float,
    margin: float,
    margin_ref: float = 0.3,
) -> float:
    """Default sort key (0..100).

    Mathematical model: the score is the risk-adjusted expected profit rate,
    scaled so a healthy deal sits in the 60-80 band.

        score = 100 * sr * (margin / margin_ref) ** 0.5

    * `sr` is the probability of success (0..1) — this is the "safe" part.
    * `margin / margin_ref` is how far the deal beats a healthy reference
      margin (default 30%) — this is the "profitable" part.
    * The square root keeps low margins from over-rewarding a hot product:
      to score well you need BOTH a likely sale AND a worthwhile margin.

    Examples (margin_ref = 30%):
        sr 90%, margin 40% -> 100 * 0.9 * sqrt(1.33) = 103.8  (great)
        sr 90%, margin  5% -> 100 * 0.9 * sqrt(0.17) =  36.7  (safe but thin)
        sr 30%, margin 60% -> 100 * 0.3 * sqrt(2.00) =  42.4  (fat but risky)
    """
    sr = clamp(sr, 0.0, 1.0)
    if margin_ref <= 0:
        raise ValueError("margin_ref must be > 0")
    margin_ratio = max(0.0, margin / margin_ref)
    return 100.0 * sr * math.sqrt(margin_ratio)


@dataclass
class Offer:
    """A single supplier offer for a product, fully costed."""

    product_id: str
    supplier_id: str
    supplier_name: str
    region_id: str
    region_name: str
    currency: str
    unit_price: float
    unit_price_pln: float
    landing_cost_pln: float
    profit_per_unit: float
    profit_margin: float
    freight_pln: float
    duty_pln: float
    vat_pln: float
    lead_days: int
    success_rate: float = 0.0
    opportunity: float = 0.0

    def to_dict(self) -> dict[str, Any]:
        return {
            "product_id": self.product_id,
            "supplier_id": self.supplier_id,
            "supplier_name": self.supplier_name,
            "region_id": self.region_id,
            "region_name": self.region_name,
            "currency": self.currency,
            "unit_price": round(self.unit_price, 2),
            "unit_price_pln": round(self.unit_price_pln, 2),
            "landing_cost_pln": round(self.landing_cost_pln, 2),
            "profit_per_unit": round(self.profit_per_unit, 2),
            "profit_margin": round(self.profit_margin, 4),
            "profit_margin_pct": round(self.profit_margin * 100, 1),
            "freight_pln": round(self.freight_pln, 2),
            "duty_pln": round(self.duty_pln, 2),
            "vat_pln": round(self.vat_pln, 2),
            "lead_days": self.lead_days,
            "success_rate": round(self.success_rate, 4),
            "opportunity": round(self.opportunity, 2),
        }
