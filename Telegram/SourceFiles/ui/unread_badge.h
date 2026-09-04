/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/text/text_custom_emoji.h"
#include "ui/rp_widget.h"

namespace style {
struct VerifiedBadge;
} // namespace style

namespace Ui {

class UnreadBadge : public RpWidget {
public:
	using RpWidget::RpWidget;

	void setText(const QString &text, bool active);
	int textBaseline() const;

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	QString _text;
	bool _active = false;

};

struct BotVerifyDetails {
	UserId botId = 0;
	DocumentId iconId = 0;
	TextWithEntities description;

	explicit operator bool() const {
		return iconId != 0;
	}
	friend inline bool operator==(
		const BotVerifyDetails &,
		const BotVerifyDetails &) = default;
};

enum class TextBadgeType : uchar {
	Scam,
	Fake,
	Direct,
};

class PeerBadge {
public:
	PeerBadge();
	~PeerBadge();

	class Layout {
	public:
		[[nodiscard]] int width() const {
			return _width;
		}

	private:
		friend class PeerBadge;

		TextBadgeType _textBadge = TextBadgeType::Scam;
		int _width = 0;
		bool _paintText = false;
		bool _paintVerify = false;
		bool _paintEmoji = false;
		bool _paintStar = false;

	};

	struct Descriptor {
		not_null<PeerData*> peer;
		QRect rectForName;
		int nameWidth = 0;
		int outerWidth = 0;
		const style::icon *verified = nullptr;
		const style::icon *premium = nullptr;
		const style::color *scam = nullptr;
		const style::color *direct = nullptr;
		const style::color *premiumFg = nullptr;
		Fn<void()> customEmojiRepaint;
		crl::time now = 0;
		bool prioritizeVerification = false;
		bool bothVerifyAndStatus = false;
		bool paused = false;
	};
	[[nodiscard]] Layout layout(const Descriptor &descriptor);
	int draw(Painter &p, Descriptor &&descriptor, const Layout &layout);
	int drawGetWidth(Painter &p, Descriptor &&descriptor);
	[[nodiscard]] QRect emojiStatusRect() const;
	void paintEmojiStatusFrame(QPainter &p, crl::time now, bool paused);
	void paintEmojiStatusFrame(
		QPainter &p,
		crl::time now,
		bool paused,
		QPoint position);
	void unload();

	[[nodiscard]] bool ready(const BotVerifyDetails *details) const;
	void set(
		not_null<const BotVerifyDetails*> details,
		Text::CustomEmojiFactory factory,
		Fn<void()> repaint);

	// How much horizontal space the badge took.
	int drawVerified(
		QPainter &p,
		QPoint position,
		const style::VerifiedBadge &st);

private:
	struct EmojiStatus;
	struct BotVerifiedData;

	[[nodiscard]] bool preparePremiumEmojiStatus(
		const Descriptor &descriptor);
	void drawTextBadge(
		Painter &p,
		const Descriptor &descriptor,
		TextBadgeType type);
	void drawVerifyCheck(Painter &p, const Descriptor &descriptor);
	void drawPremiumEmojiStatus(Painter &p, const Descriptor &descriptor);
	void drawPremiumStar(Painter &p, const Descriptor &descriptor);

	std::unique_ptr<EmojiStatus> _emojiStatus;
	mutable std::unique_ptr<BotVerifiedData> _botVerifiedData;

};

QSize TextBadgeSize(TextBadgeType type);
void DrawTextBadge(
	Painter &p,
	QRect rect,
	int outerWidth,
	const style::color &color,
	const QString &phrase,
	int phraseWidth,
	bool mirror = false);
void DrawTextBadge(
	TextBadgeType,
	Painter &p,
	QRect rect,
	int outerWidth,
	const style::color &color);

} // namespace Ui
