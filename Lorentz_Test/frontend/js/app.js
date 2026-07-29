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
      return `<div class="port-item"><strong>${port.device}</strong> — ${port.description || "без описания"}; ${identity}${hint}</div>`;
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
const helloResult = document.getElementById("hello-result");

function renderHello(result) {
  const css = result.result === "PASS" ? "pass" : "fail";
  const details = result.checks.map((check) =>
    `<li><strong>${check.status}</strong> ${check.name}: ${check.actual} (ожидалось ${check.expected})</li>`
  ).join("");
  const error = result.error ? `<p><strong>Ошибка:</strong> ${result.error}</p>` : "";
  const firmware = `<p>${result.firmware_note}. Ожидается firmware ${result.expected_firmware_version}; её проверка будет отдельным шагом через диагностическую консоль.</p>`;
  const report = result.report_file ? `<p><strong>JSON:</strong> ${result.report_file}</p>` : "";
  helloResult.className = `test-result ${css}`;
  helloResult.innerHTML = `<strong>${result.result}</strong> — ${result.port}, ${result.duration_ms} ms${error}${details ? `<ul class="check-list">${details}</ul>` : ""}${firmware}${report}`;
}

async function runHello() {
  const serialNumber = document.getElementById("device-serial").value.trim();
  const operator = document.getElementById("operator-name").value.trim();
  if (!serialNumber || !operator) {
    helloResult.className = "test-result fail";
    helloResult.textContent = "Введите серийный номер и имя оператора.";
    return;
  }
  runHelloButton.disabled = true;
  helloResult.className = "test-result running";
  helloResult.textContent = "RUNNING — открытие USB и ожидание ответа HELLO…";
  try {
    const result = await requestJson("/api/tests/lcp/hello", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify({
        serial_number: serialNumber,
        operator,
        port: nullable(String(form.elements.lcp_port.value || "")),
      }),
    });
    renderHello(result);
  } catch (error) {
    helloResult.className = "test-result fail";
    helloResult.textContent = `FAIL — ${error.message}`;
  } finally {
    runHelloButton.disabled = false;
  }
}

runHelloButton.addEventListener("click", runHello);
