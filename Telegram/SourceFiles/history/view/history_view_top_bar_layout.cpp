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
		int titleAvailableWidth,
		int titleRenderedWidth,
		int minimumTitleWidth,
		int badgeWidth,
		int spacing,
		int chipWidth,
		int chipHeight,
		int titleBaselineFromTop,
		int chipTextBaselineFromTop) {
	if (titleLine.isEmpty()
		|| titleNaturalWidth <= 0
		|| titleAvailableWidth <= 0
		|| titleRenderedWidth <= 0
		|| titleRenderedWidth > titleAvailableWidth
		|| minimumTitleWidth <= 0
		|| badgeWidth < 0
		|| spacing < 0
		|| chipWidth <= 0
		|| chipHeight <= 0
		|| titleBaselineFromTop < 0
		|| titleBaselineFromTop > titleLine.height()
		|| chipTextBaselineFromTop < 0
		|| chipTextBaselineFromTop > chipHeight) {
		return {};
	}
	const auto meaningfulTitleWidth = std::min(
		titleNaturalWidth,
		minimumTitleWidth);
	if (titleAvailableWidth < meaningfulTitleWidth
		|| titleAvailableWidth + badgeWidth + spacing + chipWidth
			> titleLine.width()) {
		return {};
	}
	const auto chipLeft = titleLine.x()
		+ titleRenderedWidth
		+ badgeWidth
		+ spacing;
	const auto chipTop = titleLine.y()
		+ titleBaselineFromTop
		- chipTextBaselineFromTop;
	return {
		.nameBadgeRect = QRect(
			titleLine.topLeft(),
			QSize(
				titleRenderedWidth + badgeWidth,
				titleLine.height())),
		.chipRect = QRect(chipLeft, chipTop, chipWidth, chipHeight),
		.titleAvailableWidth = titleAvailableWidth,
		.titleRenderedWidth = titleRenderedWidth,
	};
}

} // namespace HistoryView
