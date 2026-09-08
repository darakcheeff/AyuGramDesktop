// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/features/watchers/watcher_rule.h"
#include <vector>
#include <functional>

class HistoryItem;

namespace AyuWatchers {

[[nodiscard]] bool ProcessIncomingMessage(not_null<HistoryItem*> item);
[[nodiscard]] bool ShouldBypassMute(not_null<const HistoryItem*> item);

class Manager final {
public:
	static Manager &Instance();

	[[nodiscard]] const std::vector<WatcherRule> &rules() const;
	[[nodiscard]] std::vector<WatcherRule> rulesForPeer(uint64 peerId) const;

	void addRule(const WatcherRule &rule);
	void updateRule(const WatcherRule &rule);
	void deleteRule(const QString &ruleId);
	void resetCounter(const QString &ruleId);

	void sendTestWebhook(
		const QString &url,
		const QString &ruleTitle,
		Fn<void(bool success, QString message)> callback);

	void reload();
	void save();

private:
	Manager();
	void load();

	std::vector<WatcherRule> _rules;
};

} // namespace AyuWatchers
