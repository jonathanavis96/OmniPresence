/**
 * options.js — OmniPresence Browser Bridge options page logic.
 */

// ─── Helpers ──────────────────────────────────────────────────────────────────

function loadState() {
  return new Promise((resolve) => {
    chrome.storage.local.get({ enabled: true, whitelist: {} }, resolve);
  });
}

function saveState(patch) {
  return new Promise((resolve) => {
    chrome.storage.local.set(patch, resolve);
  });
}

function showSaved() {
  const banner = document.getElementById("saved-banner");
  banner.classList.add("show");
  setTimeout(() => banner.classList.remove("show"), 1800);
}

// ─── Render ───────────────────────────────────────────────────────────────────

function renderWhitelist(whitelist) {
  const tbody = document.getElementById("whitelist-tbody");
  // Clear existing rows safely without innerHTML
  while (tbody.firstChild) tbody.removeChild(tbody.firstChild);

  const domains = Object.keys(whitelist).sort();
  if (domains.length === 0) {
    const emptyTr = document.createElement("tr");
    emptyTr.className = "empty-row";
    const emptyTd = document.createElement("td");
    emptyTd.colSpan = 2;
    emptyTd.textContent = "No domains whitelisted.";
    emptyTr.appendChild(emptyTd);
    tbody.appendChild(emptyTr);
    return;
  }

  domains.forEach((domain) => {
    const tr = document.createElement("tr");

    const domainTd = document.createElement("td");
    domainTd.className = "domain-cell";
    domainTd.textContent = domain;

    const actionTd = document.createElement("td");
    actionTd.className = "action-cell";

    const removeBtn = document.createElement("button");
    removeBtn.className = "remove-btn";
    removeBtn.textContent = "Remove";
    removeBtn.addEventListener("click", async () => {
      const state = await loadState();
      delete state.whitelist[domain];
      await saveState({ whitelist: state.whitelist });
      renderWhitelist(state.whitelist);
      showSaved();
    });

    actionTd.appendChild(removeBtn);
    tr.appendChild(domainTd);
    tr.appendChild(actionTd);
    tbody.appendChild(tr);
  });
}

// ─── Init ─────────────────────────────────────────────────────────────────────

async function init() {
  const state = await loadState();

  // Master toggle
  const enabledToggle = document.getElementById("opt-enabled");
  enabledToggle.checked = state.enabled;
  enabledToggle.addEventListener("change", async () => {
    await saveState({ enabled: enabledToggle.checked });
    showSaved();
  });

  // Whitelist
  renderWhitelist(state.whitelist);

  // Add domain
  const newDomainInput = document.getElementById("opt-new-domain");
  const addBtn = document.getElementById("opt-add-btn");

  async function addDomain() {
    const raw = newDomainInput.value.trim().toLowerCase();
    if (!raw) return;
    const domain = raw.replace(/^https?:\/\//, "").split("/")[0];
    if (!domain) return;

    const s = await loadState();
    if (s.whitelist[domain]) {
      newDomainInput.value = "";
      return; // already exists
    }
    s.whitelist[domain] = true;
    await saveState({ whitelist: s.whitelist });
    renderWhitelist(s.whitelist);
    showSaved();
    newDomainInput.value = "";
  }

  addBtn.addEventListener("click", addDomain);
  newDomainInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") addDomain();
  });
}

init();
