// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

namespace Data {
struct StarGift;
} // namespace Data

namespace Window {
class SessionController;
} // namespace Window

class PeerData;

namespace AyuUi {

// Developer tool: renders a star gift service message plate locally, using
// the regular HistoryView::ServiceBox + HistoryView::PremiumGift pipeline,
// so that layout, animation and click handling can be verified without
// initiating any purchase or transfer request.
void ShowGiftRenderInspector(
	not_null<Window::SessionController*> controller);

// Opens the regular star gift catalog for `peer` in preview mode: picking a
// gift, writing a caption and pressing send all behave as usual, but nothing
// is paid for and nothing is transmitted.
void StartGiftPreview(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);

// Appends a local-only gift service message to the chat with `peer`. The item
// carries MessageFlag::Local, so it lives in memory only: it is never stored,
// never synced, and vanishes on restart. The recipient sees nothing.
void RenderLocalGiftPreview(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	const Data::StarGift &gift,
	const TextWithEntities &message);

} // namespace AyuUi
