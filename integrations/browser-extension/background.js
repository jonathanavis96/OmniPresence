/**
 * background.js — OmniPresence Browser Bridge service worker.
 *
 * Listens to tab activation, tab updates, and window focus changes.
 * Debounces to ~1.5 s before POSTing sanitized context to OmniPresence.
 * Never sends exact page titles unless the domain is explicitly whitelisted.
 */

import { getCategoryForDomain, detectDashboard, detectBrowser } from "./categories.js";

const ENDPOINT = "http://127.0.0.1:47831/integrations/browser/context";
const DEBOUNCE_MS = 1500;

let debounceTimer = null;

// ─── Storage helpers ──────────────────────────────────────────────────────────

/**
 * Load extension state from chrome.storage.local.
 * @returns {Promise<{enabled: boolean, whitelist: Record<string, boolean>}>}
 */
async function loadState() {
  return new Promise((resolve) => {
    chrome.storage.local.get(
      { enabled: true, whitelist: {} },
      (result) => resolve(result)
    );
  });
}

// ─── Payload builder ──────────────────────────────────────────────────────────

/**
 * Build the sanitized payload for a given tab.
 * @param {chrome.tabs.Tab} tab
 * @param {{enabled: boolean, whitelist: Record<string, boolean>}} state
 * @returns {object|null} null if we should not report (e.g. extension off, no URL)
 */
function buildPayload(tab, state) {
  if (!state.enabled) return null;

  const rawUrl = tab.url || "";

  // Skip chrome:// and edge:// internal pages
  if (
    rawUrl.startsWith("chrome://") ||
    rawUrl.startsWith("edge://") ||
    rawUrl.startsWith("about:") ||
    rawUrl === ""
  ) {
    return null;
  }

  let url;
  try {
    url = new URL(rawUrl);
  } catch {
    return null;
  }

  const hostname = url.hostname.toLowerCase();

  // Dashboard detection takes priority over generic domain category
  const dashboardLabel = detectDashboard(url);
  const category = dashboardLabel
    ? dashboardLabel
    : getCategoryForDomain(hostname);

  // Derive bare domain (strip leading www.)
  const domain = hostname.replace(/^www\./, "") || hostname;

  // Title allowance: check whitelist
  const titleAllowed = Boolean(state.whitelist[domain] || state.whitelist[hostname]);
  const rawTitle = (tab.title || "").trim();
  const safeTitle = titleAllowed && rawTitle ? rawTitle : null;

  const browser = detectBrowser();

  return {
    source: "browser",
    browser,
    domain,
    category,
    title_allowed: titleAllowed,
    safe_title: safeTitle,
    dashboard_label: dashboardLabel,
  };
}

// ─── POST ─────────────────────────────────────────────────────────────────────

/**
 * POST payload to OmniPresence. Fails silently if the server is not running.
 * @param {object} payload
 */
async function postContext(payload) {
  try {
    await fetch(ENDPOINT, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
  } catch {
    // OmniPresence not running — ignore
  }
}

// ─── Core reporter ────────────────────────────────────────────────────────────

/**
 * Query the currently active tab and POST its context, debounced.
 */
function scheduleReport() {
  if (debounceTimer !== null) {
    clearTimeout(debounceTimer);
  }
  debounceTimer = setTimeout(async () => {
    debounceTimer = null;

    // Get the currently focused window's active tab
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    if (!tab) return;

    const state = await loadState();
    const payload = buildPayload(tab, state);
    if (payload) {
      await postContext(payload);
    }
  }, DEBOUNCE_MS);
}

// ─── Event listeners ─────────────────────────────────────────────────────────

// User switches to a different tab
chrome.tabs.onActivated.addListener(() => {
  scheduleReport();
});

// Tab URL or title changes (navigated, loaded, etc.)
chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
  // Only report once the tab has finished loading to get the final title
  if (changeInfo.status === "complete" && tab.active) {
    scheduleReport();
  }
});

// User switches windows (or minimises all windows: windowId == -1)
chrome.windows.onFocusChanged.addListener((windowId) => {
  if (windowId === chrome.windows.WINDOW_ID_NONE) return; // all windows lost focus
  scheduleReport();
});
