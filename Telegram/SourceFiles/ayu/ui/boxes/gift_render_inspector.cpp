// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/gift_render_inspector.h"

#include "boxes/star_gift_box.h"

#include "lang_auto.h"
#include "api/api_premium.h"
#include "base/unixtime.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_star_gift.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "history/admin_log/history_admin_log_item.h"
#include "history/view/history_view_element.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/text/text_utilities.h"
#include "ui/rp_widget.h"
#include "ui/vertical_list.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_theme.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/section_widget.h"
#include "window/window_session_controller.h"
#include "window/themes/window_theme.h"

namespace AyuUi {
namespace {

// Renders a single star gift service plate through the production
// HistoryView pipeline. The hosting item is created with
// FakeHistoryItem | Local and a non-history entry id, so it never reaches
// the message list, local storage or the network.
class GiftRenderPreview final : public Ui::RpWidget {
public:
	GiftRenderPreview(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	~GiftRenderPreview();

	void showGift(const Data::StarGift &gift);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	class PreviewDelegate;
	struct State;

	void toggleStickerRegistered(bool registered);
	void updateWidgetSize(int width);

	const not_null<Window::SessionController*> _controller;
	const not_null<State*> _state;

};

class GiftRenderPreview::PreviewDelegate final
	: public HistoryView::SimpleElementDelegate {
public:
	using HistoryView::SimpleElementDelegate::SimpleElementDelegate;

private:
	HistoryView::Context elementContext() override {
		return HistoryView::Context::ContactPreview;
	}
};

struct GiftRenderPreview::State {
	AdminLog::OwnedItem item;
	std::unique_ptr<PreviewDelegate> delegate;
	std::unique_ptr<Ui::ChatStyle> style;
	std::unique_ptr<Ui::ChatTheme> theme;
	History *history = nullptr;
	DocumentData *sticker = nullptr;
	int currentHeight = 0;
};

GiftRenderPreview::GiftRenderPreview(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: RpWidget(parent)
, _controller(controller)
, _state(lifetime().make_state<State>()) {
	_state->delegate = std::make_unique<PreviewDelegate>(
		controller,
		crl::guard(this, [=] { update(); }));
	_state->style = std::make_unique<Ui::ChatStyle>(
		controller->session().colorIndicesValue());
	_state->style->apply(controller->defaultChatTheme().get());
	_state->theme = Window::Theme::DefaultChatThemeOn(lifetime());
	_state->history = controller->session().data().history(
		PeerData::kServiceNotificationsId);

	widthValue(
	) | rpl::filter(
		rpl::mappers::_1 > 0
	) | rpl::on_next([=](int w) {
		updateWidgetSize(w);
	}, lifetime());
}

GiftRenderPreview::~GiftRenderPreview() {
	toggleStickerRegistered(false);
}

void GiftRenderPreview::toggleStickerRegistered(bool registered) {
	const auto view = _state->item.get();
	if (const auto item = view ? view->data().get() : nullptr) {
		if (_state->sticker) {
			const auto owner = &item->history()->owner();
			if (registered) {
				owner->registerDocumentItem(_state->sticker, item);
			} else {
				owner->unregisterDocumentItem(_state->sticker, item);
			}
		}
	}
	if (!registered) {
		_state->sticker = nullptr;
	}
}

void GiftRenderPreview::showGift(const Data::StarGift &gift) {
	toggleStickerRegistered(false);
	_state->item = nullptr;

	const auto history = _state->history;
	const auto from = history->peer;

	// Mirrors the GiftCode population of the messageActionStarGift branch
	// in HistoryItem::applyAction, but sourced from the cached catalog
	// entry instead of a server update.
	auto fields = Data::GiftCode{
		.stargiftId = gift.id,
		.document = gift.document,
		.stargiftReleasedBy = gift.releasedBy,
		.unique = gift.unique,
		.starsConverted = int(gift.starsConverted),
		.starsToUpgrade = int(gift.starsToUpgrade),
		.limitedCount = gift.limitedCount,
		.limitedLeft = gift.limitedLeft,
		.count = gift.stars,
		.type = Data::GiftType::StarGift,
		.upgradable = gift.upgradable,
		.saved = true,
	};

	const auto item = history->makeMessage({
		.id = history->nextNonHistoryEntryId(),
		.flags = (MessageFlag::FakeHistoryItem
			| MessageFlag::Local
			| MessageFlag::HasFromId),
		.from = from->id,
		.date = base::unixtime::now(),
	}, PreparedServiceText{ { QString() } });

	// The data-level media override makes the item own a real
	// Data::MediaGiftBox, so Element::createView() builds the production
	// HistoryView::ServiceBox + HistoryView::PremiumGift pair, with the
	// stock click handlers left intact.
	item->overrideMedia(std::make_unique<Data::MediaGiftBox>(
		item,
		from,
		std::move(fields)));

	_state->item = AdminLog::OwnedItem(_state->delegate.get(), item);

	_state->sticker = gift.document;
	toggleStickerRegistered(true);

	if (width() > 0) {
		updateWidgetSize(width());
	}
	update();
}

void GiftRenderPreview::updateWidgetSize(int width) {
	const auto view = _state->item.get();
	if (!view) {
		if (_state->currentHeight != 0) {
			_state->currentHeight = 0;
			resize(width, 0);
		}
		return;
	}
	const auto padding = st::settingsForwardPrivacyPadding;
	const auto height = view->resizeGetHeight(width);
	const auto full = padding
		+ view->marginTop()
		+ height
		+ view->marginBottom()
		+ padding;
	_state->currentHeight = full;
	resize(width, full);
}

void GiftRenderPreview::paintEvent(QPaintEvent *e) {
	const auto view = _state->item.get();
	if (!view) {
		return;
	}

	auto p = Painter(this);
	p.setClipRect(e->rect());
	Window::SectionWidget::PaintBackground(
		p,
		_state->theme.get(),
		QSize(width(), window()->height()),
		e->rect());

	auto hq = PainterHighQualityEnabler(p);
	const auto theme = _controller->defaultChatTheme().get();
	auto context = theme->preparePaintContext(
		_state->style.get(),
		rect(),
		rect(),
		rect(),
		_controller->isGifPausedAtLeastFor(
			Window::GifPauseReason::Layer));
	context.outbg = view->hasOutLayout();

	const auto padding = st::settingsForwardPrivacyPadding;
	p.translate(0, padding + view->marginTop());
	view->draw(p, context);
}

[[nodiscard]] QString DescribeGift(const Data::StarGift &gift) {
	const auto sticker = gift.document->sticker();
	const auto emoji = sticker ? sticker->alt : QString();
	auto result = QString();
	if (!emoji.isEmpty()) {
		result += emoji + QChar(' ');
	}
	result += u"#%1"_q.arg(gift.id);
	result += u" / %1 stars"_q.arg(gift.stars);
	if (gift.limitedCount > 0) {
		result += u" / limited %1 of %2"_q
			.arg(gift.limitedLeft)
			.arg(gift.limitedCount);
	}
	if (gift.unique) {
		result += u" / unique"_q;
	}
	if (gift.soldOut) {
		result += u" / sold out"_q;
	}
	return result;
}

void GiftRenderInspectorBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	struct State {
		std::unique_ptr<Api::PremiumGiftCodeOptions> api;
		rpl::lifetime request;
	};
	const auto state = box->lifetime().make_state<State>();

	box->setTitle(rpl::single(u"Gift render inspector"_q));
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	const auto container = box->verticalLayout();
	const auto preview = container->add(
		object_ptr<GiftRenderPreview>(container, controller));
	Ui::AddSkip(container);
	Ui::AddDivider(container);
	Ui::AddSkip(container);

	const auto status = container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			u"Loading catalog..."_q,
			st::boxDividerLabel),
		st::boxRowPadding);

	const auto list = container->add(
		object_ptr<Ui::VerticalLayout>(container));

	const auto fill = [=] {
		const auto &gifts = state->api->starGifts();
		if (gifts.empty()) {
			status->setText(u"Catalog is empty."_q);
			return;
		}
		status->setText(u"%1 gifts in local cache, pick one to render."_q
			.arg(gifts.size()));
		for (const auto &gift : gifts) {
			const auto copy = gift;
			Settings::AddButtonWithIcon(
				list,
				rpl::single(DescribeGift(copy)),
				st::settingsButton
			)->setClickedCallback([=] {
				preview->showGift(copy);
			});
		}
		list->resizeToWidth(box->width());
	};

	state->api = std::make_unique<Api::PremiumGiftCodeOptions>(
		controller->session().user());
	if (!state->api->starGifts().empty()) {
		fill();
	} else {
		state->request = state->api->requestStarGifts(
		) | rpl::on_error_done([=](const QString &error) {
			status->setText(u"Request failed: %1"_q.arg(error));
		}, [=] {
			fill();
		});
	}
}

} // namespace

void StartGiftPreview(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	Ui::ShowStarGiftBox(controller, peer, true);
}

void RenderLocalGiftPreview(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		const Data::StarGift &gift,
		const TextWithEntities &message) {
	const auto session = &controller->session();
	const auto history = session->data().history(peer);
	const auto self = session->user();

	// Same GiftCode population as the messageActionStarGift branch of
	// HistoryItem::applyAction, sourced from the catalog entry rather than
	// from a server update.
	auto fields = Data::GiftCode{
		.stargiftId = gift.id,
		.document = gift.document,
		.stargiftReleasedBy = gift.releasedBy,
		.unique = gift.unique,
		.message = message,
		.starsConverted = int(gift.starsConverted),
		.starsToUpgrade = int(gift.starsToUpgrade),
		.limitedCount = gift.limitedCount,
		.limitedLeft = gift.limitedLeft,
		.count = gift.stars,
		.type = Data::GiftType::StarGift,
		.upgradable = gift.upgradable,
		.saved = true,
	};

	const auto cost = TextWithEntities{
		tr::lng_action_gift_for_stars(tr::now, lt_count, gift.stars),
	};
	auto prepared = PreparedServiceText();
	prepared.text = tr::lng_action_gift_sent(
		tr::now,
		lt_cost,
		cost,
		Ui::Text::WithEntities);

	// MessageFlag::Local keeps the item out of storage and off the wire;
	// addNewLocalMessage() requires it and asserts on anything else.
	//
	// HistoryEntry, which History::addNewItem() insists on before it will
	// insert anything, is added by FinalizeMessageFlags() in the shared
	// HistoryItem constructor.
	const auto item = history->makeMessage({
		.id = session->data().nextLocalMessageId(),
		.flags = (MessageFlag::Local
			| MessageFlag::Outgoing
			| MessageFlag::HasFromId),
		.from = self->id,
		.date = base::unixtime::now(),
	}, std::move(prepared));

	// The data-level media override makes the item own a real
	// Data::MediaGiftBox, so Element::createView() builds the production
	// HistoryView::ServiceBox + HistoryView::PremiumGift pair. This mirrors
	// the _media assignment of the messageActionStarGift branch in
	// HistoryItem::applyAction.
	item->overrideMedia(std::make_unique<Data::MediaGiftBox>(
		item,
		self,
		std::move(fields)));

	history->addNewLocalMessage(item);

	// Deliberately silent on success: nothing on screen should hint that
	// this plate is local. The two guards below only ever fire when the
	// item failed to appear, which is the one case worth interrupting for.
	if (!item->isHistoryEntry()) {
		controller->showToast(u"The gift could not be shown."_q);
	} else if (!history->loadedAtBottom()) {
		controller->showToast(u"Scroll to the bottom of the chat."_q);
	}
}

void ShowGiftRenderInspector(
		not_null<Window::SessionController*> controller) {
	controller->show(Box(GiftRenderInspectorBox, controller));
}

} // namespace AyuUi
