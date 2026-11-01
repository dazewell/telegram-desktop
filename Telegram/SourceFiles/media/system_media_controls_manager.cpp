/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "media/system_media_controls_manager.h"

#include "base/platform/base_platform_system_media_controls.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/version.h"
#include "data/data_document_media.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history_item_components.h"
#include "history/history_item_helpers.h"
#include "history/history_item.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "media/audio/media_audio.h"
#include "media/streaming/media_streaming_instance.h"
#include "media/streaming/media_streaming_player.h"
#include "media/system_media_controls_video.h"
#include "ui/text/format_song_document_name.h"

namespace Media {
namespace {

constexpr auto kSkipMs = crl::time(15 * 1000);

[[nodiscard]] auto RepeatModeToLoopStatus(Media::RepeatMode mode) {
	using Mode = Media::RepeatMode;
	using Status = base::Platform::SystemMediaControls::LoopStatus;
	switch (mode) {
	case Mode::None: return Status::None;
	case Mode::One: return Status::Track;
	case Mode::All: return Status::Playlist;
	}
	Unexpected("RepeatModeToLoopStatus in SystemMediaControlsManager");
}

} // namespace

struct SystemMediaControlsManager::PlayerState {
	struct Track {
		AudioMsgId started;
		std::optional<Player::TrackState> finished;
	};

	Track &track(AudioMsgId::Type type) {
		return (type == AudioMsgId::Type::Voice) ? voice : song;
	}
	const Track &track(AudioMsgId::Type type) const {
		return (type == AudioMsgId::Type::Voice) ? voice : song;
	}

	Track voice;
	Track song;
	std::optional<base::Platform::SystemMediaControls::PlaybackStatus> status;
	bool enabled = false;
};

auto SystemMediaControlsManager::activePlayerType() const
-> std::optional<AudioMsgId::Type> {
	using namespace Media::Player;
	const auto player = instance();
	using Type = AudioMsgId::Type;
	const auto allowed = [&](Type type) {
		return Core::App().settings().mediaKeysAllowed(type == Type::Voice)
			&& player->current(type);
	};
	for (const auto type : { Type::Voice, Type::Song }) {
		if (allowed(type)
			&& !IsStoppedOrStopping(player->getState(type).state)) {
			return type;
		}
	}
	for (const auto type : { Type::Voice, Type::Song }) {
		const auto &track = _playerState->track(type);
		if (allowed(type)
			&& track.started == player->current(type)
			&& track.finished) {
			return type;
		}
	}
	return std::nullopt;
}

Player::TrackState SystemMediaControlsManager::playerTrackState(
		AudioMsgId::Type type) const {
	const auto player = Player::instance();
	const auto &track = _playerState->track(type);
	return (track.finished && track.started == player->current(type))
		? *track.finished
		: player->getState(type);
}

bool SystemMediaControlsManager::Supported() {
	return base::Platform::SystemMediaControls::Supported();
}

float64 SystemMediaControlsManager::lookupPlaybackRate() const {
	const auto mediaPlayer = Media::Player::instance();
	const auto type = activePlayerType();
	const auto current = type ? mediaPlayer->current(*type) : AudioMsgId();
	if (!current || !current.changeablePlaybackSpeed()) {
		return 1.;
	}
	const auto document = current.audio();
	return (document
		&& !document->isVoiceMessage()
		&& !document->isVideoMessage())
		? Core::App().settings().audioPlaybackSpeed()
		: Core::App().settings().voicePlaybackSpeed();
}

void SystemMediaControlsManager::applyPlayerTrack(AudioMsgId::Type audioType) {
	const auto mediaPlayer = Media::Player::instance();
	const auto current = mediaPlayer->current(audioType);
	if (!current) {
		return;
	}
	if (_lastAudioMsgId == current) {
		return;
	}
	_lifetimeDownload.destroy();
	_streamed = nullptr;
	_cachedMediaView = nullptr;
	const auto document = current.audio();

	const auto &[title, performer] = (audioType
			== AudioMsgId::Type::Voice)
		? Ui::Text::FormatVoiceName(
			document,
			current.contextId()).composedName()
		: Ui::Text::FormatSongNameFor(document).composedName();
	_controls->setArtist(performer);
	_controls->setTitle(title);

	if (_controls->seekingSupported()) {
		const auto state = playerTrackState(audioType);
		_controls->setDuration(state.length);
		// macOS NowPlaying and Linux MPRIS update the track position
		// according to the rate property
		// while the playback status is "playing",
		// so we should change the track position only when
		// the track is changed
		// or when the position is changed by the user.
		_controls->setPosition(state.position);
		_controls->setPlaybackRate(lookupPlaybackRate());
	}
	_streamed = std::make_unique<Media::Streaming::Instance>(
		document,
		current.contextId(),
		nullptr);

	// Setting a thumbnail can take a long time,
	// so we need to update the display before that.
	_controls->updateDisplay();

	if (audioType == AudioMsgId::Type::Voice) {
		if (const auto item = document->owner().message(
				current.contextId())) {
			constexpr auto kUserpicSize = 50;
			const auto forwarded = item->Get<HistoryMessageForwarded>();
			const auto peer = (forwarded && forwarded->originalSender)
				? forwarded->originalSender
				: (!item->out() || item->isPost())
				? item->fromOriginal().get()
				: item->history()->peer.get();
			const auto userpic = PeerData::GenerateUserpicImage(
				peer,
				_cachedUserpicView,
				kUserpicSize,
				0);
			if (!userpic.isNull()) {
				_controls->setThumbnail(userpic);
			} else {
				peer->session().downloaderTaskFinished(
				) | rpl::on_next([=] {
					const auto userpic = PeerData::GenerateUserpicImage(
						peer,
						_cachedUserpicView,
						kUserpicSize,
						0);
					if (!userpic.isNull()) {
						_controls->setThumbnail(userpic);
						_lifetimeDownload.destroy();
					}
				}, _lifetimeDownload);
				_controls->clearThumbnail();
			}
		} else {
			_controls->clearThumbnail();
		}
	} else if (document && document->isSongWithCover()) {
		const auto view = document->createMediaView();
		view->thumbnailWanted(current.contextId());
		_cachedMediaView = view;
		if (const auto imagePtr = view->thumbnail()) {
			_controls->setThumbnail(imagePtr->original());
		} else {
			document->session().downloaderTaskFinished(
			) | rpl::on_next([=] {
				if (const auto imagePtr = view->thumbnail()) {
					_controls->setThumbnail(imagePtr->original());
					_lifetimeDownload.destroy();
				}
			}, _lifetimeDownload);
			_controls->clearThumbnail();
		}
	} else {
		_controls->clearThumbnail();
	}

	_lastAudioMsgId = current;
}

void SystemMediaControlsManager::clearPlayerTrack() {
	_lifetimeDownload.destroy();
	_cachedMediaView = nullptr;
	_streamed = nullptr;
	_lastAudioMsgId = AudioMsgId();
	_playerState->status.reset();
}

void SystemMediaControlsManager::updatePlayerPlaybackStatus(
		AudioMsgId::Type type,
		bool updatePosition) {
	using Status = base::Platform::SystemMediaControls::PlaybackStatus;
	using namespace Player;
	const auto state = playerTrackState(type);
	const auto status = IsStoppedOrStopping(state.state)
		? Status::Stopped
		: (IsPausedOrPausing(state.state)
			|| (_streamed && _streamed->valid()
				&& (_streamed->player().buffering()
					|| !_streamed->player().playing())))
		? Status::Paused
		: Status::Playing;
	if (_playerState->status == status) {
		return;
	}
	_playerState->status = status;
	if (updatePosition && _controls->seekingSupported()) {
		_controls->setPosition(state.position);
	}
	_controls->setPlaybackStatus(status);
	_controls->updateDisplay();
}

void SystemMediaControlsManager::updatePlayerState() {
	const auto type = activePlayerType();
	if (!type || Core::App().passcodeLocked()) {
		if (_playerState->enabled) {
			syncPlayerStateToControls();
		}
	} else if (!_playerState->enabled
		|| _lastAudioMsgId != Player::instance()->current(*type)) {
		syncPlayerStateToControls();
	} else {
		updatePlayerPlaybackStatus(*type);
	}
}

void SystemMediaControlsManager::syncPlayerStateToControls() {
	const auto mediaPlayer = Media::Player::instance();
	const auto type = activePlayerType();
	if (!type || Core::App().passcodeLocked()) {
		clearPlayerTrack();
		_playerState->enabled = false;
		_controls->setEnabled(false);
		_controls->clearMetadata();
		return;
	}
	_controls->setEnabled(true);
	_controls->setIsPlayPauseEnabled(true);
	_controls->setIsStopEnabled(true);
	_controls->setIsNextEnabled(mediaPlayer->nextAvailable(*type));
	_controls->setIsPreviousEnabled(mediaPlayer->previousAvailable(*type));
	const auto &settings = Core::App().settings();
	_controls->setLoopStatus(RepeatModeToLoopStatus(settings.playerRepeatMode()));
	_controls->setShuffle(settings.playerOrderMode() == OrderMode::Shuffle);
	if (_controls->volumeSupported()) {
		_controls->setVolume(settings.songVolume());
	}
	applyPlayerTrack(*type);
	_playerState->enabled = true;
	_playerState->status.reset();
	updatePlayerPlaybackStatus(*type, false);
}

bool SystemMediaControlsManager::videoActive() const {
	return _videoDelegate
		&& !Core::App().passcodeLocked()
		&& Core::App().settings().mediaKeysAllowed(_videoState.message);
}

void SystemMediaControlsManager::syncControls() {
	if (videoActive()) {
		applyVideoState();
	} else {
		syncPlayerStateToControls();
	}
}

SystemMediaControlsManager::SystemMediaControlsManager()
: _controls(std::make_unique<base::Platform::SystemMediaControls>())
, _playerState(std::make_unique<PlayerState>()) {

	using Command = base::Platform::SystemMediaControls::Command;

	_controls->setApplicationName(AppName.utf16());
	const auto inited = _controls->init();
	if (!inited) {
		LOG(("SystemMediaControlsManager failed to init."));
		return;
	}
	using TrackState = Media::Player::TrackState;
	const auto mediaPlayer = Media::Player::instance();

	auto trackFilter = rpl::filter([=](const TrackState &state) {
		const auto type = state.id.type();
		return (type == AudioMsgId::Type::Song)
			|| (type == AudioMsgId::Type::Voice);
	});

	mediaPlayer->updatedNotifier(
	) | trackFilter | rpl::on_next([=](const TrackState &state) {
		const auto type = state.id.type();
		auto &track = _playerState->track(type);
		if (track.started != state.id) {
			track = {};
		}
		if (!Player::IsStoppedOrStopping(state.state)) {
			track.started = state.id;
			track.finished.reset();
		} else if (state.state == Player::State::StoppedAtEnd
			&& track.started == state.id
			&& Core::App().settings().mediaKeysAllowed(
				type == AudioMsgId::Type::Voice)) {
			track.finished = state;
		}
		if (!videoActive()) {
			updatePlayerState();
		}
	}, _lifetime);

	for (const auto type : { AudioMsgId::Type::Voice, AudioMsgId::Type::Song }) {
		mediaPlayer->startsPlay(type) | rpl::on_next([=] {
			auto &track = _playerState->track(type);
			track.started = mediaPlayer->current(type);
			track.finished.reset();
			if (!videoActive()) {
				updatePlayerState();
			}
		}, _lifetime);

		// Drop session-owned objects on every explicit stop, including logout
		// and stops while an overlay owns the native controls.
		mediaPlayer->stops(type) | rpl::on_next([=] {
			_playerState->track(type) = {};
			clearPlayerTrack();
			syncControls();
		}, _lifetime);

		mediaPlayer->playlistChanges(type) | rpl::on_next([=] {
			if (Core::App().passcodeLocked() || videoActive()) {
				return;
			}
			const auto selected = activePlayerType();
			if (selected && *selected == type) {
				_controls->setIsNextEnabled(mediaPlayer->nextAvailable(type));
				_controls->setIsPreviousEnabled(
					mediaPlayer->previousAvailable(type));
			}
		}, _lifetime);
	}

	mediaPlayer->trackChanged() | rpl::on_next([=](AudioMsgId::Type type) {
		if (type != AudioMsgId::Type::Song && type != AudioMsgId::Type::Voice) {
			return;
		}
		auto &track = _playerState->track(type);
		if (track.started != mediaPlayer->current(type)) {
			track = {};
		}
		if (!videoActive()) {
			updatePlayerState();
		}
	}, _lifetime);

	rpl::merge(
		Core::App().settings().mediaKeysScopeValue() | rpl::to_empty,
		Core::App().passcodeLockChanges() | rpl::to_empty
	) | rpl::on_next([=] {
		for (const auto type : { AudioMsgId::Type::Voice, AudioMsgId::Type::Song }) {
			if (!Core::App().settings().mediaKeysAllowed(
					type == AudioMsgId::Type::Voice)) {
				_playerState->track(type) = {};
			}
		}
		clearPlayerTrack();
		syncControls();
	}, _lifetime);

	using Media::RepeatMode;
	using Media::OrderMode;

	Core::App().settings().playerRepeatModeValue(
	) | rpl::on_next([=](RepeatMode mode) {
		if (videoActive()) {
			return;
		}
		_controls->setLoopStatus(RepeatModeToLoopStatus(mode));
	}, _lifetime);

	Core::App().settings().playerOrderModeValue(
	) | rpl::on_next([=](OrderMode mode) {
		if (mode != OrderMode::Shuffle) {
			_lastOrderMode = mode;
		}
		if (videoActive()) {
			return;
		}
		_controls->setShuffle(mode == OrderMode::Shuffle);
	}, _lifetime);

	rpl::merge(
		Core::App().settings().voicePlaybackSpeedChanges() | rpl::to_empty,
		Core::App().settings().audioPlaybackSpeedChanges() | rpl::to_empty
	) | rpl::on_next([=] {
		if (Core::App().passcodeLocked()
			|| videoActive()
			|| !activePlayerType()) {
			return;
		}
		_controls->setPlaybackRate(lookupPlaybackRate());
	}, _lifetime);

	_controls->commandRequests(
	) | rpl::on_next([=](Command command) {
		if (Core::App().passcodeLocked()
			|| (!videoActive() && !activePlayerType())) {
			return;
		}
		if (videoActive()) {
			switch (command) {
			case Command::PlayPause: _videoDelegate->smtcPlayPause(); break;
			case Command::Play: _videoDelegate->smtcPlay(); break;
			case Command::Pause: _videoDelegate->smtcPause(); break;
			case Command::Stop: _videoDelegate->smtcStop(); break;
			case Command::Next: _videoDelegate->smtcNext(); break;
			case Command::Previous: _videoDelegate->smtcPrevious(); break;
			case Command::Raise: Core::App().activate(); break;
			case Command::Quit: _videoDelegate->smtcStop(); break;
			case Command::SkipForward:
			case Command::SkipBackward: {
				if (_videoState.duration > 0) {
					const auto delta = (command == Command::SkipForward)
						? kSkipMs
						: -kSkipMs;
					const auto position = std::clamp(
						crl::time(_videoState.position + delta),
						crl::time(0),
						_videoState.duration);
					_videoDelegate->smtcSeek(position);
				}
				break;
			}
			default: break;
			}
			return;
		}
		const auto type = *activePlayerType();
		switch (command) {
		case Command::PlayPause: mediaPlayer->playPause(type); break;
		case Command::Play: mediaPlayer->play(type); break;
		case Command::Pause: mediaPlayer->pause(type); break;
		case Command::Next: mediaPlayer->next(type); break;
		case Command::Previous: mediaPlayer->previous(type); break;
		case Command::Stop: mediaPlayer->stop(type); break;
		case Command::Raise: Core::App().activate(); break;
		case Command::LoopNone: {
			Core::App().settings().setPlayerRepeatMode(RepeatMode::None);
			Core::App().saveSettingsDelayed();
			break;
		}
		case Command::LoopTrack: {
			Core::App().settings().setPlayerRepeatMode(RepeatMode::One);
			Core::App().saveSettingsDelayed();
			break;
		}
		case Command::LoopPlaylist: {
			Core::App().settings().setPlayerRepeatMode(RepeatMode::All);
			Core::App().saveSettingsDelayed();
			break;
		}
		case Command::Shuffle: {
			const auto current = Core::App().settings().playerOrderMode();
			Core::App().settings().setPlayerOrderMode((current == OrderMode::Shuffle)
				? _lastOrderMode
				: OrderMode::Shuffle);
			Core::App().saveSettingsDelayed();
			break;
		}
		case Command::Quit: {
			Media::Player::instance()->stopAndClose();
			break;
		}
		case Command::SkipForward:
		case Command::SkipBackward: {
			const auto state = playerTrackState(type);
			if (state.length > 0) {
				const auto delta = (command == Command::SkipForward)
					? kSkipMs
					: -kSkipMs;
				const auto position = std::clamp(
					crl::time(state.position + delta),
					crl::time(0),
					crl::time(state.length));
				mediaPlayer->finishSeeking(
					type,
					position / float64(state.length));
			}
			break;
		}
		}
	}, _lifetime);

	if (_controls->seekingSupported()) {
		rpl::merge(
			mediaPlayer->seekingChanges(AudioMsgId::Type::Song)
				| rpl::filter([](Player::Instance::Seeking seeking) {
					return seeking == Player::Instance::Seeking::Finish;
				}) | rpl::map_to(AudioMsgId::Type::Song),
			mediaPlayer->seekingChanges(AudioMsgId::Type::Voice)
				| rpl::filter([](Player::Instance::Seeking seeking) {
					return seeking == Player::Instance::Seeking::Finish;
				}) | rpl::map_to(AudioMsgId::Type::Voice)
		) | rpl::on_next([=](AudioMsgId::Type type) {
			if (Core::App().passcodeLocked()
				|| videoActive()
				|| activePlayerType() != type) {
				return;
			}
			_controls->setPosition(playerTrackState(type).position);
			_controls->updateDisplay();
		}, _lifetime);

		_controls->seekRequests(
		) | rpl::on_next([=](float64 progress) {
			if (Core::App().passcodeLocked()
				|| (!videoActive() && !activePlayerType())) {
				return;
			}
			if (videoActive()) {
				_videoDelegate->smtcSeek(
					crl::time(progress * _videoState.duration));
				return;
			}
			const auto type = *activePlayerType();
			mediaPlayer->finishSeeking(type, progress);
		}, _lifetime);

		_controls->updatePositionRequests(
		) | rpl::on_next([=] {
			if (Core::App().passcodeLocked()
				|| (!videoActive() && !activePlayerType())) {
				return;
			}
			if (videoActive()) {
				_controls->setPosition(_videoState.position);
				return;
			}
			const auto type = *activePlayerType();
			_controls->setPosition(playerTrackState(type).position);
		}, _lifetime);
	}

	if (_controls->volumeSupported()) {
		rpl::single(
			Core::App().settings().songVolume()
		) | rpl::then(
			Core::App().settings().songVolumeChanges()
		) | rpl::on_next([=](float64 volume) {
			if (videoActive()) {
				return;
			}
			_controls->setVolume(volume);
		}, _lifetime);

		_controls->volumeChangeRequests(
		) | rpl::on_next([=](float64 volume) {
			if (Core::App().passcodeLocked()
				|| videoActive()
				|| !activePlayerType()) {
				return;
			}
			Player::mixer()->setSongVolume(volume);
			if (volume > 0) {
				Core::App().settings().setRememberedSongVolume(volume);
			}
			Core::App().settings().setSongVolume(volume);
		}, _lifetime);
	}

	_inited = true;
}

void SystemMediaControlsManager::videoStart(
		not_null<SystemMediaControlsVideoDelegate*> delegate,
		VideoState state) {
	if (!_inited) {
		return;
	}
	_videoDelegate = delegate;
	_videoState = state;
	_videoThumbnail = QImage();
	syncControls();
}

void SystemMediaControlsManager::applyVideoState() {
	using PlaybackStatus
		= base::Platform::SystemMediaControls::PlaybackStatus;
	clearPlayerTrack();
	_playerState->enabled = false;
	_controls->setEnabled(true);
	_controls->setIsPlayPauseEnabled(true);
	_controls->setIsStopEnabled(true);
	_controls->setIsNextEnabled(_videoState.nextAvailable);
	_controls->setIsPreviousEnabled(_videoState.previousAvailable);
	_controls->setTitle(_videoState.title);
	_controls->setArtist(_videoState.artist);
	if (_videoThumbnail.isNull()) {
		_controls->clearThumbnail();
	} else {
		_controls->setThumbnail(_videoThumbnail);
	}
	if (_controls->seekingSupported()) {
		_controls->setDuration(_videoState.duration);
		_controls->setPosition(_videoState.position);
		_controls->setPlaybackRate(
			Core::App().settings().videoPlaybackSpeed());
	}
	_controls->setPlaybackStatus(_videoState.playing
		? PlaybackStatus::Playing
		: PlaybackStatus::Paused);
	_controls->updateDisplay();
}

void SystemMediaControlsManager::videoUpdate(VideoState state) {
	if (!_inited || !_videoDelegate) {
		return;
	}
	const auto wasActive = videoActive();
	if (!wasActive || state.message != _videoState.message) {
		_videoState = state;
		if (wasActive || videoActive()) {
			_lastAudioMsgId = AudioMsgId();
			syncControls();
		}
		return;
	}
	using PlaybackStatus
		= base::Platform::SystemMediaControls::PlaybackStatus;
	auto display = false;
	if (state.title != _videoState.title) {
		_controls->setTitle(state.title);
		display = true;
	}
	if (state.artist != _videoState.artist) {
		_controls->setArtist(state.artist);
		display = true;
	}
	if (state.nextAvailable != _videoState.nextAvailable) {
		_controls->setIsNextEnabled(state.nextAvailable);
	}
	if (state.previousAvailable != _videoState.previousAvailable) {
		_controls->setIsPreviousEnabled(state.previousAvailable);
	}
	const auto statusChanged = (state.playing != _videoState.playing);
	if (_controls->seekingSupported()) {
		if (state.duration != _videoState.duration) {
			_controls->setDuration(state.duration);
		}
		if (statusChanged || !state.playing) {
			_controls->setPosition(state.position);
			_controls->setPlaybackRate(
				Core::App().settings().videoPlaybackSpeed());
		}
	}
	if (statusChanged) {
		_controls->setPlaybackStatus(state.playing
			? PlaybackStatus::Playing
			: PlaybackStatus::Paused);
	}
	if (display) {
		_controls->updateDisplay();
	}
	_videoState = state;
}

void SystemMediaControlsManager::videoSetThumbnail(const QImage &thumbnail) {
	if (!_inited || !_videoDelegate) {
		return;
	}
	_videoThumbnail = thumbnail;
	if (!videoActive()) {
		return;
	}
	if (thumbnail.isNull()) {
		_controls->clearThumbnail();
	} else {
		_controls->setThumbnail(thumbnail);
	}
	_controls->updateDisplay();
}

void SystemMediaControlsManager::videoFinish(
		not_null<SystemMediaControlsVideoDelegate*> delegate) {
	if (!_inited || (_videoDelegate != delegate)) {
		return;
	}
	_videoDelegate = nullptr;
	_videoState = VideoState();
	_videoThumbnail = QImage();
	clearPlayerTrack();
	syncControls();
}

SystemMediaControlsManager::~SystemMediaControlsManager() = default;

} // namespace Media
