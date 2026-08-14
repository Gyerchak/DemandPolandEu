#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "engine.hpp"
#include "json_io.hpp"

using namespace dpe;

static void print_money(double v) { printf("%'.2f PLN", v); }
static void print_pct(double v) { printf("%.1f%%", v * 100.0); }

static void print_table(const std::vector<ProductRow>& rows) {
    const char* header =
        "  #  Produkt                             Popyt   SR   Marza   Zysk/szt  "
        "Koszt/szt  Region              Dostawca               Okazja";
    printf("%s\n", header);
    for (size_t i = 0; i < std::strlen(header); ++i) printf("-");
    printf("\n");
    int idx = 1;
    for (const auto& row : rows) {
        const Offer& o = row.best_offer;
        printf("%3d  %-34s %4.0f%% %4.0f%% %6.1f%% %10.2f %11.2f  %-18s %-21s %6.1f\n",
               idx++, row.name.substr(0, 33).c_str(), row.demand * 100.0,
               row.success_rate * 100.0, row.profit_margin * 100.0, row.profit_per_unit,
               o.landing_cost_pln, o.region_name.substr(0, 17).c_str(),
               o.supplier_name.substr(0, 21).c_str(), row.opportunity);
    }
    printf("\n");
}

static void print_detail(const std::vector<ProductRow>& rows, const std::string& product_id) {
    for (const auto& row : rows) {
        if (row.id != product_id) continue;
        const Offer& o = row.best_offer;
        printf("=== %s ===\n", row.name.c_str());
        printf("  Popyt: %.0f%%  Popularnosc: %.0f%%  Success rate: %.1f%%\n",
               row.demand * 100.0, row.popularity * 100.0, row.success_rate * 100.0);
        printf("  Cena lokalna: ");
        print_money(row.local_price_pln);
        printf("  Waga: %.1f kg\n", row.weight_kg);
        printf("  Najlepsza oferta: %s (%s)\n", o.supplier_name.c_str(), o.region_name.c_str());
        printf("  Koszt ladowania: ");
        print_money(o.landing_cost_pln);
        printf("\n  Marza: ");
        print_pct(row.profit_margin);
        printf("  Zysk/szt: ");
        print_money(row.profit_per_unit);
        printf("\n  Okazja (score): %.1f\n\n", row.opportunity);
        printf("  %-26s %-20s %9s %11s %9s %6s %4s\n", "Dostawca", "Region", "Cena",
               "Koszt", "Zysk", "Marza", "Dni");
        for (const auto& offer : row.offers) {
            printf("  %-25s %-19s %9.0f %11.2f %9.2f %6.1f%% %4d\n",
                   offer.supplier_name.substr(0, 25).c_str(),
                   offer.region_name.substr(0, 19).c_str(), offer.unit_price_pln,
                   offer.landing_cost_pln, offer.profit_per_unit,
                   offer.profit_margin * 100.0, offer.lead_days);
        }
        return;
    }
    fprintf(stderr, "Produkt '%s' nie znaleziony.\n", product_id.c_str());
}

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s <command> [options]\n"
            "\n"
            "Commands:\n"
            "  rank [--sort KEY] [--top N] [--vat]   ranked table (default)\n"
            "  detail <id> [--vat]                  one product detail\n"
            "  dump [--vat]                         full JSON result\n"
            "  web [--host H] [--port P]            web dashboard\n"
            "  test                                 run unit tests\n"
            "\n"
            "Sort keys: opportunity (default), demand, popularity, profit_margin,\n"
            "           profit_per_unit, success_rate\n",
            prog);
}

// Minimal hand-rolled argument parser (no dependency).
struct Args {
    std::string cmd;
    std::string sort = "opportunity";
    std::string product_id;
    std::string host = "127.0.0.1";
    int port = 8000;
    int top = 0;
    bool vat = false;
    bool ok = true;
};

static Args parse_args(int argc, char** argv) {
    Args a;
    a.cmd = argc > 1 ? argv[1] : "rank";
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--sort" && i + 1 < argc) a.sort = argv[++i];
        else if (arg == "--top" && i + 1 < argc) a.top = std::atoi(argv[++i]);
        else if (arg == "--vat") a.vat = true;
        else if (arg == "--host" && i + 1 < argc) a.host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) a.port = std::atoi(argv[++i]);
        else if (arg.rfind("-", 0) != 0) a.product_id = arg;
        else { a.ok = false; }
    }
    return a;
}

int run_web(const std::string& data_dir, const std::string& host, int port);

int main(int argc, char** argv) {
    Args a = parse_args(argc, argv);
    if (!a.ok) {
        usage(argv[0]);
        return 2;
    }

    if (a.cmd == "test") {
        // Placeholder — real tests live in tests/test_main.cpp (see CMake).
        printf("Unit tests: run via ctest after building.\n");
        return 0;
    }

    // Locate data dir: project root is the folder containing this binary's
    // working directory is the repo root (run.sh cd's there).
    std::string dir = ".";

    if (a.cmd == "web") {
        return run_web(dir, a.host, a.port);
    }

    auto rows = build_rows(dir, SuccessWeights{}, 0.3, a.vat);

    if (a.cmd == "rank") {
        sort_rows(rows, a.sort);
        if (a.top > 0 && (size_t)a.top < rows.size()) rows.resize((size_t)a.top);
        print_table(rows);
        return 0;
    }
    if (a.cmd == "detail") {
        if (a.product_id.empty()) { usage(argv[0]); return 2; }
        print_detail(rows, a.product_id);
        return 0;
    }
    if (a.cmd == "dump") {
        sort_rows(rows, "opportunity");
        printf("%s\n", rows_to_json(rows).dump(2).c_str());
        return 0;
    }

    usage(argv[0]);
    return 2;
}
