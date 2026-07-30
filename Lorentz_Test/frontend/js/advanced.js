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

let retentionCountdownTimer = null;

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

function exactConfirmation(input, expected, element) {
  const actual = input.value.trim();
  if (actual === expected) return actual;
  setResult(element, `Введите подтверждение точно: ${expected}`, "fail", "validation");
  input.focus();
  input.select();
  return null;
}

function startRetentionCountdown(seconds = 30) {
  if (retentionCountdownTimer !== null) clearInterval(retentionCountdownTimer);
  const readyAt = Date.now() + (seconds * 1000);
  runRetentionVerifyButton.disabled = true;

  const update = () => {
    const remaining = Math.max(0, Math.ceil((readyAt - Date.now()) / 1000));
    if (remaining <= 0) {
      clearInterval(retentionCountdownTimer);
      retentionCountdownTimer = null;
      runRetentionVerifyButton.disabled = false;
      runRetentionVerifyButton.textContent = "2. Проверить";
      return;
    }
    runRetentionVerifyButton.textContent = `2. Проверить (${remaining} с)`;
  };

  update();
  retentionCountdownTimer = window.setInterval(update, 250);
}

async function runConfirmed(button, element, url, confirmation, runningText, renderExtra) {
  const identity = testIdentity();
  if (identity.error) {
    setResult(element, identity.error, "fail", "validation");
    identity.focus?.focus();
    return null;
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
    return result;
  } catch (error) {
    setResult(element, `FAIL — ${error.message}`, "fail", "result");
    return null;
  } finally {
    button.disabled = false;
  }
}

runFlashButton.addEventListener("click", () => {
  const confirmation = exactConfirmation(flashConfirmationInput, "FLASH A/B", flashResult);
  if (confirmation === null) return;
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
  const confirmation = exactConfirmation(
    watchdogConfirmationInput,
    "WATCHDOG RESET",
    watchdogResult,
  );
  if (confirmation === null) return;
  runConfirmed(
    runWatchdogButton,
    watchdogResult,
    "/api/tests/lcp/watchdog-reset",
    confirmation,
    "RUNNING — watchdog reset и ожидание USB reconnect…",
    (result) => `<p>boot_count ${escapeHtml(result.before_boot_count)} → ${escapeHtml(result.after_boot_count)}; reset=${escapeHtml(result.reset_type)}; recovery=${escapeHtml(result.recovery_performed)}; port=${escapeHtml(result.reconnected_port || "unknown")}</p>`,
  );
});

runRetentionPrepareButton.addEventListener("click", async () => {
  const result = await runConfirmed(
    runRetentionPrepareButton,
    retentionResult,
    "/api/tests/lcp/rtc-retention/prepare",
    "RTC RETENTION PREPARE",
    "RUNNING — синхронизация RTC и сохранение baseline…",
    (value) => `<p>phase=${escapeHtml(value.phase)}; state=${escapeHtml(value.state_file || "none")}; boot_count=${escapeHtml(value.before_boot_count)}; battery=${escapeHtml(value.battery_state)}</p>`,
  );
  if (result?.result === "PASS") startRetentionCountdown(30);
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
