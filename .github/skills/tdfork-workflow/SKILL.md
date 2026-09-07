---
name: tdfork-workflow
description: 'Run feature development and bug fixes for this Windows Telegram Desktop fork. Use for planning, implementing, testing, reviewing, or handing back repository changes through the serialized one-checkout workflow.'
---

# Telegram Desktop Fork Workflow

This is the normative source for phase sequencing, implementation policy, and
evidence. Git topology belongs to `../tdfork-branch-flow/SKILL.md`, review
criteria to `../tdfork-code-review/SKILL.md`, and process cleanup to
`../tdfork-process-lifecycle/SKILL.md`.

## Repository Facts

- The fork remote is `https://github.com/dazewell/telegram-desktop.git`; the
  upstream remote is `https://github.com/telegramdesktop/tdesktop.git`.
- The fork and upstream development branches are `origin/dev` and `source/dev`.
- Application code is rooted at `Telegram/SourceFiles`, resources at
  `Telegram/Resources`, and target composition at `Telegram/CMakeLists.txt`.
- The local Windows build tree is `out`, configured for x64 Visual Studio 2022,
  toolset `v143`, MSVC 19.44, and Qt 6.11.2 under
  `C:/TBuild/Libraries/win64/Qt-6.11.2`.
- The normal implementation gate from the repository root is:

  ```powershell
  cmake --build out --config Debug --target Telegram
  ```

  The September 7, 2026 baseline took about eight minutes after invalidation
  and about six seconds as a no-op incremental build. Report actual duration.
- The expected application artifact is `out/Debug/Telegram.exe`. The Telegram
  target also builds configured test dependencies such as
  `test_contact_time_zone`, `test_text`, and `test_update_verify`.
- Windows pull-request CI builds Debug configurations across architecture, Qt,
  and generator variants. CI is a fallback only when local execution is
  genuinely unavailable; do not describe queued or unrelated CI as proof.

Re-verify cache paths and versions before relying on them. Never regenerate,
clean, bootstrap dependencies, alter submodules, or rewrite environment
variables merely because these recorded values differ.

## Architecture

Telegram Desktop is a C++/Qt Widgets application. The codebase uses QObject
parent-child ownership, `QPointer`, `base::weak_ptr`, guarded callbacks,
`MTP::Sender`, `rpl` producers and lifetimes, and `crl::on_main` dispatch.
Session-owned state commonly lives under `Main::Session` or `Data::Session`;
account and session isolation must be explicit.

Core and session settings use ordered `QDataStream` serialization. Append new
fields, guard old data reads, and prefer an established key/value preference
when appropriate. Treat updater, crash reporting, launcher modes, windows,
tray, notifications, registry, shell links, paths, Unicode, and portable data
as coupled lifecycle surfaces on Windows.

Use `Telegram/Resources/langs/lang.strings` for user-visible text and project
`.style` files for scale-sensitive dimensions. Follow nearby patterns and
`REVIEW.md`; use `_q` string literals, `auto` where the type is evident, and
avoid narration comments. Never add debug machinery to production code solely
to make a test observable; use established test infrastructure.

## Ownership And Change Shape

The repository is primarily upstream-owned. Fork-specific behavior currently
touches contact time zones, quick unlock, default schedule time, and a Qt
rebuild helper, but those paths remain upstream integration points rather than
a license for broad rewrites.

Search history and nearby code before adding anything. Reuse existing
components and enter at the narrowest behavioral chokepoint. Keep edits to
upstream-hot files minimal and isolate fork logic where the architecture
supports it. Do not reformat or repair unrelated code.

An implementation brief must include:

- request, non-goals, and off-state equivalence;
- verified entry point and reusable patterns;
- affected UI, settings, storage, account/session, platform, and build paths;
- acceptance checks and evidence plan;
- trade-off budget: accepted complexity, upstream footprint, runtime cost, and
  consciously deferred alternatives.

Split work when independent behavior, migration risk, platform ownership, or
reviewability makes one focused branch unrealistic.

## Phase Sequence

1. Preflight the one checkout, acquire exclusive ownership, record branch,
   HEAD, status, remotes, worktrees, and active processes.
2. Invoke `tdfork-scout` synchronously and wait for its report.
3. For user-visible behavior, invoke `tdfork-ux` synchronously and wait.
4. Give the complete plan and reports to `tdfork-architect` for round 1.
5. Present one consolidated decision gate containing only unresolved choices.
6. Hand off serially to `tdfork-implementer` on one approved ordinary branch.
7. Run focused tests and the proven local Debug/Telegram build.
8. For visible work, perform a focused reachability or smoke check at relevant
   scaling, theme, RTL, window, input, and account states.
9. Give the real diff and current-head evidence to `tdfork-architect` round 2.
10. Return Critical and Important findings to the implementer.
11. Allow at most two fix and incremental re-review cycles.
12. Treat repeated fixes in one region as a design problem and return to the
    decision owner instead of accumulating patches.
13. Require final-state review for concurrency, lifetime, media, storage,
    updater, security-sensitive, or scope-expanded work.
14. Re-run affected tests/builds after the last code change and bind evidence
    to the current HEAD or uncommitted diff.
15. Confirm `FEATURES.md` and codemap obligations.
16. When a PR is authorized, answer every review point and verify every thread
    disposition; never claim remote state without querying it.
17. Hand back branch/PR, evidence, limitations, and accepted Minor findings.
18. Never merge for the user.
19. Stop and independently verify all owned processes before releasing the
    checkout.

Roles run one at a time. A role must finish and release repository access
before the next starts. Never create nested orchestrators. If the host cannot
prove that a custom-agent call remains in this checkout without creating a
worktree or clone, use an explicit same-session persona handoff. If no safe
implementer handoff exists, stop and ask the user to select
`tdfork-implementer` manually in this checkout; the orchestrator must not
silently implement non-trivial product code.

## Asynchronous And Stateful Work

For timers, queued callbacks, requests, reactive streams, background work, or
multi-window/session state, enumerate interleavings before implementation:
owner destruction, cancellation, stale result arrival, account switch,
shutdown/restart, updater launch, clock and locale change, duplicate events,
and failure recovery. State who owns each callback, subscription, timer,
request, and persisted value. Verify the off state is equivalent to current
behavior.

## Validation

Run the narrowest meaningful check first. Existing examples include:

```powershell
cmake --build out --config Debug --target test_contact_time_zone
out\Debug\test_contact_time_zone.exe
cmake --build out --config Debug --target test_text
cmake --build out --config Debug --target Telegram
```

Only run a target that exists in the configured tree and is relevant to the
change. Use `git diff --check` as an additional hygiene check, never as a
substitute for compilation or behavior testing. A full clean or Release build
requires explicit justification and approval.

Evidence reports must name the exact command, working directory, start/end or
duration, exit code, key output, artifact, tested commit/diff, and limitations.
Distinguish observed behavior from inference. Do not hide failed attempts or
credit documentation changes with build recovery without a mechanism.

## Documentation

Update `FEATURES.md` only for user-visible fork behavior. Add a stable feature
identifier when commit or release policy needs one; do not inventory upstream
features or speculate.

Add codemap entries only for durable discoveries established by the same
change. UI entry points belong in `docs/codemap/ui-to-code.md`, non-obvious
upstream behavior in `upstream-traps.md`, and disproven approaches in
`dead-ends.md`. Every entry needs a current file-and-line citation and date;
readers must re-verify it.

## Authorship And Reporting

Never put assistant authorship, co-author trailers, generated-by footers, or
assistant attribution in product source, commit messages, PR titles, or PR
bodies. Workflow documents may describe this process. Commit subjects describe
the change, not who produced it.

Report only what was established. Never say tests passed when they did not run,
that a review is resolved without checking the thread, or that a branch is
ready to merge. Handback states what exists and leaves merging to the user.
