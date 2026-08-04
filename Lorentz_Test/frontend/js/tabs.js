"use strict";

const FRONTEND_BUILD = "0.9.0-p2";
const TAB_STORAGE_KEY = "lorentz-test-active-tab";
const tabButtons = [...document.querySelectorAll("[data-tab-target]")];
const tabPanels = [...document.querySelectorAll("[data-tab-panel]")];

function prepareProductLayout() {
  const stationPanel = document.querySelector('[data-tab-panel="station"]');
  const stationCard = stationPanel?.querySelector(":scope > .card");
  const precheckCard = document.querySelector(".precheck-card");
  if (stationPanel && stationCard && precheckCard) {
    stationCard.insertAdjacentElement("afterend", precheckCard);
  }

  const frontendBadge = [...document.querySelectorAll(".topbar .status")]
    .find((item) => item.textContent.trim().startsWith("FRONTEND"));
  if (frontendBadge) frontendBadge.textContent = `FRONTEND ${FRONTEND_BUILD}`;
}

function activateTab(tabName, persist = true) {
  const available = tabPanels.some((panel) => panel.dataset.tabPanel === tabName);
  const resolved = available ? tabName : "operator";
  for (const button of tabButtons) {
    const active = button.dataset.tabTarget === resolved;
    button.classList.toggle("active", active);
    button.setAttribute("aria-selected", active ? "true" : "false");
  }
  for (const panel of tabPanels) {
    panel.hidden = panel.dataset.tabPanel !== resolved;
  }
  if (persist) localStorage.setItem(TAB_STORAGE_KEY, resolved);
}

prepareProductLayout();

for (const button of tabButtons) {
  button.addEventListener("click", () => activateTab(button.dataset.tabTarget));
}

activateTab(localStorage.getItem(TAB_STORAGE_KEY) || "operator", false);
window.lorentzTabs = {activate: activateTab};
