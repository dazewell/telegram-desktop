# Telegram Desktop Fork Agent Router

This repository is the Windows-focused `dazewell/telegram-desktop` fork of
Telegram Desktop. Before acting on a development request, read
`.github/skills/tdfork-workflow/SKILL.md`.

Use the workflow roles in `.github/agents/`:

- `tdfork-orchestrator` owns an end-to-end request and serialized handoffs.
- `tdfork-scout` performs read-only repository reconnaissance.
- `tdfork-ux` specifies user-visible behavior without editing.
- `tdfork-architect` reviews plans and real diffs without editing.
- `tdfork-implementer` is the only role allowed to change product files.

Route git and pull-request questions to
`.github/skills/tdfork-branch-flow/SKILL.md`, reviews to
`.github/skills/tdfork-code-review/SKILL.md`, and process ownership or cleanup
to `.github/skills/tdfork-process-lifecycle/SKILL.md`.

All work happens serially in this one configured checkout. Never create or use
a worktree, clone, second checkout, parallel mutating session, or nested
orchestrator. A new editor session may be required before changed agents and
skills are discoverable.

## User Action Requests

Whenever user clarification, a decision, approval, or manual action is needed,
use the host's available interactive question or approval tool (`ask_user`,
`askQuestions`, or equivalent). Discover deferred tools before calling them;
use only tools actually exposed by the host. Combine pending decisions where
practical. Keep progress-only updates as text, and do not request confirmation
for already-authorized routine work.

If no suitable tool is available, say so explicitly and make the blocking
action conspicuous in the final response. Never silently wait for user action
in progress text or claim that a notification was delivered. Do not invent
tool names, promise notification delivery, or change role tool restrictions
to expose an unavailable tool.

Never request secrets through a question or approval tool. Ask the user to
enter them directly in the terminal or a secure UI.
