// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "base/basic_types.h"

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace AyuWatchers {

void ShowWatchersBox(
	not_null<Window::SessionController*> controller,
	PeerData *peer = nullptr);

} // namespace AyuWatchers
