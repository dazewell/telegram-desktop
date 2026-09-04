/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/history_view_top_bar_layout.h"

#include <algorithm>

namespace HistoryView {

TopBarTimeZoneLayout ComputeTopBarTimeZoneLayout(
		QRect titleLine,
		int titleNaturalWidth,
		int minimumTitleWidth,
		int badgeWidth,
		int spacing,
		int chipWidth,
		int chipHeight) {
	if (titleLine.isEmpty()
		|| titleNaturalWidth <= 0
		|| minimumTitleWidth <= 0
		|| badgeWidth < 0
		|| spacing < 0
		|| chipWidth <= 0
		|| chipHeight <= 0) {
		return {};
	}
	const auto reservedWidth = badgeWidth + spacing + chipWidth;
	const auto titleAvailable = titleLine.width() - reservedWidth;
	const auto meaningfulTitleWidth = std::min(
		titleNaturalWidth,
		minimumTitleWidth);
	if (titleAvailable < meaningfulTitleWidth) {
		return {};
	}
	const auto titleWidth = std::min(titleNaturalWidth, titleAvailable);
	const auto chipLeft = titleLine.x()
		+ titleWidth
		+ badgeWidth
		+ spacing;
	const auto chipTop = titleLine.y()
		+ (titleLine.height() - chipHeight) / 2;
	return {
		.nameBadgeRect = QRect(
			titleLine.topLeft(),
			QSize(titleWidth + badgeWidth, titleLine.height())),
		.chipRect = QRect(chipLeft, chipTop, chipWidth, chipHeight),
		.titleWidth = titleWidth,
	};
}

} // namespace HistoryView