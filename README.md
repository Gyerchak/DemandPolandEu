# DemandPolandEu — Market Trade Monitor

Tool for finding the **most profitable import & export trades** between markets.
Poland is the **home market**; **Visegrad** is the main export/import market.
Default currency: **EUR**.

Every product has a **buy** and a **sell** price per market. For each pair
(home ↔ counterparty market) the tool computes two trades:

- **Import**: buy in the counterparty market → sell at home.
- **Export**: buy at home → sell in the counterparty market.

## How costs work

```
cost  = buy + freight + duty + handling          # payments until it ships to you
total = cost + VAT                                # VAT on the sale price (when enabled)
profit = sell - total
margin = profit / sell
opportunity = 100 × success_rate × sqrt(margin / margin_ref)   # margin_ref = 30% default
```

`success_rate` = 0.5 × popularity + 0.5 × demand (chance the product sells in
the target market).

## Quick start

```bash
./run.sh              # web dashboard at http://127.0.0.1:8000
./run.sh rank         # CLI: import + export ranked by opportunity
./run.sh rank --home visegrad --markets china,europe --vat
./run.sh rank --category Electronics
./run.sh markets      # list the 14 markets
./run.sh categories   # list product categories
./run.sh detail electric-scooter
./run.sh dump         # full JSON result
./run.sh test         # unit tests
```

## Markets (data/markets.json)

| Market | Role | Watch |
|--------|------|-------|
| poland | home | ✅ main watch |
| visegrad (PL,CZ,SK,HU) | **main** | ✅ main watch |
| china | other | ✅ |
| europe (EU core) | other | ✅ |
| baltic (LT,LV,EE) | other | ✅ |
| turkiye | other | ✅ |
| westeu (UK,CH,NO) | other | ✅ |
| turkic (AZ,KZ,UZ,...) | other | |
| ukraine | other | |
| belarus | other | |
| kaliningrad | other | |
| belarus-russia (excl. Kaliningrad) | other | |
| north-africa (MA,TN,DZ,LY) | other | |
| balkan (non-EU) | other | |

**Main markets to watch:** visegrad, china, poland, europe, baltic, turkiye,
westeu.

## Categories & fallback

Products carry a **broad category** (Electronics, Sport, Home & Garden,
Appliances, Energy, Tools, Automotive). If a product has no category:

1. `data/categories.json` keyword map is searched (product name / id).
2. If nothing matches → **`unknown`** (manually editable later).

## Web dashboard

- **Import trades** table (left) and **Export trades** table (right), both
  ranked by opportunity by default.
- **Category filter**, **home market** selector, **VAT on sale** toggle.
- **Watch** chips — pick any combination of markets to monitor (defaults to the
  main-watch set), or tick "All markets".
- **Trade & Tax Guide** section — a reminder of core rules (cost to you, sale
  VAT, duty, registration, invoices, EUR invoicing).

## Data files (edit to add your own products/markets)

- `data/markets.json` — markets with currency, FX to EUR, freight, duty,
  handling, VAT, role (home/main/other) and watch flag.
- `data/products.json` — products with weight and per-market `buy`, `sell`,
  `demand`, `popularity` (prices in local currency; converted to EUR by FX).
- `data/categories.json` — keyword → category fallback map.

## Project layout

```
src/           engine (costing + scoring), JSON I/O, CLI, minimal HTTP server
data/          markets / products / categories
web/           dashboard UI (static HTML/CSS/JS, English)
tests/         unit tests for scoring + category fallback (ctest)
run.sh         launcher (build + run)
CMakeLists.txt build config
```

## Launching from OpenCodeBox

- `OpenCodeBox/project-runs/run-DemandPolandEu.sh`
- or from inside this folder: `./opencode-run.sh`
