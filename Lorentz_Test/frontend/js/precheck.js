"use strict";

const runPrecheckButton = document.getElementById("run-precheck");
const precheckResult = document.getElementById("precheck-result");

function precheckStatusCss(status) {
  if (status === "PASS" || status === "READY") return "pass";
  if (status === "WARNING" || status === "SKIPPED") return "waiting";
  return "fail";
}

function renderPrecheck(result) {
  const checks = (result.checks || []).map((item) => `
    <li class="diagnostic-command command-${String(item.status).toLowerCase()}">
      <div>
        <strong>${escapeHtml(item.status)}</strong>
        ${escapeHtml(item.title)}
        ${item.blocking ? '<span class="blocking-label">блокирует запуск</span>' : ""}
      </div>
      <div>${escapeHtml(item.detail)}</div>
    </li>`).join("");
  const failures = result.blocking_failures?.length
    ? `<p><strong>Блокирующие проблемы:</strong> ${escapeHtml(result.blocking_failures.join(", "))}</p>`
    : "";
  const warnings = result.warnings?.length
    ? `<p><strong>Предупреждения:</strong> ${escapeHtml(result.warnings.join(", "))}</p>`
    : "";

  precheckResult.className = `test-result ${precheckStatusCss(result.result)}`;
  precheckResult.dataset.kind = "result";
  precheckResult.innerHTML = `
    <strong>${escapeHtml(result.result)}</strong>
    — ${escapeHtml(result.duration_ms)} ms
    <p>USB ${escapeHtml(result.port || "не задан")}; firmware ${escapeHtml(result.firmware_version || "не подтверждена")}.</p>
    ${failures}${warnings}
    <ul class="check-list command-list precheck-list">${checks}</ul>`;
}

async function executePrecheck(identity, options = {}) {
  const saveStationFirst = options.saveStationFirst !== false;
  const previousDisabled = runPrecheckButton.disabled;
  runPrecheckButton.disabled = true;
  setResult(precheckResult, "RUNNING — проверка готовности стенда…", "running", "running");

  try {
    if (saveStationFirst) {
      const config = await requestJson("/api/station", {
        method: "PUT",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(readForm()),
      });
      fillForm(config);
      setMessage("Настройки сохранены перед pre-check.", true);
    }

    const result = await requestJson("/api/tests/lcp/precheck", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: requestPayload(identity),
    });
    renderPrecheck(result);
    return result;
  } catch (error) {
    setResult(precheckResult, `NOT READY — ${error.message}`, "fail", "result");
    throw error;
  } finally {
    runPrecheckButton.disabled = previousDisabled;
  }
}

async function runStandalonePrecheck() {
  const identity = testIdentity();
  if (identity.error) {
    setResult(precheckResult, identity.error, "fail", "validation");
    identity.focus?.focus();
    return;
  }
  try {
    await executePrecheck(identity);
  } catch (error) {
    // The result card already contains the actionable backend error.
  }
}

runPrecheckButton.addEventListener("click", runStandalonePrecheck);
window.lorentzPrecheck = {execute: executePrecheck, render: renderPrecheck};
