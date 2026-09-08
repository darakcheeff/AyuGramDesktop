// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/cloud_sync/cloud_folders_sync.h"

#include "ayu/libs/json.hpp"
#include "main/main_session.h"
#include "main/main_account.h"
#include "data/data_session.h"
#include "data/data_chat_filters.h"
#include "data/data_channel.h"
#include "data/data_user.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "data/notify/data_notify_settings.h"
#include "history/history.h"
#include "history/history_item.h"
#include "apiwrap.h"
#include "api/api_common.h"
#include "api/api_sending.h"
#include "core/core_settings.h"
#include "base/timer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <map>
#include <memory>

namespace AyuCloudSync {
namespace {

constexpr auto kSyncChannelTitle = "⚙️ AyuGram Sync Storage";
constexpr auto kSyncChannelAbout = "AyuGram Cloud Sync Storage: #ayugram_sync_folders #ayugram_storage";
constexpr auto kSyncTag = "#ayugram_sync_folders";

QString GetLocalCachePath(not_null<Main::Session*> session) {
	return cWorkingDir() + u"tdata/ayu_local_folders_"_q + QString::number(session->userId().bare) + u".json"_q;
}

class SyncManager final {
public:
	explicit SyncManager(not_null<Main::Session*> session)
	: _session(session)
	, _debounceTimer([=] { syncNow(); }) {
	}

	void start() {
		loadFromLocalCache();
		if (const auto existing = findExistingSyncChannel()) {
			_channel = existing;
			ensureChannelArchivedAndMuted(existing);
			fetchHistoryFromChannel(existing);
		}
		// Schedule sync after short delay once session is connected
		_debounceTimer.callOnce(2000);
	}

	void scheduleSync() {
		saveToLocalCache();
		_debounceTimer.callOnce(1000);
	}

	void syncNow() {
		const auto &list = _session->data().chatsFilters().list();
		const auto count = ranges::count_if(list, [](const auto &f) { return f.id() > 0; });
		if (count == 0 && !_channel) {
			return;
		}

		if (!_channel) {
			_channel = findExistingSyncChannel();
		}

		if (_channel) {
			ensureChannelArchivedAndMuted(_channel);
			uploadToChannel(_channel);
		} else {
			findOrCreateSyncChannel([=](ChannelData *ch) {
				if (ch) {
					uploadToChannel(ch);
				}
			});
		}
	}

private:
	ChannelData *findExistingSyncChannel() {
		if (_channel) {
			return _channel;
		}
		const auto checkHistory = [&](History *history) -> ChannelData* {
			if (history && history->peer) {
				if (const auto channel = history->peer->asChannel()) {
					if (channel->name() == QString::fromUtf8(kSyncChannelTitle) && channel->amCreator()) {
						return channel;
					}
				}
			}
			return nullptr;
		};

		const auto archive = _session->data().folder(Data::Folder::kId);
		for (const auto &row : archive->chatsList()->indexed()->all()) {
			if (const auto ch = checkHistory(row->history())) {
				return ch;
			}
		}
		for (const auto &row : _session->data().chatsList()->indexed()->all()) {
			if (const auto ch = checkHistory(row->history())) {
				return ch;
			}
		}
		return nullptr;
	}

	void ensureChannelArchivedAndMuted(not_null<ChannelData*> ch) {
		_session->data().notifySettings().update(ch, Data::MuteValue{ .forever = true });
		const auto history = _session->data().history(ch);
		if (history->folderId() != Data::Folder::kId) {
			_session->api().toggleHistoryArchived(history, true);
		}
	}

	void loadFromLocalCache() {
		const auto path = GetLocalCachePath(_session);
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return;
		}
		const auto data = file.readAll();
		if (data.isEmpty()) {
			return;
		}
		try {
			const auto j = nlohmann::json::parse(data.toStdString());
			_localTimestamp = j.value("updatedAt", uint64(0));
			if (j.contains("channelId") && j["channelId"].is_number()) {
				const auto chId = j["channelId"].get<uint64>();
				if (chId != 0) {
					_channel = _session->data().channel(ChannelId(chId));
				}
			}
			applyJson(j);
		} catch (...) {
		}
	}

	void saveToLocalCache() {
		const auto path = GetLocalCachePath(_session);
		QFile file(path);
		if (file.open(QIODevice::WriteOnly)) {
			auto j = serializeLocalFolders();
			if (_channel) {
				j["channelId"] = _channel->id.bare;
			}
			file.write(QByteArray::fromStdString(j.dump()));
		}
	}

	nlohmann::json serializeLocalFolders() {
		nlohmann::json root;
		root["version"] = 1;
		_localTimestamp = QDateTime::currentSecsSinceEpoch();
		root["updatedAt"] = _localTimestamp;
		if (_channel) {
			root["channelId"] = _channel->id.bare;
		}

		const auto &list = _session->data().chatsFilters().list();
		auto order = std::vector<FilterId>();
		for (const auto &f : list) {
			order.push_back(f.id());
		}
		root["order"] = order;

		auto folders = nlohmann::json::array();
		for (const auto &filter : list) {
			if (!filter.id()) {
				continue;
			}
			using Flag = Data::ChatFilter::Flag;
			nlohmann::json fj;
			fj["id"] = filter.id();
			fj["isLocal"] = Data::IsLocalFilterId(filter.id());
			fj["title"] = filter.titleText().text.toStdString();
			fj["iconEmoji"] = filter.iconEmoji().toStdString();
			if (filter.colorIndex()) {
				fj["colorIndex"] = int(*filter.colorIndex());
			}
			fj["contacts"] = (filter.flags() & Flag::Contacts) != 0;
			fj["nonContacts"] = (filter.flags() & Flag::NonContacts) != 0;
			fj["groups"] = (filter.flags() & Flag::Groups) != 0;
			fj["channels"] = (filter.flags() & Flag::Channels) != 0;
			fj["bots"] = (filter.flags() & Flag::Bots) != 0;
			fj["noMuted"] = (filter.flags() & Flag::NoMuted) != 0;
			fj["noRead"] = (filter.flags() & Flag::NoRead) != 0;
			fj["noArchived"] = (filter.flags() & Flag::NoArchived) != 0;
			fj["staticTitle"] = (filter.flags() & Flag::StaticTitle) != 0;

			auto alwaysArr = nlohmann::json::array();
			for (const auto &h : filter.always()) {
				alwaysArr.push_back(h->peer->id.value);
			}
			fj["always"] = alwaysArr;

			auto pinnedArr = nlohmann::json::array();
			for (const auto &h : filter.pinned()) {
				pinnedArr.push_back(h->peer->id.value);
			}
			fj["pinned"] = pinnedArr;

			auto neverArr = nlohmann::json::array();
			for (const auto &h : filter.never()) {
				neverArr.push_back(h->peer->id.value);
			}
			fj["never"] = neverArr;

			folders.push_back(fj);
		}
		root["folders"] = folders;
		return root;
	}

	void applyJson(const nlohmann::json &j) {
		if (j.contains("folders") && j["folders"].is_array()) {
			for (const auto &fj : j["folders"]) {
				const auto filterId = fj.value("id", FilterId(0));
				if (!filterId) {
					continue;
				}
				if (!Data::IsLocalFilterId(filterId)) {
					continue;
				}
				using Flag = Data::ChatFilter::Flag;
				const auto flags = (fj.value("contacts", false) ? Flag::Contacts : Flag(0))
					| (fj.value("nonContacts", false) ? Flag::NonContacts : Flag(0))
					| (fj.value("groups", false) ? Flag::Groups : Flag(0))
					| (fj.value("channels", false) ? Flag::Channels : Flag(0))
					| (fj.value("bots", false) ? Flag::Bots : Flag(0))
					| (fj.value("noMuted", false) ? Flag::NoMuted : Flag(0))
					| (fj.value("noRead", false) ? Flag::NoRead : Flag(0))
					| (fj.value("noArchived", false) ? Flag::NoArchived : Flag(0))
					| (fj.value("staticTitle", false) ? Flag::StaticTitle : Flag(0));

				base::flat_set<not_null<History*>> always;
				std::vector<not_null<History*>> pinned;
				base::flat_set<not_null<History*>> never;

				if (fj.contains("always") && fj["always"].is_array()) {
					for (const auto &item : fj["always"]) {
						uint64 pid = 0;
						if (item.is_number()) {
							pid = item.get<uint64>();
						} else if (item.is_string()) {
							pid = QString::fromStdString(item.get<std::string>()).toULongLong();
						}
						if (pid) {
							always.insert(_session->data().history(PeerId(pid)));
						}
					}
				}
				if (fj.contains("pinned") && fj["pinned"].is_array()) {
					for (const auto &item : fj["pinned"]) {
						uint64 pid = 0;
						if (item.is_number()) {
							pid = item.get<uint64>();
						} else if (item.is_string()) {
							pid = QString::fromStdString(item.get<std::string>()).toULongLong();
						}
						if (pid) {
							pinned.push_back(_session->data().history(PeerId(pid)));
						}
					}
				}
				if (fj.contains("never") && fj["never"].is_array()) {
					for (const auto &item : fj["never"]) {
						uint64 pid = 0;
						if (item.is_number()) {
							pid = item.get<uint64>();
						} else if (item.is_string()) {
							pid = QString::fromStdString(item.get<std::string>()).toULongLong();
						}
						if (pid) {
							never.insert(_session->data().history(PeerId(pid)));
						}
					}
				}

				const auto title = Data::ChatFilterTitle{
					.text = TextWithEntities{ QString::fromStdString(fj.value("title", "")) },
					.isStatic = fj.value("staticTitle", false),
				};
				const auto iconEmoji = QString::fromStdString(fj.value("iconEmoji", ""));
				const auto colorVal = fj.value("colorIndex", -1);
				const auto colorIndex = (colorVal >= 0 && colorVal < 256)
					? std::make_optional(static_cast<uint8>(colorVal))
					: std::nullopt;

				auto filter = Data::ChatFilter(
					filterId,
					title,
					iconEmoji,
					colorIndex,
					flags,
					std::move(always),
					std::move(pinned),
					std::move(never));

				_session->data().chatsFilters().set(std::move(filter));
			}
		}
		if (j.contains("order") && j["order"].is_array()) {
			std::vector<FilterId> order;
			for (const auto &item : j["order"]) {
				if (item.is_number_integer()) {
					order.push_back(item.get<FilterId>());
				}
			}
			if (!order.empty()) {
				_session->data().chatsFilters().reorderLocally(order);
			}
		}
	}

	void fetchHistoryFromChannel(not_null<ChannelData*> ch) {
		_session->api().request(MTPmessages_GetHistory(
			ch->input(),
			MTP_int(0),
			MTP_int(0),
			MTP_int(0),
			MTP_int(5),
			MTP_int(0),
			MTP_int(0),
			MTP_long(0)
		)).done([=](const MTPmessages_Messages &result) {
			result.match([&](const MTPDmessages_messagesNotModified &) {
			}, [&](const auto &data) {
				for (const auto &msg : data.vmessages().v) {
					if (msg.type() != mtpc_message) {
						continue;
					}
					const auto text = qs(msg.c_message().vmessage());
					const auto tagStr = QString::fromUtf8(kSyncTag);
					if (text.startsWith(tagStr)) {
						const auto jsonStr = text.mid(tagStr.length()).trimmed();
						try {
							const auto j = nlohmann::json::parse(jsonStr.toStdString());
							const auto remoteTime = j.value("updatedAt", uint64(0));
							if (remoteTime > _localTimestamp) {
								applyJson(j);
								saveToLocalCache();
								_localTimestamp = remoteTime;
							} else if (_localTimestamp > remoteTime) {
								uploadToChannel(ch);
							}
						} catch (...) {
						}
						break;
					}
				}
			});
		}).send();
	}

	void uploadToChannel(not_null<ChannelData*> ch) {
		const auto j = serializeLocalFolders();
		const auto jsonStr = QString::fromStdString(j.dump());
		const auto text = QString::fromUtf8(kSyncTag) + u"\n"_q + jsonStr;

		auto action = Api::SendAction(_session->data().history(ch));
		action.options.silent = true;
		auto message = Api::MessageToSend(action);
		message.textWithTags = { text };
		_session->api().sendMessage(std::move(message));
	}

	void findOrCreateSyncChannel(Fn<void(ChannelData*)> done) {
		if (const auto existing = findExistingSyncChannel()) {
			_channel = existing;
			ensureChannelArchivedAndMuted(existing);
			done(existing);
			return;
		}

		using Flag = MTPchannels_CreateChannel::Flag;
		_session->api().request(MTPchannels_CreateChannel(
			MTP_flags(Flag::f_broadcast),
			MTP_string(QString::fromUtf8(kSyncChannelTitle)),
			MTP_string(QString::fromUtf8(kSyncChannelAbout)),
			MTPInputGeoPoint(),
			MTPstring(),
			MTP_int(0)
		)).done([=](const MTPUpdates &result) {
			_session->api().applyUpdates(result);
			ChannelData *ch = nullptr;
			result.match([&](const MTPDupdates &data) {
				_session->data().processChats(data.vchats());
				for (const auto &chat : data.vchats().v) {
					if (chat.type() == mtpc_channel) {
						ch = _session->data().channel(chat.c_channel().vid());
						break;
					}
				}
			}, [&](const MTPDupdatesCombined &data) {
				_session->data().processChats(data.vchats());
				for (const auto &chat : data.vchats().v) {
					if (chat.type() == mtpc_channel) {
						ch = _session->data().channel(chat.c_channel().vid());
						break;
					}
				}
			}, [](const auto &) {
			});

			if (ch) {
				_channel = ch;
				ensureChannelArchivedAndMuted(ch);
				saveToLocalCache();
			}
			done(ch);
		}).fail([=](const MTP::Error &error) {
			LOG(("AyuCloudSync: Failed to create sync channel: %1").arg(error.type()));
			done(nullptr);
		}).send();
	}

	const not_null<Main::Session*> _session;
	base::Timer _debounceTimer;
	ChannelData *_channel = nullptr;
	uint64 _localTimestamp = 0;
};

std::map<not_null<Main::Session*>, std::unique_ptr<SyncManager>> Managers;

SyncManager *GetManager(not_null<Main::Session*> session) {
	auto it = Managers.find(session);
	if (it != Managers.end()) {
		return it->second.get();
	}
	auto manager = std::make_unique<SyncManager>(session);
	const auto raw = manager.get();
	Managers.emplace(session, std::move(manager));
	session->lifetime().add([session] {
		Managers.erase(session);
	});
	return raw;
}

} // namespace

void init(not_null<Main::Session*> session) {
	GetManager(session)->start();
}

void scheduleSync(not_null<Main::Session*> session) {
	GetManager(session)->scheduleSync();
}

void syncNow(not_null<Main::Session*> session) {
	GetManager(session)->syncNow();
}

} // namespace AyuCloudSync
