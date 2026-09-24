// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/gift_delayed_actions/gift_delayed_actions.h"

#include "ayu/features/test_replies/test_replies_manager.h"
#include "base/random.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "main/main_session.h"

namespace {

constexpr auto kMinDelay = crl::time(20'000);
constexpr auto kMaxDelay = crl::time(100'000);

// Sessions are few and each registers itself once, on first use.
base::flat_map<not_null<Main::Session*>, GiftDelayedActions*> Instances;

[[nodiscard]] crl::time RandomDelay() {
	return kMinDelay + base::RandomIndex(int(kMaxDelay - kMinDelay + 1));
}

} // namespace

GiftDelayedActions::GiftDelayedActions(not_null<Main::Session*> session)
: _session(session) {
}

GiftDelayedActions &GiftDelayedActions::For(
		not_null<Main::Session*> session) {
	if (const auto i = Instances.find(session); i != end(Instances)) {
		return *i->second;
	}
	auto &lifetime = session->lifetime();
	const auto result = lifetime.make_state<GiftDelayedActions>(session);
	Instances.emplace(session, result);
	lifetime.add([=] {
		Instances.remove(session);
	});
	return *result;
}

void GiftDelayedActions::schedule(not_null<History*> history) {
	Expects(&history->session() == _session);

	auto i = _timers.find(history);
	if (i == end(_timers)) {
		// The timer is kept after firing and reused on the next schedule,
		// so it is never destroyed from inside its own callback.
		i = _timers.emplace(
			history,
			std::make_unique<base::Timer>([=] {
				onGiftDelayedAction(history);
			})).first;
	}
	// callOnce() cancels the pending countdown before starting a new one.
	i->second->callOnce(RandomDelay());
}

void GiftDelayedActions::cancel(not_null<History*> history) {
	if (const auto i = _timers.find(history); i != end(_timers)) {
		i->second->cancel();
	}
}

void GiftDelayedActions::onGiftDelayedAction(History *history) {
	const auto reply = TestRepliesManager::Instance().getNextUnusedReply();
	if (reply.isEmpty()) {
		LOG(("GiftDelayedActions: no unused test replies left for %1."
			).arg(history->peer->id.value));
		return;
	}
	// TODO: add the reply to `history`.
	DEBUG_LOG(("GiftDelayedActions: picked a reply for %1: %2"
		).arg(history->peer->id.value
		).arg(reply));
}
