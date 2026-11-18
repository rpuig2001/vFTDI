#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "SequenceFeedData.h"

struct FTDiSession;

class SequenceFeedService
{
public:
	explicit SequenceFeedService(std::string url);
	~SequenceFeedService();
	bool GetSnapshot(std::vector<SequenceFeedData::Entry>& entries, unsigned long long& revision) const;

private:
	std::string Url;
	std::atomic<bool> Stop{ false };
	std::thread Worker;
	mutable std::mutex SnapshotMutex;
	std::mutex WaitMutex;
	std::condition_variable WaitCondition;
	std::vector<SequenceFeedData::Entry> Snapshot;
	unsigned long long Revision = 0;
	bool HasSnapshot = false;

	void Run();
	bool Refresh();
};

bool ApplySequenceFeedSnapshot(FTDiSession& session, SequenceFeedService* sequenceFeed);
