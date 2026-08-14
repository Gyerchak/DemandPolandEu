#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "engine.hpp"
#include "json_io.hpp"

using namespace dpe;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char buf[3] = {s[i + 1], s[i + 2], 0};
            out += (char)std::strtol(buf, nullptr, 16);
            i += 2;
        } else if (s[i] == '+') out += ' ';
        else out += s[i];
    }
    return out;
}

std::string query_value(const std::string& path, const std::string& key) {
    size_t q = path.find('?');
    if (q == std::string::npos) return {};
    std::string query = path.substr(q + 1);
    std::stringstream ss(query);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        std::string k = pair.substr(0, eq);
        std::string v = eq == std::string::npos ? "" : pair.substr(eq + 1);
        if (url_decode(k) == key) return url_decode(v);
    }
    return {};
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string mime_type(const std::string& path) {
    if (path.size() > 5 && path.substr(path.size() - 5) == ".html") return "text/html; charset=utf-8";
    if (path.size() > 4 && path.substr(path.size() - 4) == ".css") return "text/css; charset=utf-8";
    if (path.size() > 3 && path.substr(path.size() - 3) == ".js") return "application/javascript; charset=utf-8";
    if (path.size() > 5 && path.substr(path.size() - 5) == ".json") return "application/json; charset=utf-8";
    return "application/octet-stream";
}

void send_response(int fd, const std::string& body, const std::string& mime, int status = 200) {
    std::string reason = status == 200 ? "OK" : status == 404 ? "Not Found" : "Forbidden";
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Cache-Control: no-store\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, reason.c_str(), mime.c_str(), body.size());
    (void)::write(fd, header, strlen(header));
    (void)::write(fd, body.data(), body.size());
}

}  // namespace

int run_web(const std::string& data_dir, const std::string& host, int port) {
    (void)data_dir;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { fprintf(stderr, "socket() failed\n"); return 1; }
    int one = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind(%s:%d) failed\n", host.c_str(), port);
        close(server_fd);
        return 1;
    }
    listen(server_fd, 8);
    printf("DemandPolandEu monitor on http://%s:%d\n", host.c_str(), port);
    fflush(stdout);

    for (;;) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;

        char buf[8192];
        ssize_t n = read(client, buf, sizeof(buf) - 1);
        std::string req;
        if (n > 0) { buf[n] = 0; req = buf; }

        std::string line = req.substr(0, req.find("\r\n"));
        size_t sp1 = line.find(' ');
        size_t sp2 = sp1 == std::string::npos ? std::string::npos : line.find(' ', sp1 + 1);
        std::string target = sp2 == std::string::npos ? "/" : line.substr(sp1 + 1, sp2 - sp1 - 1);
        if (target.empty()) target = "/";
        size_t path_end = target.find('?');
        std::string path = path_end == std::string::npos ? target : target.substr(0, path_end);

        if (path == "/api/markets") {
            auto markets = load_markets(".");
            nlohmann::json j = markets_to_json(markets);
            send_response(client, j.dump(), "application/json; charset=utf-8");
            close(client);
            continue;
        }

        if (path == "/api/categories") {
            auto cats = list_categories(".");
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& c : cats) arr.push_back(c);
            send_response(client, arr.dump(), "application/json; charset=utf-8");
            close(client);
            continue;
        }

        if (path == "/api/trades") {
            auto markets = load_markets(".");
            std::string home = query_value(target, "home");
            if (home.empty()) {
                for (const auto& m : markets) if (m.role == "home") { home = m.id; break; }
            }
            std::string markets_q = query_value(target, "markets");
            std::vector<std::string> watch;
            if (!markets_q.empty() && markets_q != "all") watch = split(markets_q, ',');
            std::string vat = query_value(target, "vat");
            bool include_vat = (vat == "1" || vat == "true" || vat == "yes");
            std::string mr = query_value(target, "margin_ref");
            double margin_ref = mr.empty() ? 0.3 : std::atof(mr.c_str());
            std::string sort = query_value(target, "sort");
            if (sort.empty()) sort = "opportunity";
            std::string category = query_value(target, "category");

            auto trades = build_trades(".", home, watch, include_vat, margin_ref);
            if (!category.empty()) {
                trades.erase(std::remove_if(trades.begin(), trades.end(),
                            [&](const Trade& t) { return t.category != category; }),
                            trades.end());
            }
            sort_trades(trades, sort);

            nlohmann::json imports = nlohmann::json::array();
            nlohmann::json exports = nlohmann::json::array();
            for (const auto& t : trades) {
                if (t.kind == "import") imports.push_back(trade_to_json(t));
                else exports.push_back(trade_to_json(t));
            }

            nlohmann::json resp{
                {"home", home},
                {"count", trades.size()},
                {"imports", imports},
                {"exports", exports},
                {"meta",
                 {{"cost", "cost = buy + freight + duty + handling (payments until it ships to you)"},
                  {"vat", "VAT is added on the sale price (category/market VAT), included when vat=1"},
                  {"opportunity_formula", "opportunity = 100 * success_rate * sqrt(margin / margin_ref)"},
                  {"margin_ref", margin_ref}}},
            };
            send_response(client, resp.dump(), "application/json; charset=utf-8");
            close(client);
            continue;
        }

        if (path == "/" || path.empty()) {
            std::string body = read_file("web/index.html");
            send_response(client, body.empty() ? "not found" : body,
                          body.empty() ? "text/plain" : "text/html; charset=utf-8",
                          body.empty() ? 404 : 200);
            close(client);
            continue;
        }

        std::string rel = path.substr(1);
        if (rel.find("..") != std::string::npos) {
            send_response(client, "forbidden", "text/plain", 403);
            close(client);
            continue;
        }
        std::string body = read_file("web/" + rel);
        if (body.empty()) send_response(client, "not found", "text/plain", 404);
        else send_response(client, body, mime_type(rel));
        close(client);
    }

    close(server_fd);
    return 0;
}
