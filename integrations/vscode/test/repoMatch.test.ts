// Run: node --test integrations/vscode/test/  (Node 22.18+ strips the types)
import test from "node:test";
import assert from "node:assert/strict";
import { findRepoForPath } from "../src/repoMatch.ts";

const repo = (p: string) => ({ rootUri: { fsPath: p } });

test("a sibling folder sharing a name prefix is not treated as inside the repo", () => {
  assert.equal(findRepoForPath([repo("/home/u/code/app")], "/home/u/code/app-docs"), undefined);
});

test("the innermost repository wins over an enclosing one", () => {
  const outer = repo("/home/u/code");
  const inner = repo("/home/u/code/app");
  assert.equal(findRepoForPath([outer, inner], "/home/u/code/app/src"), inner);
});

test("exact root and nested paths still match, on both separators", () => {
  const r = repo("/home/u/code/app");
  assert.equal(findRepoForPath([r], "/home/u/code/app"), r);
  assert.equal(findRepoForPath([r], "/home/u/code/app/pkg"), r);
  const w = repo("C:\\code\\app");
  assert.equal(findRepoForPath([w], "C:\\code\\app\\pkg"), w);
  assert.equal(findRepoForPath([w], "C:\\code\\app2"), undefined);
});
