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
      <th>Total cost</th><th>Sell</th><th>Profit</th><th>Margin</th><th>Opp</th>
    </tr>`;
}

function productCell(t) {
  const name = t.product_name;
  if (t.supplier_url) {
    return `<a class="prod" href="${t.supplier_url}" target="_blank" rel="noopener">${name}</a>`;
  }
  const q = encodeURIComponent(name);
  return `<a class="prod" href="https://www.alibaba.com/trade/search?SearchText=${q}" target="_blank" rel="noopener" title="Alibaba search">${name}</a>`;
}

function rowHtml(t) {
  return `
    <tr>
      <td><b>${productCell(t)}</b></td>
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
  const allHomes = allCheck.checked;              // every watched market = its own home
  const watch = Array.from(selected).join(",");
  const params = new URLSearchParams({
    sort: sortSel.value,
    category: catSel.value,
    vat: vatCheck.checked ? "1" : "0",
    margin_ref: 0.3,
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
    meta.innerHTML = `${data.count} trades (${data.imports.length} import, ${data.exports.length} export) &middot; home: <b>${data.home}</b> &middot; sort: <b>${sortSel.value}</b>`;
    renderTable(importsTbody, data.imports.slice(0, 40));
    renderTable(exportsTbody, data.exports.slice(0, 40));
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

refreshBtn.addEventListener("click", load);

// Heartbeat: tell the server we're still here every 2s.
// When this page closes, pings stop and the server shuts itself down.
setInterval(() => { fetch("/api/ping").catch(() => {}); }, 2000);
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
