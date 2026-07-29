"use strict";

async function checkBackend() {
  const badge = document.getElementById("backend-status");
  try {
    const response = await fetch("/api/health");
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    badge.textContent = `BACKEND ${data.version}`;
    badge.className = "status pass";
  } catch (error) {
    badge.textContent = "BACKEND FAIL";
    badge.className = "status fail";
  }
}

checkBackend();
