// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/watchers/watcher_edit_box.h"
#include "ayu/features/watchers/watchers_manager.h"

#include "data/data_peer.h"
#include "data/data_channel.h"
#include "lang/lang_keys.h"
#include "lang_auto.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"
#include "styles/style_menu_icons.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QRegularExpression>
#include <QDateTime>

namespace AyuWatchers {

void ShowWatcherEditBox(
		not_null<Window::SessionController*> controller,
		WatcherRule rule,
		Fn<void(WatcherRule)> onSave) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		const auto isNew = rule.id.isEmpty();
		box->setTitle(rpl::single(isNew
			? QString::fromUtf8("Добавить правило мониторинга")
			: QString::fromUtf8("Редактировать правило")));

		const auto content = box->verticalLayout();

		// Rule Title
		const auto titleField = box->addRow(
			object_ptr<Ui::InputField>(
				content,
				st::defaultInputField,
				rpl::single(QString::fromUtf8("Название правила (напр. Важное)")),
				rule.title),
			st::settingsCheckboxPadding);

		// Regex Pattern
		const auto regexField = box->addRow(
			object_ptr<Ui::InputField>(
				content,
				st::defaultInputField,
				rpl::single(QString::fromUtf8("Ключевые слова или Regex (напр. биткоин|btc|eth)")),
				rule.regex),
			st::settingsCheckboxPadding);

		// Error message for regex
		const auto errorWrap = box->addRow(
			object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
				content,
				object_ptr<Ui::FlatLabel>(
					content,
					QString(),
					st::settingLocalPasscodeError)),
			st::settingsCheckboxPadding);
		errorWrap->hide(anim::type::instant);

		// Case Insensitive checkbox
		const auto caseInsensitive = box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				QString::fromUtf8("Игнорировать регистр букв"),
				rule.caseInsensitive,
				st::defaultBoxCheckbox),
			st::settingsCheckboxPadding);

		// Scope (current chat only or all chats)
		Ui::Checkbox *peerScope = nullptr;
		if (rule.peerId != 0 || !rule.peerName.isEmpty()) {
			const auto chatName = rule.peerName.isEmpty()
				? QString::fromUtf8("текущему чату")
				: rule.peerName;
			peerScope = box->addRow(
				object_ptr<Ui::Checkbox>(
					box,
					QString::fromUtf8("Только для чата: ") + chatName,
					(rule.peerId != 0),
					st::defaultBoxCheckbox),
				st::settingsCheckboxPadding);
		}

		// Divider & Actions Header
		box->addRow(
			object_ptr<Ui::FlatLabel>(
				content,
				QString::fromUtf8("Действия при получении сообщения:"),
				st::boxLabel),
			st::settingsCheckboxPadding);

		// Action 1: Bypass Mute Notification
		const auto notifyBypassMute = box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				QString::fromUtf8("🔔 Звук и пуш (в обход беззвучного режима)"),
				rule.notifyBypassMute,
				st::defaultBoxCheckbox),
			st::settingsCheckboxPadding);

		// Action 2: Forward to Saved Messages
		const auto forwardToChat = box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				QString::fromUtf8("💬 Отправлять сводку в «Избранное» со ссылкой"),
				rule.forwardToChat,
				st::defaultBoxCheckbox),
			st::settingsCheckboxPadding);

		// Action 3: Counter
		const auto trackCounter = box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				QString::fromUtf8("🔢 Вести счётчик совпадений"),
				rule.trackCounter,
				st::defaultBoxCheckbox),
			st::settingsCheckboxPadding);

		// Action 4: Webhook
		const auto sendWebhook = box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				QString::fromUtf8("🌐 Отправлять Webhook (n8n / HTTP POST)"),
				rule.sendWebhook,
				st::defaultBoxCheckbox),
			st::settingsCheckboxPadding);

		// Webhook Settings Container
		const auto webhookWrap = box->addRow(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				content,
				object_ptr<Ui::VerticalLayout>(content)),
			st::settingsCheckboxPadding);
		const auto webhookInner = webhookWrap->entity();

		const auto webhookUrlField = webhookInner->add(
			object_ptr<Ui::InputField>(
				webhookInner,
				st::defaultInputField,
				rpl::single(QString::fromUtf8("URL Webhook (напр. https://n8n.../webhook/tg)")),
				rule.webhookUrl));

		const auto testBtn = webhookInner->add(
			object_ptr<Ui::SettingsButton>(
				webhookInner,
				rpl::single(QString::fromUtf8("⚡ Отправить тестовый запрос в n8n")),
				st::settingsButtonNoIcon));

		const auto testStatusLabel = webhookInner->add(
			object_ptr<Ui::FlatLabel>(
				webhookInner,
				QString(),
				st::boxLabel));

		testBtn->setClickedCallback([=] {
			const auto url = webhookUrlField->getTextWithTags().text.trimmed();
			if (url.isEmpty()) {
				testStatusLabel->setText(QString::fromUtf8("❌ Укажите URL вебхука"));
				return;
			}
			testStatusLabel->setText(QString::fromUtf8("⏳ Отправка запроса..."));
			Manager::Instance().sendTestWebhook(
				url,
				titleField->getTextWithTags().text.trimmed(),
				[=](bool ok, QString msg) {
					testStatusLabel->setText(ok
						? (QString::fromUtf8("✅ ") + msg)
						: (QString::fromUtf8("❌ ") + msg));
				});
		});

		if (!rule.sendWebhook) {
			webhookWrap->hide(anim::type::instant);
		}
		sendWebhook->checkedChanges() | rpl::on_next([=](bool checked) {
			webhookWrap->toggle(checked, anim::type::normal);
		}, sendWebhook->lifetime());

		// Save handler
		auto saveAndClose = [=, rId = rule.id, pId = rule.peerId, pName = rule.peerName, count = rule.matchCount]() mutable {
			const auto rxText = regexField->getTextWithTags().text.trimmed();
			if (rxText.isEmpty()) {
				errorWrap->entity()->setText(QString::fromUtf8("Введите ключевые слова или регулярное выражение"));
				errorWrap->show(anim::type::normal);
				return;
			}
			QRegularExpression testRx(
				rxText,
				caseInsensitive->checked()
					? QRegularExpression::CaseInsensitiveOption
					: QRegularExpression::NoPatternOption);
			if (!testRx.isValid()) {
				errorWrap->entity()->setText(QString::fromUtf8("Синтаксис regex невалиден: ") + testRx.errorString());
				errorWrap->show(anim::type::normal);
				return;
			}

			WatcherRule result;
			result.id = rId.isEmpty() ? QString::number(QDateTime::currentMSecsSinceEpoch()) : rId;
			result.title = titleField->getTextWithTags().text.trimmed();
			if (result.title.isEmpty()) {
				result.title = rxText;
			}
			result.regex = rxText;
			result.enabled = true;
			result.caseInsensitive = caseInsensitive->checked();
			if (peerScope) {
				result.peerId = peerScope->checked() ? pId : 0;
				result.peerName = peerScope->checked() ? pName : QString();
			} else {
				result.peerId = pId;
				result.peerName = pName;
			}
			result.notifyBypassMute = notifyBypassMute->checked();
			result.forwardToChat = forwardToChat->checked();
			result.forwardTargetId = 0; // Saved Messages
			result.trackCounter = trackCounter->checked();
			result.matchCount = count;
			result.sendWebhook = sendWebhook->checked();
			result.webhookUrl = webhookUrlField->getTextWithTags().text.trimmed();

			if (rId.isEmpty()) {
				Manager::Instance().addRule(result);
			} else {
				Manager::Instance().updateRule(result);
			}

			box->closeBox();
			if (onSave) {
				onSave(result);
			}
		};

		box->addButton(tr::lng_settings_save(), saveAndClose);
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
		if (!rule.id.isEmpty()) {
			box->addLeftButton(tr::lng_box_delete(), [=, id = rule.id] {
				Manager::Instance().deleteRule(id);
				box->closeBox();
				if (onSave) {
					onSave(WatcherRule{});
				}
			});
		}
	}));
}

void ShowWatcherQuickAddBox(
		not_null<Window::SessionController*> controller,
		const QString &selectedText,
		PeerData *peer) {
	WatcherRule r;
	r.title = selectedText.trimmed().left(30);
	r.regex = QRegularExpression::escape(selectedText.trimmed());
	if (peer) {
		r.peerId = peer->id.value;
		r.peerName = peer->name();
	}
	r.notifyBypassMute = true;
	r.forwardToChat = false;
	r.trackCounter = true;
	r.sendWebhook = false;

	ShowWatcherEditBox(controller, r);
}

} // namespace AyuWatchers
