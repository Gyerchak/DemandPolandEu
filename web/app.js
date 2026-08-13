"use strict";

const $ = (sel) => document.querySelector(sel);
const sortSel = $("#sort");
const marginRef = $("#margin-ref");
const vatCheck = $("#vat");
const refreshBtn = $("#refresh");
const tbody = $("#rank tbody");
const meta = $("#meta");
const detail = $("#detail");

function scoreClass(v) {
  if (v >= 65) return "hi";
  if (v >= 35) return "mid";
  return "lo";
}

function bar(value, extra = "") {
  const pct = Math.round(Math.min(100, Math.max(0, value * 100)));
  const cls = pct >= 66 ? "good" : pct >= 34 ? "warn" : "bad";
  return `<span class="bar ${cls}"><span style="width:${pct}%"></span></span>`;
}

function regionTag(regionId) {
  if (regionId.startsWith("cn")) return `<span class="tag cn">Chiny</span>`;
  if (regionId.startsWith("pl")) return `<span class="tag pl">PL</span>`;
  return `<span class="tag eu">UE</span>`;
}

function fmtMoney(v) {
  return v.toLocaleString("pl-PL", { minimumFractionDigits: 2, maximumFractionDigits: 2 }) + " PLN";
}

function fmtPct(v) {
  return (v * 100).toFixed(1) + "%";
}

async function load() {
  meta.innerHTML = "";
  detail.classList.add("hidden");
  const params = new URLSearchParams({
    sort: sortSel.value,
    vat: vatCheck.checked ? "1" : "0",
    margin_ref: marginRef.value,
  });
  try {
    const res = await fetch("/api/rank?" + params.toString());
    if (!res.ok) throw new Error("HTTP " + res.status);
    const data = await res.json();
    render(data);
  } catch (err) {
    meta.innerHTML = `<span class="err">Błąd: ${err.message}</span>`;
  }
}

function render(data) {
  meta.innerHTML = `${data.count} produktów &middot; sortowanie: <b>${data.sort}</b>`;
  tbody.innerHTML = "";
  data.rows.forEach((row, idx) => {
    const best = row.best_offer;
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${idx + 1}</td>
      <td><b>${row.name}</b><br><small style="color:var(--muted)">${row.category}</small></td>
      <td>${(row.demand * 100).toFixed(0)}%${bar(row.demand)}</td>
      <td>${(row.popularity * 100).toFixed(0)}%${bar(row.popularity)}</td>
      <td>${(row.success_rate * 100).toFixed(1)}%${bar(row.success_rate)}</td>
      <td style="color:${row.profit_margin > 0 ? "var(--good)" : "var(--bad)"}">${fmtPct(row.profit_margin)}</td>
      <td>${fmtMoney(row.profit_per_unit)}</td>
      <td>${fmtMoney(best.landing_cost_pln)}</td>
      <td>${regionTag(best.region_id)} <small>${best.region_name}</small></td>
      <td>${best.supplier_name}</td>
      <td><span class="score ${scoreClass(row.opportunity)}">${row.opportunity.toFixed(0)}</span></td>
      <td class="expand" title="Szczegóły ofert">▼</td>
    `;
    tr.querySelector(".expand").addEventListener("click", () => showDetail(row));
    tbody.appendChild(tr);
  });
}

function showDetail(row) {
  detail.innerHTML = `
    <h2>${row.name} — wszystkie oferty</h2>
    <p style="color:var(--muted);margin-top:4px">
      Cena lokalna: <b>${fmtMoney(row.local_price_pln)}</b> &middot;
      Popyt: ${(row.demand * 100).toFixed(0)}% &middot;
      Popularność: ${(row.popularity * 100).toFixed(0)}% &middot;
      Success rate: <b>${(row.success_rate * 100).toFixed(1)}%</b> &middot;
      Opportunity: <b>${row.opportunity.toFixed(1)}</b>
    </p>
    <table>
      <thead><tr>
        <th>Dostawca</th><th>Region</th><th>Dostawca/PLN</th><th>Fracht</th><th>Cło</th>
        <th>Koszt lądowania</th><th>Zysk/szt</th><th>Marża</th><th>Czas</th>
      </tr></thead>
      <tbody>
        ${row.offers.map((o) => `
          <tr>
            <td>${o.supplier_name}</td>
            <td>${regionTag(o.region_id)} ${o.region_name}</td>
            <td>${o.unit_price_pln.toLocaleString("pl-PL", { maximumFractionDigits: 2 })}</td>
            <td>${fmtMoney(o.freight_pln)}</td>
            <td>${fmtMoney(o.duty_pln)}</td>
            <td><b>${fmtMoney(o.landing_cost_pln)}</b></td>
            <td style="color:${o.profit_per_unit > 0 ? "var(--good)" : "var(--bad)"}">${fmtMoney(o.profit_per_unit)}</td>
            <td>${fmtPct(o.profit_margin)}</td>
            <td>${o.lead_days} dni</td>
          </tr>`).join("")}
      </tbody>
    </table>
    <button id="close-detail">Zamknij</button>
  `;
  detail.classList.remove("hidden");
  $("#close-detail").addEventListener("click", () => detail.classList.add("hidden"));
}

refreshBtn.addEventListener("click", load);
["change", "input"].forEach((ev) => [sortSel, marginRef, vatCheck].forEach((el) => el.addEventListener(ev, load)));

load();
