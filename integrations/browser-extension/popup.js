/**
 * popup.js — OmniPresence Browser Bridge popup UI.
 *
 * Shows current domain/category, master on/off toggle,
 * and a simple whitelist editor (add/remove domains that allow title reporting).
 */

// ─── DOM refs ─────────────────────────────────────────────────────────────────

const masterToggle = document.getElementById("master-toggle");
const toggleLabel = document.getElementById("toggle-state-label");
const currentDomainEl = document.getElementById("current-domain");
const currentCategoryEl = document.getElementById("current-category");
const whitelistEl = document.getElementById("whitelist-items");
const emptyStateEl = document.getElementById("empty-state");
const newDomainInput = document.getElementById("new-domain");
const addDomainBtn = document.getElementById("add-domain-btn");
const optionsLink = document.getElementById("options-link");

// ─── Storage helpers ──────────────────────────────────────────────────────────

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

// ─── Render whitelist ─────────────────────────────────────────────────────────

function renderWhitelist(whitelist) {
  const domains = Object.keys(whitelist);

  // Remove all existing domain items (keep empty state if needed)
  whitelistEl.querySelectorAll("li.domain-item").forEach((el) => el.remove());

  if (domains.length === 0) {
    emptyStateEl.style.display = "";
    return;
  }

  emptyStateEl.style.display = "none";

  domains.forEach((domain) => {
    const li = document.createElement("li");
    li.className = "domain-item";

    const nameSpan = document.createElement("span");
    nameSpan.className = "domain-name";
    nameSpan.textContent = domain;

    const removeBtn = document.createElement("button");
    removeBtn.className = "remove-btn";
    removeBtn.title = "Remove";
    removeBtn.textContent = "×";
    removeBtn.addEventListener("click", async () => {
      const state = await loadState();
      delete state.whitelist[domain];
      await saveState({ whitelist: state.whitelist });
      renderWhitelist(state.whitelist);
    });

    li.appendChild(nameSpan);
    li.appendChild(removeBtn);
    whitelistEl.appendChild(li);
  });
}

// ─── Current tab status ───────────────────────────────────────────────────────

async function updateCurrentStatus() {
  try {
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    if (!tab || !tab.url) {
      currentDomainEl.textContent = "—";
      currentCategoryEl.textContent = "No active tab";
      return;
    }

    const url = new URL(tab.url);
    const hostname = url.hostname;

    if (!hostname) {
      currentDomainEl.textContent = "Internal page";
      currentCategoryEl.textContent = "Not reported";
      return;
    }

    const domain = hostname.replace(/^www\./, "");
    currentDomainEl.textContent = domain;

    // We can't easily import ES modules in popup.js (it's not a module),
    // so do a lightweight category check against the storage-cached last payload
    // OR just show the domain and let the user read it.
    // For a richer display we rely on messaging the service worker.
    chrome.runtime.sendMessage({ type: "GET_LAST_PAYLOAD" }, (response) => {
      if (chrome.runtime.lastError || !response) {
        currentCategoryEl.textContent = "(extension active)";
        return;
      }
      currentCategoryEl.textContent = response.category || "(extension active)";
    });
  } catch {
    currentDomainEl.textContent = "—";
    currentCategoryEl.textContent = "—";
  }
}

// ─── Initialise ───────────────────────────────────────────────────────────────

async function init() {
  const state = await loadState();

  // Master toggle
  masterToggle.checked = state.enabled;
  toggleLabel.textContent = state.enabled ? "On" : "Off";

  masterToggle.addEventListener("change", async () => {
    const enabled = masterToggle.checked;
    toggleLabel.textContent = enabled ? "On" : "Off";
    await saveState({ enabled });
  });

  // Whitelist
  renderWhitelist(state.whitelist);

  // Add domain
  function addDomain() {
    const raw = newDomainInput.value.trim().toLowerCase();
    if (!raw) return;
    // Strip protocol if accidentally pasted
    const domain = raw.replace(/^https?:\/\//, "").split("/")[0];
    if (!domain) return;

    loadState().then(async (s) => {
      s.whitelist[domain] = true;
      await saveState({ whitelist: s.whitelist });
      renderWhitelist(s.whitelist);
      newDomainInput.value = "";
    });
  }

  addDomainBtn.addEventListener("click", addDomain);
  newDomainInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") addDomain();
  });

  // Options link
  optionsLink.addEventListener("click", (e) => {
    e.preventDefault();
    chrome.runtime.openOptionsPage();
  });

  // Current status
  await updateCurrentStatus();
}

init();
