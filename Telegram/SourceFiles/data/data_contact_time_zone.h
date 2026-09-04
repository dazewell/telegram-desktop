#pragma once

#include "ui/text/text_entity.h"

#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QTimeZone>

#include <optional>

namespace Data {

enum class ContactTimeZoneKind {
	FixedOffset,
	Named,
};

struct ContactTimeZoneSyntax {
	ContactTimeZoneKind kind = ContactTimeZoneKind::FixedOffset;
	QString rawPayload;
	int fixedOffsetSeconds = 0;
};

struct ContactTimeZone {
	ContactTimeZoneKind kind = ContactTimeZoneKind::FixedOffset;
	QTimeZone namedZone;
	int fixedOffsetSeconds = 0;
};

struct ParsedContactTimeZoneNote {
	TextWithEntities visible;
	std::optional<QString> rawPayload;
	std::optional<ContactTimeZone> resolved;
};

enum class ContactTimeZoneComposeError {
	None,
	InvalidPayload,
	InvalidEntity,
	TooLong,
};

struct ComposedContactTimeZoneNote {
	std::optional<TextWithEntities> note;
	int editableLimit = 0;
	ContactTimeZoneComposeError error = ContactTimeZoneComposeError::None;
};

[[nodiscard]] std::optional<ContactTimeZoneSyntax> ParseContactTimeZoneSyntax(
        QStringView payload);

[[nodiscard]] std::optional<ContactTimeZone> ResolveContactTimeZone(
        const ContactTimeZoneSyntax &syntax);

[[nodiscard]] ParsedContactTimeZoneNote ParseContactTimeZoneNote(
        const TextWithEntities &note);

[[nodiscard]] int ContactTimeZoneEditableLimit(int serverLimit,
        const std::optional<QString> &rawPayload,
        bool visibleNotePresent);

[[nodiscard]] ComposedContactTimeZoneNote ComposeContactTimeZoneNote(
        const TextWithEntities &visible,
        const std::optional<QString> &rawPayload,
        int serverLimit);

[[nodiscard]] QDateTime ContactTimeZoneDateTime(qint64 timestamp,
        const ContactTimeZone &zone);

[[nodiscard]] QString FormatContactTimeZoneTime(qint64 timestamp,
        const ContactTimeZone &zone,
        const QLocale &locale = QLocale());

[[nodiscard]] bool ContactTimeZonesEquivalent(const ContactTimeZone &peerZone,
        const QTimeZone &systemZone,
        QDateTime horizonStartUtc,
        QDateTime horizonEndUtc);

} // namespace Data
