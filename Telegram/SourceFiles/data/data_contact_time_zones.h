#pragma once

#include "base/flat_map.h"
#include "base/flat_set.h"
#include "base/timer.h"
#include "data/data_contact_time_zone.h"
#include "data/data_peer_id.h"
#include "rpl/event_stream.h"
#include "rpl/lifetime.h"

#include <map>
#include <vector>

class PeerData;
class UserData;

namespace Data {

class Session;

struct ContactTimeZoneContext {
	bool secret = false;
	bool topic = false;
	bool replies = false;
	bool scheduled = false;
};

struct ContactTimeZoneView {
	const ContactTimeZone *zone = nullptr;
	QString currentTime;
};

[[nodiscard]] bool ContactTimeZoneEligible(const PeerData *peer,
	ContactTimeZoneContext context = {});

class ContactTimeZones final {
public:
	explicit ContactTimeZones(not_null<Session*> owner);
	~ContactTimeZones();

	void applyAuthoritative(not_null<UserData*> user);

	[[nodiscard]] bool authoritativeKnown(UserId userId) const;
	[[nodiscard]] const QString *payload(UserId userId) const;
	[[nodiscard]] const ContactTimeZoneView *lookup(
		const PeerData *peer,
		ContactTimeZoneContext context = {}) const;
	[[nodiscard]] const std::vector<QString> &shortTimeSamples() const;

	[[nodiscard]] rpl::producer<UserId> userChanges() const;
	[[nodiscard]] rpl::producer<> globalChanges() const;

private:
	void persist();
	void refreshAll();
	void refreshCurrent();
	void rebuildSamples();
	void rearmMinuteTimer();
	[[nodiscard]] const ContactTimeZone *resolve(const QString &payload);

	const not_null<Session*> _owner;
	ContactTimeZonePayloads _payloads;
	base::flat_set<uint64> _authoritativeKnown;
	std::map<QString, std::optional<ContactTimeZone>> _resolved;
	base::flat_map<UserId, ContactTimeZoneView> _views;
	std::vector<QString> _shortTimeSamples;
	QLocale _locale;
	QTimeZone _systemZone;
	base::Timer _minuteTimer;
	rpl::event_stream<UserId> _userChanges;
	rpl::event_stream<> _globalChanges;
	rpl::lifetime _lifetime;

};

} // namespace Data