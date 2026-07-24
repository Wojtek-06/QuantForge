const $ = (id) => document.getElementById(id);
const COLORS = ["#3d9cf0", "#3ecf8e", "#f0c14a", "#f07178", "#b48ead"];

let lastPayload = null;

async function loadConfigs() {
  const res = await fetch("/api/configs");
  const configs = await res.json();
  const sel = $("config");
  sel.innerHTML = "";
  for (const c of configs) {
    const opt = document.createElement("option");
    opt.value = c.name;
    opt.textContent = c.name;
    sel.appendChild(opt);
  }
  if ([...sel.options].some((o) => o.value === "walk_forward_search")) {
    sel.value = "walk_forward_search";
  } else if ([...sel.options].some((o) => o.value === "default_experiment")) {
    sel.value = "default_experiment";
  }
}

async function loadHealth() {
  try {
    const res = await fetch("/api/health");
    const data = await res.json();
    const badge = $("healthBadge");
    const risk = data.risk_engine || {};
    if (data.ok) {
      badge.textContent = risk.core_risk_engine_loaded
        ? "API · RiskEngine"
        : "API · BS fallback";
      badge.className = risk.core_risk_engine_loaded ? "badge ok" : "badge warn";
    }
  } catch {
    $("healthBadge").textContent = "API down";
    $("healthBadge").className = "badge warn";
  }
}

function drawSeries(canvas, seriesList, { fillUnder = false } = {}) {
  const ctx = canvas.getContext("2d");
  const dpr = devicePixelRatio || 1;
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  canvas.width = width * dpr;
  canvas.height = height * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#10161d";
  ctx.fillRect(0, 0, width, height);

  let min = Infinity;
  let max = -Infinity;
  for (const s of seriesList) {
    for (const v of s.values || []) {
      min = Math.min(min, v);
      max = Math.max(max, v);
    }
  }
  if (!Number.isFinite(min) || !Number.isFinite(max) || min === max) {
    min = (Number.isFinite(min) ? min : 0) - 1;
    max = (Number.isFinite(max) ? max : 0) + 1;
  }

  const pad = 14;
  // zero line
  if (min < 0 && max > 0) {
    const y0 = height - pad - ((0 - min) / (max - min)) * (height - 2 * pad);
    ctx.strokeStyle = "#2a3644";
    ctx.beginPath();
    ctx.moveTo(pad, y0);
    ctx.lineTo(width - pad, y0);
    ctx.stroke();
  }

  seriesList.forEach((s, idx) => {
    const values = s.values || [];
    if (!values.length) return;
    const color = s.color || COLORS[idx % COLORS.length];
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.6;
    ctx.beginPath();
    values.forEach((v, i) => {
      const x = pad + (i / Math.max(1, values.length - 1)) * (width - 2 * pad);
      const y = height - pad - ((v - min) / (max - min)) * (height - 2 * pad);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();
    if (fillUnder) {
      const lastX = pad + ((values.length - 1) / Math.max(1, values.length - 1)) * (width - 2 * pad);
      const baseY = height - pad - ((0 - min) / (max - min)) * (height - 2 * pad);
      ctx.lineTo(lastX, baseY);
      ctx.lineTo(pad, baseY);
      ctx.closePath();
      ctx.fillStyle = color + "22";
      ctx.fill();
    }
  });
}

function fmt(n) {
  if (typeof n !== "number" || Number.isNaN(n)) return "—";
  return n.toFixed(2);
}

function renderTable(results) {
  const body = $("resultsBody");
  body.innerHTML = "";
  if (!results.length) {
    body.innerHTML = `<tr><td colspan="8" class="muted">No results.</td></tr>`;
    return;
  }
  for (const row of results) {
    const m = row.metrics || {};
    const tr = document.createElement("tr");
    const mtmClass = (m.mtm_pnl || 0) >= 0 ? "good" : "bad";
    tr.innerHTML = `
      <td>${row.strategy}</td>
      <td class="${mtmClass}">${fmt(m.mtm_pnl)}</td>
      <td>${row.inventory ?? "—"}</td>
      <td>${fmt(m.max_drawdown)}</td>
      <td>${m.fills ?? "—"}</td>
      <td>${fmt(m.fill_rate)}</td>
      <td>${fmt(row.var_95 ?? row.risk_summary?.var_95)}</td>
      <td>${row.risk_killed ? "yes" : "no"}</td>
    `;
    body.appendChild(tr);
  }
}

function renderEquity(results) {
  const series = results.map((r, i) => ({
    name: r.strategy,
    color: COLORS[i % COLORS.length],
    values: (r.equity_curve || []).slice(0, 1500),
  }));
  drawSeries($("equityChart"), series);
  $("equityLegend").innerHTML = series
    .map((s) => `<span style="--swatch:${s.color}">${s.name}</span>`)
    .join("");

  const ddSeries = results.map((r, i) => ({
    name: r.strategy,
    color: COLORS[i % COLORS.length],
    values: (r.drawdown_curve || []).slice(0, 1500),
  }));
  drawSeries($("ddChart"), ddSeries, { fillUnder: true });
}

function renderWalkForward(wf) {
  const el = $("wfPanel");
  if (!wf || !wf.folds) {
    el.innerHTML = `<p class="muted">Walk-forward not requested.</p>`;
    drawSeries($("wfChart"), []);
    return;
  }
  const rows = wf.folds.map((f) => {
    const p = f.selected_params || {};
    const knob = p.half_spread != null
      ? `hs=${p.half_spread}`
      : (p.gamma != null ? `γ=${fmt(p.gamma)}` : "");
    return `
    <tr>
      <td>${f.fold}</td>
      <td>${fmt(f.is_mtm)}</td>
      <td class="${f.oos_mtm >= 0 ? "good" : "bad"}">${fmt(f.oos_mtm)}</td>
      <td>${f.oos_fills ?? "—"}</td>
      <td>${fmt(f.oos_var_95)}</td>
      <td class="mono">${knob}</td>
    </tr>`;
  }).join("");

  const searchNote = wf.param_search_enabled
    ? `IS search (${wf.search_method || "grid"}) frozen for OOS`
    : "fixed params";

  el.innerHTML = `
    <p class="muted">Strategy <span class="mono">${wf.strategy}</span>
      · mean OOS MTM <strong class="${wf.oos_mtm_mean >= 0 ? "good" : "bad"}">${fmt(wf.oos_mtm_mean)}</strong>
      · ${searchNote}</p>
    <table>
      <thead><tr><th>Fold</th><th>IS MTM</th><th>OOS MTM</th><th>OOS fills</th><th>OOS VaR95</th><th>Params</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>`;

  drawSeries($("wfChart"), [
    { name: "IS", color: COLORS[0], values: wf.folds.map((f) => f.is_mtm) },
    { name: "OOS", color: COLORS[1], values: wf.folds.map((f) => f.oos_mtm) },
  ]);
}

function renderLeakage(comparison) {
  const leak = (comparison.leakage || [])[0];
  const el = $("leakPanel");
  if (!leak) {
    el.innerHTML = `<p class="muted">No leakage case in this payload.</p>`;
    return;
  }
  const delta = (leak.lob_mtm ?? 0) - (leak.naive_mtm ?? 0);
  el.innerHTML = `
    <p class="muted">${leak.title || "naive bar vs LOB"}</p>
    <div class="leak-card">
      <div class="metric"><div class="label">LOB MTM</div><div class="value ${leak.lob_mtm >= 0 ? "good" : "bad"}">${fmt(leak.lob_mtm)}</div></div>
      <div class="metric"><div class="label">Naive bar MTM</div><div class="value">${fmt(leak.naive_mtm)}</div></div>
      <div class="metric"><div class="label">Fantasy fills</div><div class="value">${leak.fantasy_fills ?? "—"}</div></div>
      <div class="metric"><div class="label">LOB − naive</div><div class="value ${delta >= 0 ? "good" : "warn"}">${fmt(delta)}</div></div>
    </div>`;
}

function renderPayload(data) {
  lastPayload = data;
  const comparison = data.comparison || data;
  const experiment = comparison.experiment || comparison;
  const results = experiment.results || [];
  renderTable(results);
  renderEquity(results);
  renderWalkForward(data.walk_forward);
  renderLeakage(comparison);
  $("backendTag").textContent = data.backend ? `backend: ${data.backend}` : "";
}

async function loadHistory() {
  const res = await fetch("/api/experiments/history?limit=20");
  const items = await res.json();
  const el = $("historyList");
  if (!items.length) {
    el.innerHTML = `<span class="muted">No runs yet.</span>`;
    return;
  }
  el.innerHTML = items.map((it) => {
    const when = new Date(it.created_at * 1000).toLocaleString();
    const wf = it.walk_forward ? " · wf" : "";
    return `<button class="history-item" data-id="${it.id}">${when}<br/>${it.config}${wf}</button>`;
  }).join("");
  el.querySelectorAll(".history-item").forEach((btn) => {
    btn.addEventListener("click", async () => {
      el.querySelectorAll(".history-item").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      const res = await fetch(`/api/experiments/history/${btn.dataset.id}`);
      const row = await res.json();
      if (row.payload) renderPayload(row.payload);
      $("status").textContent = `Loaded history ${row.id}`;
    });
  });
}

async function runExperiment() {
  const btn = $("runBtn");
  const status = $("status");
  btn.disabled = true;
  status.textContent = "Running experiment…";
  try {
    const res = await fetch("/api/experiments/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        config: $("config").value,
        walk_forward: $("wf").checked,
        save: true,
      }),
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || "request failed");
    renderPayload(data);
    await loadHistory();
    status.textContent = data.history_id
      ? `Done · saved ${data.history_id}`
      : "Done.";
  } catch (err) {
    status.textContent = String(err.message || err);
  } finally {
    btn.disabled = false;
  }
}

function renderStress(data) {
  $("riskSource").textContent = data.fallback
    ? "source: fallback_bs"
    : (data.base?.source || data.source || "risk");
  if (data.scenarios) {
    $("stressSummary").innerHTML = `
      Scenarios <strong>${data.n_scenarios}</strong>
      · worst PnL <span class="bad">${fmt(data.worst_pnl)}</span>
      · best PnL <span class="good">${fmt(data.best_pnl)}</span>
      · stress VaR95 <span class="warn">${fmt(data.stress_var_95)}</span>`;
    $("stressOut").textContent = JSON.stringify(
      {
        base: data.base,
        worst_pnl: data.worst_pnl,
        best_pnl: data.best_pnl,
        stress_var_95: data.stress_var_95,
        core: data.core_risk_engine_loaded,
      },
      null,
      2
    );
    drawSeries($("stressChart"), [
      {
        name: "scenario pnl",
        color: COLORS[3],
        values: data.scenarios.map((s) => s.pnl),
      },
    ]);
  } else {
    $("stressSummary").textContent = `Analytic ${fmt(data.analytic_value)} · delta ${fmt(data.delta)}`;
    $("stressOut").textContent = JSON.stringify(data, null, 2);
    drawSeries($("stressChart"), []);
  }
}

async function runStress() {
  const res = await fetch("/api/risk/stress", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      spot: Number($("spot").value),
      strike: Number($("strike").value),
      vol: Number($("vol").value),
      overnight: true,
    }),
  });
  const data = await res.json();
  renderStress(data);
}

$("runBtn").addEventListener("click", runExperiment);
$("stressBtn").addEventListener("click", runStress);
$("wf").checked = true;

Promise.all([loadConfigs(), loadHealth(), loadHistory()]).catch((e) => {
  $("status").textContent = String(e);
});
