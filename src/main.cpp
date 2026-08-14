#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "engine.hpp"
#include "json_io.hpp"

using namespace dpe;




static void print_trades(const std::vector<Trade>& trades, const std::string& kind) {
    printf("\n== %s ==\n", kind.c_str());
    printf("  %-30s %-18s %-18s %10s %10s %9s %7s %7s\n",
           "Product", "From", "To", "Cost", "Sell", "Profit", "Margin", "Opp");
    for (const auto& t : trades) {
        printf("  %-30s %-18s %-18s %10.2f %10.2f %9.2f %6.1f%% %6.1f\n",
               t.product_name.substr(0, 29).c_str(),
               t.from_market.substr(0, 17).c_str(),
               t.to_market.substr(0, 17).c_str(),
               t.total_eur, t.sell_eur, t.profit_eur, t.margin * 100.0, t.opportunity);
    }
    printf("\n");
}

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s <command> [options]\n"
            "\n"
            "Commands:\n"
            "  rank [--home MARKET] [--markets a,b,c] [--all] [--category C]\n"
            "       [--sort KEY] [--top N] [--vat]          import + export ranked\n"
            "  detail <product-id> [--home M] [--vat]        one product detail\n"
            "  dump [--home M] [--vat]                       full JSON result\n"
            "  markets                                       list markets\n"
            "  categories                                    list categories\n"
            "  web [--host H] [--port P]                     web dashboard\n"
            "  test                                          run unit tests\n"
            "\n"
            "Default home market: the market with role=home (Poland).\n"
            "Sort keys: opportunity (default), profit, margin, success_rate, sell\n"
            "Main markets to watch: visegrad, china, poland, europe, baltic, turkiye, westeu\n",
            prog);
}

struct Args {
    std::string cmd;
    std::string home;
    std::vector<std::string> markets;
    std::string category;
    std::string sort = "opportunity";
    std::string product_id;
    std::string host = "127.0.0.1";
    int port = 8000;
    int top = 0;
    bool all = false;
    bool vat = false;
    bool ok = true;
};

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static Args parse_args(int argc, char** argv) {
    Args a;
    a.cmd = argc > 1 ? argv[1] : "rank";
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--home" && i + 1 < argc) a.home = argv[++i];
        else if (arg == "--markets" && i + 1 < argc) a.markets = split(argv[++i], ',');
        else if (arg == "--category" && i + 1 < argc) a.category = argv[++i];
        else if (arg == "--sort" && i + 1 < argc) a.sort = argv[++i];
        else if (arg == "--top" && i + 1 < argc) a.top = std::atoi(argv[++i]);
        else if (arg == "--vat") a.vat = true;
        else if (arg == "--all") a.all = true;
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
    if (!a.ok) { usage(argv[0]); return 2; }

    std::string dir = ".";

    if (a.cmd == "test") {
        printf("Unit tests: run via ctest after building.\n");
        return 0;
    }
    if (a.cmd == "web") return run_web(dir, a.host, a.port);

    if (a.cmd == "markets") {
        auto markets = load_markets(dir);
        for (const auto& m : markets) {
            printf("%-14s %-24s role=%-6s watch=%d cur=%s vat=%.0f%% duty=%.0f%%\n",
                   m.id.c_str(), m.name.c_str(), m.role.c_str(), (int)m.watch,
                   m.currency.c_str(), m.vat_rate * 100.0, m.duty_rate * 100.0);
        }
        return 0;
    }
    if (a.cmd == "categories") {
        for (const auto& c : list_categories(dir)) printf("%s\n", c.c_str());
        return 0;
    }

    if (a.cmd == "rank" || a.cmd == "dump") {
        auto markets = load_markets(dir);
        std::string home = a.home;
        if (home.empty()) {
            for (const auto& m : markets) if (m.role == "home") { home = m.id; break; }
        }
        auto trades = build_trades(dir, home, a.all ? std::vector<std::string>{} : a.markets, a.vat);
        if (!a.category.empty()) {
            trades.erase(std::remove_if(trades.begin(), trades.end(),
                        [&](const Trade& t) { return t.category != a.category; }),
                        trades.end());
        }
        sort_trades(trades, a.sort);
        if (a.top > 0 && (size_t)a.top < trades.size()) trades.resize((size_t)a.top);

        if (a.cmd == "dump") {
            printf("%s\n", trades_to_json(trades).dump(2).c_str());
            return 0;
        }

        std::vector<Trade> imports, exports;
        for (const auto& t : trades) (t.kind == "import" ? imports : exports).push_back(t);
        printf("Home market: %s\n", home.c_str());
        printf("Total trades: %zu (imports %zu, exports %zu)\n",
               trades.size(), imports.size(), exports.size());
        print_trades(imports, "IMPORT  (buy abroad -> sell at home)");
        print_trades(exports, "EXPORT  (buy at home -> sell abroad)");
        return 0;
    }

    if (a.cmd == "detail") {
        if (a.product_id.empty()) { usage(argv[0]); return 2; }
        auto markets = load_markets(dir);
        std::string home = a.home;
        if (home.empty()) for (const auto& m : markets) if (m.role == "home") { home = m.id; break; }
        auto trades = build_trades(dir, home, std::vector<std::string>{}, a.vat);
        printf("=== %s (home: %s) ===\n", a.product_id.c_str(), home.c_str());
        bool any = false;
        for (const auto& t : trades) {
            if (t.product_id != a.product_id) continue;
            any = true;
            printf("[%s] %s -> %s : buy %.2f sell %.2f cost %.2f total %.2f profit %.2f "
                   "(margin %.1f%%) opp %.1f  %s\n",
                   t.kind.c_str(), t.from_market.c_str(), t.to_market.c_str(),
                   t.buy_eur, t.sell_eur, t.cost_eur, t.total_eur, t.profit_eur,
                   t.margin * 100.0, t.opportunity, t.category.c_str());
        }
        if (!any) fprintf(stderr, "Product '%s' not found.\n", a.product_id.c_str());
        return 0;
    }

    usage(argv[0]);
    return 2;
}
