#pragma once

#include "base/flat_map.h"
#include "base/flat_set.h"
#include "ui/text/text_entity.h"

#include <QtCore/QByteArray>
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

struct ContactTimeZoneContext {
	bool secret = false;
	bool topic = false;
	bool replies = false;
	bool scheduled = false;
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

using ContactTimeZonePayloads = base::flat_map<uint64, QString>;

struct ContactTimeZoneNoteUpdate {
	bool payloadChanged = false;
	bool authoritativeChanged = false;
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

[[nodiscard]] QDateTime ContactTimeZoneDateTime(
	qint64 timestamp,
	const ContactTimeZone &zone);

[[nodiscard]] QString FormatContactTimeZoneTime(
	qint64 timestamp,
	const ContactTimeZone &zone,
	const QLocale &locale = QLocale());

[[nodiscard]] bool ContactTimeZoneContextEligible(
	bool personalUser,
	bool self,
	bool repliesPeer,
	ContactTimeZoneContext context = {});

[[nodiscard]] QString FormatContactTimeZoneHistoricalMetadata(
	QString localText,
	TimeId timestamp,
	const ContactTimeZone *zone,
	const QLocale &locale = QLocale());

[[nodiscard]] bool ContactTimeZonesEquivalent(
	const ContactTimeZone &peerZone,
	const QTimeZone &systemZone,
	QDateTime horizonStartUtc,
	QDateTime horizonEndUtc);

[[nodiscard]] QByteArray SerializeContactTimeZonePayloads(
	const ContactTimeZonePayloads &payloads);

[[nodiscard]] std::optional<ContactTimeZonePayloads>
DeserializeContactTimeZonePayloads(const QByteArray &serialized);

[[nodiscard]] ContactTimeZoneNoteUpdate ApplyContactTimeZoneNote(
	ContactTimeZonePayloads &payloads,
	base::flat_set<uint64> &authoritativeKnown,
	uint64 userId,
	const TextWithEntities &note,
	bool authoritativeLoaded);

[[nodiscard]] qint64 ContactTimeZoneMinuteDelay(qint64 nowMs);

} // namespace Data
