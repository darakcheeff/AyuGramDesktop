// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/watchers/watchers_manager.h"

#include "history/history.h"
#include "history/history_item.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/data_peer.h"
#include "data/data_channel.h"
#include "main/main_session.h"
#include "apiwrap.h"
#include "api/api_common.h"
#include "api/api_sending.h"
#include "core/core_settings.h"
#include "core/application.h"
#include "window/notifications_manager.h"

#include <QRegularExpression>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

namespace AyuWatchers {
namespace {

base::flat_set<FullMsgId> BypassMuteIds;

QString GetConfigPath() {
	return cWorkingDir() + u"tdata/ayu_watchers.json"_q;
}

void DispatchWebhook(const WatcherRule &rule, not_null<HistoryItem*> item, const QString &matchedSubstring) {
	if (rule.webhookUrl.trimmed().isEmpty()) {
		return;
	}

	const auto history = item->history();
	const auto peer = history->peer;
	const auto from = item->from();
	const auto text = item->originalText().text;

	QString link;
	if (peer->isChannel()) {
		if (!peer->username().isEmpty()) {
			link = u"https://t.me/%1/%2"_q.arg(peer->username()).arg(item->id.bare);
		} else {
			link = u"https://t.me/c/%1/%2"_q.arg(peerToChannel(peer->id).bare).arg(item->id.bare);
		}
	} else {
		link = u"https://t.me/c/%1"_q.arg(item->id.bare);
	}

	nlohmann::json root;
	root["event"] = "keyword_match";
	root["rule_id"] = rule.id.toStdString();
	root["rule_title"] = rule.title.toStdString();
	root["matched_regex"] = rule.regex.toStdString();
	root["matched_text"] = matchedSubstring.toStdString();
	root["chat"] = {
		{"id", peer->id.value},
		{"title", peer->name().toStdString()},
		{"username", peer->username().toStdString()},
		{"type", peer->isChannel() ? (peer->isMegagroup() ? "supergroup" : "channel") : (peer->isChat() ? "chat" : "user")}
	};
	root["sender"] = {
		{"id", from ? from->id.value : 0},
		{"name", from ? from->name().toStdString() : ""},
		{"username", from ? from->username().toStdString() : ""}
	};
	root["message"] = {
		{"id", item->id.bare},
		{"text", text.toStdString()},
		{"date", item->date()},
		{"link", link.toStdString()}
	};

	auto *nam = new QNetworkAccessManager();
	QNetworkRequest req(QUrl(rule.webhookUrl.trimmed()));
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	QByteArray body = QByteArray::fromStdString(root.dump());
	auto *reply = nam->post(req, body);

	auto *timer = new QTimer(reply);
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, reply, [=] {
		reply->abort();
	});
	timer->start(8000);

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		reply->deleteLater();
		nam->deleteLater();
	});
}

void DispatchForwardAlert(const WatcherRule &rule, not_null<HistoryItem*> item) {
	const auto history = item->history();
	const auto peer = history->peer;
	const auto from = item->from();
	const auto senderName = from ? from->name() : (peer ? peer->name() : QString());
	const auto senderUsername = from ? from->username() : QString();
	const auto chatTitle = peer ? peer->name() : QString();
	const auto messageText = item->originalText().text;

	QString link;
	if (peer->isChannel()) {
		if (!peer->username().isEmpty()) {
			link = u"https://t.me/%1/%2"_q.arg(peer->username()).arg(item->id.bare);
		} else {
			link = u"https://t.me/c/%1/%2"_q.arg(peerToChannel(peer->id).bare).arg(item->id.bare);
		}
	} else {
		link = u"https://t.me/c/%1"_q.arg(item->id.bare);
	}

	const auto targetHistory = (rule.forwardTargetId == 0)
		? history->owner().history(history->session().user())
		: history->owner().history(PeerId(rule.forwardTargetId));

	QString alertText = QString::fromUtf8("🔔 [") + rule.title + QString::fromUtf8("] Совпадение в чате: ")
		+ chatTitle + u"\n"_q
		+ QString::fromUtf8("От: ") + senderName
		+ (senderUsername.isEmpty() ? QString() : QString(" (@%1)").arg(senderUsername)) + u"\n"_q
		+ QString::fromUtf8("Текст:\n") + messageText.left(500) + (messageText.length() > 500 ? u"..."_q : QString()) + u"\n\n"_q
		+ QString::fromUtf8("🔗 Ссылка: ") + link;

	auto action = Api::SendAction(targetHistory);
	action.options.silent = true;
	auto message = Api::MessageToSend(action);
	message.textWithTags = { alertText };
	history->session().api().sendMessage(std::move(message));
}

} // namespace

Manager &Manager::Instance() {
	static Manager instance;
	return instance;
}

Manager::Manager() {
	load();
}

const std::vector<WatcherRule> &Manager::rules() const {
	return _rules;
}

std::vector<WatcherRule> Manager::rulesForPeer(uint64 peerId) const {
	std::vector<WatcherRule> result;
	for (const auto &r : _rules) {
		if (r.peerId == peerId || r.peerId == 0) {
			result.push_back(r);
		}
	}
	return result;
}

void Manager::addRule(const WatcherRule &rule) {
	_rules.push_back(rule);
	save();
}

void Manager::updateRule(const WatcherRule &rule) {
	for (auto &r : _rules) {
		if (r.id == rule.id) {
			r = rule;
			break;
		}
	}
	save();
}

void Manager::deleteRule(const QString &ruleId) {
	_rules.erase(
		std::remove_if(_rules.begin(), _rules.end(), [&](const WatcherRule &r) {
			return r.id == ruleId;
		}),
		_rules.end());
	save();
}

void Manager::resetCounter(const QString &ruleId) {
	for (auto &r : _rules) {
		if (r.id == ruleId) {
			r.matchCount = 0;
			break;
		}
	}
	save();
}

void Manager::reload() {
	load();
}

void Manager::load() {
	_rules.clear();
	QFile file(GetConfigPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	const auto data = file.readAll();
	if (data.isEmpty()) {
		return;
	}
	try {
		const auto j = nlohmann::json::parse(data.toStdString());
		if (j.contains("rules") && j["rules"].is_array()) {
			for (const auto &rj : j["rules"]) {
				WatcherRule r;
				from_json(rj, r);
				if (!r.id.isEmpty() && (!r.regex.isEmpty() || r.senderUserId != 0)) {
					_rules.push_back(std::move(r));
				}
			}
		}
	} catch (...) {
	}
}

void Manager::save() {
	nlohmann::json root;
	root["version"] = 1;
	auto arr = nlohmann::json::array();
	for (const auto &r : _rules) {
		nlohmann::json rj;
		to_json(rj, r);
		arr.push_back(rj);
	}
	root["rules"] = arr;

	QFile file(GetConfigPath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		const auto dumped = root.dump(2);
		file.write(dumped.data(), dumped.size());
	}
}

void Manager::sendTestWebhook(
		const QString &url,
		const QString &ruleTitle,
		Fn<void(bool success, QString message)> callback) {
	if (url.trimmed().isEmpty()) {
		if (callback) callback(false, "URL is empty");
		return;
	}
	nlohmann::json root;
	root["event"] = "test_webhook";
	root["rule_title"] = ruleTitle.toStdString();
	root["timestamp"] = QDateTime::currentSecsSinceEpoch();
	root["message"] = "This is a test webhook payload from AyuGram Watcher!";
	root["chat"] = {
		{"id", -1001234567890LL},
		{"title", "Test Channel"},
		{"username", "test_channel"}
	};
	root["sender"] = {
		{"id", 12345678LL},
		{"name", "Test User"},
		{"username", "test_user"}
	};
	root["matched_text"] = "test keyword";

	auto *nam = new QNetworkAccessManager();
	QNetworkRequest req(QUrl(url.trimmed()));
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	QByteArray body = QByteArray::fromStdString(root.dump(2));
	auto *reply = nam->post(req, body);

	auto *timer = new QTimer(reply);
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, reply, [=] {
		reply->abort();
	});
	timer->start(10000);

	QObject::connect(reply, &QNetworkReply::finished, [=] {
		const auto error = reply->error();
		const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const auto success = (error == QNetworkReply::NoError) || (status >= 200 && status < 300);
		const auto errorString = reply->errorString();
		if (callback) {
			if (success) {
				callback(true, QString("HTTP %1 OK").arg(status ? status : 200));
			} else {
				callback(false, errorString.isEmpty() ? QString("HTTP %1").arg(status) : errorString);
			}
		}
		reply->deleteLater();
		nam->deleteLater();
	});
}

bool ProcessIncomingMessage(not_null<HistoryItem*> item) {
	if (item->out() || item->isLocal() || item->isService()) {
		return false;
	}
	const auto text = item->originalText().text;
	if (text.trimmed().isEmpty()) {
		return false;
	}

	auto &mgr = Manager::Instance();
	const auto &rules = mgr.rules();
	if (rules.empty()) {
		return false;
	}

	const auto peerId = item->history()->peer->id.value;
	const auto from = item->from();
	const auto fromUserId = from ? from->id.value : 0;
	bool bypassMute = false;
	bool saveNeeded = false;

	for (auto r : rules) {
		if (!r.enabled) {
			continue;
		}
		if (r.peerId != 0 && r.peerId != peerId) {
			continue;
		}
		if (r.senderUserId != 0 && r.senderUserId != fromUserId) {
			continue;
		}

		QString matchedText;
		if (!r.regex.trimmed().isEmpty()) {
			QRegularExpression re(
				r.regex,
				r.caseInsensitive
					? QRegularExpression::CaseInsensitiveOption
					: QRegularExpression::NoPatternOption);
			if (!re.isValid()) {
				continue;
			}
			const auto match = re.match(text);
			if (!match.hasMatch()) {
				continue;
			}
			matchedText = match.captured(0);
		} else {
			if (r.senderUserId == 0) {
				continue;
			}
			matchedText = text.left(100);
		}

		if (r.trackCounter) {
			r.matchCount++;
			mgr.updateRule(r);
			saveNeeded = true;
		}
		if (r.notifyBypassMute) {
			BypassMuteIds.insert(item->fullId());
			bypassMute = true;
		}
		if (r.forwardToChat) {
			DispatchForwardAlert(r, item);
		}
		if (r.sendWebhook) {
			DispatchWebhook(r, item, matchedText);
		}
	}

	if (BypassMuteIds.size() > 2000) {
		BypassMuteIds.clear();
	}

	return bypassMute;
}

bool ShouldBypassMute(not_null<const HistoryItem*> item) {
	return BypassMuteIds.contains(item->fullId());
}

} // namespace AyuWatchers
