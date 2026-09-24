/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/default_schedule_time.h"

#include "base/assertion.h"
#include "rpl/rpl.h"
#include "base/unixtime.h"

#include <QDateTime>
#include <QTimeZone>

#include <iostream>
#include <vector>

namespace Test {
namespace {

constexpr auto kMinute = TimeId(60);
constexpr auto kHour = TimeId(60 * kMinute);
constexpr auto kDay = TimeId(24 * kHour);

[[nodiscard]] QTimeZone NewYork() {
	const auto result = QTimeZone(QByteArray("America/New_York"));
	Expects(result.isValid());
	return result;
}

[[nodiscard]] QDateTime DateTime(
		int year,
		int month,
		int day,
		int hour,
		int minute,
		int second,
		const QTimeZone &zone) {
	const auto result = QDateTime(
		QDate(year, month, day),
		QTime(hour, minute, second),
		zone);
	Expects(result.isValid());
	return result;
}

[[nodiscard]] const Core::ScheduleTimeOption &Option(TimeId seconds) {
	const auto result = Core::LookupDefaultScheduleTimeOption(seconds);
	Expects(result != nullptr);
	return *result;
}

[[nodiscard]] const Core::ScheduleTimeOption &Option(
		Core::ScheduleTimeUnit unit,
		int count) {
	for (const auto &option : Core::DefaultScheduleTimeOptions()) {
		if (option.unit == unit && option.count == count) {
			return option;
		}
	}
	Unexpected("Missing schedule time option.");
}

void TestOptions() {
	const auto &options = Core::DefaultScheduleTimeOptions();
	Expects(options.size() == 42);
	auto expected = std::vector<Core::ScheduleTimeOption>();
	for (auto minutes = 5; minutes <= 60; minutes += 5) {
		expected.push_back({
			minutes * kMinute,
			Core::ScheduleTimeUnit::Minutes,
			minutes });
	}
	for (auto hours = 2; hours <= 12; ++hours) {
		expected.push_back({
			hours * kHour,
			Core::ScheduleTimeUnit::Hours,
			hours });
	}
	for (auto days = 1; days <= 7; ++days) {
		expected.push_back({
			days * kDay,
			Core::ScheduleTimeUnit::Days,
			days });
	}
	for (auto months = 1; months <= 11; ++months) {
		expected.push_back({
			months * 30 * kDay,
			Core::ScheduleTimeUnit::Months,
			months });
	}
	expected.push_back({ 365 * kDay, Core::ScheduleTimeUnit::Months, 12 });

	for (auto i = 0; i != int(expected.size()); ++i) {
		Expects(options[i].seconds == expected[i].seconds);
		Expects(options[i].unit == expected[i].unit);
		Expects(options[i].count == expected[i].count);
	}
	Expects(Core::DefaultScheduleTimeOption().seconds == 10 * kMinute);
}

void TestNormalization() {
	for (const auto &option : Core::DefaultScheduleTimeOptions()) {
		Expects(Core::ResolveDefaultScheduleTime(option.seconds)
			== option.seconds);
		Expects(Core::LookupDefaultScheduleTimeOption(option.seconds)
			== &option);
	}
	for (const auto invalid : {
		TimeId(0),
		TimeId(1),
		TimeId(4 * kMinute),
		TimeId(65 * kMinute),
		TimeId(13 * kHour),
		TimeId(8 * kDay),
		TimeId(366 * kDay),
	}) {
		Expects(Core::ResolveDefaultScheduleTime(invalid) == 10 * kMinute);
		Expects(Core::DefaultScheduleTimestampFrom(1000000, invalid)
			== 1000000 + 10 * kMinute);
	}
}

void TestElapsedUnits() {
	const auto zone = NewYork();
	const auto now = DateTime(2026, 3, 8, 1, 30, 0, zone);
	const auto minute = Core::DefaultScheduleDateTime(
		now,
		Option(Core::ScheduleTimeUnit::Minutes, 60));
	const auto hour = Core::DefaultScheduleDateTime(
		now,
		Option(Core::ScheduleTimeUnit::Hours, 2));
	Expects(now.secsTo(minute) == 60 * kMinute);
	Expects(now.secsTo(hour) == 2 * kHour);
}

void TestCalendarUnits() {
	const auto zone = NewYork();
	const auto beforeSpring = DateTime(2026, 3, 7, 9, 15, 0, zone);
	const auto springDay = Core::DefaultScheduleDateTime(
		beforeSpring,
		Option(Core::ScheduleTimeUnit::Days, 1));
	Expects(springDay.date() == QDate(2026, 3, 8));
	Expects(springDay.time() == QTime(9, 15));
	Expects(beforeSpring.secsTo(springDay) == 23 * kHour);

	const auto beforeFall = DateTime(2026, 10, 31, 9, 15, 0, zone);
	const auto fallDay = Core::DefaultScheduleDateTime(
		beforeFall,
		Option(Core::ScheduleTimeUnit::Days, 1));
	Expects(fallDay.date() == QDate(2026, 11, 1));
	Expects(fallDay.time() == QTime(9, 15));
	Expects(beforeFall.secsTo(fallDay) == 25 * kHour);

	const auto month = Core::DefaultScheduleDateTime(
		DateTime(2026, 1, 31, 16, 45, 0, zone),
		Option(Core::ScheduleTimeUnit::Months, 1));
	Expects(month.date() == QDate(2026, 2, 28));
	Expects(month.time() == QTime(16, 45));

	const auto leap = Core::DefaultScheduleDateTime(
		DateTime(2024, 2, 29, 10, 0, 0, zone),
		Option(Core::ScheduleTimeUnit::Months, 12));
	Expects(leap.date() == QDate(2025, 2, 28));
	Expects(leap.time() == QTime(10, 0));
}

void TestDstGapAndOverlap() {
	const auto zone = NewYork();
	const auto gapBase = DateTime(2026, 3, 7, 2, 30, 0, zone);
	const auto gap = Core::DefaultScheduleDateTime(
		gapBase,
		Option(Core::ScheduleTimeUnit::Days, 1));
	Expects(gap.isValid());
	Expects(gap.date() == QDate(2026, 3, 8));
	Expects(gap.time() == QTime(3, 30));

	const auto overlapBase = DateTime(2026, 10, 31, 1, 30, 0, zone);
	const auto overlap = Core::DefaultScheduleDateTime(
		overlapBase,
		Option(Core::ScheduleTimeUnit::Days, 1));
	Expects(overlap.isValid());
	Expects(overlap.date() == QDate(2026, 11, 1));
	Expects(overlap.time() == QTime(1, 30));
	Expects(overlap.offsetFromUtc() == -4 * kHour);
}

void TestMaxFencepost() {
	const auto now = base::unixtime::serialize(QDateTime(
		QDate(2026, 9, 24),
		QTime(16, 13, 14)));
	const auto twelveMonths = Core::DefaultScheduleTimestampFrom(
		now,
		365 * kDay);
	const auto max = Core::DefaultScheduleMaxTimestampFrom(now);
	Expects(max == twelveMonths);
	Expects(twelveMonths == base::unixtime::serialize(
		base::unixtime::parse(now).addMonths(12)));
}

} // namespace

void TestDefaultScheduleTime() {
	TestOptions();
	TestNormalization();
	TestElapsedUnits();
	TestCalendarUnits();
	TestDstGapAndOverlap();
	TestMaxFencepost();
}

} // namespace Test

int main() {
	Test::TestDefaultScheduleTime();
	std::cout << "6 default schedule time test groups passed.\n";
	return 0;
}
