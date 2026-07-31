"use strict";

const runFullTestButton = document.getElementById("run-full-test");
const fullTestResult = document.getElementById("full-test-result");
const FULL_RUN_POLL_MS = 500;

const routineStageTargets = {
  hello: helloResult,
  diagnostics: diagnosticsResult,
  rs485: activeRs485Result,
  ethernet: activeEthernetResult,
  services: activeServicesResult,
  hmi: hmiResult,
};

let activeFullRunId = null;
let fullRunPollTimer = null;
let fullRunPollToken = 0;

function fullStatusCss(status) {
  if (status === "PASS") return "pass";
  if (status === "FIXTURE_ERROR") return "fixture-error";
  if (status === "INCOMPLETE" || status === "SKIPPED" || status === "WAITING") return "waiting";
  if (status === "RUNNING") return "running";
  return "fail";
}

function hardwareStatusCss(status) {
  if (status === "PASS") return "pass";
  if (status === "FAIL") return "fail";
  if (status === "FIXTURE_ERROR") return "fixture_error";
  return "skipped";
}

function setHardwareControlsLocked(locked) {
  document.querySelectorAll('button[id^="run-"]').forEach((button) => {
    button.disabled = locked;
  });
  saveButton.disabled = locked;
}

function renderRoutineProgress(stage) {
  const element = routineStageTargets[stage.key];
  if (!element) return;

  const status = stage.effective_result || stage.status || "WAITING";
  const report = stage.report_file
    ? `<p><strong>JSON:</strong> ${escapeHtml(stage.report_file)}</p>`
    : "";
  const blocked = stage.blocked_by
    ? `<p><strong>Заблокировано этапом:</strong> ${escapeHtml(stage.blocked_by)}</p>`
    : "";
  const duration = stage.duration_ms !== null && stage.duration_ms !== undefined
    ? `, ${escapeHtml(stage.duration_ms)} ms`
    : "";
  let detail = stage.detail || "Ожидает запуска в последовательности";
  if (stage.status === "RUNNING") detail = "Выполняется в составе полной проверки";

  element.className = `test-result ${fullStatusCss(status)}`;
  element.dataset.kind = stage.status === "RUNNING" ? "running" : "result";
  element.innerHTML = `
    <strong>${escapeHtml(status)}</strong>${duration}
    <p>${escapeHtml(detail)}</p>
    ${blocked}${report}`;
}

function renderRunStageCards(state) {
  for (const stage of state.stages || []) renderRoutineProgress(stage);
}

function renderRoutineStageSummary(stage) {
  const element = routineStageTargets[stage.key];
  if (!element) return;

  const rawStatus = stage.raw_result || "SKIPPED";
  const effectiveStatus = stage.effective_result || rawStatus;
  const effective = effectiveStatus !== rawStatus
    ? `<p><strong>Итог этапа:</strong> ${escapeHtml(effectiveStatus)}</p>`
    : "";
  const blocked = stage.blocked_by
    ? `<p><strong>Заблокировано этапом:</strong> ${escapeHtml(stage.blocked_by)}</p>`
    : "";
  const error = stage.error
    ? `<p><strong>Ошибка:</strong> ${escapeHtml(stage.error)}</p>`
    : "";
  const report = stage.report_file
    ? `<p><strong>JSON:</strong> ${escapeHtml(stage.report_file)}</p>`
    : "";

  element.className = `test-result ${fullStatusCss(effectiveStatus)}`;
  element.dataset.kind = "result";
  element.innerHTML = `
    <strong>${escapeHtml(effectiveStatus)}</strong>
    — выполнено в составе полной проверки, ${escapeHtml(stage.duration_ms)} ms
    <p>${escapeHtml(stage.detail || "Результат этапа получен.")}</p>
    ${effective}${blocked}${error}${report}`;
}

function renderRoutineStageSummaries(result) {
  const rendered = new Set();
  for (const stage of result.stages || []) {
    renderRoutineStageSummary(stage);
    rendered.add(stage.key);
  }

  for (const [key, element] of Object.entries(routineStageTargets)) {
    if (rendered.has(key)) continue;
    setResult(
      element,
      "SKIPPED — backend не вернул результат этого этапа.",
      "waiting",
      "result",
    );
  }
}

function renderFullTest(result) {
  const stages = (result.stages || []).map((stage) => {
    const report = stage.report_file
      ? `<div class="full-run-report">${escapeHtml(stage.report_file)}</div>`
      : "";
    const blocked = stage.blocked_by
      ? `<div>blocked by: ${escapeHtml(stage.blocked_by)}</div>`
      : "";
    const error = stage.error
      ? `<div><strong>Ошибка:</strong> ${escapeHtml(stage.error)}</div>`
      : "";
    return `
      <li class="diagnostic-command command-${String(stage.effective_result).toLowerCase()}">
        <div>
          <strong>${escapeHtml(stage.effective_result)}</strong>
          ${escapeHtml(stage.title)}
          — raw=${escapeHtml(stage.raw_result)}, ${escapeHtml(stage.duration_ms)} ms
        </div>
        <div>${escapeHtml(stage.detail)}</div>
        ${blocked}${error}${report}
      </li>`;
  }).join("");

  const hardware = (result.hardware || []).map((point) => `
    <li class="diagnostic-command command-${hardwareStatusCss(point.result)}">
      <div>
        <strong>${escapeHtml(point.result)}</strong>
        ${escapeHtml(point.title)}
        <span class="hardware-group">[${escapeHtml(point.group)}]</span>
      </div>
      <div>${escapeHtml(point.detail)}</div>
    </li>`).join("");

  const summary = result.summary || {};
  const pending = summary.pending_points?.length
    ? `<p><strong>Не завершено:</strong> ${escapeHtml(summary.pending_points.join(", "))}</p>`
    : "";
  const failed = summary.failed_point_names?.length
    ? `<p><strong>DUT FAIL:</strong> ${escapeHtml(summary.failed_point_names.join(", "))}</p>`
    : "";
  const error = result.error
    ? `<p><strong>Ошибка:</strong> ${escapeHtml(result.error)}</p>`
    : "";
  const report = result.report_file
    ? `<p><strong>Агрегированный JSON:</strong> ${escapeHtml(result.report_file)}</p>`
    : "";
  const runMetadata = `
    <p><strong>Run ID:</strong> ${escapeHtml(result.run_id || "не указан")}</p>
    <p><strong>Версии:</strong> frontend ${escapeHtml(result.frontend_build || "unknown")}, backend ${escapeHtml(result.utility_version || "unknown")}, firmware ${escapeHtml(result.firmware_version || "unknown")}</p>`;

  fullTestResult.className = `test-result ${fullStatusCss(result.result)}`;
  fullTestResult.dataset.kind = "result";
  fullTestResult.innerHTML = `
    <strong>${escapeHtml(result.result)}</strong>
    — ${escapeHtml(result.duration_ms)} ms
    ${error}${runMetadata}
    <p>
      Аппаратные точки: ${escapeHtml(summary.evaluated_points || 0)}/${escapeHtml(summary.total_points || 0)} оценены;
      PASS=${escapeHtml(summary.passed_points || 0)},
      FAIL=${escapeHtml(summary.failed_points || 0)},
      FIXTURE_ERROR=${escapeHtml(summary.fixture_error_points || 0)},
      SKIPPED=${escapeHtml(summary.skipped_points || 0)}.
    </p>
    ${pending}${failed}${report}
    <h3>Последовательность проверки</h3>
    <ul class="check-list command-list full-run-list">${stages}</ul>
    <h3>Аппаратная матрица</h3>
    <ul class="check-list command-list hardware-list">${hardware}</ul>
    <p>${escapeHtml(result.note || "")}</p>`;
}

function renderRunningState(state) {
  const stages = (state.stages || []).map((stage, index) => {
    const status = stage.effective_result || stage.status;
    const current = stage.key === state.current_stage_key ? " current-stage" : "";
    const detail = stage.detail ? `<div>${escapeHtml(stage.detail)}</div>` : "";
    return `
      <li class="diagnostic-command command-${String(status).toLowerCase()}${current}">
        <div><strong>${escapeHtml(status)}</strong> ${escapeHtml(index + 1)}. ${escapeHtml(stage.title)}</div>
        ${detail}
      </li>`;
  }).join("");
  const current = state.current_stage_title || "подготовка";
  const progress = Math.max(0, Math.min(100, Math.round((state.current_stage_index / state.total_stages) * 100)));

  fullTestResult.className = "test-result running";
  fullTestResult.dataset.kind = "running";
  fullTestResult.innerHTML = `
    <strong>${escapeHtml(state.lifecycle)}</strong>
    — этап ${escapeHtml(state.current_stage_index)}/${escapeHtml(state.total_stages)}: ${escapeHtml(current)}
    <p><strong>Run ID:</strong> ${escapeHtml(state.run_id)}</p>
    <div class="run-progress"><div style="width:${progress}%"></div></div>
    <ul class="check-list command-list full-run-list">${stages}</ul>
    <p>Прогон выполняется backend и продолжится при обновлении или закрытии вкладки.</p>`;
  renderRunStageCards(state);
}

function renderRunError(state) {
  fullTestResult.className = "test-result fail";
  fullTestResult.dataset.kind = "result";
  fullTestResult.innerHTML = `
    <strong>ERROR</strong>
    <p><strong>Run ID:</strong> ${escapeHtml(state.run_id)}</p>
    <p>${escapeHtml(state.error || "Неизвестная ошибка backend-сессии")}</p>`;
  renderRunStageCards(state);
}

function renderRunState(state) {
  if (!state) return;
  activeFullRunId = state.run_id;
  if (state.lifecycle === "COMPLETE" && state.result) {
    renderFullTest(state.result);
    renderRoutineStageSummaries(state.result);
    setHardwareControlsLocked(false);
    activeFullRunId = null;
    return;
  }
  if (state.lifecycle === "ERROR") {
    renderRunError(state);
    setHardwareControlsLocked(false);
    activeFullRunId = null;
    return;
  }
  setHardwareControlsLocked(true);
  renderRunningState(state);
}

function stopFullRunPolling() {
  fullRunPollToken += 1;
  if (fullRunPollTimer !== null) {
    clearTimeout(fullRunPollTimer);
    fullRunPollTimer = null;
  }
}

async function pollFullRun(runId, token) {
  if (token !== fullRunPollToken) return;
  try {
    const state = await requestJson(`/api/tests/lcp/full/${encodeURIComponent(runId)}`);
    if (token !== fullRunPollToken) return;
    renderRunState(state);
    if (state.lifecycle === "WAITING" || state.lifecycle === "RUNNING") {
      fullRunPollTimer = setTimeout(() => pollFullRun(runId, token), FULL_RUN_POLL_MS);
    }
  } catch (error) {
    if (token !== fullRunPollToken) return;
    setResult(fullTestResult, `Ошибка чтения состояния прогона — ${error.message}`, "fail", "result");
    setHardwareControlsLocked(false);
    activeFullRunId = null;
  }
}

function startFullRunPolling(runId) {
  stopFullRunPolling();
  activeFullRunId = runId;
  const token = fullRunPollToken;
  pollFullRun(runId, token);
}

async function restoreFullTestRun() {
  try {
    const state = await requestJson("/api/tests/lcp/full/current");
    if (!state) return;
    renderRunState(state);
    if (state.lifecycle === "WAITING" || state.lifecycle === "RUNNING") {
      startFullRunPolling(state.run_id);
    }
  } catch (error) {
    setResult(fullTestResult, `Не удалось восстановить состояние прогона — ${error.message}`, "fail", "result");
  }
}

async function runFullTest() {
  const identity = testIdentity();
  if (identity.error) {
    setResult(fullTestResult, identity.error, "fail", "validation");
    identity.focus?.focus();
    return;
  }

  stopFullRunPolling();
  setHardwareControlsLocked(true);
  setResult(
    fullTestResult,
    "WAITING — сохранение настроек и создание backend-сессии…",
    "running",
    "running",
  );

  try {
    const config = await requestJson("/api/station", {
      method: "PUT",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify(readForm()),
    });
    fillForm(config);
    setMessage("Настройки стенда сохранены перед полной проверкой.", true);

    const state = await requestJson("/api/tests/lcp/full/start", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: requestPayload(identity),
    });
    renderRunState(state);
    startFullRunPolling(state.run_id);
  } catch (error) {
    setResult(fullTestResult, `FAIL — ${error.message}`, "fail", "result");
    setHardwareControlsLocked(false);
  }
}

runFullTestButton.addEventListener("click", runFullTest);
restoreFullTestRun();
