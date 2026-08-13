"""Ranking logic: combine products, regions and supplier offers into rows."""

from __future__ import annotations

from typing import Any

from .engine import (
    Offer,
    SuccessWeights,
    landing_cost,
    opportunity_score,
    success_rate,
)
from .loader import load_products, load_regions, load_suppliers


def build_rows(
    products: list[dict[str, Any]] | None = None,
    regions: list[dict[str, Any]] | None = None,
    suppliers: list[dict[str, Any]] | None = None,
    weights: SuccessWeights | None = None,
    margin_ref: float = 0.3,
    include_vat: bool = False,
) -> list[dict[str, Any]]:
    """Compute a ranked list of products, each with their best supplier.

    Each product row:
      - product info + demand + popularity
      - success_rate (from popularity+demand)
      - the cheapest offer across all regions/suppliers (by landing cost)
      - profit metrics and the opportunity score for that best offer
      - best_offer: the winning offer
      - offers: every supplier offer for this product, with landing cost
    """
    products = load_products() if products is None else products
    regions = load_regions() if regions is None else regions
    suppliers = load_suppliers() if suppliers is None else suppliers

    region_by_id = {r["id"]: r for r in regions}

    rows: list[dict[str, Any]] = []
    for product in products:
        product_id = product["id"]
        sr = success_rate(
            float(product.get("popularity", 0.0)),
            float(product.get("demand", 0.0)),
            weights,
        )
        local_price = float(product["local_price_pln"])

        offers: list[Offer] = []
        for supplier in suppliers:
            price_entry = supplier.get("prices", {}).get(product_id)
            if price_entry is None:
                continue
            region = region_by_id.get(supplier["region_id"])
            if region is None:
                continue
            unit_price = float(price_entry)
            fx = float(region.get("fx_to_pln", 1.0))
            unit_price_pln = unit_price * fx
            cost = landing_cost(unit_price_pln, region, product, include_vat)
            profit = local_price - cost["total"]
            margin = profit / local_price if local_price else 0.0
            offer = Offer(
                product_id=product_id,
                supplier_id=supplier["id"],
                supplier_name=supplier["name"],
                region_id=region["id"],
                region_name=region["name"],
                currency=region.get("currency", ""),
                unit_price=unit_price,
                unit_price_pln=unit_price_pln,
                landing_cost_pln=cost["total"],
                profit_per_unit=profit,
                profit_margin=margin,
                freight_pln=cost["freight"],
                duty_pln=cost["duty"],
                vat_pln=cost["vat"],
                lead_days=int(supplier.get("lead_days", region.get("lead_days", 30))),
                success_rate=sr,
                opportunity=opportunity_score(sr, margin, margin_ref),
            )
            offers.append(offer)

        if not offers:
            continue

        best = min(offers, key=lambda o: o.landing_cost_pln)
        rows.append(
            {
                "id": product_id,
                "name": product["name"],
                "category": product.get("category", ""),
                "local_price_pln": local_price,
                "demand": float(product.get("demand", 0.0)),
                "popularity": float(product.get("popularity", 0.0)),
                "weight_kg": float(product.get("weight_kg", 0.0)),
                "success_rate": sr,
                "profit_margin": best.profit_margin,
                "profit_per_unit": best.profit_per_unit,
                "opportunity": best.opportunity,
                "best_offer": best.to_dict(),
                "offers": [o.to_dict() for o in sorted(offers, key=lambda o: o.landing_cost_pln)],
            }
        )

    return rows


def sort_rows(rows: list[dict[str, Any]], key: str = "opportunity", reverse: bool = True) -> list[dict[str, Any]]:
    allowed = {"opportunity", "profit_margin", "profit_per_unit", "success_rate", "demand", "popularity"}
    if key not in allowed:
        raise ValueError(f"unknown sort key: {key!r}")
    return sorted(rows, key=lambda r: r[key], reverse=reverse)


def top_products(rows: list[dict[str, Any]], n: int = 10) -> list[dict[str, Any]]:
    return sort_rows(rows, "opportunity")[:n]
