#include "stdafx.h"
#include "SequenceFeed.h"
#include "FTDiSession.h"
#include <chrono>
#include <utility>
#include <wininet.h>

namespace {
constexpr size_t MaximumFeedBytes = 2 * 1024 * 1024;
constexpr DWORD RequestTimeoutMs = 5000;
constexpr int PollIntervalSeconds = 10;
}

SequenceFeedService::SequenceFeedService(std::string url)
	: Url(std::move(url))
{
	if (!Url.empty())
		Worker = std::thread(&SequenceFeedService::Run, this);
}

SequenceFeedService::~SequenceFeedService()
{
	Stop.store(true);
	WaitCondition.notify_all();
	if (Worker.joinable())
		Worker.join();
}

bool SequenceFeedService::GetSnapshot(std::vector<SequenceFeedData::Entry>& entries,
	unsigned long long& revision) const
{
	std::lock_guard<std::mutex> lock(SnapshotMutex);
	if (!HasSnapshot)
		return false;
	entries = Snapshot;
	revision = Revision;
	return true;
}

bool ApplySequenceFeedSnapshot(FTDiSession& session, SequenceFeedService* sequenceFeed)
{
	if (!session.SequenceFeedActive || sequenceFeed == nullptr)
		return false;
	std::vector<SequenceFeedData::Entry> entries;
	unsigned long long revision = 0;
	if (!sequenceFeed->GetSnapshot(entries, revision) || revision == session.SequenceFeedRevision)
		return false;
	session.SequenceFeedRevision = revision;
	const bool changed = SequenceFeedData::ReplaceSequence(session.ArrivalSequence, entries);
	if (changed)
		session.SequenceDirty = true;
	return changed;
}

void SequenceFeedService::Run()
{
	while (!Stop.load()) {
		Refresh();
		std::unique_lock<std::mutex> lock(WaitMutex);
		WaitCondition.wait_for(lock, std::chrono::seconds(PollIntervalSeconds), [this]() {
			return Stop.load();
		});
	}
}

bool SequenceFeedService::Refresh()
{
	HINTERNET session = InternetOpenA("vFTDI", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
	if (session == nullptr)
		return false;
	DWORD timeoutMs = RequestTimeoutMs;
	InternetSetOptionA(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
	InternetSetOptionA(session, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
	InternetSetOptionA(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
	const DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
		(Url.compare(0, 8, "https://") == 0 ? INTERNET_FLAG_SECURE : 0);
	HINTERNET request = InternetOpenUrlA(session, Url.c_str(), nullptr, 0, flags, 0);
	if (request == nullptr) {
		InternetCloseHandle(session);
		return false;
	}

	std::string response;
	char buffer[8192];
	DWORD bytesRead = 0;
	bool readSucceeded = true;
	while (true) {
		if (!InternetReadFile(request, buffer, sizeof(buffer), &bytesRead)) {
			readSucceeded = false;
			break;
		}
		if (bytesRead == 0)
			break;
		if (response.size() + bytesRead > MaximumFeedBytes) {
			readSucceeded = false;
			break;
		}
		response.append(buffer, bytesRead);
	}
	InternetCloseHandle(request);
	InternetCloseHandle(session);
	if (!readSucceeded || response.empty())
		return false;

	std::vector<SequenceFeedData::Entry> freshSnapshot;
	if (!SequenceFeedData::Parse(response, freshSnapshot))
		return false;
	std::lock_guard<std::mutex> lock(SnapshotMutex);
	const bool changed = !HasSnapshot || freshSnapshot.size() != Snapshot.size() ||
		!std::equal(freshSnapshot.begin(), freshSnapshot.end(), Snapshot.begin(),
			[](const SequenceFeedData::Entry& first, const SequenceFeedData::Entry& second) {
				return first.Callsign == second.Callsign && first.RunwaySequence == second.RunwaySequence;
			});
	if (changed) {
		Snapshot = std::move(freshSnapshot);
		++Revision;
		HasSnapshot = true;
	}
	return true;
}
