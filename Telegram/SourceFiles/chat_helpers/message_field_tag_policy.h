#pragma once

#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"

namespace ChatHelpers {

[[nodiscard]] inline QString ValidateMessageFieldTags(
		QStringView mimeTag,
		uint64 selfId,
		Fn<bool(QStringView)> allowCustomEmoji,
		Fn<bool(QStringView)> keepCustomEmojiData) {
	auto parts = TextUtilities::SplitTags(mimeTag);
	for (auto part = parts.begin(); part != parts.end();) {
		const auto tag = *part;
		if (TextUtilities::IsMentionLink(tag)
			&& TextUtilities::MentionNameDataToFields(tag).selfId != selfId) {
			part = parts.erase(part);
		} else if (Ui::InputField::IsCustomEmojiLink(tag)) {
			const auto data = Ui::InputField::CustomEmojiEntityData(tag);
			if ((keepCustomEmojiData && keepCustomEmojiData(data))
				|| allowCustomEmoji(data)) {
				++part;
			} else {
				part = parts.erase(part);
			}
		} else {
			++part;
		}
	}
	return TextUtilities::JoinTag(parts);
}

}