# UI To Code

Record verified visible-interaction entry points only.

```text
## Interaction name
Established: YYYY-MM-DD
Surface: visible location and trigger
Entry: path/to/file.cpp:line
Flow: concise entry -> owner -> effect
Verification: how the mapping was established
```

No entries were backfilled during the workflow bootstrap.

## Selected Message Text Actions

Established: 2026-09-07. Re-verify these entry points after upstream changes.

Surface: Q/C/T or the message-text context menu in the legacy chat list and the
newer chat/topic/replies list; bindings live in Chat Settings > Keyboard Shortcuts.

Entry: [Application::eventFilter](../../Telegram/SourceFiles/core/application.cpp#L717)
routes key override/press/release and input-method events to the
[contextual dispatcher](../../Telegram/SourceFiles/core/shortcuts_contextual.h#L75).
The dispatcher queries bindings from the existing shortcut manager and asks the
focused list for an eligible action. Its physical-key latch survives dialog focus
changes and modifier release, clearing on trigger release, owner destruction, or
application deactivation. Its
[Qt candidate lookup](../../Telegram/SourceFiles/core/shortcuts_contextual.h#L109)
uses the same modifier-compatible logical keys as the
[recorder](../../Telegram/SourceFiles/settings/sections/settings_shortcuts.cpp#L470),
with raw-event fallback; the manager remains the binding/conflict authority.

Flow: [HistoryInner::selectedTextAction](../../Telegram/SourceFiles/history/history_inner_widget.cpp#L2752)
and [ListWidget::selectedTextAction](../../Telegram/SourceFiles/history/view/history_view_list_widget.cpp#L5913)
share their eligibility and invocation paths with their context menus. Both use
[SelectedTextAction](../../Telegram/SourceFiles/chat_helpers/selected_text_action.h#L38)
for owner/generation-guarded acceptance. The
[destination chooser](../../Telegram/SourceFiles/history/view/controls/history_view_draft_options.cpp#L1371)
checks that guard before writing a draft and accepts after writing it.

Legacy pending replies use
[clearProcessingReply](../../Telegram/SourceFiles/history/history_widget.cpp#L10282)
to invalidate a generation and clear the reply target, cached item, and selected
action together. [Draft loading](../../Telegram/SourceFiles/history/history_widget.cpp#L2957)
and [navigation](../../Telegram/SourceFiles/history/history_widget.cpp#L3223)
reset that state before restoring a new reply.
[continueProcessingReply](../../Telegram/SourceFiles/history/history_widget.cpp#L10292)
checks the generation before cleanup or mutation, and checks requested topic
existence before re-entering reply processing. This matters because
[Forum request failure](../../Telegram/SourceFiles/data/data_forum.cpp#L457)
also invokes completion callbacks. An unresolved completion ends the current
pending reply without retry or draft/selection mutation. These are source-traced
facts, not runtime-tested HistoryWidget interleavings.

Cite enters [HistoryWidget](../../Telegram/SourceFiles/history/history_widget.cpp#L10235)
or [ComposeControls](../../Telegram/SourceFiles/history/view/controls/history_view_compose_controls.cpp#L5898),
then uses [AppendSelectedTextCite](../../Telegram/SourceFiles/chat_helpers/selected_text_cite.h#L96).
Its [preparation](../../Telegram/SourceFiles/chat_helpers/selected_text_cite.h#L37)
pads nonempty drafts to at least two trailing newline characters before computing
the quote offset, leaving the separating blank line unquoted. Extra draft newlines
and selected whitespace are preserved; empty drafts get no leading blank line.
The helper validates representability in a temporary InputField and uses the
public setter for one native undo entry.
Incoming account-sensitive tags reuse the
[message-field validator](../../Telegram/SourceFiles/chat_helpers/message_field.cpp#L90);
existing draft tags are not passed through that validator.

Configuration: [command names](../../Telegram/SourceFiles/core/shortcuts.cpp#L136),
[defaults](../../Telegram/SourceFiles/core/shortcuts.cpp#L566), and
[settings rows](../../Telegram/SourceFiles/settings/sections/settings_shortcuts.cpp#L111)
use the existing profile-wide shortcut/conflict store. Contextual commands have
no registered QAction key sequence; single-stroke validation is command-specific.

Verification: current-source tracing and the existing finite
[test_text suite](../../Telegram/SourceFiles/tests/test_text.cpp#L402) exercise the
production dispatcher, selection guard, tag validation, and real InputField
insertion/undo/redo, including literal blank-line text and unquoted separator
assertions for a draft with one trailing newline. The suite does not link the application shortcut manager;
its simulated binding lookup is not a manager JSON persistence test. Logged-in
reachability of both chat views remains unverified by targeted checks; the user
reported successful general real-account Release smoke before this spacing change.

Startup [Manager::fill](../../Telegram/SourceFiles/core/shortcuts.cpp#L297) writes
missing default files and reads custom JSON. Isolated startup produced Q/C/T in
the actual default file. Customized saving occurs in
[Manager::change](../../Telegram/SourceFiles/core/shortcuts.cpp#L338) and
[resetToDefaults](../../Telegram/SourceFiles/core/shortcuts.cpp#L356); the normal
[Shortcut Settings constructor](../../Telegram/SourceFiles/settings/sections/settings_shortcuts.cpp#L675)
requires a SessionController. Default-file output does not establish customized
save/reload, conflict reassignment, or either real message-selection owner's
runtime behavior.
