"use strict";

const $ = (s) => document.querySelector(s);
const sortSel = $("#sort");
const catSel = $("#category");
const homeSel = $("#home");
const vatCheck = $("#vat");
const allCheck = $("#all");
const refreshBtn = $("#refresh");
const marketsDiv = $("#markets");
const meta = $("#meta");
const importsTbody = $("#imports tbody");
const exportsTbody = $("#exports tbody");

let markets = [];
let selected = new Set();

const MAIN_WATCH = ["visegrad", "china", "poland", "europe", "baltic", "turkiye", "westeu"];

function fmtMoney(v) {
  return v.toLocaleString("en-IE", { minimumFractionDigits: 2, maximumFractionDigits: 2 }) + " €";
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
    .map((m) => `<option value="${m.id}">${m.name}${m.role === "home" ? " (home)" : ""}${m.role === "main" ? " (main)" : ""}</option>`)
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
      <th>Total cost</th><th>Sell</th><th>Profit</th><th>Margin</th><th>Opp</th>
    </tr>`;
}

function rowHtml(t) {
  return `
    <tr>
      <td><b>${t.product_name}</b></td>
      <td><small style="color:var(--muted)">${t.category}</small></td>
      <td>${t.from_market}</td>
      <td>${t.to_market}</td>
      <td>${fmtMoney(t.buy_eur)}</td>
      <td>${fmtMoney(t.freight_eur)}</td>
      <td>${fmtMoney(t.duty_eur)}</td>
      <td>${fmtMoney(t.handling_eur)}</td>
      <td>${fmtMoney(t.vat_eur)}</td>
      <td><b>${fmtMoney(t.total_eur)}</b></td>
      <td>${fmtMoney(t.sell_eur)}</td>
      <td style="color:${moneyClass(t.profit_eur)}"><b>${fmtMoney(t.profit_eur)}</b></td>
      <td style="color:${moneyClass(t.margin)}">${fmtPct(t.margin)}</td>
      <td><span class="score ${scoreClass(t.opportunity)}">${t.opportunity.toFixed(0)}</span></td>
    </tr>`;
}

function renderTable(tbody, rows) {
  tbody.innerHTML = rows.length ? rows.map(rowHtml).join("") : "<tr><td colspan=15 style='color:var(--muted)'>No trades</td></tr>";
}

async function load() {
  meta.innerHTML = "";
  const watch = allCheck.checked ? "all" : Array.from(selected).join(",");
  const params = new URLSearchParams({
    sort: sortSel.value,
    home: homeSel.value,
    category: catSel.value,
    markets: watch,
    vat: vatCheck.checked ? "1" : "0",
    margin_ref: 0.3,
  });
  try {
    const res = await fetch("/api/trades?" + params.toString());
    if (!res.ok) throw new Error("HTTP " + res.status);
    const data = await res.json();
    meta.innerHTML = `${data.count} trades (${data.imports.length} import, ${data.exports.length} export) &middot; home: <b>${data.home}</b> &middot; sort: <b>${sortSel.value}</b>`;
    renderTable(importsTbody, data.imports.slice(0, 40));
    renderTable(exportsTbody, data.exports.slice(0, 40));
  } catch (err) {
    meta.innerHTML = `<span class="err">Error: ${err.message}</span>`;
  }
}

refreshBtn.addEventListener("click", load);
["change", "input"].forEach((ev) =>
  [sortSel, catSel, homeSel, vatCheck, allCheck].forEach((el) => el.addEventListener(ev, load)));

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
