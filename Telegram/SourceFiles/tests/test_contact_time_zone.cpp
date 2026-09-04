#include "base/assertion.h"
#include "data/data_contact_time_zone.h"
#include "history/view/history_view_top_bar_layout.h"
#include "rpl/rpl.h"

#include <iostream>
#include <utility>
#include <vector>

namespace Test {
namespace {

[[nodiscard]] TextWithEntities Note(QString text) {
	return TextWithEntities{std::move(text), {}};
}

[[nodiscard]] QString Encoded(QStringView visible, QStringView payload) {
	return visible.isEmpty() ? QChar(0x200B) + payload.toString()
				 : visible.toString() + '\n' + QChar(0x200B)
			+ payload.toString();
}

void TestSyntax() {
	for (const auto payload : {
		     u"Z",
		     u"z",
		     u"UTC",
		     u"utc",
		     u"GMT",
		     u"gmt",
	     }) {
		const auto syntax = Data::ParseContactTimeZoneSyntax(payload);
		Expects(syntax.has_value());
		Expects(syntax->kind == Data::ContactTimeZoneKind::FixedOffset);
		Expects(syntax->fixedOffsetSeconds == 0);
	}
	const auto offsets = std::vector<std::pair<QStringView, int>>{
		{u"+05", 5 * 60 * 60},
		{u"-05", -5 * 60 * 60},
		{u"+0530", (5 * 60 * 60) + (30 * 60)},
		{u"-0530", -((5 * 60 * 60) + (30 * 60))},
		{u"+05:30", (5 * 60 * 60) + (30 * 60)},
		{u"-05:30", -((5 * 60 * 60) + (30 * 60))},
		{u"+05:", 5 * 60 * 60},
		{u"-05:", -5 * 60 * 60},
		{u"+14", 14 * 60 * 60},
		{u"-14", -14 * 60 * 60},
		{u"+1400", 14 * 60 * 60},
		{u"-1400", -14 * 60 * 60},
		{u"+14:00", 14 * 60 * 60},
		{u"-14:00", -14 * 60 * 60},
		{u"+14:", 14 * 60 * 60},
		{u"-14:", -14 * 60 * 60},
	};
	for (const auto &[payload, expected] : offsets) {
		const auto syntax = Data::ParseContactTimeZoneSyntax(payload);
		Expects(syntax.has_value());
		Expects(syntax->fixedOffsetSeconds == expected);
	}
	for (const auto payload : {
		     u"+1401",
		     u"-1401",
		     u"+1460",
		     u"+05:60",
		     u"+15",
		     u"-15:00",
		     u"+5",
		     u"+050",
		     u"+05:0",
		     u"+\u0660\u0665",
		     u"UTC ",
		     u"RandomWord",
		     u"Area//City",
		     u"Area/./City",
		     u"Area/../City",
		     u"Area/City/",
		     u"Area/Bad City",
		     u"Area/Bad\tCity",
		     u"Area/Bad\nCity",
	     }) {
		Expects(!Data::ParseContactTimeZoneSyntax(payload));
	}
	const auto named = Data::ParseContactTimeZoneSyntax(u"Europe/Berlin");
	Expects(named.has_value());
	Expects(named->kind == Data::ContactTimeZoneKind::Named);
	Expects(Data::ResolveContactTimeZone(*named).has_value());
	const auto future = Data::ParseContactTimeZoneSyntax(
		u"Future/Unresolvable_Long_Component");
	Expects(future.has_value());
	Expects(!Data::ResolveContactTimeZone(*future));
}

void TestParsing() {
	const auto missing = Data::ParseContactTimeZoneNote(Note(u"Visible"_q));
	Expects(missing.visible.text == u"Visible"_q);
	Expects(!missing.rawPayload);

	const auto markerOnly
		= Data::ParseContactTimeZoneNote(Note(Encoded({}, u"UTC")));
	Expects(markerOnly.visible.text.isEmpty());
	Expects(markerOnly.rawPayload == u"UTC"_q);
	Expects(markerOnly.resolved.has_value());
	const auto invalidMarkerOnly
		= Data::ParseContactTimeZoneNote(Note(u"\u200Bnot-a-zone"_q));
	Expects(invalidMarkerOnly.visible.text == u"\u200Bnot-a-zone"_q);
	Expects(!invalidMarkerOnly.rawPayload);

	const auto pasted = Data::ParseContactTimeZoneNote(
		Note(u"Pasted\u200Bword\n\u200BEurope/Berlin"_q));
	Expects(pasted.visible.text == u"Pasted\u200Bword"_q);
	Expects(pasted.rawPayload == u"Europe/Berlin"_q);
	const auto multiple = Data::ParseContactTimeZoneNote(
		Note(u"Visible\u200BUTC\n\u200BGMT"_q));
	Expects(multiple.visible.text == u"Visible\u200BUTC"_q);
	Expects(multiple.rawPayload == u"GMT"_q);

	const auto sameLine = Data::ParseContactTimeZoneNote(
		Note(u"Visible\u200Bpasted\u200B+05:\r\n\t "_q));
	Expects(sameLine.visible.text == u"Visible\u200Bpasted"_q);
	Expects(sameLine.rawPayload == u"+05:"_q);

	const auto invalidLast = Data::ParseContactTimeZoneNote(
		Note(u"Visible\n\u200BUTC\nordinary \u200B marker"_q));
	Expects(invalidLast.visible.text
		== u"Visible\n\u200BUTC\nordinary \u200B marker"_q);
	Expects(!invalidLast.rawPayload);

	const auto whitespace = Data::ParseContactTimeZoneNote(
		Note(u"Visible\r\n\u200BgMt\r\n \t"_q));
	Expects(whitespace.visible.text == u"Visible"_q);
	Expects(whitespace.rawPayload == u"gMt"_q);

	const auto unresolved = Data::ParseContactTimeZoneNote(Note(
		Encoded(u"Visible", u"Future/Unresolvable_Long_Component")));
	Expects(unresolved.visible.text == u"Visible"_q);
	Expects(unresolved.rawPayload
		== u"Future/Unresolvable_Long_Component"_q);
	Expects(!unresolved.resolved);

	const auto verbatim = Data::ParseContactTimeZoneNote(
		Note(Encoded(u"Visible", u"eTc/GmT+5")));
	Expects(verbatim.rawPayload == u"eTc/GmT+5"_q);
	const auto recomposed
		= Data::ComposeContactTimeZoneNote(verbatim.visible,
			verbatim.rawPayload,
			128);
	Expects(recomposed.note.has_value());
	Expects(recomposed.note->text == Encoded(u"Visible", u"eTc/GmT+5"));
}

void TestEntities() {
	auto full = Note(Encoded(u"Hello", u"UTC"));
	full.entities = {
		EntityInText(EntityType::Bold, 0, 9),
		EntityInText(EntityType::Italic, 5, 1),
		EntityInText(EntityType::Code, -1, 2),
		EntityInText(EntityType::Underline, 0, 1000),
	};
	const auto parsed = Data::ParseContactTimeZoneNote(full);
	Expects(parsed.visible.text == u"Hello"_q);
	Expects(parsed.visible.entities.size() == 1);
	Expects(parsed.visible.entities.front().type() == EntityType::Bold);
	Expects(parsed.visible.entities.front().offset() == 0);
	Expects(parsed.visible.entities.front().length() == 5);

	auto utf16 = Note(Encoded(u"A\U0001F600B", u"UTC"));
	utf16.entities = {
		EntityInText(EntityType::Bold, 1, 5),
	};
	const auto utf16Parsed = Data::ParseContactTimeZoneNote(utf16);
	Expects(utf16Parsed.visible.text.size() == 4);
	Expects(utf16Parsed.visible.entities.size() == 1);
	Expects(utf16Parsed.visible.entities.front().offset() == 1);
	Expects(utf16Parsed.visible.entities.front().length() == 3);

	auto malformed = Note(u"Plain"_q);
	malformed.entities = {
		EntityInText(EntityType::Bold, -1, 2),
		EntityInText(EntityType::Italic, 1, -2),
		EntityInText(EntityType::Code, 6, 1),
		EntityInText(EntityType::Underline, 4, 2),
	};
	const auto cleaned = Data::ParseContactTimeZoneNote(malformed);
	Expects(cleaned.visible.text == u"Plain"_q);
	Expects(cleaned.visible.entities.isEmpty());
}

void TestComposition() {
	const auto visible
		= Note(QString(121, 'a') + QString::fromUcs4(U"\U0001F600"));
	Expects(visible.text.size() == 123);
	const auto exact = Data::ComposeContactTimeZoneNote(visible,
		std::make_optional(u"UTC"_q),
		128);
	Expects(exact.note.has_value());
	Expects(exact.note->text.size() == 128);
	Expects(exact.editableLimit == 123);
	Expects(exact.note->text.endsWith(u"\n\u200BUTC"_q));
	Expects(exact.note->entities.isEmpty());

	const auto over
		= Data::ComposeContactTimeZoneNote(Note(QString(124, 'a')),
			std::make_optional(u"UTC"_q),
			128);
	Expects(!over.note);
	Expects(over.error == Data::ContactTimeZoneComposeError::TooLong);

	const auto noMarker
		= Data::ComposeContactTimeZoneNote(Note(QString(128, 'a')),
			std::nullopt,
			128);
	Expects(noMarker.note.has_value());
	Expects(noMarker.editableLimit == 128);

	const auto invalidPayload
		= Data::ComposeContactTimeZoneNote(Note(u"Visible"_q),
			std::make_optional(u"not-a-zone"_q),
			128);
	Expects(!invalidPayload.note);
	Expects(invalidPayload.error
		== Data::ContactTimeZoneComposeError::InvalidPayload);

	const auto emptyVisible
		= Data::ComposeContactTimeZoneNote(Note(QString()),
			std::make_optional(u"+05:"_q),
			128);
	Expects(emptyVisible.note.has_value());
	Expects(emptyVisible.note->text == Encoded({}, u"+05:"));
	Expects(emptyVisible.editableLimit == 123);

	const auto unresolvedPayload
		= std::make_optional(u"Future/Unresolvable_Long_Component"_q);
	const auto unresolvedEdit
		= Data::ComposeContactTimeZoneNote(Note(u"Changed"_q),
			unresolvedPayload,
			128);
	Expects(unresolvedEdit.note.has_value());
	Expects(unresolvedEdit.note->text
		== Encoded(u"Changed", u"Future/Unresolvable_Long_Component"));
	const auto unresolvedDelete
		= Data::ComposeContactTimeZoneNote(Note(QString()),
			unresolvedPayload,
			128);
	Expects(unresolvedDelete.note.has_value());
	Expects(unresolvedDelete.note->text
		== Encoded({}, u"Future/Unresolvable_Long_Component"));

	auto malformed = Note(u"Text"_q);
	malformed.entities.push_back(EntityInText(EntityType::Bold, -1, 2));
	const auto malformedResult = Data::ComposeContactTimeZoneNote(malformed,
		std::make_optional(u"UTC"_q),
		128);
	Expects(!malformedResult.note);
	Expects(malformedResult.error
		== Data::ContactTimeZoneComposeError::InvalidEntity);
}

void TestConversion() {
	const auto berlin = Data::ResolveContactTimeZone(
		*Data::ParseContactTimeZoneSyntax(u"Europe/Berlin"));
	Expects(berlin.has_value());
	const auto winter = QDateTime(QDate(2024, 1, 15), QTime(12, 0), Qt::UTC)
				    .toSecsSinceEpoch();
	const auto summer = QDateTime(QDate(2024, 7, 15), QTime(12, 0), Qt::UTC)
				    .toSecsSinceEpoch();
	Expects(Data::ContactTimeZoneDateTime(winter, *berlin).time()
		== QTime(13, 0));
	Expects(Data::ContactTimeZoneDateTime(summer, *berlin).time()
		== QTime(14, 0));

	const auto fixed = Data::ResolveContactTimeZone(
		*Data::ParseContactTimeZoneSyntax(u"+05:30"));
	Expects(fixed.has_value());
	Expects(Data::ContactTimeZoneDateTime(winter, *fixed).time()
		== QTime(17, 30));
	Expects(Data::ContactTimeZoneDateTime(summer, *fixed).time()
		== QTime(17, 30));
	const auto locale = QLocale(QLocale::English, QLocale::UnitedKingdom);
	Expects(Data::FormatContactTimeZoneTime(winter, *fixed, locale)
		== locale.toString(QTime(17, 30), QLocale::ShortFormat));

	const auto start = QDateTime(QDate(2020, 1, 1), QTime(), Qt::UTC);
	const auto end = QDateTime(QDate(2030, 1, 1), QTime(), Qt::UTC);
	Expects(Data::ContactTimeZonesEquivalent(*fixed,
		QTimeZone::fromSecondsAheadOfUtc((5 * 60 * 60) + (30 * 60)),
		start,
		end));
	Expects(!Data::ContactTimeZonesEquivalent(*fixed,
		QTimeZone::fromSecondsAheadOfUtc(5 * 60 * 60),
		start,
		end));
	Expects(Data::ContactTimeZonesEquivalent(*berlin,
		QTimeZone("Europe/Berlin"),
		start,
		end));
	Expects(!Data::ContactTimeZonesEquivalent(
		Data::ContactTimeZone{
			.kind = Data::ContactTimeZoneKind::FixedOffset,
			.fixedOffsetSeconds = 60 * 60,
		},
		QTimeZone("Europe/Berlin"),
		start,
		end));
}

void TestPersistenceAndState() {
	auto firstAccount = Data::ContactTimeZonePayloads{
		{42, u"Europe/Berlin"_q},
		{100, u"Future/Unresolvable_Long_Component"_q},
	};
	const auto serialized
		= Data::SerializeContactTimeZonePayloads(firstAccount);
	const auto restored
		= Data::DeserializeContactTimeZonePayloads(serialized);
	Expects(restored.has_value());
	Expects(*restored == firstAccount);

	auto secondAccount = Data::ContactTimeZonePayloads{
		{42, u"+05:30"_q},
	};
	const auto secondRestored = Data::DeserializeContactTimeZonePayloads(
		Data::SerializeContactTimeZonePayloads(secondAccount));
	Expects(secondRestored.has_value());
	Expects(secondRestored->find(42)->second == u"+05:30"_q);
	Expects(restored->find(42)->second == u"Europe/Berlin"_q);

	auto truncated = serialized;
	truncated.chop(1);
	Expects(!Data::DeserializeContactTimeZonePayloads(truncated));
	auto trailing = serialized;
	trailing.append('x');
	Expects(!Data::DeserializeContactTimeZonePayloads(trailing));
	auto wrongVersion = serialized;
	wrongVersion[7] = 2;
	Expects(!Data::DeserializeContactTimeZonePayloads(wrongVersion));
	const auto malformed = Data::SerializeContactTimeZonePayloads({
		{42, u"not-a-zone"_q},
	});
	Expects(!Data::DeserializeContactTimeZonePayloads(malformed));
	Expects(!Data::DeserializeContactTimeZonePayloads(QByteArray()));

	auto provisional = firstAccount;
	auto authoritative = base::flat_set<uint64>();
	const auto ignored = Data::ApplyContactTimeZoneNote(provisional,
		authoritative,
		42,
		Note(u"Ordinary note"_q),
		false);
	Expects(!ignored.payloadChanged);
	Expects(!ignored.authoritativeChanged);
	Expects(provisional.find(42)->second == u"Europe/Berlin"_q);
	const auto removed = Data::ApplyContactTimeZoneNote(provisional,
		authoritative,
		42,
		Note(u"Ordinary note"_q),
		true);
	Expects(removed.payloadChanged);
	Expects(removed.authoritativeChanged);
	Expects(!provisional.contains(42));
	const auto replaced = Data::ApplyContactTimeZoneNote(provisional,
		authoritative,
		100,
		Note(Encoded({}, u"Future/Still_Unresolvable")),
		true);
	Expects(replaced.payloadChanged);
	Expects(replaced.authoritativeChanged);
	Expects(provisional.find(100)->second
		== u"Future/Still_Unresolvable"_q);

	auto samePayload = Data::ContactTimeZonePayloads{
		{7, u"UTC"_q},
	};
	auto sameAuthoritative = base::flat_set<uint64>();
	const auto promoted = Data::ApplyContactTimeZoneNote(samePayload,
		sameAuthoritative,
		7,
		Note(Encoded({}, u"UTC")),
		true);
	Expects(!promoted.payloadChanged);
	Expects(promoted.authoritativeChanged);
	const auto repeated = Data::ApplyContactTimeZoneNote(samePayload,
		sameAuthoritative,
		7,
		Note(Encoded({}, u"UTC")),
		true);
	Expects(!repeated.payloadChanged);
	Expects(!repeated.authoritativeChanged);
}

void TestMinuteBoundary() {
	Expects(Data::ContactTimeZoneMinuteDelay(0) == 60050);
	Expects(Data::ContactTimeZoneMinuteDelay(1) == 60049);
	Expects(Data::ContactTimeZoneMinuteDelay(59999) == 51);
	Expects(Data::ContactTimeZoneMinuteDelay(60000) == 60050);
	Expects(Data::ContactTimeZoneMinuteDelay(-1) == 51);
}

void TestTopBarLayout() {
	const auto titleLine = QRect(20, 11, 300, 19);
	const auto titleBaselineFromTop = 15;
	const auto chipTopPadding = 2;
	const auto chipFontAscent = 8;
	const auto chipTextBaselineFromTop = chipTopPadding + chipFontAscent;
	const auto shortNoBadge = HistoryView::ComputeTopBarTimeZoneLayout(
		titleLine,
		80,
		19,
		0,
		6,
		50,
		13,
		titleBaselineFromTop,
		chipTextBaselineFromTop);
	Expects(shortNoBadge.titleWidth == 80);
	Expects(shortNoBadge.chipRect.x() == 106);

	const auto shortBadge = HistoryView::ComputeTopBarTimeZoneLayout(
		titleLine,
		80,
		19,
		18,
		6,
		50,
		13,
		titleBaselineFromTop,
		chipTextBaselineFromTop);
	Expects(shortBadge.titleWidth == 80);
	Expects(shortBadge.nameBadgeRect.width() == 98);
	Expects(shortBadge.chipRect.x() == 124);

	const auto longBadge = HistoryView::ComputeTopBarTimeZoneLayout(
		titleLine,
		400,
		19,
		18,
		6,
		50,
		13,
		titleBaselineFromTop,
		chipTextBaselineFromTop);
	Expects(longBadge.titleWidth == 226);
	Expects(longBadge.chipRect.x() == 270);
	Expects(longBadge.chipRect.right() == titleLine.right());

	const auto insufficient = HistoryView::ComputeTopBarTimeZoneLayout(
		QRect(20, 11, 92, 19),
		80,
		19,
		18,
		6,
		50,
		13,
		titleBaselineFromTop,
		chipTextBaselineFromTop);
	Expects(!insufficient);

	Expects(shortBadge.chipRect.y() + chipTextBaselineFromTop
		== titleLine.y() + titleBaselineFromTop);

	const auto invalidBaseline = HistoryView::ComputeTopBarTimeZoneLayout(
		titleLine,
		80,
		19,
		18,
		6,
		50,
		13,
		titleBaselineFromTop,
		14);
	Expects(!invalidBaseline);
}

} // namespace

void TestContactTimeZone() {
	TestSyntax();
	TestParsing();
	TestEntities();
	TestComposition();
	TestConversion();
	TestPersistenceAndState();
	TestMinuteBoundary();
	TestTopBarLayout();
}

} // namespace Test

namespace crl {

rpl::producer<> on_main_update_requests() { return rpl::never<>(); }

} // namespace crl

int main() {
	Test::TestContactTimeZone();
	std::cout << "8 contact time-zone test groups passed.\n";
	return 0;
}
