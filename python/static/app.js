const $ = (id) => document.getElementById(id);

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
  if ([...sel.options].some((o) => o.value === "default_experiment")) {
    sel.value = "default_experiment";
  }
}

function drawEquity(canvas, seriesList) {
  const ctx = canvas.getContext("2d");
  const w = canvas.width = canvas.clientWidth * devicePixelRatio;
  const h = canvas.height = canvas.clientHeight * devicePixelRatio;
  ctx.scale(devicePixelRatio, devicePixelRatio);
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#10161d";
  ctx.fillRect(0, 0, width, height);

  const colors = ["#3d9cf0", "#3ecf8e", "#f0c14a", "#f07178"];
  let min = Infinity;
  let max = -Infinity;
  for (const s of seriesList) {
    for (const v of s.values) {
      min = Math.min(min, v);
      max = Math.max(max, v);
    }
  }
  if (!Number.isFinite(min) || !Number.isFinite(max) || min === max) {
    min -= 1;
    max += 1;
  }

  const pad = 12;
  seriesList.forEach((s, idx) => {
    if (!s.values.length) return;
    ctx.strokeStyle = colors[idx % colors.length];
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    s.values.forEach((v, i) => {
      const x = pad + (i / Math.max(1, s.values.length - 1)) * (width - 2 * pad);
      const y = height - pad - ((v - min) / (max - min)) * (height - 2 * pad);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();
  });
}

function fmt(n) {
  if (typeof n !== "number" || Number.isNaN(n)) return "—";
  return n.toFixed(2);
}

function renderTable(results) {
  const body = $("resultsBody");
  body.innerHTML = "";
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
      <td>${fmt(row.var_95)}</td>
      <td>${row.risk_killed ? "yes" : "no"}</td>
    `;
    body.appendChild(tr);
  }
}

function renderWalkForward(wf) {
  const el = $("wfPanel");
  if (!wf || !wf.folds) {
    el.innerHTML = `<p class="muted">Walk-forward not requested.</p>`;
    return;
  }
  const rows = wf.folds.map((f) => `
    <tr>
      <td>${f.fold}</td>
      <td>${fmt(f.is_mtm)}</td>
      <td class="${f.oos_mtm >= 0 ? "good" : "bad"}">${fmt(f.oos_mtm)}</td>
      <td>${f.oos_fills}</td>
      <td>${fmt(f.oos_var_95)}</td>
    </tr>`).join("");
  el.innerHTML = `
    <p class="muted">Strategy <span class="mono">${wf.strategy}</span>
      · mean OOS MTM <strong>${fmt(wf.oos_mtm_mean)}</strong></p>
    <table>
      <thead><tr><th>Fold</th><th>IS MTM</th><th>OOS MTM</th><th>OOS fills</th><th>OOS VaR95</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>`;
}

async function runExperiment() {
  const btn = $("runBtn");
  const status = $("status");
  btn.disabled = true;
  status.textContent = "Running C++ experiment…";
  try {
    const res = await fetch("/api/experiments/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        config: $("config").value,
        walk_forward: $("wf").checked,
      }),
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || "request failed");

    const comparison = data.comparison || data;
    const experiment = comparison.experiment || comparison;
    const results = experiment.results || [];
    renderTable(results);

    const series = results.map((r) => ({
      name: r.strategy,
      values: (r.equity_curve || []).slice(0, 1500),
    }));
    drawEquity($("equityChart"), series);
    renderWalkForward(data.walk_forward);

    const leak = (comparison.leakage || [])[0];
    $("leakage").textContent = leak
      ? `Leakage foil: LOB MTM=${fmt(leak.lob_mtm)} vs naive bar MTM=${fmt(leak.naive_mtm)} (fantasy fills=${leak.fantasy_fills})`
      : "";

    status.textContent = "Done.";
  } catch (err) {
    status.textContent = String(err.message || err);
  } finally {
    btn.disabled = false;
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
    }),
  });
  const data = await res.json();
  $("stressOut").textContent = JSON.stringify(data, null, 2);
}

$("runBtn").addEventListener("click", runExperiment);
$("stressBtn").addEventListener("click", runStress);
loadConfigs().catch((e) => {
  $("status").textContent = String(e);
});
