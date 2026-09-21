// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace AyuUi {

// Developer tool: renders a star gift service message plate locally, using
// the regular HistoryView::ServiceBox + HistoryView::PremiumGift pipeline,
// so that layout, animation and click handling can be verified without
// initiating any purchase or transfer request.
void ShowGiftRenderInspector(
	not_null<Window::SessionController*> controller);

} // namespace AyuUi
