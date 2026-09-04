#include "data/data_contact_time_zones.h"

#include "core/application.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "storage/storage_account.h"

#include <QtCore/QDateTime>
#include <QtCore/QTime>

namespace Data {
namespace {

constexpr auto kPrefKey = std::string_view("contact_time_zones");
constexpr auto kEquivalenceHorizonYears = 5;

} // namespace

bool ContactTimeZoneEligible(
		const PeerData *peer,
		ContactTimeZoneContext context) {
	if (!peer
		|| context.secret
		|| context.topic
		|| context.replies
		|| context.scheduled
		|| peer->isSelf()
		|| peer->isRepliesChat()) {
		return false;
	}
	return (peer->asUser() != nullptr);
}

ContactTimeZones::ContactTimeZones(not_null<Session*> owner)
: _owner(owner)
, _minuteTimer([=] {
	refreshCurrent();
	_globalChanges.fire({});
	rearmMinuteTimer();
}) {
	const auto serialized = _owner->session().local().readPref<QByteArray>(
		kPrefKey);
	if (const auto restored = DeserializeContactTimeZonePayloads(serialized)) {
		_payloads = *restored;
	}
	refreshAll();
	rearmMinuteTimer();

	Lang::Updated(
	) | rpl::on_next([=] {
		refreshAll();
		_globalChanges.fire({});
		rearmMinuteTimer();
	}, _lifetime);

	Core::App().systemTimeChanges(
	) | rpl::on_next([=] {
		refreshAll();
		_globalChanges.fire({});
		rearmMinuteTimer();
	}, _lifetime);
}

ContactTimeZones::~ContactTimeZones() = default;

void ContactTimeZones::applyAuthoritative(not_null<UserData*> user) {
	const auto userId = peerToUser(user->id);
	const auto update = ApplyContactTimeZoneNote(
		_payloads,
		_authoritativeKnown,
		userId.bare,
		user->note(),
		true);
	if (!update.payloadChanged && !update.authoritativeChanged) {
		return;
	}
	if (update.payloadChanged) {
		persist();
		refreshAll();
	}
	_userChanges.fire_copy(userId);
	if (update.payloadChanged) {
		rearmMinuteTimer();
	}
}

bool ContactTimeZones::authoritativeKnown(UserId userId) const {
	return _authoritativeKnown.contains(userId.bare);
}

const QString *ContactTimeZones::payload(UserId userId) const {
	const auto i = _payloads.find(userId.bare);
	return (i != end(_payloads)) ? &i->second : nullptr;
}

const ContactTimeZoneView *ContactTimeZones::lookup(
		const PeerData *peer,
		ContactTimeZoneContext context) const {
	if (!ContactTimeZoneEligible(peer, context)) {
		return nullptr;
	}
	const auto i = _views.find(peerToUser(peer->id));
	return (i != end(_views)) ? &i->second : nullptr;
}

const std::vector<QString> &ContactTimeZones::shortTimeSamples() const {
	return _shortTimeSamples;
}

rpl::producer<UserId> ContactTimeZones::userChanges() const {
	return _userChanges.events();
}

rpl::producer<> ContactTimeZones::globalChanges() const {
	return _globalChanges.events();
}

void ContactTimeZones::persist() {
	if (_payloads.empty()) {
		_owner->session().local().clearPref(kPrefKey);
	} else {
		_owner->session().local().writePref<QByteArray>(
			kPrefKey,
			SerializeContactTimeZonePayloads(_payloads));
	}
}

void ContactTimeZones::refreshAll() {
	_locale = QLocale();
	_systemZone = QTimeZone::systemTimeZone();
	rebuildSamples();
	_views.clear();
	const auto now = QDateTime::currentDateTimeUtc();
	const auto horizonStart = now.addYears(-kEquivalenceHorizonYears);
	const auto horizonEnd = now.addYears(kEquivalenceHorizonYears);
	for (const auto &[bareUserId, payload] : _payloads) {
		const auto zone = resolve(payload);
		if (!zone || ContactTimeZonesEquivalent(
			*zone,
			_systemZone,
			horizonStart,
			horizonEnd)) {
			continue;
		}
		_views.emplace(UserId(bareUserId), ContactTimeZoneView{
			.zone = zone,
		});
	}
	refreshCurrent();
}

void ContactTimeZones::refreshCurrent() {
	const auto now = QDateTime::currentSecsSinceEpoch();
	for (auto &[userId, view] : _views) {
		view.currentTime = FormatContactTimeZoneTime(
			now,
			*view.zone,
			_locale);
	}
}

void ContactTimeZones::rebuildSamples() {
	auto unique = base::flat_set<QString>();
	for (auto hour = 0; hour != 24; ++hour) {
		for (auto minute = 0; minute != 60; ++minute) {
			unique.emplace(_locale.toString(
				QTime(hour, minute),
				QLocale::ShortFormat));
		}
	}
	_shortTimeSamples.assign(unique.begin(), unique.end());
}

void ContactTimeZones::rearmMinuteTimer() {
	_minuteTimer.cancel();
	if (!_views.empty()) {
		_minuteTimer.callOnce(ContactTimeZoneMinuteDelay(
			QDateTime::currentMSecsSinceEpoch()));
	}
}

const ContactTimeZone *ContactTimeZones::resolve(const QString &payload) {
	const auto i = _resolved.find(payload);
	if (i != end(_resolved)) {
		return i->second ? &*i->second : nullptr;
	}
	const auto syntax = ParseContactTimeZoneSyntax(payload);
	const auto resolved = syntax
		? ResolveContactTimeZone(*syntax)
		: std::optional<ContactTimeZone>();
	const auto added = _resolved.emplace(payload, resolved).first;
	return added->second ? &*added->second : nullptr;
}

} // namespace Data