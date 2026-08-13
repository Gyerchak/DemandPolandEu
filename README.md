# DemandPolandEu

Tool for monitoring the **Polish demand market**: tracks local (PL) prices and
demand signals for products, finds the **cheapest supplier across available
regions (mainly China and the EU)**, and prices in transport/freight, duties
and optional VAT to compute landing cost, profit margin and a combined
"opportunity" score.

Built on the Python standard library only — no dependencies to install.

## Quick start

```bash
./run.sh rank              # CLI: products ranked by opportunity score (default)
./run.sh rank --sort demand
./run.sh rank --sort profit_margin --vat
./run.sh detail electric-scooter
./run.sh web               # dashboard at http://127.0.0.1:8000
./run.sh test              # unit tests
```

`run.sh` must be launched as `bash run.sh <cmd>` if the file is not executable.

## How the scoring works

Every offer (supplier × product) is costed per unit:

```
landing_cost = supplier_price_in_PLN
             + freight (per_kg × weight + per_unit)
             + duty (rate × supplier price) + handling
             + [optional] VAT 23% on the subtotal
```

Profit for a product is the **best** (cheapest landing cost) offer:

```
profit_margin    = (local_price - landing_cost) / local_price
success_rate     = 0.5 × popularity + 0.5 × demand        # chance the product sells
opportunity      = 100 × success_rate × sqrt(margin / margin_ref)   # margin_ref = 30% default
```

The **opportunity score** is the default sort. It is deliberately "safe and
profitable": to score well a product needs BOTH a high probability of selling
(popularity + demand) AND a worthwhile margin — a hot product with a paper-thin
margin scores low, as does a fat margin on a product that won't move.

| Sort key             | Meaning                                              |
|----------------------|------------------------------------------------------|
| `opportunity`        | default: safe+profitable combined score              |
| `demand`             | level of Polish demand for the product               |
| `profit_margin`      | margin of the cheapest landed offer                  |
| `profit_per_unit`    | absolute profit in PLN of the cheapest offer         |
| `success_rate`       | popularity+demand based chance of selling            |

## Data files (edit to add your own products/suppliers)

- `data/products.json` — products with PL retail price, demand (0-1),
  popularity (0-1) and weight (for freight).
- `data/regions.json` — sourcing regions (China, EU, PL) with currency,
  FX rate, freight rates, duty rate, handling and VAT.
- `data/suppliers.json` — suppliers per region, each with a price per product
  id and lead time.

## Project layout

```
market/          engine (costing + scoring), analyzer, CLI, web server
data/            products / regions / suppliers
web/             dashboard UI (static HTML/CSS/JS)
tests/           unit tests for the scoring math
run.sh           launcher
```

## Launching from OpenCodeBox

- `OpenCodeBox/project-runs/run-DemandPolandEu.sh`
- or from inside this folder: `./opencode-run.sh`
