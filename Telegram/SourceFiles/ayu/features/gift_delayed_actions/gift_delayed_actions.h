// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "base/timer.h"

class History;

namespace Main {
class Session;
} // namespace Main

// Runs a delayed action in a chat some random time after a local gift
// preview was shown there. A repeated preview in the same chat restarts the
// countdown, so each chat has at most one pending action.
//
// One instance per session, owned by the session lifetime: every pending
// timer is destroyed together with the session (logout or app exit), before
// the histories it points to.
class GiftDelayedActions final {
public:
	explicit GiftDelayedActions(not_null<Main::Session*> session);

	[[nodiscard]] static GiftDelayedActions &For(
		not_null<Main::Session*> session);

	// Starts a new 20-100 second countdown for `history`,
	// dropping the pending one if there is any.
	void schedule(not_null<History*> history);
	void cancel(not_null<History*> history);

private:
	void onGiftDelayedAction(History *history);

	const not_null<Main::Session*> _session;
	base::flat_map<not_null<History*>, std::unique_ptr<base::Timer>> _timers;

};
