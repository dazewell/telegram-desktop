#pragma once

#include "layout/layout_selection.h"
#include "ui/text/text.h"

namespace HistoryView {

[[nodiscard]] inline int OriginalTextLengthForEdit(
		const Ui::Text::String &text) {
	auto length = text.length();
	const auto &modifications = text.modifications();
	for (auto modification = modifications.rbegin()
		; modification != modifications.rend()
		; ++modification) {
		if (!text.hasSkipBlock()
			|| modification->skipped
			|| modification->added != 1
			|| modification->position + 1 != length) {
			return -1;
		}
		const auto added = text.toString(TextSelection(
			uint16(modification->position),
			uint16(length)));
		const auto layoutOnly = (length == text.length())
			? added.isEmpty()
			: (length == text.length() - 1 && added == u"\n"_q);
		if (!layoutOnly) {
			return -1;
		}
		length = modification->position;
	}
	return (text.toString(TextSelection(0, uint16(length))).size() == length)
		? length
		: -1;
}

[[nodiscard]] inline bool IsOriginalTextSelectionForEdit(
		TextSelection selection,
		int originalLength,
		int textOffset,
		int textLength,
		bool originalTextShown) {
	return originalTextShown
		&& (selection != FullSelection)
		&& (textOffset == 0)
		&& (textLength == originalLength)
		&& (selection.from < selection.to)
		&& (selection.to <= textLength);
}

[[nodiscard]] inline bool IsEditPreparedTextSelection(
		TextSelection selection,
		QStringView original,
		QStringView prepared) {
	return (selection != FullSelection)
		&& (selection.from < selection.to)
		&& (selection.to <= original.size())
		&& (selection.to <= prepared.size())
		&& (original.left(selection.to) == prepared.left(selection.to));
}

} // namespace HistoryView