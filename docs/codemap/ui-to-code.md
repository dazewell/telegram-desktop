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

Established: 2026-09-07; affected anchors re-verified 2026-09-15. Re-verify after upstream changes.

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
[recorder](../../Telegram/SourceFiles/settings/sections/settings_shortcuts.cpp#L471),
with raw-event fallback; the manager remains the binding/conflict authority.

Flow: [HistoryInner::selectedTextAction](../../Telegram/SourceFiles/history/history_inner_widget.cpp#L2772)
and [ListWidget::selectedTextAction](../../Telegram/SourceFiles/history/view/history_view_list_widget.cpp#L5947)
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
or [ComposeControls](../../Telegram/SourceFiles/history/view/controls/history_view_compose_controls.cpp#L5920),
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
[defaults](../../Telegram/SourceFiles/core/shortcuts.cpp#L567), and
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
[Shortcut Settings constructor](../../Telegram/SourceFiles/settings/sections/settings_shortcuts.cpp#L676)
requires a SessionController. Default-file output does not establish customized
save/reload, conflict reassignment, or either real message-selection owner's
runtime behavior.

## Selected Text Edit

Established: 2026-09-15. Re-verify after upstream changes.

Surface: E on eligible selected message text; Edit Selected Message immediately
after Translate in Keyboard Shortcuts. The four commands share the existing
single-stroke manager, contextual dispatcher, filtering note, and held-key latch.

Flow: native owner resolution in
[HistoryInner::selectedEditMessage](../../Telegram/SourceFiles/history/history_inner_widget.cpp#L2752)
and [ListWidget::selectedEditMessage](../../Telegram/SourceFiles/history/view/history_view_list_widget.cpp#L5920)
uses the view text owner and
[Message::allowsSelectedTextEdit](../../Telegram/SourceFiles/history/view/history_view_message.cpp#L5272),
then the existing
[legacy range lookup](../../Telegram/SourceFiles/history/history_inner_widget.cpp#L1339)
or [list range lookup](../../Telegram/SourceFiles/history/view/history_view_list_widget.cpp#L3607).
[flatRangeForEdit](../../Telegram/SourceFiles/history/history_message_selection.h#L187)
passes through any nonempty flat display range, including non-body text; it is
not an eligibility proof. E no longer relies on the quote-owner fallback or quote
availability, so private/over-limit original selections remain independent of Q.

The shared production
[IsOriginalTextSelectionForEdit](../../Telegram/SourceFiles/history/history_selected_text_edit.h#L37)
requires original display state, a zero body offset, equal actual-body/original UTF-16
lengths, and a nonempty native range wholly within that body. Other element types
[default to rejection](../../Telegram/SourceFiles/history/view/history_view_element.h#L596).
The message boundary rejects hidden/rich-page/unavailable/summary text and compares
the owner's original/translated references: [translatedText](../../Telegram/SourceFiles/history/history_item.cpp#L3938)
returns the original reference only when no current translation is in use.
[Element text refresh](../../Telegram/SourceFiles/history/view/history_view_element.cpp#L2099)
resolves media itemForText and chooses displayed summary/original/translated text.
[GroupedMedia::itemForText](../../Telegram/SourceFiles/history/view/media/history_view_media_grouped.cpp#L104)
resolves grid-album captions; column-media hides the direct body and is rejected.

Origin evidence is the existing [Message::textState](../../Telegram/SourceFiles/history/view/history_view_message.cpp#L3898):
factcheck/log-entry offsets start after visible body plus media text, and inverted
media shifts body offsets by its selection length. The same owner's
[visible lengths](../../Telegram/SourceFiles/history/view/history_view_message.cpp#L7172)
still establish media offsets, but raw body length includes timestamp layout text.
[refreshInfoSkipBlock](../../Telegram/SourceFiles/history/view/history_view_message.cpp#L7189)
attaches the timestamp through
[String::updateSkipBlock](../../Telegram/lib_ui/ui/text/text.cpp#L919), which appends
a skip position and sometimes a layout newline. E instead uses
[OriginalTextLengthForEdit](../../Telegram/SourceFiles/history/history_selected_text_edit.h#L8):
public [modifications](../../Telegram/lib_ui/ui/text/text.cpp#L1972) must describe
only a contiguous trailing skip and optional added newline, and
[range extraction](../../Telegram/lib_ui/ui/text/text.cpp#L1981) verifies their
content and the resulting body length. Extraction
[ignores actual skip blocks](../../Telegram/lib_ui/ui/text/text.cpp#L1878), not
literal underscore characters. Skipped source text, body-interior additions, and
other extracted-length changes fail closed; no claim of arbitrary render mapping
is made. Hidden/shorter bodies and leading-media regions cannot qualify merely
because a display range fits originalText. Nonzero body offsets fail closed;
no traversal, substring search, or conversion is added. Pending source validity
re-enters both resolvers, and menu hints already use those same resolvers.

Destination eligibility is separate from source validity: existing edits block
entry, but entering edit mode does not invalidate the acceptance action.
[HistoryWidget::editMessage](../../Telegram/SourceFiles/history/history_widget.cpp#L10449)
and [ComposeControls::editMessage](../../Telegram/SourceFiles/history/view/controls/history_view_compose_controls.cpp#L5757)
accept the optional source action only after establishing the edit target and
applying [SelectTextInFieldWithMargins](../../Telegram/SourceFiles/chat_helpers/message_field.cpp#L1498),
which uses QTextCursor::KeepAnchor. Full text and entity preparation stays in
[PrepareEditText](../../Telegram/SourceFiles/chat_helpers/message_field.cpp#L374).
Source clearing precedes final composer focus, and successful results are not
revalidated against the generation changed by that clearing.

The [chat callback](../../Telegram/SourceFiles/history/view/history_view_chat_section.cpp#L715)
and [ordinary scheduled callback](../../Telegram/SourceFiles/history/view/history_view_scheduled_section.cpp#L271)
carry target ID, native range, and action by value with owner-bound guards.
Ordinary menu edit events remain ID-based: their existing receivers retrieve the
native range and pass it to the same composer API. Target-matched hints are added
at the [legacy menu](../../Telegram/SourceFiles/history/history_inner_widget.cpp#L3133)
and [new menu](../../Telegram/SourceFiles/history/view/history_view_context_menu.cpp#L983);
the menu execution and permissions are unchanged.

Verification: source trace plus finite production dispatcher/SelectedTextAction
tests for E classification, unavailable fallthrough, late rejection, held repeats,
post-entry acceptance, superseding, and owner destruction. The
[production origin-predicate tests](../../Telegram/SourceFiles/tests/test_text.cpp#L835)
exercise native range/owner metadata for translated and wrong-region rejection,
including in-bounds impostors, body/caption/UTF-16/full-text acceptance, and invalid
ranges. The [real render-block regressions](../../Telegram/SourceFiles/tests/test_text.cpp#L876)
use styled Ui::Text::String body/caption fixtures with updateSkipBlock, both sizes,
removal/disabled skip, a literal trailing underscore, native UTF-16/repeated-text
ranges, and the RTL two-position suffix. They invoke the same production bounds
helper and predicate and reject timestamp/factcheck/shifted/translated ranges and
offset-changing parser transformations. A real timestamp fixture failed before
the fix and passed afterward. They do not construct actual message/media owners.
test_text does not link
SelectTextInFieldWithMargins or the real history/composer owners. Full application
compilation and source reuse do not prove real-chat range, focus, draft, caption,
or navigation behavior; those targeted runtime checks remain unrun.
