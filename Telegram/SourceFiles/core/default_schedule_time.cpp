/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/default_schedule_time.h"

#include "base/assertion.h"
// rpl/rpl.h must precede base/unixtime.h: that lib_base header declares
// rpl::producer<> without including it itself.
#include "rpl/rpl.h"
#include "base/unixtime.h"

#include <QDateTime>

namespace Core {
namespace {

constexpr auto kMinute = TimeId(60);
constexpr auto kHour = TimeId(60 * kMinute);
constexpr auto kDay = TimeId(24 * kHour);
constexpr auto kMonth = TimeId(30 * kDay);
constexpr auto kYear = TimeId(365 * kDay);

constexpr auto kOptions = std::array<ScheduleTimeOption, 42>{ {
	{ 5 * kMinute, ScheduleTimeUnit::Minutes, 5 },
	{ 10 * kMinute, ScheduleTimeUnit::Minutes, 10 },
	{ 15 * kMinute, ScheduleTimeUnit::Minutes, 15 },
	{ 20 * kMinute, ScheduleTimeUnit::Minutes, 20 },
	{ 25 * kMinute, ScheduleTimeUnit::Minutes, 25 },
	{ 30 * kMinute, ScheduleTimeUnit::Minutes, 30 },
	{ 35 * kMinute, ScheduleTimeUnit::Minutes, 35 },
	{ 40 * kMinute, ScheduleTimeUnit::Minutes, 40 },
	{ 45 * kMinute, ScheduleTimeUnit::Minutes, 45 },
	{ 50 * kMinute, ScheduleTimeUnit::Minutes, 50 },
	{ 55 * kMinute, ScheduleTimeUnit::Minutes, 55 },
	{ 60 * kMinute, ScheduleTimeUnit::Minutes, 60 },
	{ 2 * kHour, ScheduleTimeUnit::Hours, 2 },
	{ 3 * kHour, ScheduleTimeUnit::Hours, 3 },
	{ 4 * kHour, ScheduleTimeUnit::Hours, 4 },
	{ 5 * kHour, ScheduleTimeUnit::Hours, 5 },
	{ 6 * kHour, ScheduleTimeUnit::Hours, 6 },
	{ 7 * kHour, ScheduleTimeUnit::Hours, 7 },
	{ 8 * kHour, ScheduleTimeUnit::Hours, 8 },
	{ 9 * kHour, ScheduleTimeUnit::Hours, 9 },
	{ 10 * kHour, ScheduleTimeUnit::Hours, 10 },
	{ 11 * kHour, ScheduleTimeUnit::Hours, 11 },
	{ 12 * kHour, ScheduleTimeUnit::Hours, 12 },
	{ 1 * kDay, ScheduleTimeUnit::Days, 1 },
	{ 2 * kDay, ScheduleTimeUnit::Days, 2 },
	{ 3 * kDay, ScheduleTimeUnit::Days, 3 },
	{ 4 * kDay, ScheduleTimeUnit::Days, 4 },
	{ 5 * kDay, ScheduleTimeUnit::Days, 5 },
	{ 6 * kDay, ScheduleTimeUnit::Days, 6 },
	{ 7 * kDay, ScheduleTimeUnit::Days, 7 },
	{ 1 * kMonth, ScheduleTimeUnit::Months, 1 },
	{ 2 * kMonth, ScheduleTimeUnit::Months, 2 },
	{ 3 * kMonth, ScheduleTimeUnit::Months, 3 },
	{ 4 * kMonth, ScheduleTimeUnit::Months, 4 },
	{ 5 * kMonth, ScheduleTimeUnit::Months, 5 },
	{ 6 * kMonth, ScheduleTimeUnit::Months, 6 },
	{ 7 * kMonth, ScheduleTimeUnit::Months, 7 },
	{ 8 * kMonth, ScheduleTimeUnit::Months, 8 },
	{ 9 * kMonth, ScheduleTimeUnit::Months, 9 },
	{ 10 * kMonth, ScheduleTimeUnit::Months, 10 },
	{ 11 * kMonth, ScheduleTimeUnit::Months, 11 },
	{ kYear, ScheduleTimeUnit::Months, 12 },
} };

} // namespace

const std::array<ScheduleTimeOption, 42> &DefaultScheduleTimeOptions() {
	return kOptions;
}

const ScheduleTimeOption &DefaultScheduleTimeOption() {
	return kOptions[1];
}

const ScheduleTimeOption *LookupDefaultScheduleTimeOption(TimeId seconds) {
	for (const auto &option : kOptions) {
		if (option.seconds == seconds) {
			return &option;
		}
	}
	return nullptr;
}

TimeId ResolveDefaultScheduleTime(TimeId seconds) {
	if (const auto option = LookupDefaultScheduleTimeOption(seconds)) {
		return option->seconds;
	}
	return DefaultScheduleTimeOption().seconds;
}

QDateTime DefaultScheduleDateTime(
		const QDateTime &now,
		const ScheduleTimeOption &option) {
	switch (option.unit) {
	case ScheduleTimeUnit::Minutes:
	case ScheduleTimeUnit::Hours:
		return now.addSecs(option.seconds);
	case ScheduleTimeUnit::Days:
		return now.addDays(option.count);
	case ScheduleTimeUnit::Months:
		return now.addMonths(option.count);
	}
	Unexpected("Schedule time unit.");
}

TimeId DefaultScheduleTimestampFrom(TimeId now, TimeId storedSeconds) {
	const auto option = LookupDefaultScheduleTimeOption(
		ResolveDefaultScheduleTime(storedSeconds));
	Expects(option != nullptr);
	const auto nowLocal = base::unixtime::parse(now);
	if (option->unit == ScheduleTimeUnit::Minutes
		|| option->unit == ScheduleTimeUnit::Hours) {
		return now + option->seconds;
	}
	return base::unixtime::serialize(DefaultScheduleDateTime(nowLocal, *option));
}

TimeId DefaultScheduleMaxTimestampFrom(TimeId now) {
	const auto &options = DefaultScheduleTimeOptions();
	return base::unixtime::serialize(DefaultScheduleDateTime(
		base::unixtime::parse(now),
		options.back()));
}

} // namespace Core
