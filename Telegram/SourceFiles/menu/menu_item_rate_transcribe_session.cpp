/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "menu/menu_item_rate_transcribe_session.h"

#include "api/api_transcribes.h"
#include "apiwrap.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"

namespace Menu {

Fn<void(bool)> RateTranscribeCallbackFactory(
		not_null<HistoryItem*> item) {
	return [item](bool good) {
		item->history()->peer->session().api().transcribes().rate(
			item,
			good);
	};
}

bool HasRateTranscribeItem(not_null<HistoryItem*> item) {
	const auto &peer = item->history()->peer;
	if (!peer->session().api().transcribes().entry(
			item).result.isEmpty()) {
		return !peer->session().api().transcribes().isRated(item);
	}
	return false;
}

} // namespace Menu
