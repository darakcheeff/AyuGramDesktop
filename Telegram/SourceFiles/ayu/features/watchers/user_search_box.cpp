// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/watchers/user_search_box.h"
#include "ayu/features/watchers/watcher_edit_box.h"

#include "data/data_user.h"
#include "data/data_peer.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "apiwrap.h"
#include "lang/lang_keys.h"
#include "lang_auto.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"
#include "styles/style_menu_icons.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QDateTime>

namespace AyuWatchers {

void ShowUserGlobalSearchBox(
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user,
		PeerData *currentChat) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		const auto userName = user->name();
		const auto userHandle = user->username().isEmpty() ? QString() : (u" (@"_q + user->username() + u")"_q);
		box->setTitle(rpl::single(QString::fromUtf8("Сообщения: ") + userName));

		const auto content = box->verticalLayout();

		// Action Buttons at top
		if (currentChat) {
			const auto inCurrentBtn = content->add(
				object_ptr<Ui::SettingsButton>(
					content,
					rpl::single(QString::fromUtf8("🔍 Искать в этом чате (") + currentChat->name() + u")"_q),
					st::settingsButtonNoIcon));
			inCurrentBtn->setClickedCallback([=] {
				box->closeBox();
				const auto key = Dialogs::Key{currentChat->owner().history(currentChat)};
				controller->searchInChat(key, user);
			});
		}

		const auto subBtn = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(QString::fromUtf8("👤 Подписаться на посты пользователя...")),
				st::settingsButtonNoIcon));
		subBtn->setClickedCallback([=] {
			box->closeBox();
			ShowUserWatcherSubscribeBox(controller, user, currentChat);
		});

		box->addRow(
			object_ptr<Ui::BoxContentDivider>(content),
			QMargins(0, st::settingsCheckboxPadding.top(), 0, st::settingsCheckboxPadding.bottom()));

		const auto statusLabel = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				QString::fromUtf8("⏳ Поиск общих чатов и сообщений..."),
				st::boxLabel),
			st::settingsCheckboxPadding);

		const auto resultsContainer = content->add(
			object_ptr<Ui::VerticalLayout>(content));

		box->addButton(tr::lng_close(), [=] { box->closeBox(); });

		// Structure for results state
		struct State {
			int totalMessages = 0;
			int searchedChats = 0;
			int totalChats = 0;
		};
		const auto state = box->lifetime().make_state<State>();

		auto &api = controller->session().api();
		auto &session = controller->session();

		// 1. Fetch common chats
		api.request(MTPmessages_GetCommonChats(
			user->inputUser(),
			MTP_long(0),
			MTP_int(100)
		)).done([=, &api, &session](const MTPmessages_Chats &chatsResult) {
			const auto &chats = chatsResult.match([&](const MTPDmessages_chats &data) {
				return data.vchats().v;
			}, [&](const MTPDmessages_chatsSlice &data) {
				return data.vchats().v;
			});

			std::vector<not_null<PeerData*>> targetPeers;
			if (currentChat && !currentChat->isUser()) {
				targetPeers.push_back(currentChat);
			}
			for (const auto &c : chats) {
				const auto p = session.data().processChat(c);
				if (!ranges::contains(targetPeers, p)) {
					targetPeers.push_back(p);
				}
			}

			if (targetPeers.empty()) {
				statusLabel->setText(QString::fromUtf8("Общих чатов с пользователем не найдено."));
				return;
			}

			state->totalChats = std::min(int(targetPeers.size()), 20);
			statusLabel->setText(QString::fromUtf8("⏳ Поиск в %1 чатах...").arg(state->totalChats));

			// 2. Query search in each peer (limit to first 20 peers)
			for (int i = 0; i < state->totalChats; ++i) {
				const auto peer = targetPeers[i];
				const auto flags = MTP_flags(MTPmessages_Search::Flag::f_from_id);

				api.request(MTPmessages_Search(
					flags,
					peer->input(),
					MTP_string(""),
					user->input(),
					MTPInputPeer(),
					MTPVector<MTPReaction>(),
					MTPint(),
					MTP_inputMessagesFilterEmpty(),
					MTP_int(0),
					MTP_int(0),
					MTP_int(0),
					MTP_int(0),
					MTP_int(15), // 15 per chat
					MTP_int(0),
					MTP_int(0),
					MTP_long(0)
				)).done([=](const MTPmessages_Messages &res) {
					state->searchedChats++;

					const auto &msgs = res.match(
						[](const MTPDmessages_messages &d) { return d.vmessages().v; },
						[](const MTPDmessages_messagesSlice &d) { return d.vmessages().v; },
						[](const MTPDmessages_channelMessages &d) { return d.vmessages().v; },
						[](const MTPDmessages_messagesNotModified &) { return QVector<MTPMessage>(); }
					);

					for (const auto &msg : msgs) {
						msg.match([&](const MTPDmessage &m) {
							state->totalMessages++;
							const auto msgId = m.vid().v;
							const auto date = m.vdate().v;
							const auto text = qs(m.vmessage()).trimmed();
							auto preview = text.isEmpty()
								? QString::fromUtf8("[Медиасообщение]")
								: text.left(100).replace('\n', ' ');

							const auto timeStr = QDateTime::fromSecsSinceEpoch(date).toString("dd.MM.yy HH:mm");
							const auto btnText = peer->name() + u" ("_q + timeStr + u"): "_q + preview;

							const auto itemBtn = resultsContainer->add(
								object_ptr<Ui::SettingsButton>(
									resultsContainer,
									rpl::single(btnText),
									st::settingsButtonNoIcon));

							itemBtn->setClickedCallback([=] {
								box->closeBox();
								controller->showPeerHistory(peer->id, Window::SectionShow(), msgId);
							});
						}, [&](const MTPDmessageService &) {
						}, [&](const MTPDmessageEmpty &) {
						});
					}

					statusLabel->setText(QString::fromUtf8("Найдено сообщений: %1 (проверено чатов: %2/%3)")
						.arg(state->totalMessages)
						.arg(state->searchedChats)
						.arg(state->totalChats));
				}).fail([=](const MTP::Error &) {
					state->searchedChats++;
					statusLabel->setText(QString::fromUtf8("Найдено сообщений: %1 (проверено чатов: %2/%3)")
						.arg(state->totalMessages)
						.arg(state->searchedChats)
						.arg(state->totalChats));
				});
			}
		}).fail([=](const MTP::Error &e) {
			statusLabel->setText(QString::fromUtf8("Не удалось загрузить список общих чатов."));
		});
	}));
}

} // namespace AyuWatchers
