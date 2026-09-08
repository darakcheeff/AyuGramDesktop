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
#include "data/data_histories.h"
#include "history/history.h"
#include "dialogs/dialogs_key.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
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
		box->setTitle(rpl::single(QString::fromUtf8("Сообщения пользователя: ") + userName));

		const auto content = box->verticalLayout();

		// Action Buttons at top
		if (currentChat && !currentChat->isUser()) {
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

		const auto chatsContainer = content->add(
			object_ptr<Ui::VerticalLayout>(content));

		const auto messagesContainer = content->add(
			object_ptr<Ui::VerticalLayout>(content));

		box->addButton(tr::lng_close(), [=] { box->closeBox(); });

		struct State {
			int totalMessages = 0;
			int searchedChats = 0;
			int totalChats = 0;
			bool headerAdded = false;
		};
		const auto state = box->lifetime().make_state<State>();

		const auto session = &controller->session();
		const auto api = box->lifetime().make_state<MTP::Sender>(&session->mtp());

		// 1. Fetch common chats
		api->request(MTPmessages_GetCommonChats(
			user->inputUser(),
			MTP_long(0),
			MTP_int(100)
		)).done([=](const MTPmessages_Chats &chatsResult) {
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
				if (const auto p = session->data().processChat(c)) {
					const auto actualPeer = p->migrateTo() ? p->migrateTo() : p;
					if (!ranges::contains(targetPeers, actualPeer)) {
						targetPeers.push_back(actualPeer);
					}
				}
			}

			if (targetPeers.empty()) {
				statusLabel->setText(QString::fromUtf8("Общих чатов с пользователем не найдено."));
				return;
			}

			// Add common chats buttons for direct 1-click in-chat search
			chatsContainer->add(
				object_ptr<Ui::FlatLabel>(
					chatsContainer,
					QString::fromUtf8("Общие группы (%1):").arg(targetPeers.size()),
					st::boxLabel),
				st::settingsCheckboxPadding);

			for (const auto peer : targetPeers) {
				const auto openChatBtn = chatsContainer->add(
					object_ptr<Ui::SettingsButton>(
						chatsContainer,
						rpl::single(QString::fromUtf8("💬 ") + peer->name() + QString::fromUtf8(" — открыть поиск автора")),
						st::settingsButtonNoIcon));
				openChatBtn->setClickedCallback([=] {
					box->closeBox();
					const auto key = Dialogs::Key{peer->owner().history(peer)};
					controller->searchInChat(key, user);
				});
			}

			state->totalChats = std::min(int(targetPeers.size()), 20);
			statusLabel->setText(QString::fromUtf8("⏳ Поиск сообщений в %1 чатах...").arg(state->totalChats));

			// 2. Query search in each peer
			for (int i = 0; i < state->totalChats; ++i) {
				const auto peer = targetPeers[i];
				using Flag = MTPmessages_Search::Flag;
				const auto flags = MTP_flags(Flag::f_from_id);

				api->request(MTPmessages_Search(
					flags,
					peer->input(),
					MTP_string(""),
					user->input(),
					MTP_inputPeerEmpty(),
					MTPVector<MTPReaction>(),
					MTP_int(0), // top_msg_id
					MTP_inputMessagesFilterEmpty(),
					MTP_int(0), // min_date
					MTP_int(0), // max_date
					MTP_int(0), // offset_id
					MTP_int(0), // add_offset
					MTP_int(20), // limit
					MTP_int(0), // max_id
					MTP_int(0), // min_id
					MTP_long(0) // hash
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
							if (!state->headerAdded) {
								state->headerAdded = true;
								messagesContainer->add(
									object_ptr<Ui::FlatLabel>(
										messagesContainer,
										QString::fromUtf8("Найденные сообщения:"),
										st::boxLabel),
									st::settingsCheckboxPadding);
							}

							state->totalMessages++;
							const auto msgId = m.vid().v;
							const auto date = m.vdate().v;
							const auto text = qs(m.vmessage()).trimmed();
							auto preview = text.isEmpty()
								? QString::fromUtf8("[Медиасообщение]")
								: text.left(100).replace('\n', ' ');

							const auto timeStr = QDateTime::fromSecsSinceEpoch(date).toString("dd.MM.yy HH:mm");
							const auto btnText = peer->name() + u" ("_q + timeStr + u"): "_q + preview;

							const auto itemBtn = messagesContainer->add(
								object_ptr<Ui::SettingsButton>(
									messagesContainer,
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

					if (state->totalMessages > 0) {
						statusLabel->setText(QString::fromUtf8("Найдено сообщений: %1 (проверено чатов: %2/%3)")
							.arg(state->totalMessages)
							.arg(state->searchedChats)
							.arg(state->totalChats));
					} else if (state->searchedChats >= state->totalChats) {
						statusLabel->setText(QString::fromUtf8("Сообщений в проверенных чатах не найдено. Нажмите на чат выше для ручного поиска."));
					}
				}).fail([=](const MTP::Error &) {
					state->searchedChats++;
					if (state->searchedChats >= state->totalChats && state->totalMessages == 0) {
						statusLabel->setText(QString::fromUtf8("Проверено чатов: %1. Сообщений не найдено. Нажмите на чат выше для перехода.").arg(state->totalChats));
					}
				}).send();
			}
		}).fail([=](const MTP::Error &e) {
			statusLabel->setText(QString::fromUtf8("Не удалось загрузить список общих чатов."));
		}).send();
	}));
}

} // namespace AyuWatchers
