/**
 * categories.js — Domain-to-category mapping and dashboard detection helpers.
 * Imported as an ES module by background.js (manifest type: "module").
 */

/** @type {Record<string, string>} */
export const DOMAIN_CATEGORY_MAP = {
  "youtube.com": "Watching YouTube",
  "www.youtube.com": "Watching YouTube",
  "music.youtube.com": "Listening on YouTube Music",
  "reddit.com": "Browsing Reddit",
  "www.reddit.com": "Browsing Reddit",
  "github.com": "Browsing GitHub",
  "gist.github.com": "Browsing GitHub",
  "chatgpt.com": "Using ChatGPT",
  "chat.openai.com": "Using ChatGPT",
  "twitter.com": "Browsing Twitter / X",
  "x.com": "Browsing Twitter / X",
  "linkedin.com": "Browsing LinkedIn",
  "www.linkedin.com": "Browsing LinkedIn",
  "stackoverflow.com": "Reading Stack Overflow",
  "docs.google.com": "Working in Google Docs",
  "drive.google.com": "Browsing Google Drive",
  "mail.google.com": "Reading Gmail",
  "calendar.google.com": "Checking Google Calendar",
  "notion.so": "Working in Notion",
  "www.notion.so": "Working in Notion",
  "figma.com": "Designing in Figma",
  "www.figma.com": "Designing in Figma",
  "npmjs.com": "Browsing npm",
  "www.npmjs.com": "Browsing npm",
  "developer.mozilla.org": "Reading MDN Docs",
  "wikipedia.org": "Reading Wikipedia",
  "en.wikipedia.org": "Reading Wikipedia",
  "twitch.tv": "Watching Twitch",
  "www.twitch.tv": "Watching Twitch",
  "netflix.com": "Watching Netflix",
  "www.netflix.com": "Watching Netflix",
  "spotify.com": "Listening on Spotify",
  "open.spotify.com": "Listening on Spotify",
};

/** Default category when no match is found. */
export const DEFAULT_CATEGORY = "Browsing privately";

/**
 * Returns the category string for a given hostname.
 * Strips leading "www." for a second lookup pass.
 * @param {string} hostname
 * @returns {string}
 */
export function getCategoryForDomain(hostname) {
  if (!hostname) return DEFAULT_CATEGORY;
  const lower = hostname.toLowerCase();
  if (DOMAIN_CATEGORY_MAP[lower]) return DOMAIN_CATEGORY_MAP[lower];
  // Strip www. prefix and retry
  const stripped = lower.replace(/^www\./, "");
  if (DOMAIN_CATEGORY_MAP[stripped]) return DOMAIN_CATEGORY_MAP[stripped];
  return DEFAULT_CATEGORY;
}

// LAN IP patterns
const LAN_REGEX = /^(192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})$/;

/**
 * Common router login hostnames/IP fragments.
 * @type {string[]}
 */
const ROUTER_HOSTS = [
  "192.168.0.1",
  "192.168.1.1",
  "192.168.1.254",
  "10.0.0.1",
  "10.0.0.138",
  "routerlogin.net",
  "routerlogin.com",
  "tplinkwifi.net",
  "tplinkmodem.net",
  "myrouter.local",
  "router.asus.com",
];

/**
 * Detects whether a URL points to a local dashboard and returns a label,
 * or null if not a dashboard.
 * @param {URL} url
 * @returns {string|null}
 */
export function detectDashboard(url) {
  const { hostname, port, pathname } = url;

  // Pi-hole check: hostname is "pi.hole" or path is "/admin" on a LAN IP
  if (
    hostname === "pi.hole" ||
    (hostname === "pi.hole" && pathname.startsWith("/admin"))
  ) {
    return "Checking Pi-hole";
  }

  // Also match pi.hole with /admin regardless
  if (hostname === "pi.hole") {
    return "Checking Pi-hole";
  }

  // LAN IP + /admin = custom dashboard
  if (LAN_REGEX.test(hostname) && pathname.startsWith("/admin")) {
    return "Custom Dashboard";
  }

  // Pi-hole on LAN IP (common path patterns)
  if (
    LAN_REGEX.test(hostname) &&
    (pathname === "/admin" ||
      pathname.startsWith("/admin/") ||
      pathname === "/admin.html")
  ) {
    return "Checking Pi-hole";
  }

  // Router login pages
  if (ROUTER_HOSTS.includes(hostname)) {
    return "Router Dashboard";
  }

  // localhost:PORT = local dashboard
  if (hostname === "localhost" || hostname === "127.0.0.1") {
    if (port) {
      return "Local Dashboard";
    }
  }

  return null;
}

/**
 * Detects the browser name from the user-agent string.
 * @returns {"chrome"|"edge"|"firefox"|"safari"|"unknown"}
 */
export function detectBrowser() {
  const ua = navigator.userAgent;
  if (ua.includes("Edg/")) return "edge";
  if (ua.includes("Chrome/")) return "chrome";
  if (ua.includes("Firefox/")) return "firefox";
  if (ua.includes("Safari/")) return "safari";
  return "unknown";
}
