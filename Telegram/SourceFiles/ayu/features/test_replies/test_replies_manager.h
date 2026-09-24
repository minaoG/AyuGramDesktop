// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "rpl/producer.h"
#include "rpl/variable.h"

#include <vector>

// Pool of canned test phrases imported from a local .txt file.
// Phrases are handed out at random without repeats until the pool is reset.
// The pool and the "used" marks survive restarts in tdata.
class TestRepliesManager final {
public:
	// The chat top bar shows a warning dot below this many unused phrases.
	static constexpr auto kLowThreshold = 15;

	static TestRepliesManager &Instance();

	// Replaces the pool with the phrases from a .txt file. Phrases are
	// separated by blank lines; a file without blank lines is split by
	// lines. Phrases already used in the old pool stay marked as used.
	// Returns the number of phrases read, or -1 if the file can't be read.
	int importFromFile(const QString &path);
	// Same, for file contents the picker returned without a local path.
	int importFromData(const QByteArray &data);

	// Returns a random unused phrase and marks it as used,
	// or an empty string once the pool is exhausted.
	[[nodiscard]] QString getNextUnusedReply();

	void resetUsed();
	void clear();

	[[nodiscard]] int remainingCount() const;
	[[nodiscard]] int totalCount() const;
	[[nodiscard]] bool isRunningLow() const;

	// Fire the current value first, then on every change.
	[[nodiscard]] rpl::producer<int> remainingValue() const;
	[[nodiscard]] rpl::producer<int> totalValue() const;
	[[nodiscard]] rpl::producer<bool> runningLowValue() const;

private:
	TestRepliesManager();

	void load();
	void save() const;
	void refreshCounters();

	std::vector<QString> _replies;
	std::vector<bool> _used;

	rpl::variable<int> _remaining = 0;
	rpl::variable<int> _total = 0;

};
