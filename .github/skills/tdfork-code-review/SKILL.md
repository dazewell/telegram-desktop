---
name: tdfork-code-review
description: 'Perform chief-architect plan and diff reviews for this Telegram Desktop fork. Use for architect round 1, round 2, incremental re-review, or final-state risk review.'
---

# Chief Architect Review

This is the single normative source for review criteria and output. The reviewer
is read-only, distrusts summaries, and inspects the real plan, source, history,
diff, and evidence.

## Rounds

### Round 1: Design

Review the complete scout report, UX specification, implementation brief, and
relevant source before editing starts. Challenge the chokepoint, reuse claims,
state model, ownership, migration, upstream footprint, and verification plan.
Return a verdict: `APPROVE`, `REVISE`, or `BLOCK`.

### Round 2: Real Diff

Run only after the relevant local build passed or a specifically accepted CI
fallback exists. Inspect the actual diff and surrounding code, not the
implementer's summary. Verify evidence belongs to the current diff or HEAD.
Return `APPROVE`, `REVISE`, or `BLOCK`.

### Incremental Re-review

Inspect named fixes and their interaction with the full current diff. Do not
reopen unrelated preferences. At most two fix/re-review cycles are allowed;
repeated defects in one region require a design reconsideration.

### Final-state Review

Require this after fixes for concurrency, lifecycle, media, storage, updater,
security-sensitive, scope-expanded, or repeatedly patched changes. Review the
whole final state, not only the last patch.

## Checklist

- Upstream survivability: smallest reasonable edit to upstream-owned files,
  isolated fork logic, no unnecessary churn, and explicit sync conflict risk.
- Correct entry point: visible behavior reaches the intended code path; an
  existing helper, component, setting, or event is reused before invention.
- C++ ownership: object, pointer, callback, timer, request, and container
  lifetimes are explicit; no dangling captures or destruction-order hazards.
- QObject ownership: parent-child ownership, deferred deletion, event filters,
  and `QPointer` use remain valid across window and application teardown.
- Connections and reactive streams: signal/slot context objects and
  `rpl::lifetime` scopes match owner lifetime; duplicate subscriptions and stale
  emissions are handled.
- Requests and callbacks: `crl::guard`, weak references, cancellation, or
  `MTP::Sender` ownership matches whether the server operation must complete.
- Threading: GUI objects stay on the UI thread; background results marshal via
  established dispatch and cannot race teardown or a newer request.
- Lifecycle: startup, shutdown, restart, updater, crash reporter, launcher
  modes, tray, multiple windows, and secondary processes remain coherent.
- Account/session isolation: state, caches, timers, requests, and persistence
  cannot leak between accounts or outlive a session.
- Persistence: ordered streams remain backward compatible; defaults,
  corruption, partial data, migration, and rollback behavior are defined.
- Windows: native paths and Unicode, registry handles, shell integration,
  notifications, startup, AppUserModelID, filesystem permissions, and portable
  mode are considered where applicable.
- UI: exact localized strings, theme, high DPI, interface scaling, RTL,
  keyboard, mouse, touchpad, accessibility, loading, empty, error, permission,
  disabled, and unavailable states are covered.
- Build and packaging: target source lists, generated code/resources, updater,
  signing, packaging, and CI consequences are identified.
- Errors: failures propagate to an owner, recovery is bounded, partial state is
  not presented as success, and cancellation is distinct from error.
- Attribution: no assistant authorship, co-author trailers, generated-by
  footers, or attribution in product source or authored history.
- Documentation: user-visible fork behavior and durable codemap discoveries
  are updated without speculative entries.

Read `REVIEW.md` for mechanical style checks. Separate defects introduced by
the proposed change from pre-existing or upstream issues. Mention a
pre-existing issue only when it blocks or materially changes the requested
work; do not make it part of the implementer's scope by accident.

## Severity

- `Critical`: data loss, security/privacy breach, remote policy violation,
  crash/use-after-free in a reachable path, corrupt migration, or an invalid
  implementation premise. Blocks progress.
- `Important`: likely behavioral regression, broken lifecycle/account state,
  missing required state, inadequate verification, or costly upstream conflict.
  Must be fixed or explicitly returned to the user for a scope decision.
- `Minor`: bounded maintainability, clarity, or low-risk polish issue. May be
  accepted and listed at handback.

Do not inflate preferences into findings. Every finding must cite a current
`file:line`, describe the triggering state and consequence, and state the
smallest defensible correction.

## Output

```text
Verdict: APPROVE | REVISE | BLOCK

Findings
[Critical|Important|Minor] path:line - concise title
Trigger: ...
Consequence: ...
Correction: ...

Evidence checked
- plan/diff revision
- commands and results inspected

Residual risk
- accepted or unverified items
```

If there are no findings, say so explicitly and identify remaining test or
runtime gaps.
