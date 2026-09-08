// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <QString>
#include "ayu/libs/json.hpp"

namespace AyuWatchers {

struct WatcherRule {
	QString id;
	QString title;
	QString regex;
	bool enabled = true;
	bool caseInsensitive = true;
	uint64 peerId = 0; // 0 = all chats/global
	QString peerName;

	// User target
	uint64 senderUserId = 0; // 0 = any sender
	QString senderUsername;
	QString senderName;

	// Actions
	bool notifyBypassMute = true;
	bool forwardToChat = false;
	uint64 forwardTargetId = 0; // 0 = Saved Messages
	bool trackCounter = true;
	uint64 matchCount = 0;
	bool sendWebhook = false;
	QString webhookUrl;
};

inline void to_json(nlohmann::json &j, const WatcherRule &r) {
	j = nlohmann::json{
		{"id", r.id.toStdString()},
		{"title", r.title.toStdString()},
		{"regex", r.regex.toStdString()},
		{"enabled", r.enabled},
		{"caseInsensitive", r.caseInsensitive},
		{"peerId", r.peerId},
		{"peerName", r.peerName.toStdString()},
		{"senderUserId", r.senderUserId},
		{"senderUsername", r.senderUsername.toStdString()},
		{"senderName", r.senderName.toStdString()},
		{"notifyBypassMute", r.notifyBypassMute},
		{"forwardToChat", r.forwardToChat},
		{"forwardTargetId", r.forwardTargetId},
		{"trackCounter", r.trackCounter},
		{"matchCount", r.matchCount},
		{"sendWebhook", r.sendWebhook},
		{"webhookUrl", r.webhookUrl.toStdString()},
	};
}

inline void from_json(const nlohmann::json &j, WatcherRule &r) {
	r.id = QString::fromStdString(j.value("id", ""));
	r.title = QString::fromStdString(j.value("title", ""));
	r.regex = QString::fromStdString(j.value("regex", ""));
	r.enabled = j.value("enabled", true);
	r.caseInsensitive = j.value("caseInsensitive", true);
	r.peerId = j.value("peerId", uint64(0));
	r.peerName = QString::fromStdString(j.value("peerName", ""));
	r.senderUserId = j.value("senderUserId", uint64(0));
	r.senderUsername = QString::fromStdString(j.value("senderUsername", ""));
	r.senderName = QString::fromStdString(j.value("senderName", ""));
	r.notifyBypassMute = j.value("notifyBypassMute", true);
	r.forwardToChat = j.value("forwardToChat", false);
	r.forwardTargetId = j.value("forwardTargetId", uint64(0));
	r.trackCounter = j.value("trackCounter", true);
	r.matchCount = j.value("matchCount", uint64(0));
	r.sendWebhook = j.value("sendWebhook", false);
	r.webhookUrl = QString::fromStdString(j.value("webhookUrl", ""));
}

} // namespace AyuWatchers
