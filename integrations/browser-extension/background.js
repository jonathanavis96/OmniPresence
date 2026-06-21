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
  // Strip browser noise: leading unread-count "(179) " and a trailing
  // " - YouTube" site suffix, so the presence shows just the page/video title.
  const cleanTitle = (tab.title || "")
    .trim()
    .replace(/^\(\d+\+?\)\s*/, "")
    .replace(/\s+[-–]\s+YouTube$/i, "")
    .trim();
  const safeTitle = titleAllowed && cleanTitle ? cleanTitle : null;

  // Smart label: a clean human name derived from the URL path (e.g.
  // /wtv/21760/Severance -> "Severance", /the-office -> "The Office"), so rules
  // can say "Watching {{browser.label}}" without the user touching the title.
  // Falls back to the cleaned title. Gated by the same whitelist as titles.
  const pageLabel = titleAllowed ? (labelFromUrl(url) || cleanTitle || null) : null;

  const browser = detectBrowser();

  return {
    source: "browser",
    browser,
    domain,
    category,
    title_allowed: titleAllowed,
    safe_title: safeTitle,
    page_label: pageLabel,
    dashboard_label: dashboardLabel,
  };
}

/**
 * Derive a human-readable label from a URL's path — the last meaningful
 * segment, decoded, with separators turned to spaces and title-cased.
 * Skips numeric ids and hash-like segments. Returns null if nothing usable.
 * @param {URL} url
 * @returns {string|null}
 */
function labelFromUrl(url) {
  try {
    const segs = url.pathname.split("/").filter(Boolean);
    for (let i = segs.length - 1; i >= 0; i--) {
      let s = decodeURIComponent(segs[i]).replace(/\.[a-z0-9]{1,5}$/i, ""); // drop file ext
      if (/^\d+$/.test(s)) continue;            // pure numeric id
      if (/^[0-9a-f]{8,}$/i.test(s)) continue;  // hash / long id
      s = s.replace(/[-_+]+/g, " ").trim();
      if (s.length >= 2) {
        return s.replace(/\b\w/g, (c) => c.toUpperCase());
      }
    }
  } catch {
    // malformed URL — ignore
  }
  return null;
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
