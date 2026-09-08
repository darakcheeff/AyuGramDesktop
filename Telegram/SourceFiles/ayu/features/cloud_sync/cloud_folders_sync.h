// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

namespace Main {
class Session;
} // namespace Main

namespace AyuCloudSync {

void init(not_null<Main::Session*> session);
void scheduleSync(not_null<Main::Session*> session);

} // namespace AyuCloudSync
