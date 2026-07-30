"use strict";

const runFlashButton = document.getElementById("run-flash-ab");
const runWatchdogButton = document.getElementById("run-watchdog-reset");
const runRetentionPrepareButton = document.getElementById("run-rtc-retention-prepare");
const runRetentionVerifyButton = document.getElementById("run-rtc-retention-verify");
const flashConfirmationInput = document.getElementById("flash-confirmation");
const watchdogConfirmationInput = document.getElementById("watchdog-confirmation");
const flashResult = document.getElementById("flash-ab-result");
const watchdogResult = document.getElementById("watchdog-result");
const retentionResult = document.getElementById("rtc-retention-result");

function confirmedPayload(identity, confirmation) {
  return JSON.stringify({
    serial_number: identity.serialNumber,
    operator: identity.operatorName,
    port: identity.port,
    confirmation,
  });
}

function renderSteps(steps) {
  return (steps || []).map((step) => {
    const checks = (step.checks || []).map(renderCheck).join("");
    return `
      <li class="diagnostic-command command-${String(step.status).toLowerCase()}">
        <div><strong>${escapeHtml(step.status)}</strong> ${escapeHtml(step.name)}, ${escapeHtml(step.duration_ms)} ms</div>
        <div>${escapeHtml(step.detail)}</div>
        ${checks ? `<ul class="check-list nested">${checks}</ul>` : ""}
      </li>`;
  }).join("");
}

function renderAdvanced(element, result, extra = "") {
  const error = result.error ? `<p><strong>Ошибка:</strong> ${escapeHtml(result.error)}</p>` : "";
  const steps = renderSteps(result.steps);
  element.className = `test-result ${resultCss(result.result)}`;
  element.dataset.kind = "result";
  element.innerHTML = `<strong>${escapeHtml(result.result)}</strong> — ${escapeHtml(result.port || "порт не задан")}, ${escapeHtml(result.duration_ms)} ms${error}${extra}${steps ? `<ul class="check-list command-list">${steps}</ul>` : ""}${result.note ? `<p>${escapeHtml(result.note)}</p>` : ""}${reportLine(result)}`;
}

async function runConfirmed(button, element, url, confirmation, runningText, renderExtra) {
  const identity = testIdentity();
  if (identity.error) {
    setResult(element, identity.error, "fail", "validation");
    identity.focus?.focus();
    return;
  }
  button.disabled = true;
  setResult(element, runningText, "running", "running");
  try {
    const result = await requestJson(url, {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: confirmedPayload(identity, confirmation),
    });
    renderAdvanced(element, result, renderExtra ? renderExtra(result) : "");
  } catch (error) {
    setResult(element, `FAIL — ${error.message}`, "fail", "result");
  } finally {
    button.disabled = false;
  }
}

runFlashButton.addEventListener("click", () => {
  const confirmation = flashConfirmationInput.value.trim();
  runConfirmed(
    runFlashButton,
    flashResult,
    "/api/tests/lcp/flash-ab",
    confirmation,
    "RUNNING — backup, Flash A/B commit, два reboot и восстановление…",
    (result) => `<p>slots: original=${escapeHtml(result.original_slot)}, test=${escapeHtml(result.test_slot)}, restored=${escapeHtml(result.restored_slot)}; restored=${escapeHtml(result.restored)}; recovery_required=${escapeHtml(result.recovery_required)}</p>${result.backup_file ? `<p><strong>Recovery:</strong> ${escapeHtml(result.backup_file)}</p>` : ""}`,
  );
});

runWatchdogButton.addEventListener("click", () => {
  const confirmation = watchdogConfirmationInput.value.trim();
  runConfirmed(
    runWatchdogButton,
    watchdogResult,
    "/api/tests/lcp/watchdog-reset",
    confirmation,
    "RUNNING — watchdog reset и ожидание USB reconnect…",
    (result) => `<p>boot_count ${escapeHtml(result.before_boot_count)} → ${escapeHtml(result.after_boot_count)}; reset=${escapeHtml(result.reset_type)}; recovery=${escapeHtml(result.recovery_performed)}; port=${escapeHtml(result.reconnected_port || "unknown")}</p>`,
  );
});

runRetentionPrepareButton.addEventListener("click", () => {
  runConfirmed(
    runRetentionPrepareButton,
    retentionResult,
    "/api/tests/lcp/rtc-retention/prepare",
    "RTC RETENTION PREPARE",
    "RUNNING — синхронизация RTC и сохранение baseline…",
    (result) => `<p>phase=${escapeHtml(result.phase)}; state=${escapeHtml(result.state_file || "none")}; boot_count=${escapeHtml(result.before_boot_count)}; battery=${escapeHtml(result.battery_state)}</p>`,
  );
});

runRetentionVerifyButton.addEventListener("click", () => {
  runConfirmed(
    runRetentionVerifyButton,
    retentionResult,
    "/api/tests/lcp/rtc-retention/verify",
    "RTC RETENTION VERIFY",
    "RUNNING — проверка RTC после полного снятия питания…",
    (result) => `<p>phase=${escapeHtml(result.phase)}; PC=${escapeHtml(result.elapsed_pc_seconds)} s; RTC=${escapeHtml(result.elapsed_rtc_seconds)} s; error=${escapeHtml(result.retention_error_seconds)} s; boot_count ${escapeHtml(result.before_boot_count)} → ${escapeHtml(result.after_boot_count)}; battery=${escapeHtml(result.battery_state)}</p>`,
  );
});
