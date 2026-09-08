/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tests/test_main.h"

#include "base/invoke_queued.h"
#include "base/integration.h"
#include "chat_helpers/selected_text_cite.h"
#include "chat_helpers/selected_text_action.h"
#include "chat_helpers/message_field_tag_policy.h"
#include "core/shortcuts_contextual.h"
#include "ui/effects/animations.h"
#include "ui/ui_utility.h"
#include "ui/style/style_core.h"
#include "ui/text/text.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/rp_window.h"
#include "ui/painter.h"
#include "styles/style_widgets.h"

#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QDialog>
#include <QMenu>
#include <QTextEdit>

#include <algorithm>
#include <memory>
#include <span>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Test {

namespace {

[[nodiscard]] bool HasEntityType(
		const EntitiesInText &entities,
		EntityType type) {
	for (const auto &entity : entities) {
		if (entity.type() == type) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] QImage MakeObjectImage(
		QSize size,
		QColor background,
		const QString &label) {
	auto image = QImage(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	auto painter = QPainter(&image);
	auto hq = PainterHighQualityEnabler(painter);
	painter.setPen(Qt::NoPen);
	painter.setBrush(background);
	painter.drawRoundedRect(
		QRect(QPoint(), size),
		scale(6),
		scale(6));
	painter.setPen(Qt::black);
	painter.drawText(QRect(QPoint(), size), Qt::AlignCenter, label);
	return image;
}

[[nodiscard]] int RenderTextPadding() {
	return scale(6);
}

[[nodiscard]] QImage RenderTextOffscreen(
		const Ui::Text::String &text,
		int availableWidth,
		std::optional<TextSelection> selection = std::nullopt,
		std::span<Ui::Text::SpecialColor> colors = {},
		std::optional<QPen> pen = std::nullopt) {
	const auto padding = RenderTextPadding();
	const auto image = QImage(
		QSize(
			std::max(text.maxWidth(), availableWidth) + (2 * padding),
			std::max(text.countHeight(availableWidth), text.minHeight())
				+ (2 * padding)),
		QImage::Format_ARGB32_Premultiplied);
	auto result = image;
	result.fill(Qt::transparent);
	auto painter = QPainter(&result);
	if (pen) {
		painter.setPen(*pen);
	}
	text.draw(painter, {
		.position = QPoint(padding, padding),
		.availableWidth = availableWidth,
		.colors = colors,
		.selection = selection.value_or(TextSelection()),
	});
	return result;
}

[[nodiscard]] std::optional<QRect> ChangedBoundsInRect(
		const QImage &first,
		const QImage &second,
		QRect rect) {
	if (first.size() != second.size()) {
		return std::nullopt;
	}
	rect = rect.intersected(QRect(QPoint(), first.size()));
	if (rect.isEmpty()) {
		return std::nullopt;
	}
	auto left = rect.right();
	auto top = rect.bottom();
	auto right = rect.left() - 1;
	auto bottom = rect.top() - 1;
	for (auto y = rect.top(); y <= rect.bottom(); ++y) {
		for (auto x = rect.left(); x <= rect.right(); ++x) {
			if (first.pixel(x, y) == second.pixel(x, y)) {
				continue;
			}
			left = std::min(left, x);
			top = std::min(top, y);
			right = std::max(right, x);
			bottom = std::max(bottom, y);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

[[nodiscard]] std::optional<QRect> SymbolHitBounds(
		const Ui::Text::String &text,
		int availableWidth,
		int offset) {
	if ((availableWidth <= 0) || (offset < 0)) {
		return std::nullopt;
	}
	const auto padding = RenderTextPadding();
	const auto height = std::max(
		text.countHeight(availableWidth),
		text.minHeight());
	auto flags = Ui::Text::StateRequest::Flags();
	flags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	auto request = Ui::Text::StateRequest();
	request.flags = flags;
	auto left = padding + availableWidth;
	auto top = padding + height;
	auto right = -1;
	auto bottom = -1;
	for (auto y = 0; y != height; ++y) {
		for (auto x = 0; x != availableWidth; ++x) {
			const auto hit = text.getState(QPoint(x, y), availableWidth, request);
			if (!hit.uponSymbol || (int(hit.symbol) != offset)) {
				continue;
			}
			left = std::min(left, x + padding);
			top = std::min(top, y + padding);
			right = std::max(right, x + padding);
			bottom = std::max(bottom, y + padding);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

[[nodiscard]] std::optional<QRect> SymbolRangeHitBounds(
		const Ui::Text::String &text,
		int availableWidth,
		int offset,
		int length) {
	if ((availableWidth <= 0) || (offset < 0) || (length <= 0)) {
		return std::nullopt;
	}
	const auto padding = RenderTextPadding();
	const auto height = std::max(
		text.countHeight(availableWidth),
		text.minHeight());
	auto flags = Ui::Text::StateRequest::Flags();
	flags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	auto request = Ui::Text::StateRequest();
	request.flags = flags;
	auto left = padding + availableWidth;
	auto top = padding + height;
	auto right = -1;
	auto bottom = -1;
	const auto end = offset + length;
	for (auto y = 0; y != height; ++y) {
		for (auto x = 0; x != availableWidth; ++x) {
			const auto hit = text.getState(QPoint(x, y), availableWidth, request);
			if (!hit.uponSymbol) {
				continue;
			}
			const auto symbol = int(hit.symbol);
			if ((symbol < offset) || (symbol >= end)) {
				continue;
			}
			left = std::min(left, x + padding);
			top = std::min(top, y + padding);
			right = std::max(right, x + padding);
			bottom = std::max(bottom, y + padding);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

[[nodiscard]] bool ImagesEqualInRect(
		const QImage &first,
		const QImage &second,
		QRect rect) {
	return !ChangedBoundsInRect(first, second, rect).has_value();
}

[[nodiscard]] bool HasPixelColorInRect(
		const QImage &image,
		QRect rect,
		QRgb color) {
	rect = rect.intersected(QRect(QPoint(), image.size()));
	if (rect.isEmpty()) {
		return false;
	}
	for (auto y = rect.top(); y <= rect.bottom(); ++y) {
		for (auto x = rect.left(); x <= rect.right(); ++x) {
			if (image.pixel(x, y) == color) {
				return true;
			}
		}
	}
	return false;
}

[[nodiscard]] bool HasPaintedPixels(const QImage &image) {
	const auto bits = image.constBits();
	if (!bits) {
		return false;
	}
	const auto count = image.sizeInBytes();
	for (auto i = qsizetype(0); i != count; ++i) {
		if (bits[i] != 0) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] std::optional<QPoint> FirstPaintedPoint(
		const QImage &image,
		QRect rect = QRect()) {
	if (rect.isNull()) {
		rect = QRect(QPoint(), image.size());
	}
	rect = rect.intersected(QRect(QPoint(), image.size()));
	if (rect.isEmpty()) {
		return std::nullopt;
	}
	const auto background = image.pixel(rect.topLeft());
	for (auto y = rect.top(); y <= rect.bottom(); ++y) {
		for (auto x = rect.left(); x <= rect.right(); ++x) {
			if (image.pixel(x, y) != background) {
				return QPoint(x, y);
			}
		}
	}
	return std::nullopt;
}

[[nodiscard]] QRect ScaleRect(QRect rect, qreal ratio) {
	return QRect(
		QPoint(
			qRound(rect.x() * ratio),
			qRound(rect.y() * ratio)),
		QSize(
			qRound(rect.width() * ratio),
			qRound(rect.height() * ratio)));
}

class FormulaLikeObject final : public Ui::Text::CustomEmoji {
public:
	FormulaLikeObject(QString entityData, QString replacementText, QImage image);

	int width() override;
	QString entityData() override;
	std::optional<Ui::Text::CustomEmojiVerticalMetrics> vertical(
		const style::TextStyle &) override;
	QString replacementText() override;
	Ui::Text::CustomEmojiSemantics semantics() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	QString _entityData;
	QString _replacementText;
	QImage _image;

};

FormulaLikeObject::FormulaLikeObject(
	QString entityData,
	QString replacementText,
	QImage image)
: _entityData(std::move(entityData))
, _replacementText(std::move(replacementText))
, _image(std::move(image)) {
}

int FormulaLikeObject::width() {
	return _image.width();
}

QString FormulaLikeObject::entityData() {
	return _entityData;
}

std::optional<Ui::Text::CustomEmojiVerticalMetrics>
FormulaLikeObject::vertical(const style::TextStyle &) {
	const auto height = _image.height();
	const auto descent = std::max(height / 5, 1);
	return Ui::Text::CustomEmojiVerticalMetrics{
		.ascent = height - descent,
		.descent = descent,
	};
}

QString FormulaLikeObject::replacementText() {
	return _replacementText;
}

Ui::Text::CustomEmojiSemantics FormulaLikeObject::semantics() {
	return {
		.isEmoji = false,
		.isRealCustomEmoji = false,
		.exportEntity = false,
		.unloadPersistentAnimation = false,
		.allowCustomEmojiClick = false,
	};
}

void FormulaLikeObject::paint(QPainter &p, const Context &context) {
	p.drawImage(context.position, _image);
}

void FormulaLikeObject::unload() {
}

bool FormulaLikeObject::ready() {
	return true;
}

bool FormulaLikeObject::readyInDefaultState() {
	return true;
}

} // namespace

QString name() {
	return u"text"_q;
}

int selectedTextShortcutsTest() {
	const auto arguments = App::arguments();
	const auto reportOption = arguments.indexOf(
		u"--selected-text-shortcuts-report"_q);
	if (reportOption < 0 || reportOption + 1 >= arguments.size()) {
		return 2;
	}
	auto output = QFile(arguments[reportOption + 1]);
	if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return 2;
	}
	auto report = QString();
	auto failed = 0;
	const auto flush = [&] {
		output.write(report.toUtf8());
		output.flush();
		report.clear();
	};
	const auto check = [&](bool passed, const QString &name) {
		report += (passed ? u"PASS "_q : u"FAIL "_q) + name + '\n';
		if (!passed) {
			++failed;
		}
		flush();
	};
	const auto validate = [](QStringView tag) { return tag.toString(); };
	report += u"SUITE contextual-dispatch\n"_q;
	{
		using C = Shortcuts::Command;
		auto window = QWidget();
		auto list = QWidget(&window);
		auto composer = QTextEdit(&window);
		list.setFocusPolicy(Qt::StrongFocus);
		window.show();
		App::setActiveWindow(&window);
		list.setFocus();
		auto dispatcher = Shortcuts::ContextualDispatcher();
		auto bindings = base::flat_map<QKeySequence, C>{ { QKeySequence(u"C"_q), C::CiteSelectedText } };
		auto selection = 0;
		auto executions = 0;
		auto paused = false;
		auto rejected = false;
		const auto dispatch = [&](QObject *target, QEvent *event) {
			return dispatcher.handle(target, event, paused,
				[&](const QKeySequence &keys) {
					const auto found = bindings.find(keys);
					return found == bindings.end() ? std::vector<C>() : std::vector<C>{ found->second };
				}, [&](not_null<Shortcuts::ContextualRequest*> request) {
					if (request->owner == &list && selection == 1) {
						request->execute = [&] {
							if (rejected) return false;
							++executions;
							composer.setFocus();
							return true;
						};
					}
				});
		};
		const auto key = [&](QEvent::Type type, Qt::Key code = Qt::Key_C,
				Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
			auto event = QKeyEvent(type, code, modifiers);
			return dispatch(App::focusWidget(), &event);
		};
		check(!key(QEvent::ShortcutOverride), u"no selection untouched"_q);
		selection = 2;
		check(!key(QEvent::ShortcutOverride), u"message selection untouched"_q);
		selection = 1;
		check(key(QEvent::ShortcutOverride) && executions == 0, u"override query side effect free"_q);
		check(key(QEvent::KeyPress) && executions == 1, u"eligible key executes once"_q);
		check(key(QEvent::KeyRelease), u"release consumed in composer"_q);
		composer.setPlainText(u"composer selection"_q);
		composer.selectAll();
		check(!key(QEvent::ShortcutOverride), u"composer selection untouched"_q);
		list.setFocus();
		rejected = true;
		check(key(QEvent::KeyPress) && executions == 1,
			u"eligible rejection cannot leak trigger into draft"_q);
		check(key(QEvent::KeyRelease), u"rejected trigger release consumed"_q);
		rejected = false;
		paused = true;
		check(!key(QEvent::ShortcutOverride), u"recorder pause untouched"_q);
		paused = false;
		bindings.clear();
		check(!key(QEvent::ShortcutOverride), u"removed binding untouched"_q);
		bindings.emplace(QKeySequence(u"Ctrl+T"_q), C::TranslateSelectedText);
		check(!key(QEvent::ShortcutOverride), u"old remapped key untouched"_q);
		check(key(QEvent::ShortcutOverride, Qt::Key_T, Qt::ControlModifier), u"remapped key matched"_q);
		auto preedit = QInputMethodEvent(u"composition"_q, {});
		dispatch(&list, &preedit);
		check(!key(QEvent::ShortcutOverride, Qt::Key_T, Qt::ControlModifier), u"IME preedit untouched"_q);
		auto commit = QInputMethodEvent();
		dispatch(&list, &commit);
		check(key(QEvent::ShortcutOverride, Qt::Key_T, Qt::ControlModifier), u"IME commit releases eligibility"_q);
		auto popup = QMenu(&window);
		popup.addAction(u"menu"_q);
		popup.popup(QPoint(20, 20));
		auto override = QKeyEvent(QEvent::ShortcutOverride, Qt::Key_T, Qt::ControlModifier);
		check(!dispatch(&list, &override), u"popup blocks background selection"_q);
		popup.close();
		auto modal = QDialog(&window);
		modal.setModal(true);
		modal.show();
		check(!dispatch(&list, &override), u"modal blocks background selection"_q);
		modal.close();
		App::setActiveWindow(&window);
		list.setFocus();
		check(key(QEvent::KeyPress, Qt::Key_T, Qt::ControlModifier),
			u"dialog trigger accepted"_q);
		auto deactivate = QEvent(QEvent::WindowDeactivate);
		dispatch(&window, &deactivate);
		modal.show();
		App::setActiveWindow(&modal);
		auto repeat = QKeyEvent(
			QEvent::KeyPress,
			Qt::Key_T,
			Qt::ControlModifier,
			u"t"_q,
			true);
		auto syntheticRelease = QKeyEvent(
			QEvent::KeyRelease,
			Qt::Key_T,
			Qt::ControlModifier,
			{},
			true);
		auto release = QKeyEvent(
			QEvent::KeyRelease,
			Qt::Key_T,
			Qt::ControlModifier);
		check(dispatch(&modal, &repeat), u"dialog repeat suppressed"_q);
		check(dispatch(&modal, &syntheticRelease),
			u"dialog synthetic release suppressed"_q);
		check(dispatch(&modal, &repeat),
			u"dialog synthetic release retains latch"_q);
		check(dispatch(&modal, &release), u"dialog physical release consumed"_q);
		check(!dispatch(&modal, &override),
			u"dialog new press remains ineligible"_q);
		modal.close();
		App::setActiveWindow(&window);
		list.setFocus();
		check(key(QEvent::KeyPress, Qt::Key_T, Qt::ControlModifier),
			u"next physical trigger accepted"_q);
		auto applicationDeactivate = QEvent(QEvent::ApplicationDeactivate);
		dispatch(&window, &applicationDeactivate);
		check(!dispatch(&composer, &repeat), u"application deactivation clears latch"_q);
		check(!dispatch(&composer, &release), u"deactivated release untouched"_q);
		App::setActiveWindow(&window);
		list.setFocus();
		bindings.clear();
		bindings.emplace(QKeySequence(u"Ctrl+Tab"_q), C::QuoteSelectedText);
		selection = 0;
		check(!key(QEvent::ShortcutOverride, Qt::Key_Tab, Qt::ControlModifier),
			u"off-context CtrlTab override untouched"_q);
		check(!key(QEvent::KeyPress, Qt::Key_Tab, Qt::ControlModifier),
			u"off-context CtrlTab press untouched"_q);
		selection = 1;
#ifdef Q_OS_WIN
		const auto shifted = [&](Qt::Key code,
				Qt::Key baseCode,
				quint32 scanCode,
				quint32 virtualKey,
				const QString &name) {
			const auto handle = reinterpret_cast<HWND>(window.winId());
			PostMessageW(handle, WM_KEYDOWN, virtualKey, 1 | (scanCode << 16));
			PostMessageW(handle, WM_KEYUP, virtualKey, 0xC0000001 | (scanCode << 16));
			App::processEvents();
			bindings.clear();
			bindings.emplace(
				QKeySequence(Qt::ShiftModifier | baseCode),
				C::CiteSelectedText);
			list.setFocus();
			const auto before = executions;
			auto shiftedOverride = QKeyEvent(
				QEvent::ShortcutOverride,
				code,
				Qt::ShiftModifier,
				scanCode,
				virtualKey,
				1);
			auto shiftedPress = QKeyEvent(
				QEvent::KeyPress,
				code,
				Qt::ShiftModifier,
				scanCode,
				virtualKey,
				1);
			const auto candidates = QKeyMapper::possibleKeys(&shiftedPress);
			check(std::ranges::any_of(candidates, [&](const auto candidate) {
				return QKeySequence(candidate)
					== QKeySequence(Qt::ShiftModifier | baseCode);
			}), name + u" active-layout recorder candidate available"_q);
			check(QKeySequence(shiftedPress.keyCombination())
				!= QKeySequence(Qt::ShiftModifier | baseCode),
				name + u" raw-only lookup cannot match recorded binding"_q);
			check(dispatch(&list, &shiftedOverride) && executions == before,
				name + u" normalized override"_q);
			check(dispatch(&list, &shiftedPress) && executions == before + 1,
				name + u" normalized press"_q);
			auto shiftRelease = QKeyEvent(
				QEvent::KeyRelease,
				Qt::Key_Shift,
				Qt::NoModifier,
				0x2A,
				0x10,
				0);
			check(!dispatch(&composer, &shiftRelease),
				name + u" modifier release untouched"_q);
			auto unshiftedRepeat = QKeyEvent(
				QEvent::KeyPress,
				baseCode,
				Qt::NoModifier,
				scanCode,
				virtualKey,
				0,
				{},
				true);
			auto unshiftedRelease = QKeyEvent(
				QEvent::KeyRelease,
				baseCode,
				Qt::NoModifier,
				scanCode,
				virtualKey,
				0);
			check(dispatch(&composer, &unshiftedRepeat)
				&& executions == before + 1,
				name + u" held key survives modifier release"_q);
			check(dispatch(&composer, &unshiftedRelease),
				name + u" physical release clears latch"_q);
			check(!dispatch(&composer, &unshiftedRepeat),
				name + u" released key no longer latched"_q);
		};
		shifted(Qt::Key_Backtab, Qt::Key_Tab, 0x0F, 0x09, u"ShiftTab"_q);
		shifted(Qt::Key_Exclam, Qt::Key_1, 0x02, 0x31, u"Shift1"_q);
#endif
		window.hide();
		check(!dispatch(&list, &override), u"hidden source untouched"_q);
	}
	report += u"SUITE contextual-hidden-parent\n"_q;
	{
		using C = Shortcuts::Command;
		auto window = QWidget();
		auto parent = QWidget(&window);
		auto owner = QWidget(&parent);
		auto other = QWidget(&window);
		owner.setFocusPolicy(Qt::StrongFocus);
		other.setFocusPolicy(Qt::StrongFocus);
		window.show();
		App::setActiveWindow(&window);
		owner.setFocus();
		auto dispatcher = Shortcuts::ContextualDispatcher();
		auto enabled = false;
		auto executions = 0;
		const auto dispatch = [&](QEvent::Type type) {
			auto event = QKeyEvent(type, Qt::Key_C, Qt::NoModifier);
			return dispatcher.handle(
				&owner,
				&event,
				false,
				[&](const QKeySequence &keys) {
					return (enabled && keys == QKeySequence(u"C"_q))
						? std::vector<C>{ C::CiteSelectedText }
						: std::vector<C>();
				},
				[&](not_null<Shortcuts::ContextualRequest*> request) {
					if (request->owner == &owner) {
						request->execute = [&] {
							++executions;
							return true;
						};
					}
				});
		};
		check(owner.isVisible() && owner.hasFocus(),
			u"nested dispatch owner initially visible and focused"_q);
		check(!dispatch(QEvent::ShortcutOverride),
			u"nested dispatch disabled override untouched"_q);
		check(!dispatch(QEvent::KeyPress) && executions == 0,
			u"nested dispatch disabled press untouched"_q);
		enabled = true;
		check(dispatch(QEvent::ShortcutOverride) && executions == 0,
			u"nested dispatch shown owner eligible without latching"_q);
		parent.hide();
		const auto focusAfterHide = App::focusWidget();
		report += u"OBSERVED hidden-parent focus moved=%1 fallback=%2\n"_q.arg(
			focusAfterHide != &owner).arg(focusAfterHide == &other);
		check(!owner.isHidden() && !owner.isVisible(),
			u"nested dispatch child not explicitly hidden"_q);
		check(window.isActiveWindow() && !window.isMinimized(),
			u"nested dispatch top window remains active"_q);
		check(!dispatch(QEvent::ShortcutOverride),
			u"hidden-parent direct override rejected"_q);
		check(!dispatch(QEvent::KeyPress) && executions == 0,
			u"hidden-parent direct press cannot execute"_q);
		check(App::focusWidget() == focusAfterHide,
			u"hidden-parent rejected dispatch preserves focus"_q);
	}
	report += u"SUITE destination-tag-policy\n"_q;
	{
		const auto filter = [](QStringView tag) {
			return ChatHelpers::ValidateMessageFieldTags(tag, 7,
				[](QStringView data) { return data == u"123"; }, nullptr);
		};
		const auto emojiTag = Ui::InputField::CustomEmojiLink(u"123"_q);
		check(ChatHelpers::ValidateMessageFieldTags(
			emojiTag,
			7,
			nullptr,
			nullptr).isEmpty(), u"empty allow callback removes custom emoji"_q);
		check(ChatHelpers::ValidateMessageFieldTags(
			Ui::InputField::kTagBold,
			7,
			nullptr,
			nullptr) == Ui::InputField::kTagBold,
			u"empty allow callback preserves ordinary tags"_q);
		check(ChatHelpers::ValidateMessageFieldTags(
			emojiTag,
			7,
			nullptr,
			[](QStringView) { return true; }) == emojiTag,
			u"keep callback retains emoji with empty allow callback"_q);
		check(ChatHelpers::ValidateMessageFieldTags(
			emojiTag,
			7,
			nullptr,
			[](QStringView) { return false; }).isEmpty(),
			u"false keep callback denies emoji with empty allow callback"_q);
		check(filter(emojiTag) == emojiTag, u"allowed custom emoji retained"_q);
		check(filter(Ui::InputField::CustomEmojiLink(u"456"_q)).isEmpty(),
			u"destination rejects custom emoji"_q);
		check(filter(Ui::InputField::kTagBold) == Ui::InputField::kTagBold,
			u"supported inline formatting retained"_q);
		const auto mention = [](uint64 selfId) {
			return TextUtilities::ConvertEntitiesToTextTags({ EntityInText(
				EntityType::MentionName, 0, 1,
				TextUtilities::MentionNameDataFromFields({ selfId, 1, 2 })) }).front().id;
		};
		const auto own = mention(7);
		const auto foreign = mention(8);
		check(filter(own) == own, u"same-account mention retained"_q);
		check(filter(foreign).isEmpty(), u"foreign-account mention removed"_q);
		const auto draft = TextWithTags{ u"prior"_q, { { 0, 5, foreign } } };
		const auto prepared = ChatHelpers::PrepareSelectedTextCite(
			draft, { u"new"_q, { { 0, 3, foreign } } }, filter);
		check(prepared.tags.front() == draft.tags.front(),
			u"prior draft tags never revalidated"_q);
		check(std::ranges::none_of(prepared.tags, [&](const auto &tag) {
			return tag.offset >= 5 && tag.id.contains(foreign);
		}), u"only incoming foreign mention removed"_q);
	}
	report += u"SUITE held-key\n"_q;
	{
		auto window = QWidget();
		auto owner = std::make_unique<QWidget>(&window);
		auto other = QWidget(&window);
		window.show();
		App::setActiveWindow(&window);
		auto latch = Shortcuts::ContextualKeyLatch();
		auto press = QKeyEvent(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier);
		auto repeat = QKeyEvent(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, u"c"_q, true);
		auto syntheticRelease = QKeyEvent(QEvent::KeyRelease, Qt::Key_C, Qt::NoModifier, {}, true);
		auto release = QKeyEvent(QEvent::KeyRelease, Qt::Key_C, Qt::NoModifier);
		latch.start(owner.get(), press);
		other.setFocus();
		check(latch.consume(repeat), u"repeat suppressed after focus transfer"_q);
		check(latch.consume(syntheticRelease), u"synthetic release suppressed"_q);
		check(latch.consume(repeat), u"synthetic release retains latch"_q);
		check(latch.consume(release), u"physical release consumed"_q);
		check(!latch.consume(press), u"next physical press available"_q);
		latch.start(owner.get(), press);
		owner.reset();
		check(!latch.consume(repeat), u"destroyed key owner clears latch"_q);
	}
	report += u"SUITE selection-lifetime\n"_q;
	{
		auto owner = std::make_unique<QWidget>();
		owner->show();
		auto generation = uint64(0);
		auto cleared = 0;
		const auto make = [&] {
			return ChatHelpers::MakeSelectedTextAction(owner.get(), generation,
				[] { return true; }, [&] { ++cleared; ++generation; }, true);
		};
		auto first = make();
		check(first.isValid(), u"current source valid"_q);
		first.markPending();
		check(cleared == 0, u"pending retains selection"_q);
		auto newer = make();
		first.accept();
		check(cleared == 0, u"superseded invocation cannot clear"_q);
		newer.accept();
		check(cleared == 1, u"accepted clears once"_q);
		newer.accept();
		check(cleared == 1, u"duplicate acceptance cannot clear"_q);
		auto changed = make();
		++generation;
		changed.accept();
		check(cleared == 1, u"changed selection cannot clear"_q);
		auto destroyed = make();
		owner.reset();
		check(!destroyed.isValid(), u"destroyed owner invalid"_q);
	}
	report += u"SUITE selection-hidden-parent\n"_q;
	{
		auto window = QWidget();
		auto parent = QWidget(&window);
		auto owner = QWidget(&parent);
		auto other = QWidget(&window);
		owner.setFocusPolicy(Qt::StrongFocus);
		other.setFocusPolicy(Qt::StrongFocus);
		window.show();
		App::setActiveWindow(&window);
		owner.setFocus();
		auto generation = uint64(0);
		auto cleared = 0;
		const auto action = ChatHelpers::MakeSelectedTextAction(
			&owner,
			generation,
			[] { return true; },
			[&] { ++cleared; },
			true);
		check(action.isValid() && owner.hasFocus(),
			u"nested selection initially valid and focused"_q);
		action.markPending();
		const auto pendingGeneration = generation;
		parent.hide();
		other.setFocus();
		check(!owner.isHidden() && !owner.isVisible(),
			u"nested selection child not explicitly hidden"_q);
		check(generation == pendingGeneration,
			u"ancestor visibility independent of generation"_q);
		check(!action.isValid(), u"hidden-parent selection invalid"_q);
		action.accept();
		check(cleared == 0
			&& *action.result == ChatHelpers::SelectedTextResult::Pending,
			u"hidden-parent pending acceptance neither clears nor accepts"_q);
		action.accepted();
		check(cleared == 0, u"hidden-parent clear callback rejected"_q);
		check(other.hasFocus(), u"hidden-parent cancel focus baseline"_q);
		action.cancelled();
		check(other.hasFocus() && !owner.hasFocus(),
			u"hidden-parent cancellation cannot steal focus"_q);
	}
	const auto prepare = [&](QString draft, QString selected) {
		return ChatHelpers::PrepareSelectedTextCite(
			{ draft },
			{ selected },
			validate).text;
	};
	report += u"SUITE append-calculation\n"_q;
	check(!Shortcuts::ValidBinding(
		QKeySequence(),
		Shortcuts::Command::QuoteSelectedText), u"empty contextual binding rejected"_q);
	check(!Shortcuts::ValidBinding(
		QKeySequence(),
		Shortcuts::Command::Search), u"empty legacy binding rejected"_q);
	check(Shortcuts::ValidBinding(QKeySequence(u"Q"_q),
		Shortcuts::Command::QuoteSelectedText), u"contextual single key"_q);
	check(!Shortcuts::ValidBinding(QKeySequence(u"Ctrl+Q, C"_q),
		Shortcuts::Command::CiteSelectedText), u"contextual multistroke rejected"_q);
	check(Shortcuts::ValidBinding(QKeySequence(u"Ctrl+Q, C"_q),
		Shortcuts::Command::Search), u"legacy multistroke preserved"_q);
	check(!Shortcuts::IsContextual(Shortcuts::Command::Search),
		u"global command remains global"_q);
	check(prepare({}, u"quote"_q) == u"quote\n"_q, u"empty draft"_q);
	check(prepare(u"draft"_q, u"quote"_q) == u"draft\n\nquote\n"_q,
		u"required separator"_q);
	check(prepare(u"draft\n"_q, u"quote"_q) == u"draft\n\nquote\n"_q,
		u"single newline padded to blank line"_q);
	check(prepare(u"draft\n\n"_q, u"quote\n"_q)
		== u"draft\n\nquote\n"_q, u"preserved trailing newlines"_q);
	check(prepare(u"draft\n\n\n"_q, u"quote"_q)
		== u"draft\n\n\nquote\n"_q, u"extra trailing newlines preserved"_q);
	check(prepare(u"draft\n"_q, u" \nquote\n\n"_q)
		== u"draft\n\n \nquote\n\n"_q, u"selected whitespace"_q);
	check(prepare(u"draft \n"_q, u"\n first\nsecond \n\n"_q)
		== u"draft \n\n\n first\nsecond \n\n"_q,
		u"draft and selected whitespace preserved"_q);
	for (auto newlines = 0; newlines != 4; ++newlines) {
		const auto prepared = ChatHelpers::PrepareSelectedTextCite(
			{ u"draft"_q + QString(newlines, '\n'),
				{ { 0, 5, Ui::InputField::kTagBold } } },
			{ u"quote"_q },
			validate);
		check(prepared.tags == TextWithTags::Tags{
			{ 0, 5, Ui::InputField::kTagBold },
			{ 5 + std::max(newlines, 2), 5, Ui::InputField::kTagBlockquote },
		}, u"suffix %1 quote bounds and prior tags"_q.arg(newlines));
	}
	check(prepare(u"draft"_q, {}) == u"draft"_q, u"empty selection"_q);
	const auto emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
	report += u"SUITE custom-emoji-field\n"_q;
	{
		auto context = Ui::Text::MarkedContext();
		context.customEmojiFactory = [](QStringView data, const Ui::Text::MarkedContext &)
		-> std::unique_ptr<Ui::Text::CustomEmoji> {
			return std::make_unique<Ui::Text::PaletteDependentCustomEmoji>([] {
				return MakeObjectImage(QSize(20, 20), Qt::green, u"E"_q);
			}, data.toString());
		};
		auto field = Ui::InputField(nullptr, st::defaultInputField, Ui::InputField::Mode::MultiLine);
		field.setCustomTextContext(context, [] { return true; });
		field.setMarkdownReplacesEnabled(true);
		field.setTextWithTags({ u"draft "_q + emoji, {
			{ 6, int(emoji.size()), Ui::InputField::CustomEmojiLink(u"321"_q) },
		} }, Ui::InputField::HistoryAction::Clear);
		const auto before = field.getTextWithTags();
		const auto selected = TextWithTags{ emoji, {
			{ 0, int(emoji.size()), Ui::InputField::CustomEmojiLink(u"123"_q) },
		} };
		check(ChatHelpers::AppendSelectedTextCite(&field, selected, validate, -1, context),
			u"custom emoji append accepted"_q);
		const auto after = field.getTextWithTags();
		check(after.text == before.text + u"\n\n"_q + emoji + '\n',
			u"custom emoji exact text"_q);
		const auto entities = TextUtilities::ConvertTextTagsToEntities(after.tags);
		check(std::ranges::count_if(entities, [](const EntityInText &entity) {
			return entity.type() == EntityType::CustomEmoji;
		}) == 2, u"prior and incoming custom emoji retained"_q);
		field.undo();
		check(field.getTextWithTags() == before, u"custom emoji one undo restores draft"_q);
		field.redo();
		check(field.getTextWithTags() == after, u"custom emoji redo restores citation"_q);
	}
	const auto samples = std::vector<TextWithTags>{
		{ u"draft text"_q, { { 0, 5, Ui::InputField::kTagBold } } },
		{ u"draft\n\n"_q },
		{ u"draft "_q + emoji + u" text"_q },
		{ u"draft quote\n"_q,
			{ { 0, 11, Ui::InputField::kTagBlockquote } } },
		{ u"draft code\n"_q,
			{ { 0, 10, Ui::InputField::kTagPre } } },
		{ u"draft collapsed\n"_q,
			{ { 0, 15, Ui::InputField::kTagBlockquoteCollapsed } } },
		{ u"draft\n"_q, { { 0, 5, Ui::InputField::kTagBold } } },
	};
	report += u"SUITE input-field-public-api\n"_q;
	auto sampleIndex = 0;
	for (const auto &sample : samples) {
		const auto label = u"case %1 "_q.arg(++sampleIndex);
		report += u"BEGIN "_q + label + '\n';
		flush();
		auto field = Ui::InputField(
			nullptr,
			st::defaultInputField,
			Ui::InputField::Mode::MultiLine);
		field.setCustomTextContext({}, [] { return false; });
		field.setMarkdownReplacesEnabled(true);
		field.setTextWithTags(sample, Ui::InputField::HistoryAction::Clear);
		const auto before = field.getTextWithTags();
		auto cursor = field.textCursor();
		cursor.setPosition(std::min(5, field.document()->characterCount() - 1));
		cursor.setPosition(1, QTextCursor::KeepAnchor);
		field.setTextCursor(cursor);
		const auto selected = TextWithTags{
			u" first\nsecond "_q + emoji + u"\n"_q,
			{ { 1, 5, Ui::InputField::kTagItalic } },
		};
		const auto expected = ChatHelpers::PrepareSelectedTextCite(
			before,
			selected,
			validate);
		check(ChatHelpers::AppendSelectedTextCite(&field, selected, validate),
			label + u"accepted"_q);
		const auto after = field.getTextWithTags();
		check(after.text == expected.text, label + u"exact text"_q);
		check(after.tags == expected.tags, label + u"exact formatting"_q);
		if (sample.text == u"draft\n"_q) {
			check(after.text == u"draft\n\n first\nsecond "_q + emoji + '\n',
				label + u"literal blank-line separation"_q);
			check(std::ranges::none_of(after.tags, [](const auto &tag) {
				return tag.offset < 7
					&& tag.offset + tag.length > 5
					&& tag.id.contains(Ui::InputField::kTagBlockquote);
			}), label + u"separating blank line unquoted"_q);
		}
		for (const auto &tag : expected.tags) {
			report += u"EXPECTED TAG %1,%2:%3\n"_q.arg(tag.offset).arg(
				tag.length).arg(tag.id);
		}
		for (const auto &tag : after.tags) {
			report += u"ACTUAL TAG %1,%2:%3\n"_q.arg(tag.offset).arg(
				tag.length).arg(tag.id);
		}
		check(field.textCursor().position()
			== field.document()->characterCount() - 1,
			label + u"final caret"_q);
		check(!field.textCursor().hasSelection(), label + u"final no selection"_q);
		field.undo();
		check(field.getTextWithTags().text == before.text,
			label + u"undo text"_q);
		check(field.getTextWithTags().tags == before.tags,
			label + u"undo formatting"_q);
		check(!field.isUndoAvailable(), label + u"one undo entry"_q);
		field.redo();
		check(field.getTextWithTags().text == after.text,
			label + u"redo text"_q);
		check(field.getTextWithTags().tags == after.tags,
			label + u"redo formatting"_q);
		cursor = field.textCursor();
		cursor.movePosition(QTextCursor::End);
		field.setTextCursor(cursor);
		auto key = QKeyEvent(
			QEvent::KeyPress,
			Qt::Key_N,
			Qt::NoModifier,
			u"next"_q);
		App::sendEvent(field.rawTextEdit(), &key);
		const auto typed = field.getTextWithTags();
		check(typed.text == after.text + u"next"_q,
			label + u"next paragraph text"_q);
		check(std::ranges::none_of(typed.tags, [&](const auto &tag) {
			return tag.offset + tag.length > after.text.size();
		}), label + u"next paragraph normal attributes"_q);
	}
	report += u"SUITE incoming-text-and-atomicity\n"_q;
	const auto incoming = std::vector<TextWithTags>{
		{ u"plain"_q },
		{ u"\n"_q },
		{ u"\n\n"_q },
		{ u" \nquote\n\n"_q },
		{ u"first\nsecond"_q, { { 0, 12, Ui::InputField::kTagPre } } },
		{ u"first\nsecond"_q,
			{ { 0, 12, Ui::InputField::kTagBlockquoteCollapsed } } },
		{ u"inline"_q, { { 0, 6, Ui::InputField::kTagCode } } },
		{ u"inline"_q, { { 0, 6, Ui::InputField::kTagUnderline } } },
		{ u"inline"_q, { { 0, 6, Ui::InputField::kTagStrikeOut } } },
		{ u"inline"_q, { { 0, 6, Ui::InputField::kTagSpoiler } } },
		{ u"link"_q, { { 0, 4, u"https://example.com"_q } } },
	};
	for (auto index = 0; index != incoming.size(); ++index) {
		auto field = Ui::InputField(
			nullptr,
			st::defaultInputField,
			Ui::InputField::Mode::MultiLine);
		field.setCustomTextContext({}, [] { return false; });
		field.setMarkdownReplacesEnabled(true);
		const auto &selected = incoming[index];
		const auto label = u"incoming %1 "_q.arg(index);
		const auto expected = ChatHelpers::PrepareSelectedTextCite(
			{}, selected, validate);
		if (std::ranges::all_of(selected.text, [](QChar ch) { return ch == '\n'; })) {
			check(!ChatHelpers::AppendSelectedTextCite(&field, selected, validate),
				label + u"unrepresentable empty quote rejected"_q);
			check(field.getLastText().isEmpty() && !field.isUndoAvailable(),
				label + u"rejection unchanged"_q);
			continue;
		}
		check(ChatHelpers::AppendSelectedTextCite(&field, selected, validate),
			label + u"accepted"_q);
		check(field.getTextWithTags().text == expected.text,
			label + u"exact text"_q);
		check(field.getTextWithTags().tags == expected.tags,
			label + u"exact formatting"_q);
		field.undo();
		check(field.getLastText().isEmpty() && !field.isUndoAvailable(),
			label + u"one undo"_q);
		field.redo();
		check(field.getTextWithTags().text == expected.text,
			label + u"redo text"_q);
	}
	{
		auto field = Ui::InputField(
			nullptr,
			st::defaultInputField,
			Ui::InputField::Mode::MultiLine);
		field.setMaxLength(10);
		field.setTextWithTags(samples.front(), Ui::InputField::HistoryAction::Clear);
		const auto before = field.getTextWithTags();
		check(!ChatHelpers::AppendSelectedTextCite(
			&field, { u"too long"_q }, validate, 10), u"capacity rejected"_q);
		check(field.getTextWithTags() == before, u"capacity rejection unchanged"_q);
		check(!field.isUndoAvailable(), u"capacity rejection no undo entry"_q);
		check(!ChatHelpers::AppendSelectedTextCite(&field, {}, validate, 10),
			u"empty rejected"_q);
		check(!ChatHelpers::AppendSelectedTextCite(
			&field, { QString(QChar::ObjectReplacementCharacter) }, validate),
			u"unrepresentable object rejected"_q);
		check(field.getTextWithTags() == before && !field.isUndoAvailable(),
			u"normalization rejection atomic"_q);
	}
	report += u"COMPLETE selected-text-shortcuts failures=%1\n"_q.arg(failed);
	flush();
	if (output.error() != QFileDevice::NoError) {
		return 2;
	}
	return failed ? 1 : 0;
}

void test(not_null<Ui::RpWindow*>, not_null<Ui::RpWidget*> body) {
	const auto formulaEntityData =
		u"iv-markdown:inline-text-object;formula;copy;tex"_q;
	const auto formulaReplacementText = u"$\\frac{a}{b}$"_q;
	const auto controlEntityData = u"test-custom-emoji"_q;
	const auto formulaImage = MakeObjectImage(
		QSize(scale(64), scale(28)),
		QColor(32, 96, 192, 48),
		u"a / b"_q);
	const auto controlImage = MakeObjectImage(
		QSize(scale(24), scale(24)),
		QColor(224, 176, 32, 160),
		u"*"_q);

	auto context = Ui::Text::MarkedContext();
	context.customEmojiFactory = [
		formulaEntityData,
		formulaReplacementText,
		formulaImage,
		controlImage
	](QStringView data, const Ui::Text::MarkedContext &)
	-> std::unique_ptr<Ui::Text::CustomEmoji> {
		if (data == formulaEntityData) {
			return std::make_unique<FormulaLikeObject>(
				data.toString(),
				formulaReplacementText,
				formulaImage);
		}
		if (data == u"test-custom-emoji"_q) {
			return std::make_unique<Ui::Text::PaletteDependentCustomEmoji>(
				[controlImage] {
					return controlImage;
				},
				data.toString());
		}
		return std::unique_ptr<Ui::Text::CustomEmoji>();
	};

	auto formulaData = TextWithEntities();
	formulaData.append(u"Alpha "_q);
	const auto formulaPosition = formulaData.text.size();
	formulaData.append(QChar::ObjectReplacementCharacter);
	formulaData.entities.push_back(EntityInText(
		EntityType::CustomEmoji,
		formulaPosition,
		1,
		formulaEntityData));
	formulaData.append(u" omega"_q);

	const auto formulaText = new Ui::Text::String(scale(64));
	formulaText->setMarkedText(
		st::defaultTextStyle,
		formulaData,
		kMarkupTextOptions,
		context);

	const auto formulaCollapsedText = Ui::Text::String(
		st::defaultTextStyle,
		u"Alpha   omega"_q,
		kMarkupTextOptions,
		scale(64));
	const auto objectPlaceholderWidth = st::defaultTextStyle.font->width(u" "_q);
	Expects(
		formulaText->maxWidth()
			>= formulaCollapsedText.maxWidth()
				- objectPlaceholderWidth
				+ formulaImage.width());

	const auto expectedFormulaExport = u"Alpha $\\frac{a}{b}$ omega"_q;
	Expects(formulaText->toString() == expectedFormulaExport);
	const auto formulaMime = formulaText->toTextForMimeData();
	Expects(formulaMime.expanded == expectedFormulaExport);
	Expects(formulaMime.rich.text == expectedFormulaExport);
	Expects(!HasEntityType(formulaMime.rich.entities, EntityType::CustomEmoji));
	Expects(formulaMime.tags.size() == 1);
	const auto expectedFormulaTag = TextForMimeDataTag{
		.offset = int(formulaPosition),
		.length = int(formulaReplacementText.size()),
		.id = Ui::InputField::kTagIvMath,
	};
	Expects(formulaMime.tags.front() == expectedFormulaTag);
	const auto formulaRich = formulaText->toTextWithEntities();
	Expects(formulaRich.text == expectedFormulaExport);
	Expects(!HasEntityType(formulaRich.entities, EntityType::CustomEmoji));
	Expects(
		formulaText->adjustSelection(
			TextSelection(uint16(formulaPosition), uint16(formulaPosition)),
			TextSelectType::Words)
		== TextSelection(
			uint16(formulaPosition),
			uint16(formulaPosition + 1)));
	Expects(
		formulaText->adjustSelection(
			TextSelection(uint16(formulaPosition), uint16(formulaPosition + 1)),
			TextSelectType::Paragraphs)
		== TextSelection(
			uint16(formulaPosition),
			uint16(formulaPosition + 1)));
	Expects(!formulaText->hasCustomEmoji());
	Expects(!formulaText->isOnlyCustomEmoji());
	Expects(!formulaText->isIsolatedEmoji());

	auto longFormulaSource = QString();
	while (longFormulaSource.size() <= 4096) {
		longFormulaSource.append(u"\\alpha+\\beta "_q);
	}
	auto longFormulaData = TextWithEntities();
	longFormulaData.append(u"Before "_q);
	const auto longFormulaPosition = longFormulaData.text.size();
	longFormulaData.append(longFormulaSource);
	longFormulaData.entities.push_back(EntityInText(
		EntityType::CustomEmoji,
		longFormulaPosition,
		longFormulaSource.size(),
		formulaEntityData));
	longFormulaData.append(u" after"_q);
	const auto longFormulaText = Ui::Text::String(
		st::defaultTextStyle,
		longFormulaData,
		kMarkupTextOptions,
		scale(64),
		context);
	Expects(longFormulaText.maxWidth() >= formulaImage.width());
	Expects(longFormulaText.countHeight(scale(200)) > 0);
	Expects(
		longFormulaText.toString()
			== u"Before "_q + formulaReplacementText + u" after"_q);
	const auto longFormulaRender = RenderTextOffscreen(
		longFormulaText,
		scale(200));
	Expects(HasPaintedPixels(longFormulaRender));

	const auto longSpacedEmojiText = Ui::Text::String(
		st::defaultTextStyle,
		QString::fromUtf8("\xF0\x9F\x98\x80")
			+ QString(4100, QChar(' '))
			+ u"x"_q,
		kDefaultTextOptions,
		scale(64));
	Expects(longSpacedEmojiText.maxWidth() > 0);
	Expects(longSpacedEmojiText.countHeight(scale(200)) > 0);

	auto controlData = TextWithEntities();
	controlData.append(QChar::ObjectReplacementCharacter);
	controlData.entities.push_back(EntityInText(
		EntityType::CustomEmoji,
		0,
		1,
		controlEntityData));

	const auto controlText = new Ui::Text::String(scale(64));
	controlText->setMarkedText(
		st::defaultTextStyle,
		controlData,
		kMarkupTextOptions,
		context);
	Expects(controlText->hasCustomEmoji());
	Expects(controlText->isOnlyCustomEmoji());
	Expects(controlText->isIsolatedEmoji());
	Expects(
		HasEntityType(
			controlText->toTextWithEntities().entities,
			EntityType::CustomEmoji));
	const auto controlProbeText = u"a / b"_q;
	const auto controlLineSource = u"Alpha a / b omega"_q;
	const auto controlProbePosition = controlLineSource.indexOf(controlProbeText);
	Expects(controlProbePosition >= 0);
	const auto controlLineText = Ui::Text::String(
		st::defaultTextStyle,
		controlLineSource,
		kMarkupTextOptions,
		scale(64));
	const auto selectionWidth = std::max(
		formulaText->maxWidth(),
		controlLineText.maxWidth());
	const auto formulaLinesGeometry = formulaText->countLinesGeometry(
		selectionWidth);
	const auto controlLinesGeometry = controlLineText.countLinesGeometry(
		selectionWidth);
	Expects(formulaLinesGeometry.size() == 1);
	Expects(controlLinesGeometry.size() == 1);
	if ((formulaLinesGeometry.size() == 1)
		&& (controlLinesGeometry.size() == 1)) {
		const auto &formulaLine = formulaLinesGeometry.front();
		const auto &controlLine = controlLinesGeometry.front();
		Expects(formulaLine.baseline > controlLine.baseline);
		Expects(formulaLine.baseline < formulaLine.bottom);
		Expects(controlLine.baseline < controlLine.bottom);
	}
	{
		auto fieldStyle = st::defaultMultiSelectSearchField;
		fieldStyle.textMargins = QMargins(0, 0, 0, 0);
		fieldStyle.placeholderMargins = QMargins(0, 0, 0, 0);
		const auto placeholderText = u"bot query"_q;
		const auto longPlaceholderText = u"bot query placeholder placeholder placeholder placeholder placeholder"_q;
		const auto inlineBotPrefix = u"@bot "_q;
		const auto field = std::make_unique<Ui::InputField>(
			body,
			fieldStyle,
			Ui::InputField::Mode::MultiLine,
			rpl::single(placeholderText),
			QString());
		field->resize(scale(320), fieldStyle.heightMin);
		field->show();
		field->setDocumentMargin(4.);
		field->setAdditionalMargin(style::ConvertScale(4) - 4);
		field->finishAnimating();
		field->setPlaceholderHidden(true);
		field->finishAnimating();
		const auto emptyFieldImage = Ui::GrabWidgetToImage(field.get());
		field->setPlaceholderHidden(false);
		field->finishAnimating();
		const auto placeholderImage = Ui::GrabWidgetToImage(field.get());
		const auto placeholderPoint = FirstPaintedPoint(placeholderImage);
		const auto placeholderBounds = ChangedBoundsInRect(
			emptyFieldImage,
			placeholderImage,
			QRect(QPoint(), placeholderImage.size()));
		Expects(placeholderPoint.has_value());
		Expects(placeholderBounds.has_value());
		field->setText(placeholderText);
		field->finishAnimating();
		const auto liveTextImage = Ui::GrabWidgetToImage(field.get());
		const auto liveTextPoint = FirstPaintedPoint(liveTextImage);
		const auto liveTextBounds = ChangedBoundsInRect(
			emptyFieldImage,
			liveTextImage,
			QRect(QPoint(), liveTextImage.size()));
		Expects(liveTextPoint.has_value());
		Expects(liveTextBounds.has_value());
		if (placeholderPoint && liveTextPoint) {
			Expects(placeholderPoint->y() == liveTextPoint->y());
			Expects(placeholderPoint->x() == liveTextPoint->x());
		}
		const auto expectedTextRect = ScaleRect(
			field->rect().marginsRemoved(field->fullTextMargins()),
			placeholderImage.devicePixelRatio());
		field->clear();
		field->setPlaceholder(rpl::single(longPlaceholderText));
		field->finishAnimating();
		const auto longPlaceholderImage = Ui::GrabWidgetToImage(field.get());
		const auto longPlaceholderBounds = ChangedBoundsInRect(
			emptyFieldImage,
			longPlaceholderImage,
			QRect(QPoint(), longPlaceholderImage.size()));
		Expects(longPlaceholderBounds.has_value());
		if (longPlaceholderBounds) {
			Expects(expectedTextRect.contains(*longPlaceholderBounds));
		}
		field->clear();
		field->setPlaceholder(
			rpl::single(placeholderText),
			inlineBotPrefix.size());
		field->setText(inlineBotPrefix);
		field->finishAnimating();
		const auto inlinePlaceholderImage = Ui::GrabWidgetToImage(field.get());
		const auto inlineSkipWidth = qRound(
			fieldStyle.style.font->width(inlineBotPrefix)
				* inlinePlaceholderImage.devicePixelRatio());
		const auto inlineScanLeft = placeholderPoint
			? std::min(
				placeholderPoint->x() + inlineSkipWidth,
				inlinePlaceholderImage.width() - 1)
			: 0;
		const auto inlinePlaceholderPoint = FirstPaintedPoint(
			inlinePlaceholderImage,
			QRect(
				inlineScanLeft,
				0,
				inlinePlaceholderImage.width() - inlineScanLeft,
				inlinePlaceholderImage.height()));
		Expects(inlinePlaceholderPoint.has_value());
		if (placeholderPoint && inlinePlaceholderPoint) {
			Expects(placeholderPoint->y() == inlinePlaceholderPoint->y());
		}
	}
	const auto formulaSelection = TextSelection(
		0,
		uint16(formulaData.text.size()));
	const auto formulaUnselected = RenderTextOffscreen(
		*formulaText,
		selectionWidth);
	const auto formulaSelected = RenderTextOffscreen(
		*formulaText,
		selectionWidth,
		formulaSelection);
	const auto formulaHitBounds = SymbolHitBounds(
		*formulaText,
		selectionWidth,
		formulaPosition);
	Expects(formulaHitBounds.has_value());
	const auto controlSelection = TextSelection(
		0,
		uint16(controlLineSource.size()));
	const auto controlUnselected = RenderTextOffscreen(
		controlLineText,
		selectionWidth);
	const auto controlSelected = RenderTextOffscreen(
		controlLineText,
		selectionWidth,
		controlSelection);
	const auto controlHitBounds = SymbolRangeHitBounds(
		controlLineText,
		selectionWidth,
		controlProbePosition,
		controlProbeText.size());
	Expects(controlHitBounds.has_value());
	if (formulaHitBounds && controlHitBounds) {
		const auto formulaChangedBounds = ChangedBoundsInRect(
			formulaUnselected,
			formulaSelected,
			QRect(
				formulaHitBounds->x(),
				0,
				formulaHitBounds->width(),
				formulaSelected.height()));
		const auto controlChangedBounds = ChangedBoundsInRect(
			controlUnselected,
			controlSelected,
			QRect(
				controlHitBounds->x(),
				0,
				controlHitBounds->width(),
				controlSelected.height()));
		Expects(formulaChangedBounds.has_value());
		Expects(controlChangedBounds.has_value());
		if (formulaChangedBounds && controlChangedBounds) {
			Expects(
				formulaChangedBounds->top()
					<= controlChangedBounds->top());
			Expects(
				formulaChangedBounds->bottom()
					>= controlChangedBounds->bottom());
			Expects(
				(formulaChangedBounds->top()
					< controlChangedBounds->top())
				|| (formulaChangedBounds->bottom()
					> controlChangedBounds->bottom()));
		}
	}

	auto leadingFormulaData = TextWithEntities();
	leadingFormulaData.append(QChar::ObjectReplacementCharacter);
	leadingFormulaData.entities.push_back(EntityInText(
		EntityType::CustomEmoji,
		0,
		1,
		formulaEntityData));
	const auto leadingFormulaText = Ui::Text::String(
		st::defaultTextStyle,
		leadingFormulaData,
		kMarkupTextOptions,
		scale(32),
		context);
	const auto leadingFormulaRender = RenderTextOffscreen(
		leadingFormulaText,
		std::max(formulaImage.width() / 2, 1));
	Expects(HasPaintedPixels(leadingFormulaRender));

	auto skipOnlyText = Ui::Text::String(
		st::defaultTextStyle,
		u""_q,
		kDefaultTextOptions,
		scale(32));
	const auto skipBlockWidth = scale(36);
	const auto skipBlockHeight = scale(7);
	Expects(skipOnlyText.updateSkipBlock(skipBlockWidth, skipBlockHeight));
	Expects(skipOnlyText.countHeight(skipBlockWidth * 2) == skipBlockHeight);
	Expects(
		skipOnlyText.countDimensions(Ui::Text::SimpleGeometry(
			skipBlockWidth * 2,
			0,
			0,
			false)).height == skipBlockHeight);

	const auto colorizedText = u"Colorized link span"_q;
	const auto colorizedWidth = scale(280);
	const auto plainColorizedText = Ui::Text::String(
		st::defaultTextStyle,
		colorizedText,
		kMarkupTextOptions,
		scale(96));
	const auto emptyColorizedText = Ui::Text::String(
		st::defaultTextStyle,
		Ui::Text::Colorized(colorizedText),
		kMarkupTextOptions,
		scale(96));
	const auto emptyColorizedRender = RenderTextOffscreen(
		emptyColorizedText,
		colorizedWidth);
	const auto colorizedBoundsRect = QRect(
		QPoint(),
		emptyColorizedRender.size());
	const auto linkPenRender = RenderTextOffscreen(
		plainColorizedText,
		colorizedWidth,
		std::nullopt,
		{},
		st::defaultTextPalette.linkFg->p);
	const auto plainColorizedRender = RenderTextOffscreen(
		plainColorizedText,
		colorizedWidth);
	Expects(ImagesEqualInRect(
		emptyColorizedRender,
		linkPenRender,
		colorizedBoundsRect));
	Expects(!ImagesEqualInRect(
		emptyColorizedRender,
		plainColorizedRender,
		colorizedBoundsRect));

	const auto &specialFg = st::defaultTextPalette.linkFg;
	const auto &specialBg = st::defaultTextPalette.markBg;
	auto specialColors = std::vector<Ui::Text::SpecialColor>{
		Ui::Text::SpecialColor{
			&specialFg->p,
			&specialFg->p,
			&specialBg->b,
			&specialBg->b },
	};
	const auto explicitColorizedNoBgText = Ui::Text::String(
		st::defaultTextStyle,
		Ui::Text::Colorized(colorizedText, 1, 0),
		kMarkupTextOptions,
		scale(96));
	const auto explicitColorizedBgText = Ui::Text::String(
		st::defaultTextStyle,
		Ui::Text::Colorized(colorizedText, 1, 1),
		kMarkupTextOptions,
		scale(96));
	const auto explicitColorizedNoBgRender = RenderTextOffscreen(
		explicitColorizedNoBgText,
		colorizedWidth,
		std::nullopt,
		specialColors);
	const auto explicitColorizedBgRender = RenderTextOffscreen(
		explicitColorizedBgText,
		colorizedWidth,
		std::nullopt,
		specialColors);
	const auto specialPenRender = RenderTextOffscreen(
		plainColorizedText,
		colorizedWidth,
		std::nullopt,
		{},
		specialFg->p);
	const auto colorizedSymbolBounds = SymbolRangeHitBounds(
		explicitColorizedBgText,
		colorizedWidth,
		0,
		colorizedText.size());
	Expects(ImagesEqualInRect(
		explicitColorizedNoBgRender,
		specialPenRender,
		colorizedBoundsRect));
	Expects(colorizedSymbolBounds.has_value());
	if (colorizedSymbolBounds) {
		Expects(!ImagesEqualInRect(
			explicitColorizedBgRender,
			explicitColorizedNoBgRender,
			*colorizedSymbolBounds));
		Expects(HasPixelColorInRect(
			explicitColorizedBgRender,
			*colorizedSymbolBounds,
			specialBg->c.rgba()));
		Expects(!HasPixelColorInRect(
			explicitColorizedNoBgRender,
			*colorizedSymbolBounds,
			specialBg->c.rgba()));
	}

	body->paintRequest() | rpl::on_next([=](QRect clip) {
		auto p = QPainter(body);
		p.fillRect(clip, QColor(255, 255, 255));
		const auto left = scale(24);
		const auto top = scale(24);
		const auto width = body->width() - (2 * left);
		formulaText->draw(p, {
			.position = QPoint(left, top),
			.availableWidth = width,
		});
		controlText->draw(p, {
			.position = QPoint(
				left,
				top + formulaText->countHeight(width) + scale(20)),
			.availableWidth = width,
		});
	}, body->lifetime());
}

} // namespace Test
