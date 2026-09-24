/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <array>

class QDateTime;

namespace Core {

enum class ScheduleTimeUnit {
	Minutes,
	Hours,
	Days,
	Months,
};

struct ScheduleTimeOption {
	TimeId seconds = 0;
	ScheduleTimeUnit unit = ScheduleTimeUnit::Minutes;
	int count = 0;
};

[[nodiscard]] const std::array<ScheduleTimeOption, 42> &
DefaultScheduleTimeOptions();

[[nodiscard]] const ScheduleTimeOption &DefaultScheduleTimeOption();
[[nodiscard]] const ScheduleTimeOption *LookupDefaultScheduleTimeOption(
	TimeId seconds);
[[nodiscard]] TimeId ResolveDefaultScheduleTime(TimeId seconds);

[[nodiscard]] QDateTime DefaultScheduleDateTime(
	const QDateTime &now,
	const ScheduleTimeOption &option);
[[nodiscard]] TimeId DefaultScheduleTimestampFrom(
	TimeId now,
	TimeId storedSeconds);
[[nodiscard]] TimeId DefaultScheduleMaxTimestampFrom(TimeId now);

} // namespace Core
