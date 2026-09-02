/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "ui/effects/animations.h"
#include "ui/layers/box_content.h"
#include "base/bytes.h"
#include "base/timer.h"

namespace base {
enum class SystemUnlockResult;
} // namespace base

namespace Ui {
class PasswordInput;
class LinkButton;
class RoundButton;
class CheckView;
} // namespace Ui

namespace Main {
class Session;
} // namespace Main

namespace Window {

class Controller;
class SlideAnimation;

enum class PasscodeAttempt : uchar {
	Empty,
	Flood,
	Wrong,
	Correct,
};

// Bumps the bad tries counters on a wrong passcode. Does not unlock:
// Core::App().unlockPasscode() destroys the main lock screen widget, so
// the caller runs it as its own last statement.
[[nodiscard]] PasscodeAttempt TryPasscode(const QString &passcode);

// Quick unlock submits the passcode automatically once it has been fully
// typed. Shared by every unlock surface so they can't drift.
//
// Returns true only when quick unlock is enabled, the local passcode length is
// known, and the text just *grew* to that length: deleting a character back
// down to it must not spend a passcode attempt. `lastLength` holds the caller's
// per-field state and is updated in place on every call.
//
// The caller must defer the actual submit (InvokeQueued or similar): unlocking
// destroys the field that emitted the change signal.
[[nodiscard]] bool QuickUnlockTriggered(int &lastLength, int nowLength);

// Performs a quick unlock attempt and unlocks on success. Returns true if it
// unlocked, in which case the calling widget may already be destroyed.
//
// A failed attempt is deliberately silent and leaves the field untouched. The
// stored length can be stale - a passcode set by an older build was never
// recorded - so reaching it does not guarantee a match. Showing an error and
// selecting the text, as the manual submit does, would replace what the user
// typed on the next keystroke.
//
// The attempt still bumps the bad tries counter on failure. That is deliberate:
// skipping it would let quick unlock brute-force passcodes without ever
// tripping the flood limit. A successful unlock resets the counter anyway, so
// at most one attempt per entry is spent.
bool TryQuickUnlock(const QString &passcode);

class LockWidget : public Ui::RpWidget {
public:
	LockWidget(QWidget *parent, not_null<Controller*> window);
	~LockWidget();

	[[nodiscard]] not_null<Controller*> window() const;

	virtual void setInnerFocus();

	void showAnimated(QPixmap oldContentCache);
	void showFinished();

protected:
	void paintEvent(QPaintEvent *e) override;
	virtual void paintContent(QPainter &p);

private:
	const not_null<Controller*> _window;
	std::unique_ptr<SlideAnimation> _showAnimation;

};

class PasscodeLockWidget : public LockWidget {
public:
	PasscodeLockWidget(QWidget *parent, not_null<Controller*> window);

	void setInnerFocus() override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	enum class SystemUnlockType : uchar {
		None,
		Default,
		Biometrics,
		Companion,
	};

	void paintContent(QPainter &p) override;

	void setupSystemUnlockInfo();
	void setupSystemUnlock();
	void suggestSystemUnlock();
	void systemUnlockDone(base::SystemUnlockResult result);
	void changed();
	void checkQuickUnlock();
	void submit();
	void error();

	rpl::variable<SystemUnlockType> _systemUnlockAvailable;
	rpl::variable<SystemUnlockType> _systemUnlockAllowed;
	object_ptr<Ui::PasswordInput> _passcode;
	object_ptr<Ui::RoundButton> _submit;
	object_ptr<Ui::LinkButton> _logout;
	QString _error;
	int _quickUnlockLength = 0;

	rpl::lifetime _systemUnlockSuggested;
	base::Timer _systemUnlockCooldown;

};

struct TermsLock {
	bytes::vector id;
	TextWithEntities text;
	std::optional<int> minAge;
	bool popup = false;

	inline bool operator==(const TermsLock &other) const {
		return (id == other.id);
	}
	inline bool operator!=(const TermsLock &other) const {
		return !(*this == other);
	}

	static TermsLock FromMTP(
		Main::Session *session,
		const MTPDhelp_termsOfService &data);

};

class TermsBox : public Ui::BoxContent {
public:
	TermsBox(
		QWidget*,
		const TermsLock &data,
		rpl::producer<QString> agree,
		rpl::producer<QString> cancel);
	TermsBox(
		QWidget*,
		const TextWithEntities &text,
		rpl::producer<QString> agree,
		rpl::producer<QString> cancel,
		bool attentionAgree = false);

	rpl::producer<> agreeClicks() const;
	rpl::producer<> cancelClicks() const;
	QString lastClickedMention() const;

protected:
	void prepare() override;

	void keyPressEvent(QKeyEvent *e) override;

private:
	TermsLock _data;
	rpl::producer<QString> _agree;
	rpl::producer<QString> _cancel;
	rpl::event_stream<> _agreeClicks;
	rpl::event_stream<> _cancelClicks;
	QString _lastClickedMention;
	bool _attentionAgree = false;

	bool _ageErrorShown = false;
	Ui::Animations::Simple _ageErrorAnimation;

};

} // namespace Window
