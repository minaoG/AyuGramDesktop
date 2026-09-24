// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/gift_delayed_actions/gift_delayed_actions.h"

#include "ayu/features/test_replies/test_replies_manager.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "ui/text/text_entity.h"
#include "ui/widgets/fields/input_field.h"

namespace {

constexpr auto kMinDelay = crl::time(20'000);
constexpr auto kMaxDelay = crl::time(100'000);

// Sessions are few and each registers itself once, on first use.
base::flat_map<not_null<Main::Session*>, GiftDelayedActions*> Instances;

[[nodiscard]] crl::time RandomDelay() {
	return kMinDelay + base::RandomIndex(int(kMaxDelay - kMinDelay + 1));
}

// Telegram-style markup, the same that the message field accepts:
// **bold**, __italic__, ~~strike~~, ||spoiler||, `code`, [text](url) and
// premium emoji written as in the Bot API: [😀](tg://emoji?id=<document id>).
// Formatting may nest; code and emoji labels are taken literally.
[[nodiscard]] TextWithEntities ParseMarkup(const QString &text) {
	static const auto kMarkup = QRegularExpression(
		uR"re(\[([^\]\n]+)\]\(([^)\s]+)\)|\*\*(.+?)\*\*|__(.+?)__|~~(.+?)~~|\|\|(.+?)\|\||`([^`\n]+)`)re"_q,
		QRegularExpression::DotMatchesEverythingOption);
	static const auto kEmojiPrefix = u"tg://emoji?id="_q;

	auto result = TextWithEntities();
	const auto append = [&](
			EntityType type,
			const QString &inner,
			bool literal,
			const QString &data = QString()) {
		auto parsed = literal
			? TextWithEntities{ inner }
			: ParseMarkup(inner);
		const auto from = int(result.text.size());
		result.entities.push_back({
			type,
			from,
			int(parsed.text.size()),
			data,
		});
		for (auto entity : parsed.entities) {
			entity.shiftRight(from);
			result.entities.push_back(entity);
		}
		result.text.append(parsed.text);
	};

	auto offset = 0;
	auto matches = kMarkup.globalMatch(text);
	while (matches.hasNext()) {
		const auto match = matches.next();
		result.text.append(
			QStringView(text).mid(offset, match.capturedStart() - offset));
		offset = match.capturedEnd();

		if (match.capturedLength(1)) {
			const auto label = match.captured(1);
			const auto url = match.captured(2);
			const auto emojiId = url.startsWith(kEmojiPrefix)
				? url.mid(kEmojiPrefix.size()).toULongLong()
				: DocumentId();
			if (emojiId) {
				append(
					EntityType::CustomEmoji,
					label,
					true,
					Data::SerializeCustomEmojiId(emojiId));
			} else if (Ui::InputField::IsValidMarkdownLink(url)) {
				append(EntityType::CustomUrl, label, false, url);
			} else {
				result.text.append(match.captured(0));
			}
		} else if (match.capturedLength(3)) {
			append(EntityType::Bold, match.captured(3), false);
		} else if (match.capturedLength(4)) {
			append(EntityType::Italic, match.captured(4), false);
		} else if (match.capturedLength(5)) {
			append(EntityType::StrikeOut, match.captured(5), false);
		} else if (match.capturedLength(6)) {
			append(EntityType::Spoiler, match.captured(6), false);
		} else {
			append(EntityType::Code, match.captured(7), true);
		}
	}
	result.text.append(QStringView(text).mid(offset));
	return result;
}

[[nodiscard]] TextWithEntities ParseReplyText(const QString &reply) {
	auto result = ParseMarkup(reply);

	// ParseEntities() walks the existing entities in order, and so does the
	// text layout: outer ranges first, then the ones nested in them.
	ranges::stable_sort(result.entities, [](
			const EntityInText &a,
			const EntityInText &b) {
		return (a.offset() < b.offset())
			|| (a.offset() == b.offset() && a.length() > b.length());
	});
	TextUtilities::ParseEntities(
		result,
		TextParseLinks
			| TextParseMentions
			| TextParseHashtags
			| TextParseBotCommands);
	return result;
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

	// A client-side id, not nextNonHistoryEntryId(): ids from that range are
	// meant for FakeHistoryItem previews that never enter the message list.
	// With a client-side id the item is registered by History as a local
	// message, so it goes into the feed now and is put back into it whenever
	// the chat slice is reloaded, like the gift plate it follows.
	//
	// MessageFlag::Local, added by addNewLocalMessage(), keeps it in memory
	// only: it is never stored, never sent, and is gone after a restart.
	const auto self = history->session().user();
	history->addNewLocalMessage({
		.id = history->owner().nextLocalMessageId(),
		.flags = (MessageFlag::Outgoing | MessageFlag::HasFromId),
		.from = self->id,
		.date = base::unixtime::now(),
	}, ParseReplyText(reply), MTP_messageMediaEmpty());
}
