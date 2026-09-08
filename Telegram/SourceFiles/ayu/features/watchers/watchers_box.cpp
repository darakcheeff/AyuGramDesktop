// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/watchers/watchers_box.h"
#include "ayu/features/watchers/watchers_manager.h"
#include "ayu/features/watchers/watcher_edit_box.h"

#include "data/data_peer.h"
#include "lang/lang_keys.h"
#include "lang_auto.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"
#include "styles/style_menu_icons.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace AyuWatchers {

void ShowWatchersBox(
		not_null<Window::SessionController*> controller,
		PeerData *peer) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		const auto title = peer
			? QString::fromUtf8("Мониторинг: ") + peer->name()
			: QString::fromUtf8("Мониторинг ключевых слов (Watchers)");
		box->setTitle(rpl::single(title));

		const auto content = box->verticalLayout();

		// Add Rule Button
		const auto addBtn = content->add(
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(QString::fromUtf8("➕ Добавить новое правило")),
				st::settingsButtonNoIcon));

		addBtn->setClickedCallback([=] {
			WatcherRule newRule;
			if (peer) {
				newRule.peerId = peer->id.value;
				newRule.peerName = peer->name();
			}
			ShowWatcherEditBox(controller, newRule, [=](WatcherRule) {
				ShowWatchersBox(controller, peer);
			});
		});

		box->addRow(
			object_ptr<Ui::BoxContentDivider>(content),
			QMargins(0, st::settingsCheckboxPadding.top(), 0, st::settingsCheckboxPadding.bottom()));

		const auto &allRules = Manager::Instance().rules();
		std::vector<WatcherRule> rules;
		for (const auto &r : allRules) {
			if (!peer || r.peerId == 0 || r.peerId == peer->id.value) {
				rules.push_back(r);
			}
		}

		if (rules.empty()) {
			content->add(
				object_ptr<Ui::FlatLabel>(
					content,
					QString::fromUtf8("Нет активных правил мониторинга.\n\nНажмите «Добавить новое правило», чтобы отслеживать сообщения по ключевым словам и регулярным выражениям с уведомлениями, пересылкой и вебхуками."),
					st::boxLabel),
				st::settingsCheckboxPadding);
		} else {
			for (const auto &rule : rules) {
				QStringList tags;
				if (rule.notifyBypassMute) tags << QString::fromUtf8("🔔 Звук");
				if (rule.forwardToChat) tags << QString::fromUtf8("💬 В Избранное");
				if (rule.sendWebhook) tags << QString::fromUtf8("🌐 Webhook");
				if (rule.trackCounter) tags << QString::fromUtf8("🔢 Срабатываний: %1").arg(rule.matchCount);

				if (rule.peerId == 0) {
					tags << QString::fromUtf8("🌍 Все чаты");
				} else {
					tags << (rule.peerName.isEmpty() ? QString::fromUtf8("📍 Чат") : (u"📍 "_q + rule.peerName));
				}

				if (!rule.enabled) {
					tags << QString::fromUtf8("⏸ Отключено");
				}

				const auto displayTitle = rule.title.isEmpty() ? rule.regex : rule.title;
				const auto buttonText = displayTitle + u"
"_q + tags.join(u" | "_q);

				const auto btn = content->add(
					object_ptr<Ui::SettingsButton>(
						content,
						rpl::single(buttonText),
						st::settingsButtonNoIcon));

				if (!rule.enabled) {
					btn->setColorOverride(st::windowSubTextFg->c);
				}

				btn->setClickedCallback([=] {
					ShowWatcherEditBox(controller, rule, [=](WatcherRule) {
						ShowWatchersBox(controller, peer);
					});
				});

				btn->setContextMenuPolicy(Qt::CustomContextMenu);
				QObject::connect(btn, &QWidget::customContextMenuRequested, [=](const QPoint &pos) {
					auto menu = new Ui::PopupMenu(btn, st::popupMenuWithIcons);
					menu->setAttribute(Qt::WA_DeleteOnClose);

					menu->addAction(
						tr::lng_theme_edit(tr::now),
						[=] {
							ShowWatcherEditBox(controller, rule, [=](WatcherRule) {
								ShowWatchersBox(controller, peer);
							});
						},
						&st::menuIconEdit);

					menu->addAction(
						rule.enabled
							? QString::fromUtf8("Приостановить")
							: QString::fromUtf8("Включить"),
						[=] {
							auto updated = rule;
							updated.enabled = !updated.enabled;
							Manager::Instance().updateRule(updated);
							ShowWatchersBox(controller, peer);
						},
						rule.enabled ? &st::menuIconBlock : &st::menuIconUnblock);

					if (rule.matchCount > 0) {
						menu->addAction(
							QString::fromUtf8("Сбросить счётчик"),
							[=] {
								Manager::Instance().resetCounter(rule.id);
								ShowWatchersBox(controller, peer);
							},
							&st::menuIconClear);
					}

					menu->addSeparator();
					menu->addAction(
						tr::lng_theme_delete(tr::now),
						[=] {
							Manager::Instance().deleteRule(rule.id);
							ShowWatchersBox(controller, peer);
						},
						&st::menuIconDelete);

					menu->popup(QCursor::pos());
				});
			}
		}

		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace AyuWatchers
