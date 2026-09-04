#include "data/data_contact_time_zone.h"

#include <algorithm>
#include <limits>

namespace Data {
namespace {

constexpr auto kMaximumOffsetHours = 14;

[[nodiscard]] bool IsAsciiLetter(QChar character) {
	const auto value = character.unicode();
	return ((value >= 'A') && (value <= 'Z'))
	       || ((value >= 'a') && (value <= 'z'));
}

[[nodiscard]] bool IsIanaCharacter(QChar character) {
	const auto value = character.unicode();
	return IsAsciiLetter(character) || ((value >= '0') && (value <= '9'))
	       || (value == '_') || (value == '-') || (value == '+')
	       || (value == '.');
}

[[nodiscard]] bool IsIanaIdentifier(QStringView payload) {
	auto componentStart = qsizetype(0);
	auto components = 0;
	for (auto index = qsizetype(0); index <= payload.size(); ++index) {
		if ((index != payload.size()) && (payload[index] != '/')) {
			continue;
		}
		const auto length = index - componentStart;
		if (length <= 0) {
			return false;
		}
		if (!IsAsciiLetter(payload[componentStart])) {
			return false;
		}
		for (auto part = componentStart; part != index; ++part) {
			if (!IsIanaCharacter(payload[part])) {
				return false;
			}
		}
		++components;
		componentStart = index + 1;
	}
	return components >= 2;
}

[[nodiscard]] std::optional<int> ParseOffset(QStringView payload) {
	if ((payload.size() < 3)
	        || ((payload.front() != '+') && (payload.front() != '-'))) {
		return std::nullopt;
	}
	const auto digit = [&](qsizetype index) -> std::optional<int> {
		if (index >= payload.size()) {
			return std::nullopt;
		}
		const auto value = payload[index].unicode();
		return ((value >= '0') && (value <= '9'))
		               ? std::make_optional(int(value - '0'))
		               : std::nullopt;
	};
	const auto hourTens = digit(1);
	const auto hourOnes = digit(2);
	if (!hourTens || !hourOnes) {
		return std::nullopt;
	}
	auto minutes = 0;
	if (payload.size() == 3) {
	} else if (payload.size() == 5) {
		const auto minuteTens = digit(3);
		const auto minuteOnes = digit(4);
		if (!minuteTens || !minuteOnes) {
			return std::nullopt;
		}
		minutes = (*minuteTens * 10) + *minuteOnes;
	} else if ((payload.size() == 4) && (payload[3] == ':')) {
	} else if ((payload.size() == 6) && (payload[3] == ':')) {
		const auto minuteTens = digit(4);
		const auto minuteOnes = digit(5);
		if (!minuteTens || !minuteOnes) {
			return std::nullopt;
		}
		minutes = (*minuteTens * 10) + *minuteOnes;
	} else {
		return std::nullopt;
	}
	const auto hours = (*hourTens * 10) + *hourOnes;
	if ((minutes >= 60) || (hours > kMaximumOffsetHours)
	        || ((hours == kMaximumOffsetHours) && (minutes != 0))) {
		return std::nullopt;
	}
	const auto sign = (payload.front() == '-') ? -1 : 1;
	return sign * ((hours * 60 * 60) + (minutes * 60));
}

[[nodiscard]] bool IsWhitespace(QStringView text) {
	for (const auto character : text) {
		if (!character.isSpace()) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] qsizetype VisibleBoundary(const QString &text,
        qsizetype markerPosition) {
	if ((markerPosition > 0) && (text[markerPosition - 1] == '\n')) {
		return ((markerPosition > 1)
		               && (text[markerPosition - 2] == '\r'))
		               ? markerPosition - 2
		               : markerPosition - 1;
	}
	if ((markerPosition > 0) && (text[markerPosition - 1] == '\r')) {
		return markerPosition - 1;
	}
	return markerPosition;
}

[[nodiscard]] TextWithEntities VisibleText(const TextWithEntities &note,
        qsizetype boundary) {
	auto result = TextWithEntities();
	result.text = note.text.left(boundary);
	if (boundary > std::numeric_limits<int>::max()) {
		return result;
	}
	const auto fullLength = note.text.size();
	if (fullLength > std::numeric_limits<int>::max()) {
		return result;
	}
	const auto visibleLength = int(boundary);
	for (const auto &entity : note.entities) {
		if (!entity.validForText(int(fullLength))
		        || (entity.offset() >= visibleLength)) {
			continue;
		}
		const auto length = std::min(entity.length(),
		        visibleLength - entity.offset());
		if (length > 0) {
			result.entities.push_back(EntityInText(entity.type(),
			        entity.offset(),
			        length,
			        entity.data()));
		}
	}
	return result;
}

} // namespace

std::optional<ContactTimeZoneSyntax> ParseContactTimeZoneSyntax(
        QStringView payload) {
	if ((payload.compare(u"Z", Qt::CaseInsensitive) == 0)
	        || (payload.compare(u"UTC", Qt::CaseInsensitive) == 0)
	        || (payload.compare(u"GMT", Qt::CaseInsensitive) == 0)) {
		return ContactTimeZoneSyntax{
		        .kind = ContactTimeZoneKind::FixedOffset,
		        .rawPayload = payload.toString(),
		        .fixedOffsetSeconds = 0,
		};
	}
	if (const auto offset = ParseOffset(payload)) {
		return ContactTimeZoneSyntax{
		        .kind = ContactTimeZoneKind::FixedOffset,
		        .rawPayload = payload.toString(),
		        .fixedOffsetSeconds = *offset,
		};
	}
	if (IsIanaIdentifier(payload)) {
		return ContactTimeZoneSyntax{
		        .kind = ContactTimeZoneKind::Named,
		        .rawPayload = payload.toString(),
		};
	}
	return std::nullopt;
}

std::optional<ContactTimeZone> ResolveContactTimeZone(
        const ContactTimeZoneSyntax &syntax) {
	if (syntax.kind == ContactTimeZoneKind::FixedOffset) {
		return ContactTimeZone{
		        .kind = ContactTimeZoneKind::FixedOffset,
		        .fixedOffsetSeconds = syntax.fixedOffsetSeconds,
		};
	}
	const auto zone = QTimeZone(syntax.rawPayload.toUtf8());
	if (!zone.isValid()) {
		return std::nullopt;
	}
	return ContactTimeZone{
	        .kind = ContactTimeZoneKind::Named,
	        .namedZone = zone,
	};
}

ParsedContactTimeZoneNote ParseContactTimeZoneNote(
        const TextWithEntities &note) {
	const auto marker = QChar(0x200B);
	auto markerPosition = note.text.lastIndexOf(marker);
	while (markerPosition >= 0) {
		auto lineEnd = markerPosition + 1;
		while ((lineEnd < note.text.size())
		        && (note.text[lineEnd] != '\r')
		        && (note.text[lineEnd] != '\n')) {
			++lineEnd;
		}
		const auto rawPayload
		        = QStringView(note.text).mid(markerPosition + 1,
		                lineEnd - markerPosition - 1);
		const auto syntax = ParseContactTimeZoneSyntax(rawPayload);
		if (syntax
		        && IsWhitespace(QStringView(note.text).mid(lineEnd))) {
			return ParsedContactTimeZoneNote{
			        .visible = VisibleText(note,
			                VisibleBoundary(note.text,
			                        markerPosition)),
			        .rawPayload = rawPayload.toString(),
			        .resolved = ResolveContactTimeZone(*syntax),
			};
		}
		if (markerPosition == 0) {
			break;
		}
		markerPosition
		        = note.text.lastIndexOf(marker, markerPosition - 1);
	}
	return ParsedContactTimeZoneNote{
	        .visible = VisibleText(note, note.text.size()),
	};
}

int ContactTimeZoneEditableLimit(int serverLimit,
        const std::optional<QString> &rawPayload,
        bool visibleNotePresent) {
	if (!rawPayload) {
		return std::max(serverLimit, 0);
	}
	const auto suffixLength
	        = qint64(1) + rawPayload->size() + (visibleNotePresent ? 1 : 0);
	return int(std::clamp(qint64(serverLimit) - suffixLength,
	        qint64(0),
	        qint64(std::numeric_limits<int>::max())));
}

ComposedContactTimeZoneNote ComposeContactTimeZoneNote(
        const TextWithEntities &visible,
        const std::optional<QString> &rawPayload,
        int serverLimit) {
	const auto editableLimit = ContactTimeZoneEditableLimit(serverLimit,
	        rawPayload,
	        !visible.text.isEmpty());
	if (rawPayload && !ParseContactTimeZoneSyntax(*rawPayload)) {
		return {
		        .editableLimit = editableLimit,
		        .error = ContactTimeZoneComposeError::InvalidPayload,
		};
	}
	if (visible.text.size() > std::numeric_limits<int>::max()) {
		return {
		        .editableLimit = editableLimit,
		        .error = ContactTimeZoneComposeError::InvalidEntity,
		};
	}
	for (const auto &entity : visible.entities) {
		if (!entity.validForText(int(visible.text.size()))) {
			return {
			        .editableLimit = editableLimit,
			        .error
			        = ContactTimeZoneComposeError::InvalidEntity,
			};
		}
	}
	const auto composedLength
	        = qint64(visible.text.size())
	          + (rawPayload ? qint64(1) + rawPayload->size() : 0)
	          + ((rawPayload && !visible.text.isEmpty()) ? 1 : 0);
	if ((serverLimit < 0) || (composedLength > serverLimit)) {
		return {
		        .editableLimit = editableLimit,
		        .error = ContactTimeZoneComposeError::TooLong,
		};
	}
	auto note = visible;
	if (rawPayload) {
		if (!note.text.isEmpty()) {
			note.text.append('\n');
		}
		note.text.append(QChar(0x200B)).append(*rawPayload);
	}
	return {
	        .note = std::move(note),
	        .editableLimit = editableLimit,
	};
}

QDateTime ContactTimeZoneDateTime(qint64 timestamp,
        const ContactTimeZone &zone) {
	return (zone.kind == ContactTimeZoneKind::Named)
	               ? QDateTime::fromSecsSinceEpoch(timestamp,
	                         zone.namedZone)
	               : QDateTime::fromSecsSinceEpoch(timestamp,
	                         QTimeZone::fromSecondsAheadOfUtc(
	                                 zone.fixedOffsetSeconds));
}

QString FormatContactTimeZoneTime(
        qint64 timestamp, const ContactTimeZone &zone, const QLocale &locale) {
	return locale.toString(ContactTimeZoneDateTime(timestamp, zone).time(),
	        QLocale::ShortFormat);
}

bool ContactTimeZonesEquivalent(const ContactTimeZone &peerZone,
        const QTimeZone &systemZone,
        QDateTime horizonStartUtc,
        QDateTime horizonEndUtc) {
	if (!systemZone.isValid() || !horizonStartUtc.isValid()
	        || !horizonEndUtc.isValid()
	        || (horizonStartUtc > horizonEndUtc)) {
		return false;
	}
	if (peerZone.kind == ContactTimeZoneKind::Named) {
		return peerZone.namedZone.isValid()
		       && (peerZone.namedZone.id() == systemZone.id());
	}
	const auto start = systemZone.offsetData(horizonStartUtc);
	const auto end = systemZone.offsetData(horizonEndUtc);
	return (start.offsetFromUtc == peerZone.fixedOffsetSeconds)
	       && (end.offsetFromUtc == peerZone.fixedOffsetSeconds)
	       && (start.daylightTimeOffset == 0)
	       && (end.daylightTimeOffset == 0)
	       && systemZone.transitions(horizonStartUtc, horizonEndUtc)
	                  .isEmpty();
}

} // namespace Data
