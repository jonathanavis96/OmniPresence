/**
 * extension.ts — OmniPresence VS Code Bridge
 *
 * Reports workspace name, repo, branch, and broad activity to the local
 * OmniPresence context server for Discord Rich Presence.
 * Exact file names are never included unless omnipresence.allowFileTitles is true.
 */

import * as vscode from "vscode";
import * as fs from "fs";
import * as path from "path";

// ─── Types ────────────────────────────────────────────────────────────────────

interface OmniPresencePayload {
  source: "vscode";
  workspace: string | null;
  repo: string | null;
  branch: string | null;
  activity: Activity;
  file_title_allowed: boolean;
  /** Only present (non-null) when file_title_allowed is true */
  file_name?: string | null;
}

type Activity = "Editing code" | "Debugging" | "Running" | "Idle" | "Viewing";

// ─── Config helpers ───────────────────────────────────────────────────────────

function getConfig() {
  const cfg = vscode.workspace.getConfiguration("omnipresence");
  return {
    endpoint: cfg.get<string>("endpoint", "http://127.0.0.1:47831/integrations/vscode/context"),
    allowFileTitles: cfg.get<boolean>("allowFileTitles", false),
    enabled: cfg.get<boolean>("enabled", true),
    debounceMs: cfg.get<number>("debounceMs", 2000),
  };
}

// ─── Git helpers ──────────────────────────────────────────────────────────────

/**
 * Attempt to read .git/HEAD from a workspace folder to determine branch.
 * Falls back to the built-in git extension if available.
 * Returns null if not determinable.
 */
async function getGitInfo(
  workspaceFolder: vscode.WorkspaceFolder | undefined
): Promise<{ repo: string | null; branch: string | null }> {
  if (!workspaceFolder) return { repo: null, branch: null };

  const fsPath = workspaceFolder.uri.fsPath;

  // Derive repo name from folder name (good enough default)
  const repoName = path.basename(fsPath);

  // Try the built-in git extension first
  try {
    const gitExt = vscode.extensions.getExtension("vscode.git");
    if (gitExt) {
      const gitApi = gitExt.isActive
        ? gitExt.exports.getAPI(1)
        : (await gitExt.activate()).getAPI(1);

      if (gitApi) {
        const repo = gitApi.repositories.find(
          (r: { rootUri: vscode.Uri }) =>
            r.rootUri.fsPath === fsPath ||
            fsPath.startsWith(r.rootUri.fsPath)
        );
        if (repo) {
          const head = repo.state?.HEAD;
          const branch = head?.name ?? null;
          // Prefer remote name if available
          const remoteRepo =
            repo.state?.remotes?.[0]?.fetchUrl
              ? extractRepoName(repo.state.remotes[0].fetchUrl)
              : null;
          return { repo: remoteRepo ?? repoName, branch };
        }
      }
    }
  } catch {
    // git extension unavailable — fall through to file-based approach
  }

  // Fallback: read .git/HEAD directly
  try {
    const headPath = path.join(fsPath, ".git", "HEAD");
    const headContent = fs.readFileSync(headPath, "utf8").trim();

    let branch: string | null = null;
    if (headContent.startsWith("ref: refs/heads/")) {
      branch = headContent.slice("ref: refs/heads/".length);
    } else if (/^[0-9a-f]{40}$/i.test(headContent)) {
      // Detached HEAD — show short hash
      branch = headContent.slice(0, 7);
    }

    return { repo: repoName, branch };
  } catch {
    // Not a git repo or .git/HEAD unreadable
    return { repo: null, branch: null };
  }
}

/**
 * Extract a bare repo name from a remote URL.
 * e.g. "git@github.com:user/my-repo.git" -> "my-repo"
 *      "https://github.com/user/my-repo.git" -> "my-repo"
 */
function extractRepoName(remoteUrl: string): string | null {
  try {
    const withoutGit = remoteUrl.replace(/\.git$/, "");
    const parts = withoutGit.split(/[/:]/).filter(Boolean);
    return parts[parts.length - 1] ?? null;
  } catch {
    return null;
  }
}

// ─── Activity detection ───────────────────────────────────────────────────────

/**
 * Determine the current broad activity based on VS Code state.
 */
function detectActivity(
  debugSession: vscode.DebugSession | undefined
): Activity {
  // Active debug session takes priority
  if (debugSession) {
    return "Debugging";
  }

  // Check if a terminal task is running (broad "Running" indicator)
  const runningTasks = vscode.tasks.taskExecutions;
  if (runningTasks.length > 0) {
    return "Running";
  }

  // Check for an active text editor
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    return "Idle";
  }

  // Read-only / diff editors
  if (
    editor.document.uri.scheme === "git" ||
    editor.document.uri.scheme === "output"
  ) {
    return "Viewing";
  }

  return "Editing code";
}

// ─── POST ─────────────────────────────────────────────────────────────────────

async function postContext(
  payload: OmniPresencePayload,
  endpoint: string
): Promise<void> {
  try {
    // Node's built-in fetch (Node 18+) or dynamic import
    const response = await fetch(endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      signal: AbortSignal.timeout(3000),
    });
    // Ignore non-2xx silently — OmniPresence may not be running
    void response;
  } catch {
    // OmniPresence not running, network error, timeout — all silent
  }
}

// ─── Main reporter ────────────────────────────────────────────────────────────

async function buildAndPost(
  activeDebugSession: vscode.DebugSession | undefined
): Promise<void> {
  const cfg = getConfig();
  if (!cfg.enabled) return;

  const workspaceFolder =
    vscode.workspace.workspaceFolders?.[0] ?? undefined;

  const workspaceName =
    workspaceFolder?.name ??
    vscode.workspace.name ??
    null;

  const { repo, branch } = await getGitInfo(workspaceFolder);

  const activity = detectActivity(activeDebugSession);

  const allowFileTitles = cfg.allowFileTitles;
  let fileName: string | null = null;
  if (allowFileTitles) {
    const editor = vscode.window.activeTextEditor;
    if (editor) {
      fileName = path.basename(editor.document.fileName);
    }
  }

  const payload: OmniPresencePayload = {
    source: "vscode",
    workspace: workspaceName,
    repo,
    branch,
    activity,
    file_title_allowed: allowFileTitles,
    ...(allowFileTitles ? { file_name: fileName } : {}),
  };

  await postContext(payload, cfg.endpoint);
}

// ─── Activation ───────────────────────────────────────────────────────────────

export function activate(context: vscode.ExtensionContext): void {
  let debounceTimer: ReturnType<typeof setTimeout> | undefined;
  let activeDebugSession: vscode.DebugSession | undefined;

  function scheduleReport(): void {
    const cfg = getConfig();
    if (debounceTimer !== undefined) clearTimeout(debounceTimer);
    debounceTimer = setTimeout(() => {
      debounceTimer = undefined;
      buildAndPost(activeDebugSession).catch(() => {
        // silent
      });
    }, cfg.debounceMs);
  }

  // Window focus changed (user switched back to VS Code)
  context.subscriptions.push(
    vscode.window.onDidChangeWindowState((state) => {
      if (state.focused) scheduleReport();
    })
  );

  // Active editor changed
  context.subscriptions.push(
    vscode.window.onDidChangeActiveTextEditor(() => {
      scheduleReport();
    })
  );

  // Document saved (code changes committed)
  context.subscriptions.push(
    vscode.workspace.onDidSaveTextDocument(() => {
      scheduleReport();
    })
  );

  // Debug session started
  context.subscriptions.push(
    vscode.debug.onDidStartDebugSession((session) => {
      activeDebugSession = session;
      scheduleReport();
    })
  );

  // Debug session ended
  context.subscriptions.push(
    vscode.debug.onDidTerminateDebugSession(() => {
      activeDebugSession = undefined;
      scheduleReport();
    })
  );

  // Task started / ended (Running state)
  context.subscriptions.push(
    vscode.tasks.onDidStartTask(() => scheduleReport()),
    vscode.tasks.onDidEndTask(() => scheduleReport())
  );

  // Config changed — re-report with new settings
  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("omnipresence")) {
        scheduleReport();
      }
    })
  );

  // Initial report on startup
  scheduleReport();
}

export function deactivate(): void {
  // Nothing to clean up — the service worker handles its own lifecycle
}
