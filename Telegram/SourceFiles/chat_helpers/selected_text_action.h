#pragma once

#include <QPointer>
#include <QWidget>

namespace ChatHelpers {

enum class SelectedTextResult { Rejected, Pending, Accepted };

struct SelectedTextAction {
	Fn<bool()> valid;
	Fn<void()> accepted;
	Fn<void()> cancelled;
	std::shared_ptr<SelectedTextResult> result;
	bool shortcut = false;

	[[nodiscard]] bool isValid() const {
		return !valid || valid();
	}
	void markPending() const {
		if (result) {
			*result = SelectedTextResult::Pending;
		}
	}
	void accept() const {
		if (!isValid()) {
			return;
		}
		if (result) {
			*result = SelectedTextResult::Accepted;
		}
		if (accepted) {
			accepted();
		}
	}
};

[[nodiscard]] inline SelectedTextAction MakeSelectedTextAction(
		not_null<QWidget*> owner,
		uint64 &generation,
		Fn<bool()> valid,
		Fn<void()> clear,
		bool shortcut) {
	const auto weak = QPointer<QWidget>(owner);
	const auto expected = ++generation;
	const auto current = &generation;
	const auto check = [=] {
		return weak && weak->isVisible() && *current == expected && valid();
	};
	return {
		.valid = check,
		.accepted = [=] { if (check()) clear(); },
		.cancelled = [=] { if (check()) weak->setFocus(); },
		.result = std::make_shared<SelectedTextResult>(SelectedTextResult::Rejected),
		.shortcut = shortcut,
	};
}

}