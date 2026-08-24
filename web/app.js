"use strict";

const MAG = "🔎";   // deep-search icon (magnifier)
const CHK = "✓";   // verified-price checkmark
const CART = "🛒";   // shopping-cart icon (best-sell button)

const $ = (s) => document.querySelector(s);
const sortSel = $("#sort");
const catSel = $("#category");
const homeSel = $("#home");
const vatCheck = $("#vat");
const allCheck = $("#all");
const realOnlyCheck = $("#realOnly");
const refreshBtn = $("#refresh");
const marketsDiv = $("#markets");
const meta = $("#meta");
const importsTbody = $("#imports tbody");
const exportsTbody = $("#exports tbody");

let markets = [];
let selected = new Set();
let channel = "main";
let lastImports = [];
let lastExports = [];
let qtyMult = 100;   // default: bulk ×100 (real trade minimum from a supplier)

const MAIN_WATCH = ["visegrad", "china", "poland", "europe", "baltic", "turkiye", "westeu"];

function fmtMoney(v) {
  // ×100 by default: every price shown is the bulk trade value (buying from a
  // real supplier means 100+ units). Columns keep their names — values are bulk.
  const bulk = v * qtyMult;
  return bulk.toLocaleString("en-IE", { minimumFractionDigits: 2, maximumFractionDigits: 2 }) + " €";
}
function fmtPct(v) { return (v * 100).toFixed(1) + "%"; }

function scoreClass(v) {
  if (v >= 65) return "hi";
  if (v >= 35) return "mid";
  return "lo";
}
function moneyClass(v) { return v >= 0 ? "good" : "bad"; }

async function loadMarkets() {
  const res = await fetch("/api/markets");
  markets = await res.json();
  homeSel.innerHTML = markets
    .map((m) => `<option value="${m.id}">${m.name}${m.role === "home" ? " ⭐" : ""}</option>`)
    .join("");
  // default home = poland
  homeSel.value = markets.find((m) => m.role === "home")?.id || markets[0].id;
  marketsDiv.innerHTML = markets
    .map((m) => `<label class="chip"><input type="checkbox" value="${m.id}" ${m.watch ? "checked" : ""}> ${m.name}</label>`)
    .join("");
  marketsDiv.querySelectorAll("input").forEach((cb) => {
    cb.addEventListener("change", () => {
      if (cb.checked) selected.add(cb.value);
      else selected.delete(cb.value);
    });
  });
  markets.forEach((m) => { if (m.watch) selected.add(m.id); });
  const legend = $("#legend");
  if (legend) {
    const notes = markets.filter((m) => m.members).map((m) => `* ${m.name} = ${m.members}`);
    legend.innerHTML = notes.length ? notes.join(" &nbsp;&middot;&nbsp; ") : "";
  }
}

async function loadCategories() {
  const res = await fetch("/api/categories");
  const cats = await res.json();
  catSel.innerHTML = '<option value="">All categories</option>' +
    cats.map((c) => `<option value="${c}">${c}</option>`).join("");
}

function tableHeaders() {
  return `
    <tr>
      <th>Product</th><th>Cat</th><th>From</th><th>To</th>
      <th>Buy</th><th>Freight</th><th>Duty</th><th>Handling</th><th>VAT</th>
      <th>Total cost</th><th>Sell</th><th>Profit</th><th>Margin</th><th>Opp</th><th>Best sell</th>
    </tr>`;
}

function productCell(t) {
  const name = t.product_name;
  const ident = (t.brand || t.model || t.ean)
    ? `<div class="ident" title="exact product identity — deep search targets this SKU">
         ${[t.brand, t.model].filter(Boolean).join(" ")}${t.ean ? " · EAN " + t.ean : ""}</div>`
    : `<div class="ident none" title="no verified identity — deep search matches the generic name only">generic</div>`;
  // REAL verified deal => open the exact product page where that price lives.
  if (t.supplier_url) {
    const fresh = t.link_checked ? ` verified ${t.link_checked}` : "";
    return `<div>${ident}<a class="prod" href="${t.supplier_url}" target="_blank" rel="noopener" title="exact source of the buy price in ${t.from_market}${fresh}">${name}</a></div>`;
  }
  // No verified deal => DEEP-SEARCH the exact identity (EAN > brand+model > name).
  return `<div>${ident}<button class="prod-link" data-deep="${t.product_name}" data-brand="${t.brand||""}" data-model="${t.model||""}" data-ean="${t.ean||""}" data-market="${t.from_market_id}"
     title="deep-search this exact product across every market">${name} ${MAG}</button></div>`;
}

// BEST-SELL: open the cheapest offer for this EXACT product in the destination
// market (its porównywarka / official-store search). Survives any table mode.
function bestSellBtn(t) {
  const mk = markets.find((m) => m.id === t.to_market_id);
  if (!mk || !mk.search) return `<span title="no destination search available">—</span>`;
  const q = encodeURIComponent(
    t.ean || [t.brand, t.model, t.product_name].filter(Boolean).join(" "));
  const href = mk.search.replace("{q}", q);
  const lbl = t.ean ? "EAN " + t.ean : ([t.brand, t.model].filter(Boolean).join(" ") || t.product_name);
  return `<a class="best-sell" href="${href}" target="_blank" rel="noopener"
     title="cheapest offer of ${lbl} in ${mk.name}">${CART} best sell</a>`;
}
function rowHtml(t) {
  const real = t.price_source === "real";   // only REAL buy prices drive profit math
  const sellTitle = (channel === "whole" && t.shops && t.shops.length)
    ? ` title="Whole-market price across: ${t.shops.join(", ")}" style="border-bottom:1px dotted var(--muted);cursor:help"`
    : "";
  if (!real) {
    return `
    <tr class="est-row">
      <td><b>${productCell(t)}</b></td>
      <td><small style="color:var(--muted)">${t.category}</small></td>
      <td>${t.from_market}</td>
      <td>${t.to_market}</td>
      <td colspan="10"><span class="est" title="buy price not verified yet — deep-search this exact product">no verified price — deep-search first</span></td>
      <td>${bestSellBtn(t)}</td>
    </tr>`;
  }
  return `
    <tr>
      <td><b>${productCell(t)}</b></td>
      <td><small style="color:var(--muted)">${t.category}</small></td>
      <td>${t.from_market}</td>
      <td>${t.to_market}</td>
      <td><span title="verified price from the linked exact product" style="color:var(--good)">${CHK}</span> ${fmtMoney(t.buy_eur)}</td>
      <td>${fmtMoney(t.freight_eur)}</td>
      <td>${fmtMoney(t.duty_eur)}</td>
      <td>${fmtMoney(t.handling_eur)}</td>
      <td>${fmtMoney(t.vat_eur)}</td>
      <td><b>${fmtMoney(t.total_eur)}</b></td>
      <td${sellTitle}>${fmtMoney(t.sell_eur)}</td>
      <td style="color:${moneyClass(t.profit_eur)}"><b>${fmtMoney(t.profit_eur)}</b></td>
      <td style="color:${moneyClass(t.margin)}">${fmtPct(t.margin)}</td>
      <td><span class="score ${scoreClass(t.opportunity)}">${t.opportunity.toFixed(0)}</span></td>
      <td>${bestSellBtn(t)}</td>
    </tr>`;
}

const PER_PAGE = 40;
let pageState = { imports: 1, exports: 1 };   // current page per tab

// Page window exactly like the request: 7 centred numbers + hard pages 1 and X,
// ellipsized: e.g. total=20 cur=4 -> "1 2 3 [4] 5 6 7 … 20"; cur=10 -> "1 … 7 8 9 10 11 12 13 … 20".
function pageWindow(cur, total) {
  if (total <= 7) { const a = []; for (let i = 1; i <= total; i++) a.push(i); return a; }
  const set = new Set([1, total]);
  const lo = Math.max(1, cur - 3), hi = Math.min(total, cur + 3);
  for (let i = lo; i <= hi; i++) set.add(i);   // 7-number window: cur-3 .. cur+3
  const nums = [...set].sort((a, b) => a - b);
  const out = [];
  let prev = 0;
  for (const n of nums) {
    if (prev && n - prev > 1) out.push(0);     // gap -> ellipsis
    out.push(n);
    prev = n;
  }
  return out;
}

function pagerHtml(side, totalPages) {
  const cur = pageState[side];
  const prev = `<button class="pg-btn" data-side="${side}" data-go="prev" ${cur<=1?"disabled":""}>⬅</button>`;
  const nxt  = `<button class="pg-btn" data-side="${side}" data-go="next" ${cur>=totalPages?"disabled":""}>➡</button>`;
  const nums = pageWindow(cur, totalPages).map((n) =>
    n === 0 ? `<span class="pg-ell">…</span>`
            : `<button class="pg-btn ${n===cur?"active":""}" data-side="${side}" data-go="${n}">${n}</button>`
  ).join("");
  const jump = `<span class="pg-jump">go to <input type="number" min="1" max="${totalPages}" id="pg-input-${side}" value="" placeholder="#">
                <button class="pg-btn" data-side="${side}" data-go="jump">Go</button></span>`;
  return `Page ${cur} / ${totalPages} &nbsp; ${prev} ${nums} ${nxt} ${jump}`;
}

function renderTable(tbody, side, rows) {
  const total = rows.length;
  const pages = Math.max(1, Math.ceil(total / PER_PAGE));
  if (pageState[side] > pages) pageState[side] = pages;
  const cur = pageState[side];
  const pageRows = rows.slice((cur - 1) * PER_PAGE, cur * PER_PAGE);
  tbody.innerHTML = pageRows.length ? pageRows.map(rowHtml).join("") : "<tr><td colspan=15 style='color:var(--muted)'>No trades</td></tr>";
  document.getElementById(`pager-${side}`).innerHTML = pages > 1
    ? pagerHtml(side, pages)
    : `<span style="color:var(--muted);font-size:12px">${total} trades — all on one page</span>`;
}

// ---------- DEEP SEARCH MODAL ----------
// Opens this product on EVERY platform of EVERY market at once — a true
// whole-supply-market scan (porównywarki + main marketplaces per region).
function openDeep(productName, marketId) {
  const q = encodeURIComponent(productName);
  const urls = [];
  const origin = markets.find((m) => m.id === marketId);
  if (origin && origin.deep) {
    for (const [name, tmpl] of origin.deep)
      urls.push({ label: name + " — " + origin.name, href: tmpl.replace("{q}", q) });
  }
  for (const m of markets) {
    if (m.id === marketId) continue;
    if (m.deep) {
      for (const [name, tmpl] of m.deep.slice(0, 2))
        urls.push({ label: name + " — " + m.name, href: tmpl.replace("{q}", q) });
    }
  }
  const box = document.createElement("div");
  box.className = "deep-modal";
  const close = () => box.remove();
  box.innerHTML = `
    <div class="deep-box">
      <h3>Deep search: ${productName}</h3>
      <p class="deep-hint">Scanning the whole supply market — ${urls.length} sources. Click one, or open all.</p>
      <div class="deep-list">${urls.map((u) =>
        `<a class="deep-link" href="${u.href}" target="_blank" rel="noopener">${u.label}</a>`).join("")}</div>
      <div class="deep-actions">
        <button id="deepAll">Open all (${urls.length} tabs)</button>
        <button id="deepClose">Close</button>
      </div>
    </div>`;
  document.body.appendChild(box);
  box.querySelector("#deepClose").addEventListener("click", close);
  box.querySelector("#deepAll").addEventListener("click", () => {
    urls.forEach((u, i) => setTimeout(() => window.open(u.href, "_blank", "noopener"), i * 180));
    close();
  });
  box.addEventListener("click", (e) => { if (e.target === box) close(); });
}
document.addEventListener("click", (e) => {
  const b = e.target.closest(".prod-link");
  if (b) { openDeep(b.dataset.deep, b.dataset.market); return; }
});
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape") document.querySelector(".deep-modal")?.remove();
});

// pager clicks: page numbers, prev/next, jump-to-page
document.addEventListener("click", (e) => {
  const b = e.target.closest(".pg-btn");
  if (!b || !b.dataset.side) return;
  const side = b.dataset.side, go = b.dataset.go;
  const total = (side === "imports" ? lastImports : lastExports).length;
  const pages = Math.max(1, Math.ceil(total / PER_PAGE));
  if (go === "prev") pageState[side] = Math.max(1, pageState[side] - 1);
  else if (go === "next") pageState[side] = Math.min(pages, pageState[side] + 1);
  else if (go === "jump") {
    const inp = document.getElementById(`pg-input-${side}`);
    const n = parseInt(inp.value, 10);
    if (n >= 1 && n <= pages) pageState[side] = n; else inp.value = "";
  }
  else pageState[side] = parseInt(go, 10);
  renderTable(side === "imports" ? importsTbody : exportsTbody, side, side === "imports" ? lastImports : lastExports);
});

async function load() {
  meta.innerHTML = "";
  const allHomes = allCheck.checked;              // every watched market = its own home
  const watch = Array.from(selected).join(",");
  const params = new URLSearchParams({
    sort: sortSel.value,
    category: catSel.value,
    vat: vatCheck.checked ? "1" : "0",
    margin_ref: 0.3,
    channel: channel,
  });
  if (allHomes) {
    params.set("homes", "all");
  } else {
    params.set("home", homeSel.value);
    params.set("markets", watch || "all");
  }
  try {
    const res = await fetch("/api/trades?" + params.toString());
    if (!res.ok) throw new Error("HTTP " + res.status);
    const data = await res.json();
    const onlyReal = realOnlyCheck.checked;
    const keepReal = (rows) => onlyReal ? rows.filter((r) => r.price_source === "real") : rows;
    const imps = keepReal(data.imports), exps = keepReal(data.exports);
    const realTot = data.imports.filter((r) => r.price_source === "real").length +
                    data.exports.filter((r) => r.price_source === "real").length;
    meta.innerHTML = `${data.count} trades (${imps.length} import shown, ${exps.length} export shown)`
      + ` &middot; real deals: <b style="color:var(--good)">${realTot}</b> &middot; home: <b>${data.home}</b>`
      + (onlyReal ? " &middot; <b>real only</b>" : "");
    lastImports = imps;
    lastExports = exps;                                   // FIXED: was data.exports
    pageState.imports = 1; pageState.exports = 1;
    renderTable(importsTbody, "imports", imps);           // FIXED: was data.imports
    renderTable(exportsTbody, "exports", exps);           // FIXED: was data.exports
  } catch (err) {
    meta.innerHTML = `<span class="err">Error: ${err.message}</span>`;
  }
}

// Sort buttons (sync with the Sort select)
const sortBtns = document.querySelectorAll(".sort-btn");
sortBtns.forEach((btn) => {
  btn.addEventListener("click", () => {
    sortSel.value = btn.dataset.sort;
    sortBtns.forEach((b) => b.classList.toggle("active", b === btn));
    load();
  });
});
// keep the button highlight in sync when sorting via the dropdown
sortSel.addEventListener("change", () => {
  sortBtns.forEach((b) => b.classList.toggle("active", b.dataset.sort === sortSel.value));
});

// Import / Export tabs
const tabs = document.querySelectorAll(".tab");
tabs.forEach((btn) => {
  btn.addEventListener("click", () => {
    tabs.forEach((b) => b.classList.remove("active"));
    btn.classList.add("active");
    const tab = btn.dataset.tab;
    document.querySelectorAll(".panel").forEach((p) => {
      p.classList.toggle("hidden", p.id !== `panel-${tab}`);
    });
  });
});

// Main Market / Whole Market switch
function setChannel(c) {
  channel = c;
  document.querySelectorAll(".chan-btn").forEach((b) => b.classList.toggle("active", b.id === (c === "whole" ? "chWhole" : "chMain")));
  $("#imports thead").innerHTML = tableHeaders();
  $("#exports thead").innerHTML = tableHeaders();
  load();
}
$("#chMain").addEventListener("click", () => setChannel("main"));
$("#chWhole").addEventListener("click", () => setChannel("whole"));

refreshBtn.addEventListener("click", load);

// Heartbeat: tell the server we're still here every 2s.
// When this page closes, pings stop and the server shuts itself down.
setInterval(() => { fetch("/api/ping").catch(() => {}); }, 2000);
["change", "input"].forEach((ev) =>
  [sortSel, catSel, homeSel, vatCheck, allCheck, realOnlyCheck].forEach((el) => el.addEventListener(ev, load)));

(async () => {
  try {
    await loadMarkets();
    await loadCategories();
    $("#imports thead").innerHTML = tableHeaders();
    $("#exports thead").innerHTML = tableHeaders();
    await load();
  } catch (err) {
    meta.innerHTML = `<span class="err">Init error: ${err.message}</span>`;
  }
})();
