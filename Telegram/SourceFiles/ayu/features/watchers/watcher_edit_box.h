// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/features/watchers/watcher_rule.h"
#include "base/basic_types.h"

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace AyuWatchers {

void ShowWatcherEditBox(
	not_null<Window::SessionController*> controller,
	WatcherRule rule,
	Fn<void(WatcherRule)> onSave = nullptr);

void ShowWatcherQuickAddBox(
	not_null<Window::SessionController*> controller,
	const QString &selectedText,
	PeerData *peer = nullptr);

} // namespace AyuWatchers
