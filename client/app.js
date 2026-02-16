const $ = (id) => document.getElementById(id);

let running = false;
let timer = null;

let lastTempC = null;
let lastTsIso = null;

function nowLocal() {
  return new Date().toLocaleString();
}

function log(line) {
  const box = $("log");
  box.textContent += `[${nowLocal()}] ${line}\n`;
  box.scrollTop = box.scrollHeight;
}

function setState(isRunning) {
  running = isRunning;

  const pill = $("statePill");
  pill.dataset.state = running ? "running" : "stopped";
  pill.textContent = running ? "Running" : "Stopped";

  $("startBtn").disabled = running;
  $("stopBtn").disabled = !running;
}

function formatTimestamp(iso) {
  try {
    return new Date(iso).toLocaleString();
  } catch {
    return "—";
  }
}

function displayTemp() {
  const units = $("units").value;

  $("tempUnit").textContent = units === "F" ? "°F" : "°C";

  if (lastTempC === null) {
    $("tempValue").textContent = "— —";
    $("tempTs").textContent = "—";
    return;
  }

  const value = units === "F"
    ? (lastTempC * 9/5 + 32)
    : lastTempC;

  $("tempValue").textContent = value.toFixed(2);
  $("tempTs").textContent = lastTsIso ? formatTimestamp(lastTsIso) : "—";
}

async function checkHealth() {
  try {
    const r = await fetch("/health", { cache: "no-store" });
    return r.ok;
  } catch {
    return false;
  }
}

async function readOnce() {
  const ok = await checkHealth();
  if (!ok) {
    log("API offline (health check failed).");
    return;
  }

  try {
    const r = await fetch("/temperature", { cache: "no-store" });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);

    const data = await r.json();

    lastTempC = Number(data.value_c);
    lastTsIso = data.timestamp ?? null;

    displayTemp();
    log(`Read Once -> ${lastTempC.toFixed(2)} °C (ts: ${formatTimestamp(lastTsIso)})`);
  } catch (e) {
    log(`Read Once failed: ${String(e)}`);
  }
}

function startCollection() {
  const seconds = Math.max(1, parseInt($("interval").value || "2", 10));
  $("interval").value = String(seconds);

  if (timer) clearInterval(timer);

  timer = setInterval(readOnce, seconds * 1000);
  setState(true);
  log(`Start Collection (interval: ${seconds}s)`);
}

function stopCollection() {
  if (timer) clearInterval(timer);
  timer = null;
  setState(false);
  log("Stop Collection");
}

function rearmIfRunning() {
  if (!running) return;
  startCollection();
}

function wireUI() {
  $("readOnceBtn").addEventListener("click", readOnce);
  $("startBtn").addEventListener("click", startCollection);
  $("stopBtn").addEventListener("click", stopCollection);

  $("clearLogBtn").addEventListener("click", () => {
    $("log").textContent = "";
  });

  $("units").addEventListener("change", () => {
    displayTemp();
    log(`Display units set to ${$("units").value === "F" ? "°F" : "°C"}`);
  });

  $("interval").addEventListener("change", () => {
    rearmIfRunning();
    log(`Auto-refresh set to ${$("interval").value}s`);
  });
}

(async function main() {
  wireUI();
  setState(false);
  log("UI loaded.");
  await readOnce(); // initial fetch to prove wiring
})();
