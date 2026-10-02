/**
 * Pure helper (no vscode import) so it can be unit tested with node --test.
 */
export interface RepoLike {
  rootUri: { fsPath: string };
}

function isWithin(root: string, fsPath: string): boolean {
  if (fsPath === root) return true;
  if (!fsPath.startsWith(root)) return false;
  // Require a path separator at the boundary so "/code/app" does not claim
  // "/code/app-docs". A root that already ends in a separator (e.g. "C:\")
  // is a boundary by itself.
  const last = root[root.length - 1];
  if (last === "/" || last === "\\") return true;
  const next = fsPath[root.length];
  return next === "/" || next === "\\";
}

/**
 * Pick the git repository that contains `fsPath`. When repositories nest,
 * the innermost (longest root) wins.
 */
export function findRepoForPath<T extends RepoLike>(repos: readonly T[], fsPath: string): T | undefined {
  let best: T | undefined;
  for (const r of repos) {
    const root = r.rootUri.fsPath;
    if (isWithin(root, fsPath) && (!best || root.length > best.rootUri.fsPath.length)) {
      best = r;
    }
  }
  return best;
}
