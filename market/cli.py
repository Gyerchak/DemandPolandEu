"""Command-line interface for the demand market monitor."""

from __future__ import annotations

import argparse
import json
import sys
from typing import Any

from .analyzer import build_rows, sort_rows
from .engine import SuccessWeights


def _fmt_money(value: float) -> str:
    return f"{value:,.2f} PLN"


def _fmt_pct(value: float) -> str:
    return f"{value * 100:.1f}%"


def _fmt_score(value: float) -> str:
    return f"{value:.1f}"


def print_table(rows: list[dict[str, Any]], key: str = "opportunity") -> None:
    sort_key = "opportunity" if key not in {"demand", "profit_margin", "profit_per_unit", "success_rate"} else key
    rows = sort_rows(rows, sort_key)
    header = (
        f"{'#':>2}  {'Produkt':<34} {'Popyt':>5} {'SR':>5} {'Marza':>6} {'Zysk/szt':>10} "
        f"{'Koszt/szt':>11} {'Region':<18} {'Dostawca':<22} {'Okazja':>6}"
    )
    print(header)
    print("-" * len(header))
    for idx, row in enumerate(rows, start=1):
        offer = row["best_offer"]
        print(
            f"{idx:>2}  {row['name'][:33]:<34} "
            f"{row['demand'] * 100:>4.0f}% {row['success_rate'] * 100:>4.0f}% "
            f"{_fmt_pct(row['profit_margin']):>6} {row['profit_per_unit']:>10,.2f} "
            f"{offer['landing_cost_pln']:>11,.2f} "
            f"{offer['region_name'][:17]:<18} {offer['supplier_name'][:21]:<22} "
            f"{_fmt_score(row['opportunity']):>6}"
        )
    print()


def print_detail(rows: list[dict[str, Any]], product_id: str) -> None:
    row = next((r for r in rows if r["id"] == product_id), None)
    if row is None:
        print(f"Produkt '{product_id}' nie znaleziony.", file=sys.stderr)
        return
    print(f"=== {row['name']} ===")
    print(f"  Popyt: {row['demand'] * 100:.0f}%   Popularnosc: {row['popularity'] * 100:.0f}%   "
          f"Success rate: {row['success_rate'] * 100:.1f}%")
    print(f"  Cena lokalna: {_fmt_money(row['local_price_pln'])}   Waga: {row['weight_kg']:.1f} kg")
    print(f"  Najlepsza oferta: {row['best_offer']['supplier_name']} ({row['best_offer']['region_name']})")
    print(f"  Koszt ladowania: {_fmt_money(row['best_offer']['landing_cost_pln'])}")
    print(f"  Marza: {_fmt_pct(row['profit_margin'])}   Zysk/szt: {_fmt_money(row['profit_per_unit'])}")
    print(f"  Okazja (score): {_fmt_score(row['opportunity'])}")
    print()
    print(f"  {'Dostawca':<26} {'Region':<20} {'Cena':>9} {'Koszt':>11} {'Zysk':>9} {'Marza':>6} {'Dni':>4}")
    for offer in row["offers"]:
        print(
            f"  {offer['supplier_name'][:25]:<26} {offer['region_name'][:19]:<20} "
            f"{offer['unit_price_pln']:>9,.0f} {offer['landing_cost_pln']:>11,.2f} "
            f"{offer['profit_per_unit']:>9,.2f} {_fmt_pct(offer['profit_margin']):>6} {offer['lead_days']:>4}"
        )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="market", description="Polish demand market monitor")
    sub = parser.add_subparsers(dest="command")

    rank = sub.add_parser("rank", help="show ranked product table")
    rank.add_argument("--sort", default="opportunity",
                      choices=["opportunity", "demand", "profit_margin", "profit_per_unit", "success_rate"])
    rank.add_argument("--top", type=int, default=0, help="limit rows to top N")
    rank.add_argument("--vat", action="store_true", help="include VAT in landing cost")

    detail = sub.add_parser("detail", help="show one product in detail")
    detail.add_argument("product", help="product id")
    detail.add_argument("--vat", action="store_true", help="include VAT in landing cost")

    dump = sub.add_parser("dump", help="dump full JSON result")
    dump.add_argument("--vat", action="store_true", help="include VAT in landing cost")

    args = parser.parse_args(argv)

    if args.command is None:
        parser.print_help()
        return 0

    weights = SuccessWeights()
    rows = build_rows(weights=weights, include_vat=getattr(args, "vat", False))

    if args.command == "rank":
        ranked = sort_rows(rows, args.sort)
        if args.top:
            ranked = ranked[: args.top]
        print_table(ranked, args.sort)
        return 0

    if args.command == "detail":
        print_detail(rows, args.product)
        return 0

    if args.command == "dump":
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
