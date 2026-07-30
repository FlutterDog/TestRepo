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

const routineStages = [
  {
    key: "hello",
    title: "USB и firmware",
    url: "/api/tests/lcp/hello",
    element: helloResult,
    render: renderHello,
    running: "RUNNING — проверка бинарного USB и firmware…",
  },
  {
    key: "diagnostics",
    title: "Пассивная диагностика",
    url: "/api/tests/lcp/diagnostics",
    element: diagnosticsResult,
    render: renderDiagnostics,
    running: "RUNNING — пассивная диагностика…",
  },
  {
    key: "rs485",
    title: "RS-485 S1–S4 и X2X",
    url: "/api/tests/lcp/active-rs485",
    element: activeRs485Result,
    render: renderActiveRs485,
    running: "RUNNING — Python запускает slave S1–S4/X2X и ждёт опрос LCP…",
  },
  {
    key: "ethernet",
    title: "Ethernet ETH1/ETH2",
    url: "/api/tests/lcp/active-ethernet",
    element: activeEthernetResult,
    render: renderActiveEthernet,
    running: "RUNNING — Modbus TCP FC03 для ETH1/ETH2…",
  },
  {
    key: "services",
    title: "Config, microSD, RTC и RTOS",
    url: "/api/tests/lcp/active-services",
    element: activeServicesResult,
    render: renderActiveServices,
    running: "RUNNING — config validate, SD, RTC и RTOS…",
  },
  {
    key: "hmi",
    title: "HMI echo",
    url: "/api/tests/lcp/hmi",
    element: hmiResult,
    render: renderHmi,
    running: "RUNNING — три HMI echo-кадра…",
  },
];

function nestedStatuses(stage, result) {
  if (stage.key === "rs485" || stage.key === "ethernet") {
    return (result.interfaces || []).map((item) => item.status);
  }
  if (stage.key === "services") {
    return (result.steps || []).map((item) => item.status);
  }
  return [];
}

function effectiveRoutineStatus(stage, result) {
  const top = result.result || "FAIL";
  if (top === "FAIL" || top === "FIXTURE_ERROR" || top === "SKIPPED") return top;

  const nested = nestedStatuses(stage, result);
  if (nested.includes("FAIL")) return "FAIL";
  if (nested.includes("FIXTURE_ERROR")) return "FIXTURE_ERROR";
  if (nested.includes("SKIPPED")) return "INCOMPLETE";
  return top;
}

function overallRoutineStatus(items) {
  const statuses = items.map((item) => item.status);
  if (statuses.includes("FAIL")) return "FAIL";
  if (statuses.includes("FIXTURE_ERROR")) return "FIXTURE_ERROR";
  if (statuses.includes("INCOMPLETE") || statuses.includes("SKIPPED")) return "INCOMPLETE";
  return statuses.length > 0 && statuses.every((status) => status === "PASS") ? "PASS" : "FAIL";
}

function fullStatusCss(status) {
  if (status === "PASS") return "pass";
  if (status === "FIXTURE_ERROR") return "fixture-error";
  if (status === "INCOMPLETE" || status === "SKIPPED") return "waiting";
  if (status === "RUNNING") return "running";
  return "fail";
}

function renderFullRun(items, overall, durationMs, currentTitle = null) {
  const rows = routineStages.map((stage) => {
    const item = items.find((candidate) => candidate.key === stage.key);
    const status = item?.status || (currentTitle === stage.title ? "RUNNING" : "WAITING");
    const detail = item?.result?.report_file
      ? `<div class="full-run-report">${escapeHtml(item.result.report_file)}</div>`
      : item?.error
        ? `<div>${escapeHtml(item.error)}</div>`
        : "";
    return `
      <li class="diagnostic-command command-${String(status).toLowerCase()}">
        <div><strong>${escapeHtml(status)}</strong> ${escapeHtml(stage.title)}</div>
        ${detail}
      </li>`;
  }).join("");

  const suffix = overall === "RUNNING" && currentTitle
    ? ` — сейчас: ${escapeHtml(currentTitle)}`
    : ` — ${escapeHtml(durationMs)} ms`;
  const explanation = overall === "INCOMPLETE"
    ? "Часть интерфейсов не настроена или была пропущена. Проверенные части не дали DUT FAIL."
    : overall === "FIXTURE_ERROR"
      ? "Контроллер полностью не оценён из-за ошибки стенда, подключения или настройки endpoint."
      : "";

  fullTestResult.className = `test-result ${fullStatusCss(overall)}`;
  fullTestResult.dataset.kind = "result";
  fullTestResult.innerHTML = `
    <strong>${escapeHtml(overall)}</strong>${suffix}
    ${explanation ? `<p>${escapeHtml(explanation)}</p>` : ""}
    <ul class="check-list command-list full-run-list">${rows}</ul>`;
}

async function runRoutineStage(stage, identity) {
  setResult(stage.element, stage.running, "running", "running");
  try {
    const result = await requestJson(stage.url, {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: requestPayload(identity),
    });
    stage.render(result);
    return {key: stage.key, status: effectiveRoutineStatus(stage, result), result};
  } catch (error) {
    setResult(stage.element, `FAIL — ${error.message}`, "fail", "result");
    return {
      key: stage.key,
      status: "FAIL",
      result: null,
      error: error.message,
    };
  }
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
  const started = performance.now();
  const items = [];
  renderFullRun(items, "RUNNING", 0, "сохранение настроек стенда");

  try {
    const config = await requestJson("/api/station", {
      method: "PUT",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify(readForm()),
    });
    fillForm(config);
    setMessage("Настройки стенда сохранены перед полной проверкой.", true);

    for (const stage of routineStages) {
      renderFullRun(items, "RUNNING", Math.round(performance.now() - started), stage.title);
      const item = await runRoutineStage(stage, identity);
      items.push(item);
      renderFullRun(items, "RUNNING", Math.round(performance.now() - started));
    }

    renderFullRun(items, overallRoutineStatus(items), Math.round(performance.now() - started));
  } catch (error) {
    setResult(fullTestResult, `FAIL — ${error.message}`, "fail", "result");
  } finally {
    runFullTestButton.disabled = false;
    routineTestButtons.forEach((button) => { button.disabled = false; });
  }
}

runFullTestButton.addEventListener("click", runFullTest);
