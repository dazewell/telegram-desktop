---
name: tdfork-branch-flow
description: 'Apply git, branch, commit, upstream-sync, push, and pull-request policy for the dazewell Telegram Desktop fork. Use before changing branches or publishing work.'
---

# Branch Flow

This is the single normative source for git topology and publication.

## Topology

- Fork: `origin`, `https://github.com/dazewell/telegram-desktop.git`.
- Upstream: `source`, `https://github.com/telegramdesktop/tdesktop.git`.
- Fork trunk and pull-request base: `dev`.
- Upstream development branch: `source/dev`.
- GitHub allows merge commits, squash merges, and rebase merges. Recent fork
  feature work used a merge commit into `dev`; the user chooses the final merge
  method.
- The default-branch ruleset blocks deletion and non-fast-forward updates and
  requests Copilot review. It does not currently require status checks.

Do not create or use git worktrees for this repository. All work happens
serially in the one configured checkout.

Never clone a second implementation checkout, start a worktree-backed session,
or let two roles own branches concurrently.

## Ordinary Feature Branch

1. Record `git status --short --branch`, `git worktree list --porcelain`, HEAD,
   remotes, and processes using the checkout or `out`.
2. Require no tracked user changes before switching. Untracked paths may remain
   only when they do not overlap the change; list and preserve them.
3. Do not switch while an editor build, debugger, Telegram instance, updater,
   watcher, or owned child process still uses the checkout or build tree.
4. Start from the approved `dev` commit and create one in-place branch. Prefer
   `YYYY-MM-DD_short-description`, matching recent fork practice.
5. Keep one focused change on the branch. Do not stash, reset, clean, discard,
   or rewrite user work.

## Commits

Commit only with explicit authorization. Use a concise plain-language subject
matching repository history. Do not add assistant authorship, co-author
trailers, generated-by footers, or similar attribution. Keep product commits
free of workflow metadata.

After review, add follow-up commits. Do not amend or rebase already-pushed
history and never force-push. Do not commit directly to `dev`.

## Push And Pull Request

Push and open a PR only when authorized. Target `dev`; describe behavior,
scope, verification, and known limitations without assistant attribution.
Query GitHub for current checks and review threads rather than assuming them.
Reply to each actionable review point and verify thread state. Never merge,
enable auto-merge, delete a branch, or alter remote policy for the user.

When push or PR authorization is absent, hand back the local branch, exact HEAD
or dirty paths, validation evidence, and the commands the user may choose to
run. Do not imply publication occurred.

## Upstream Synchronization

Upstream synchronization is documented, not automated. Fetching is read-only,
but integrating `source/dev` can create a large conflict surface and must be an
explicitly approved part of the branch plan. Start from a clean tracked tree,
record both tips, fetch `source`, and merge using the repository's established
practice only after approval. Never force-push, rewrite published history, or
mix an incidental upstream sync into a small feature without disclosure.

## Handback

Before handback, verify status, current branch, HEAD, worktree list, remote
publication state, PR base/head, checks, reviews, and process ledger. Leave the
feature branch checked out unless the user explicitly requests a safe switch.
State that the user owns the merge decision.
