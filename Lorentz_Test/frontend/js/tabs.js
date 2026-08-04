"use strict";

const TAB_STORAGE_KEY = "lorentz-test-active-tab";
const tabButtons = [...document.querySelectorAll("[data-tab-target]")];
const tabPanels = [...document.querySelectorAll("[data-tab-panel]")];

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

for (const button of tabButtons) {
  button.addEventListener("click", () => activateTab(button.dataset.tabTarget));
}

activateTab(localStorage.getItem(TAB_STORAGE_KEY) || "operator", false);
window.lorentzTabs = {activate: activateTab};
