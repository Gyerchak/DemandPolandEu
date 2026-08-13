"""Minimal stdlib web server exposing the monitor as a JSON API + static UI."""

from __future__ import annotations

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlparse

from .analyzer import build_rows, sort_rows
from .engine import SuccessWeights

WEB_DIR = Path(__file__).resolve().parent.parent / "web"
MIME = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".svg": "image/svg+xml",
}

_weights = SuccessWeights()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):  # noqa: A002
        pass

    def _send_json(self, obj: Any, status: int = 200) -> None:
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", MIME[".json"])
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _send_file(self, path: Path) -> None:
        if not path.exists() or not path.is_file():
            self.send_error(404)
            return
        content = path.read_bytes()
        ext = path.suffix.lower()
        self.send_response(200)
        self.send_header("Content-Type", MIME.get(ext, "application/octet-stream"))
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        query = parse_qs(parsed.query)
        path = parsed.path

        if path == "/api/rank":
            sort_key = query.get("sort", ["opportunity"])[0]
            include_vat = query.get("vat", ["0"])[0] in {"1", "true", "yes"}
            margin_ref = float(query.get("margin_ref", ["0.3"])[0])
            rows = build_rows(weights=_weights, include_vat=include_vat, margin_ref=margin_ref)
            rows = sort_rows(rows, sort_key)
            self._send_json(
                {
                    "count": len(rows),
                    "sort": sort_key,
                    "rows": rows,
                    "meta": {
                        "success_formula": "success_rate = 0.5*popularity + 0.5*demand",
                        "opportunity_formula": "opportunity = 100 * success_rate * sqrt(margin / margin_ref)",
                        "margin_ref": margin_ref,
                    },
                }
            )
            return

        if path == "/":
            self._send_file(WEB_DIR / "index.html")
            return

        rel = Path(path.lstrip("/"))
        if not (WEB_DIR / rel).resolve().is_relative_to(WEB_DIR.resolve()):
            self.send_error(403)
            return
        self._send_file(WEB_DIR / rel)

    do_POST = do_GET  # noqa: N815


def serve(host: str = "127.0.0.1", port: int = 8000) -> None:
    httpd = ThreadingHTTPServer((host, port), Handler)
    print(f"DemandPolandEu monitor on http://{host}:{port}")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="market-web", description="Serve the web dashboard")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args(argv)
    serve(args.host, args.port)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
