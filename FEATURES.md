# Fork Features

This catalog records user-visible behavior maintained by this fork. It is not a
list of upstream Telegram Desktop features or internal workflow changes.

Add an entry with the same change that introduces or materially changes fork
behavior. Use a stable identifier only when branch, commit, release, or support
policy needs one. Each entry should state the user-visible behavior, affected
platforms or accounts, configuration/default, and the principal code entry
point. Do not add speculative or unverified entries.

No historical features were backfilled during the workflow bootstrap.

## Selected Message Text Shortcuts

- **Q: Quote & Reply** uses the existing selected-quote eligibility and length
	limit. It never substitutes a whole-message reply when a quote is unavailable.
- **C: Cite** appends the complete displayed selection to the end of the current
	writable composer in Quote format. It preserves existing draft text, tags,
	and blank lines, pads a nonempty draft to at least two trailing newline
	characters when needed, and leaves a blank unquoted line before the citation.
	An empty draft gets no leading blank line. The caret ends in an empty unquoted
	paragraph. It does not send, copy to the clipboard, add attribution, or replace
	reply/forward headers or attachments.
- **T: Translate Selected Text** opens the existing translation dialog under its
	existing provider and restriction policy, independently of composer writability.
- **E: Edit Selected Message** enters the existing message editor with the full
	original message and the same native text range highlighted, so typing replaces
	that selection. It does not save or send the message. It is unavailable while
	the receiving composer is already editing any message, including the same one.

Q/C/T apply to message text selections in both the legacy chat list and
the newer chat/topic/replies list, including displayed bodies, captions,
translations, and transcriptions represented by the existing selection pipeline.
Whole-message and composer selections do not activate these commands. Source
selection is cleared only after an accepted action for the same selection; opening
or cancelling a Quote destination chooser is not acceptance.
In the legacy chat view, draft loading and navigation invalidate pending reply
actions together with their reply state. A failed topic lookup ends that pending
reply without retrying, clearing the source selection, or changing the draft;
late completions cannot affect a replacement reply.

E supports the legacy chat list, shared chat/topic/replies composers, and the
ordinary scheduled list for selections proven to belong to the displayed original
body or editable caption with unchanged native offsets. Existing edit rights apply, including eligible
channel administrators. Empty, whole-message, composer, media-only, and non-native
selection ranges do not activate E. Translated or replacement text, factchecks,
other non-body regions, hidden body text, and media-shifted ranges are rejected
even when their displayed offsets fit the original text length. Grid-album captions
use the native caption owner; column-media captions without that direct body
mapping are unavailable. E does not depend on quote availability or its length cap.
Body bounds exclude the timestamp skip block and its optional layout newline using
the rendered text's metadata and extraction API. Literal trailing underscores stay
editable; timestamp positions do not. Offset-changing render transformations fail
closed rather than being clamped or mapped into the original message.
The prepared edit text must also preserve every UTF-16 position through the end
of the selection. In support mode, selections within the retained body remain
eligible, but those reaching into a removed trailing support hashtag or its
removed newline are rejected without clearing the source selection. Preparation
changes strictly after the selection do not disable it.
Rich-page/article and to-do editors, active
media editing, recording, blocked/hidden composers, and scheduled-topic lists are
excluded. No translated-text offset conversion or substring search is performed.
The native range is captured before source clearing; only accepted entry clears
the matching source selection. Normal draft preservation and Cancel restoration
use the existing edit pipeline. Ordinary menu Edit behavior is unchanged.

Settings > Chat Settings > Keyboard Shortcuts provides profile-wide remapping,
removal, and reset through the existing shortcut configuration and conflict store.
The defaults are Q/C/T/E; these selection-only commands accept single-stroke bindings,
including bare letters. Edit Selected Message follows Translate, with the shared
selection-only note below all four rows and retained when filtering. Context menus
expose Cite and current binding hints. Edit shows its current binding only when its
target matches an eligible selected E owner.
Contextual keyboard handling excludes inactive/minimized windows, unrelated focus,
search/settings, popups, modals, and input-method composition, and suppresses held
key repeats through focus changes until physical release, including modifier
release before the trigger key. Dispatch uses Qt logical-key candidates compatible
with the recorder, including Shift+Tab and shifted punctuation remaps.

Cite is unavailable while editing, recording, without a visible writable composer,
or when selected-text copying is protected. Incoming mention/custom-emoji tags use
the destination account's validation without revalidating existing draft tags.
Nested quote and block-code formatting is flattened without discarding text.
Unrepresentable text/formatting or a capacity overflow rejects the entire insertion
without truncation; newline-only selections cannot form a representable quote and
are rejected. One native Undo/Redo restores content and formatting; restoring the
previous caret or composer selection is not guaranteed.

Implemented in shared desktop code; local automated validation targets Windows.
The finite helper suite covers shifted-key normalization on the active Windows
layout. Isolated startup generated the real manager's Q/C/T default JSON entries;
a custom-binding UI probe was inconclusive, not a persistence pass. The user
reported successful general real-account Release smoke before the Cite blank-line
correction. Targeted logged-in integration, alternate layouts/theme/RTL/scale
smoke, and customized manager
save/reload/conflicts are not yet runtime-verified. The isolated current client
still crashes on window close; an older Release artifact also crashes, but at a
different fault site. Attribution and runtime acceptance remain unresolved.
Pending-reply navigation and failed-topic handling have source-trace and compile
validation only; the helper suite does not exercise real HistoryWidget callbacks.
E's dispatcher, source-action acceptance, and production selection-origin predicate
are covered by the finite helper suite. Predicate checks include translated or
wrong-region metadata even when offsets fit the original, body/caption and UTF-16
ranges, full text beyond quote limits, and empty/invalid ranges.
Real styled text fixtures cover body/caption timestamp blocks, zero/one/two layout
positions, resize/removal, literal underscores, repeated text and UTF-16 native
offsets, and rejection of offset-changing transformations. These fixtures call the
same body-bounds helper and predicate as the message owner, not a live caption or
composer integration.
Its actual composer range, repeated-text/UTF-16/entity/caption handling, focus,
draft restoration, account/topic navigation, and settings persistence still need
targeted real-chat checks. The real message-owner boundary and field range
application helper are not linked into test_text;
source reuse and a full Debug build are not runtime proof of those integrations.

Principal entry points: [contextual dispatcher](Telegram/SourceFiles/core/shortcuts_contextual.h),
[legacy selection actions](Telegram/SourceFiles/history/history_inner_widget.cpp),
[section selection actions](Telegram/SourceFiles/history/view/history_view_list_widget.cpp),
and [Cite insertion](Telegram/SourceFiles/chat_helpers/selected_text_cite.h).
