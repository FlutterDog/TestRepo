"use strict";

const form = document.getElementById("station-form");
const stationMessage = document.getElementById("station-message");
const saveButton = document.getElementById("save-station");
const refreshPortsButton = document.getElementById("refresh-ports");
const serialPortsList = document.getElementById("serial-ports");
const portsSummary = document.getElementById("ports-summary");

async function requestJson(url, options = {}) {
  const response = await fetch(url, options);
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    const detail = typeof body.detail === "string" ? body.detail : JSON.stringify(body.detail || body);
    throw new Error(detail || `HTTP ${response.status}`);
  }
  return body;
}

function escapeHtml(value) {
  const map = {"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#039;"};
  return String(value ?? "").replace(/[&<>"']/g, (character) => map[character]);
}

function setMessage(text, ok) {
  stationMessage.textContent = text;
  stationMessage.className = `message ${ok ? "pass" : "fail"}`;
}

function nullable(value) {
  const text = value.trim();
  return text === "" ? null : text;
}

function fillForm(config) {
  for (const [key, value] of Object.entries(config)) {
    const control = form.elements.namedItem(key);
    if (!control) continue;
    if (control.type === "checkbox") control.checked = Boolean(value);
    else control.value = value ?? "";
  }
}

function readForm() {
  const data = new FormData(form);
  return {
    station_name: data.get("station_name"),
    lcp_port: nullable(String(data.get("lcp_port") || "")),
    s1_endpoint: nullable(String(data.get("s1_endpoint") || "")),
    s2_endpoint: nullable(String(data.get("s2_endpoint") || "")),
    s3_endpoint: nullable(String(data.get("s3_endpoint") || "")),
    s4_endpoint: nullable(String(data.get("s4_endpoint") || "")),
    hmi_endpoint: nullable(String(data.get("hmi_endpoint") || "")),
    x2x_endpoint: nullable(String(data.get("x2x_endpoint") || "")),
    eth1_ip: data.get("eth1_ip"),
    eth2_ip: data.get("eth2_ip"),
    shared_hmi_x2x_adapter: form.elements.shared_hmi_x2x_adapter.checked,
    post_test_action: data.get("post_test_action"),
    expected_firmware_version: data.get("expected_firmware_version"),
    serial_baudrate: Number(data.get("serial_baudrate")),
    serial_timeout_seconds: Number(data.get("serial_timeout_seconds")),
  };
}

async function loadPorts() {
  refreshPortsButton.disabled = true;
  portsSummary.textContent = "Поиск последовательных портов…";
  try {
    const ports = await requestJson("/api/ports");
    serialPortsList.replaceChildren(...ports.map((port) => {
      const option = document.createElement("option");
      option.value = port.device;
      option.label = port.description || port.hwid;
      return option;
    }));
    if (ports.length === 0) {
      portsSummary.textContent = "Последовательные порты не обнаружены.";
      return;
    }
    portsSummary.innerHTML = ports.map((port) => {
      const hint = port.lcp_candidate ? ' <span class="port-candidate">возможный LCP</span>' : "";
      const identity = port.vid !== null && port.pid !== null
        ? `VID:PID ${port.vid.toString(16).padStart(4, "0").toUpperCase()}:${port.pid.toString(16).padStart(4, "0").toUpperCase()}`
        : port.hwid;
      return `<div class="port-item"><strong>${escapeHtml(port.device)}</strong> — ${escapeHtml(port.description || "без описания")}; ${escapeHtml(identity)}${hint}</div>`;
    }).join("");
  } catch (error) {
    portsSummary.textContent = `Ошибка перечисления COM-портов: ${error.message}`;
  } finally {
    refreshPortsButton.disabled = false;
  }
}

async function checkBackend() {
  const badge = document.getElementById("backend-status");
  try {
    const data = await requestJson("/api/health");
    badge.textContent = `BACKEND ${data.version}`;
    badge.className = "status pass";
  } catch (error) {
    badge.textContent = "BACKEND FAIL";
    badge.className = "status fail";
  }
}

async function loadStation() {
  try {
    fillForm(await requestJson("/api/station"));
  } catch (error) {
    setMessage(`Не удалось загрузить настройки: ${error.message}`, false);
  }
}

async function saveStation() {
  saveButton.disabled = true;
  try {
    const config = await requestJson("/api/station", {
      method: "PUT",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify(readForm()),
    });
    fillForm(config);
    setMessage("Настройки стенда сохранены.", true);
  } catch (error) {
    setMessage(`Ошибка сохранения: ${error.message}`, false);
  } finally {
    saveButton.disabled = false;
  }
}

saveButton.addEventListener("click", saveStation);
refreshPortsButton.addEventListener("click", loadPorts);
checkBackend();
loadStation();
loadPorts();

const runHelloButton = document.getElementById("run-hello");
const runDiagnosticsButton = document.getElementById("run-diagnostics");
const runActiveButton = document.getElementById("run-active-rs485");
const helloResult = document.getElementById("hello-result");
const diagnosticsResult = document.getElementById("diagnostics-result");
const activeResult = document.getElementById("active-rs485-result");
const deviceSerialInput = document.getElementById("device-serial");
const operatorInput = document.getElementById("operator-name");
const lcpPortInput = form.elements.namedItem("lcp_port");

function setResult(element, text, cssClass, kind = "result") {
  element.className = `test-result ${cssClass}`;
  element.textContent = text;
  element.dataset.kind = kind;
}

function resultCss(status) {
  if (status === "PASS") return "pass";
  if (status === "RUNNING") return "running";
  if (status === "SKIPPED") return "waiting";
  return "fail";
}

function testIdentity() {
  const serialNumber = deviceSerialInput.value.trim();
  const operatorName = operatorInput.value.trim();
  const port = nullable(String(lcpPortInput?.value || ""));
  const missingFields = [];
  if (!serialNumber) missingFields.push("серийный номер");
  if (!operatorName) missingFields.push("имя оператора");
  if (missingFields.length > 0) {
    return {error: `Заполните: ${missingFields.join(" и ")}.`, focus: serialNumber ? operatorInput : deviceSerialInput};
  }
  if (!port) {
    return {error: "Выберите USB-порт LCP в настройках стенда.", focus: lcpPortInput};
  }
  return {serialNumber, operatorName, port};
}

function requestPayload(identity) {
  return JSON.stringify({
    serial_number: identity.serialNumber,
    operator: identity.operatorName,
    port: identity.port,
  });
}

function renderCheck(check) {
  const css = `check-${String(check.status).toLowerCase()}`;
  return `<li class="${css}"><strong>${escapeHtml(check.status)}</strong> ${escapeHtml(check.name)}: ${escapeHtml(check.actual)} (ожидалось ${escapeHtml(check.expected)})</li>`;
}

function renderHello(result) {
  const details = result.checks.map(renderCheck).join("");
  const error = result.error ? `<p><strong>Ошибка:</strong> ${escapeHtml(result.error)}</p>` : "";
  const firmware = result.firmware_version
    ? `<p><strong>Firmware:</strong> ${escapeHtml(result.firmware_name)}, ${escapeHtml(result.firmware_stage)}, ${escapeHtml(result.firmware_target)}.</p>`
    : "";
  const note = result.firmware_note ? `<p>${escapeHtml(result.firmware_note)}</p>` : "";
  const report = result.report_file ? `<p><strong>JSON:</strong> ${escapeHtml(result.report_file)}</p>` : "";
  helloResult.className = `test-result ${resultCss(result.result)}`;
  helloResult.dataset.kind = "result";
  helloResult.innerHTML = `<strong>${escapeHtml(result.result)}</strong> — ${escapeHtml(result.port || "порт не задан")}, ${escapeHtml(result.duration_ms)} ms${error}${details ? `<ul class="check-list">${details}</ul>` : ""}${firmware}${note}${report}`;
}

async function runHello() {
  const identity = testIdentity();
  if (identity.error) {
    setResult(helloResult, identity.error, "fail", "validation");
    identity.focus?.focus();
    return;
  }
  runHelloButton.disabled = true;
  setResult(helloResult, "RUNNING — проверка бинарного USB и firmware…", "running", "running");
  try {
    renderHello(await requestJson("/api/tests/lcp/hello", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: requestPayload(identity),
    }));
  } catch (error) {
    setResult(helloResult, `FAIL — ${error.message}`, "fail", "result");
  } finally {
    runHelloButton.disabled = false;
  }
}

function renderEndpointAccess(item) {
  const endpoint = item.endpoint || "не настроен";
  return `<li class="endpoint-${String(item.status).toLowerCase()}"><strong>${escapeHtml(item.status)}</strong> ${escapeHtml(item.name)} [${escapeHtml(endpoint)}]: ${escapeHtml(item.detail)}</li>`;
}

function renderDiagnostics(result) {
  const endpoints = (result.endpoint_access || []).map(renderEndpointAccess).join("");
  const endpointBlock = endpoints
    ? `<h3>Доступ к endpoint стенда</h3><ul class="check-list endpoint-list">${endpoints}</ul>`
    : "";
  const commands = result.commands.map((item) => {
    const checks = (item.checks || []).map(renderCheck).join("");
    const error = item.error ? `<p><strong>Ошибка:</strong> ${escapeHtml(item.error)}</p>` : "";
    const capture = item.capture_status !== "PASS"
      ? `<span class="capture-fail">capture=${escapeHtml(item.capture_status)}</span>`
      : "";
    return `
      <li class="diagnostic-command command-${String(item.status).toLowerCase()}">
        <div><strong>${escapeHtml(item.status)}</strong> ${escapeHtml(item.title)} [${escapeHtml(item.command)}], ${escapeHtml(item.duration_ms)} ms ${capture}</div>
        ${error}
        ${checks ? `<ul class="check-list nested">${checks}</ul>` : ""}
      </li>`;
  }).join("");
  const error = result.error ? `<p><strong>Ошибка:</strong> ${escapeHtml(result.error)}</p>` : "";
  const report = result.report_file ? `<p><strong>JSON:</strong> ${escapeHtml(result.report_file)}</p>` : "";
  diagnosticsResult.className = `test-result ${resultCss(result.result)}`;
  diagnosticsResult.dataset.kind = "result";
  diagnosticsResult.innerHTML = `<strong>${escapeHtml(result.result)}</strong> — ${escapeHtml(result.port || "порт не задан")}, ${escapeHtml(result.duration_ms)} ms${error}${endpointBlock}${commands ? `<ul class="check-list command-list">${commands}</ul>` : ""}<p>${escapeHtml(result.note)}</p>${report}`;
}

async function runDiagnostics() {
  const identity = testIdentity();
  if (identity.error) {
    setResult(diagnosticsResult, identity.error, "fail", "validation");
    identity.focus?.focus();
    return;
  }
  runDiagnosticsButton.disabled = true;
  setResult(diagnosticsResult, "RUNNING — пассивная диагностика…", "running", "running");
  try {
    renderDiagnostics(await requestJson("/api/tests/lcp/diagnostics", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: requestPayload(identity),
    }));
  } catch (error) {
    setResult(diagnosticsResult, `FAIL — ${error.message}`, "fail", "result");
  } finally {
    runDiagnosticsButton.disabled = false;
  }
}

function renderActive(result) {
  const interfaces = (result.interfaces || []).map((item) => {
    const endpoint = item.endpoint || "не настроен";
    const values = item.expected_values?.length
      ? `<div>values: expected=${escapeHtml(JSON.stringify(item.expected_values))}, actual=${escapeHtml(JSON.stringify(item.actual_values || []))}</div>`
      : "";
    const requests = item.observed_requests?.length
      ? `<div>requests: ${escapeHtml(item.observed_requests.join(", "))}</div>`
      : "";
    return `
      <li class="diagnostic-command command-${String(item.status).toLowerCase()}">
        <div><strong>${escapeHtml(item.status)}</strong> ${escapeHtml(item.name)} [${escapeHtml(endpoint)}], ${escapeHtml(item.serial || "serial unknown")}</div>
        <div>${escapeHtml(item.detail)}</div>
        ${values}${requests}
      </li>`;
  }).join("");
  const error = result.error ? `<p><strong>Ошибка:</strong> ${escapeHtml(result.error)}</p>` : "";
  const report = result.report_file ? `<p><strong>JSON:</strong> ${escapeHtml(result.report_file)}</p>` : "";
  activeResult.className = `test-result ${resultCss(result.result)}`;
  activeResult.dataset.kind = "result";
  activeResult.innerHTML = `<strong>${escapeHtml(result.result)}</strong> — ${escapeHtml(result.port || "порт не задан")}, ${escapeHtml(result.duration_ms)} ms${error}${interfaces ? `<ul class="check-list command-list">${interfaces}</ul>` : ""}<p>${escapeHtml(result.note)}</p>${report}`;
}

async function runActiveRs485() {
  const identity = testIdentity();
  if (identity.error) {
    setResult(activeResult, identity.error, "fail", "validation");
    identity.focus?.focus();
    return;
  }
  runActiveButton.disabled = true;
  setResult(activeResult, "RUNNING — Python запускает slave S1–S4/X2X и ждёт опрос LCP…", "running", "running");
  try {
    renderActive(await requestJson("/api/tests/lcp/active-rs485", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: requestPayload(identity),
    }));
  } catch (error) {
    setResult(activeResult, `FAIL — ${error.message}`, "fail", "result");
  } finally {
    runActiveButton.disabled = false;
  }
}

function clearValidation() {
  [helloResult, diagnosticsResult, activeResult].forEach((element) => {
    if (element.dataset.kind === "validation") setResult(element, "WAITING", "waiting", "waiting");
  });
}

[deviceSerialInput, operatorInput, lcpPortInput].forEach((control) => {
  control?.addEventListener("input", clearValidation);
  control?.addEventListener("change", clearValidation);
});

runHelloButton.addEventListener("click", runHello);
runDiagnosticsButton.addEventListener("click", runDiagnostics);
runActiveButton.addEventListener("click", runActiveRs485);
