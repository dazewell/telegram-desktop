#pragma once

#include "ui/text/text.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"

#include <QTextBlock>
#include <QTextDocument>

namespace ChatHelpers {

[[nodiscard]] inline TextWithTags::Tags CanonicalCiteTags(TextWithTags::Tags tags) {
	for (auto &tag : tags) {
		auto normalized = QString();
		for (const auto part : TextUtilities::SplitTags(tag.id)) {
			normalized = TextUtilities::TagWithAdded(
				normalized,
				Ui::InputField::IsCustomEmojiLink(part)
					? Ui::InputField::kCustomEmojiTagStart
						+ Ui::InputField::CustomEmojiEntityData(part)
					: part.toString());
		}
		tag.id = std::move(normalized);
	}
	return tags;
}

[[nodiscard]] inline TextWithTags SelectedTextCiteTags(
		const TextForMimeData &selected) {
	auto tags = TextUtilities::ConvertEntitiesToTextTags(selected.rich.entities);
	for (const auto &tag : selected.tags) {
		tags.push_back({ tag.offset, tag.length, tag.id });
	}
	return { selected.rich.text, TextUtilities::SimplifyTags(std::move(tags)) };
}

[[nodiscard]] inline TextWithTags PrepareSelectedTextCite(
		const TextWithTags &draft,
		const TextWithTags &selected,
		Fn<QString(QStringView)> validateIncomingTag) {
	Expects(validateIncomingTag != nullptr);
	if (selected.text.isEmpty()) {
		return draft;
	}
	auto result = draft;
	if (!result.text.isEmpty()) {
		while (!result.text.endsWith(u"\n\n"_q)) {
			result.text += '\n';
		}
	}
	const auto offset = int(result.text.size());
	result.text += selected.text;
	if (!result.text.endsWith('\n')) {
		result.text += '\n';
	}
	const auto length = int(result.text.size()) - offset - 1;
	auto position = 0;
	for (const auto &tag : selected.tags) {
		const auto from = std::clamp(tag.offset, position, length);
		const auto till = std::clamp(tag.offset + tag.length, from, length);
		if (from > position) {
			result.tags.push_back({
				offset + position,
				from - position,
				Ui::InputField::kTagBlockquote,
			});
		}
		if (till > from) {
			const auto validated = validateIncomingTag(tag.id);
			auto parts = TextUtilities::SplitTags(validated);
			parts.erase(std::remove_if(parts.begin(), parts.end(), [](auto part) {
				return part == Ui::InputField::kTagBlockquote
					|| part == Ui::InputField::kTagBlockquoteCollapsed
					|| part.startsWith(Ui::InputField::kTagPre);
			}), parts.end());
			result.tags.push_back({
				offset + from,
				till - from,
				TextUtilities::TagWithAdded(
					TextUtilities::JoinTag(parts),
					Ui::InputField::kTagBlockquote),
			});
		}
		position = till;
	}
	if (position < length) {
		result.tags.push_back({
			offset + position,
			length - position,
			Ui::InputField::kTagBlockquote,
		});
	}
	return result;
}

[[nodiscard]] inline bool AppendSelectedTextCite(
		not_null<Ui::InputField*> field,
		const TextWithTags &selected,
		Fn<QString(QStringView)> validateIncomingTag,
		int maxLength = -1,
		Ui::Text::MarkedContext context = {}) {
	if (std::ranges::all_of(selected.text, [](QChar ch) { return ch == '\n'; })) {
		return false;
	}
	const auto prepared = PrepareSelectedTextCite(
		field->getTextWithTags(),
		selected,
		std::move(validateIncomingTag));
	if (maxLength >= 0 && prepared.text.size() > maxLength) {
		return false;
	}
	auto probe = Ui::InputField(nullptr, field->st(), Ui::InputField::Mode::MultiLine);
	probe.setCustomTextContext(std::move(context), [] { return true; });
	probe.setMarkdownReplacesEnabled(true);
	probe.setTextWithTags(prepared, Ui::InputField::HistoryAction::Clear);
	const auto normalized = probe.getTextWithTags();
	if (normalized.text != prepared.text
		|| CanonicalCiteTags(normalized.tags) != CanonicalCiteTags(prepared.tags)) {
		return false;
	}
	field->setTextWithTags(prepared);
	auto cursor = field->textCursor();
	cursor.movePosition(QTextCursor::End);
	field->setTextCursor(cursor);
	return true;
}

}