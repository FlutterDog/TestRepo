"use strict";

const runFullTestButton = document.getElementById("run-full-test");
const fullTestResult = document.getElementById("full-test-result");

const routineTestButtons = [
  runHelloButton,
  runDiagnosticsButton,
  runActiveRs485Button,
  runActiveEthernetButton,
  runActiveServicesButton,
  runHmiButton,
];

function fullStatusCss(status) {
  if (status === "PASS") return "pass";
  if (status === "FIXTURE_ERROR") return "fixture-error";
  if (status === "INCOMPLETE" || status === "SKIPPED") return "waiting";
  if (status === "RUNNING") return "running";
  return "fail";
}

function hardwareStatusCss(status) {
  if (status === "PASS") return "pass";
  if (status === "FAIL") return "fail";
  if (status === "FIXTURE_ERROR") return "fixture_error";
  return "skipped";
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

  fullTestResult.className = `test-result ${fullStatusCss(result.result)}`;
  fullTestResult.dataset.kind = "result";
  fullTestResult.innerHTML = `
    <strong>${escapeHtml(result.result)}</strong>
    — ${escapeHtml(result.duration_ms)} ms
    ${error}
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

async function runFullTest() {
  const identity = testIdentity();
  if (identity.error) {
    setResult(fullTestResult, identity.error, "fail", "validation");
    identity.focus?.focus();
    return;
  }

  runFullTestButton.disabled = true;
  routineTestButtons.forEach((button) => { button.disabled = true; });
  setResult(
    fullTestResult,
    "RUNNING — backend последовательно выполняет USB, диагностику, RS-485/X2X, Ethernet, внутренние сервисы и HMI…",
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

    const result = await requestJson("/api/tests/lcp/full", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: requestPayload(identity),
    });
    renderFullTest(result);
  } catch (error) {
    setResult(fullTestResult, `FAIL — ${error.message}`, "fail", "result");
  } finally {
    runFullTestButton.disabled = false;
    routineTestButtons.forEach((button) => { button.disabled = false; });
  }
}

runFullTestButton.addEventListener("click", runFullTest);
