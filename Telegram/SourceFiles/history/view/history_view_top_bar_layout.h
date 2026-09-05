/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QRect>

namespace HistoryView {

struct TopBarTimeZoneLayout {
	QRect nameBadgeRect;
	QRect chipRect;
	int titleAvailableWidth = 0;
	int titleRenderedWidth = 0;

	explicit operator bool() const {
		return !chipRect.isEmpty();
	}
};

[[nodiscard]] TopBarTimeZoneLayout ComputeTopBarTimeZoneLayout(
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
	int chipTextBaselineFromTop);

} // namespace HistoryView
