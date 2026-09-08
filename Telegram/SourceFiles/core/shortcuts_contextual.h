#pragma once

#include "core/shortcuts.h"

#include <private/qkeymapper_p.h>
#include <QApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPointer>
#include <QWidget>

namespace Shortcuts {

[[nodiscard]] inline bool ValidBinding(
		const QKeySequence &keys,
		Command command) {
	return !keys.isEmpty() && (!IsContextual(command) || keys.count() == 1);
}

class ContextualKeyLatch {
public:
	void start(not_null<QWidget*> owner, const QKeyEvent &event) {
		_owner = owner;
		_window = owner->window();
		_key = event.key();
		_scanCode = event.nativeScanCode();
	}

	void clear() {
		_owner = nullptr;
		_window = nullptr;
		_key = 0;
		_scanCode = 0;
	}

	[[nodiscard]] bool consume(const QKeyEvent &event) {
		if (!_owner || !_window) {
			clear();
			return false;
		}
		if ((_scanCode && event.nativeScanCode())
			? (_scanCode != event.nativeScanCode())
			: (_key != event.key())) {
			return false;
		}
		if (event.type() == QEvent::KeyRelease && !event.isAutoRepeat()) {
			clear();
		}
		return true;
	}

private:
	QPointer<QWidget> _owner;
	QPointer<QWidget> _window;
	int _key = 0;
	quint32 _scanCode = 0;

};

class ContextualDispatcher {
public:
	bool handle(
		not_null<QObject*> object,
		not_null<QEvent*> event,
		bool paused,
		Fn<std::vector<Command>(const QKeySequence &)> lookup,
		Fn<void(not_null<ContextualRequest*>)> requestHandler);

private:
	ContextualKeyLatch _heldKey;
	QPointer<QObject> _inputMethodOwner;

};

inline bool ContextualDispatcher::handle(
		not_null<QObject*> object,
		not_null<QEvent*> event,
		bool paused,
		Fn<std::vector<Command>(const QKeySequence &)> lookup,
		Fn<void(not_null<ContextualRequest*>)> requestHandler) {
	const auto type = event->type();
	if (type == QEvent::InputMethod) {
		const auto input = static_cast<QInputMethodEvent*>(event.get());
		_inputMethodOwner = input->preeditString().isEmpty() ? nullptr : object.get();
		return false;
	}
	if (type == QEvent::ApplicationDeactivate) {
		_heldKey.clear();
		_inputMethodOwner = nullptr;
		return false;
	}
	if (type != QEvent::ShortcutOverride && type != QEvent::KeyPress
		&& type != QEvent::KeyRelease) {
		return false;
	}
	const auto key = static_cast<QKeyEvent*>(event.get());
	if (_heldKey.consume(*key)) {
		key->accept();
		return true;
	}
	const auto owner = QApplication::focusWidget();
	if (paused || !owner || object != owner || !owner->isVisible()
		|| _inputMethodOwner == owner || !owner->window()->isActiveWindow()
		|| owner->window()->isMinimized() || QApplication::activePopupWidget()
		|| QApplication::activeModalWidget() || type == QEvent::KeyRelease
		|| key->isAutoRepeat()) {
		return false;
	}
	auto candidates = QKeyMapper::possibleKeys(key);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	const auto raw = key->keyCombination();
#else
	const auto raw = key->keyCombination().toCombined();
#endif
	if (!candidates.contains(raw)) {
		candidates.push_back(raw);
	}
	for (const auto candidate : candidates) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		const auto modifiers = candidate.keyboardModifiers();
#else
		const auto modifiers = Qt::KeyboardModifiers(
			candidate & Qt::KeyboardModifierMask);
#endif
		if (modifiers != key->modifiers()) {
			continue;
		}
		for (const auto command : lookup(QKeySequence(candidate))) {
			auto request = ContextualRequest{ .command = command, .owner = owner };
			requestHandler(&request);
			if (!request.execute) {
				continue;
			}
			if (type == QEvent::ShortcutOverride) {
				key->accept();
				return true;
			}
			_heldKey.start(owner, *key);
			request.execute();
			key->accept();
			return true;
		}
	}
	return false;
}

}