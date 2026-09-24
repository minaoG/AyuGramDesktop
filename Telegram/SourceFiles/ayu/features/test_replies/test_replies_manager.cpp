// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/test_replies/test_replies_manager.h"

#include "base/random.h"
#include "rpl/combine.h"
#include "settings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

[[nodiscard]] QString StoragePath() {
	return cWorkingDir() + u"tdata/ayu_test_replies.json"_q;
}

// Blank-line separated blocks when the file has any, plain lines otherwise.
// Empty entries and exact duplicates are dropped.
[[nodiscard]] std::vector<QString> ParseReplies(QString text) {
	if (text.startsWith(QChar(0xFEFF))) {
		text.remove(0, 1);
	}
	text.replace(u"\r\n"_q, u"\n"_q);
	text.replace(QChar('\r'), QChar('\n'));

	static const auto blankLine = QRegularExpression(u"\\n[ \\t]*\\n"_q);
	const auto parts = text.contains(blankLine)
		? text.split(blankLine, Qt::SkipEmptyParts)
		: text.split(QChar('\n'), Qt::SkipEmptyParts);

	auto result = std::vector<QString>();
	auto seen = QSet<QString>();
	result.reserve(parts.size());
	for (const auto &part : parts) {
		const auto reply = part.trimmed();
		if (reply.isEmpty() || seen.contains(reply)) {
			continue;
		}
		seen.insert(reply);
		result.push_back(reply);
	}
	return result;
}

} // namespace

TestRepliesManager &TestRepliesManager::Instance() {
	static TestRepliesManager instance;
	return instance;
}

TestRepliesManager::TestRepliesManager() {
	load();
}

int TestRepliesManager::importFromFile(const QString &path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		LOG(("TestReplies: could not open '%1'.").arg(path));
		return -1;
	}
	return importFromData(file.readAll());
}

int TestRepliesManager::importFromData(const QByteArray &data) {
	auto replies = ParseReplies(QString::fromUtf8(data));
	if (replies.empty()) {
		return 0;
	}

	auto wasUsed = QSet<QString>();
	for (auto i = 0; i != int(_replies.size()); ++i) {
		if (_used[i]) {
			wasUsed.insert(_replies[i]);
		}
	}
	_used.assign(replies.size(), false);
	for (auto i = 0; i != int(replies.size()); ++i) {
		_used[i] = wasUsed.contains(replies[i]);
	}
	_replies = std::move(replies);

	refreshCounters();
	save();
	return int(_replies.size());
}

QString TestRepliesManager::getNextUnusedReply() {
	const auto remaining = _remaining.current();
	if (remaining <= 0) {
		return QString();
	}
	auto skip = base::RandomIndex(remaining);
	for (auto i = 0; i != int(_replies.size()); ++i) {
		if (_used[i] || skip-- > 0) {
			continue;
		}
		_used[i] = true;
		refreshCounters();
		save();
		return _replies[i];
	}
	Unexpected("Unused reply count is out of sync in TestRepliesManager.");
}

void TestRepliesManager::resetUsed() {
	_used.assign(_replies.size(), false);
	refreshCounters();
	save();
}

void TestRepliesManager::clear() {
	_replies.clear();
	_used.clear();
	refreshCounters();
	save();
}

int TestRepliesManager::remainingCount() const {
	return _remaining.current();
}

int TestRepliesManager::totalCount() const {
	return _total.current();
}

bool TestRepliesManager::isRunningLow() const {
	return (totalCount() > 0) && (remainingCount() < kLowThreshold);
}

rpl::producer<int> TestRepliesManager::remainingValue() const {
	return _remaining.value();
}

rpl::producer<int> TestRepliesManager::totalValue() const {
	return _total.value();
}

rpl::producer<bool> TestRepliesManager::runningLowValue() const {
	return rpl::combine(
		_remaining.value(),
		_total.value()
	) | rpl::map([](int remaining, int total) {
		return (total > 0) && (remaining < kLowThreshold);
	}) | rpl::distinct_until_changed();
}

void TestRepliesManager::refreshCounters() {
	_total = int(_replies.size());
	_remaining = int(ranges::count(_used, false));
}

void TestRepliesManager::load() {
	QFile file(StoragePath());
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		LOG(("TestReplies: storage file is corrupted, ignoring it."));
		return;
	}
	const auto root = document.object();
	for (const auto &value : root.value(u"replies"_q).toArray()) {
		_replies.push_back(value.toString());
	}
	_used.assign(_replies.size(), false);
	for (const auto &value : root.value(u"used"_q).toArray()) {
		const auto index = value.toInt(-1);
		if (index >= 0 && index < int(_used.size())) {
			_used[index] = true;
		}
	}
	refreshCounters();
}

void TestRepliesManager::save() const {
	auto replies = QJsonArray();
	auto used = QJsonArray();
	for (auto i = 0; i != int(_replies.size()); ++i) {
		replies.append(_replies[i]);
		if (_used[i]) {
			used.append(i);
		}
	}
	auto root = QJsonObject();
	root.insert(u"replies"_q, replies);
	root.insert(u"used"_q, used);

	QSaveFile file(StoragePath());
	if (!file.open(QIODevice::WriteOnly)
		|| file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0
		|| !file.commit()) {
		LOG(("TestReplies: could not write the storage file."));
	}
}
