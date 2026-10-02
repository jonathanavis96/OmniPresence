// Unit tests for the browser bridge service worker's pure helpers.
// Run: node --test integrations/browser-extension/test/
import test from "node:test";
import assert from "node:assert/strict";

// background.js registers chrome.* listeners at import time; stub just enough.
const noopEvent = { addListener() {} };
globalThis.chrome = {
  tabs: { onActivated: noopEvent, onUpdated: noopEvent, query: async () => [] },
  windows: { onFocusChanged: noopEvent, WINDOW_ID_NONE: -1 },
  storage: { local: { get: (d, cb) => cb(d) } },
};

const { labelFromUrl, postContext } = await import("../background.js");

test("labelFromUrl skips a malformed percent-escape and keeps searching", () => {
  // The last segment is not valid URI encoding; the one before it is usable.
  assert.equal(labelFromUrl(new URL("https://example.com/severance/50%zz")), "Severance");
});

test("labelFromUrl still reads normal paths", () => {
  assert.equal(labelFromUrl(new URL("https://example.com/wtv/21760/the-office")), "The Office");
  assert.equal(labelFromUrl(new URL("https://example.com/12345")), null);
});

test("postContext aborts a request the local server never answers", { timeout: 3000 }, async () => {
  const realFetch = globalThis.fetch;
  let signal;
  globalThis.fetch = (_url, init) => {
    signal = init.signal;
    return new Promise((_resolve, reject) => {
      init.signal?.addEventListener("abort", () => reject(init.signal.reason));
    });
  };
  try {
    const started = Date.now();
    await postContext({ source: "browser" }, 50);
    assert.ok(signal, "fetch must be given an abort signal");
    assert.ok(Date.now() - started < 2000, "must not wait on a hung server");
  } finally {
    globalThis.fetch = realFetch;
  }
});
