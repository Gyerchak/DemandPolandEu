"""Load the JSON data files (products, regions, suppliers)."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

DATA_DIR = Path(__file__).resolve().parent.parent / "data"


def load_json(name: str, data_dir: Path | None = None) -> Any:
    path = (data_dir or DATA_DIR) / name
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def load_products(data_dir: Path | None = None) -> list[dict[str, Any]]:
    return load_json("products.json", data_dir).get("products", [])


def load_regions(data_dir: Path | None = None) -> list[dict[str, Any]]:
    return load_json("regions.json", data_dir).get("regions", [])


def load_suppliers(data_dir: Path | None = None) -> list[dict[str, Any]]:
    return load_json("suppliers.json", data_dir).get("suppliers", [])
