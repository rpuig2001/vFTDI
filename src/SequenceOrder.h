#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

constexpr double AutoSequenceMinimumAverageSavingSeconds = 5.0;
constexpr double AutoSequenceMaximumIndividualDelaySeconds = 30.0;
constexpr size_t AutoSequenceMaximumForwardPositions = 2;

struct SuggestionMenuDismissDetector {
	bool Armed = false;

	void Reset()
	{
		Armed = false;
	}

	bool Update(bool active, bool foreground, bool leftDown, bool rightDown, bool middleDown, bool escapeDown)
	{
		if (!active) {
			Reset();
			return false;
		}
		if (!foreground)
			return true;
		if (!Armed) {
			if (!leftDown && !rightDown && !middleDown && !escapeDown)
				Armed = true;
			return false;
		}
		return leftDown || rightDown || middleDown || escapeDown;
	}
};

inline bool IsCurrentSuggestionMenuDismissal(unsigned long long dismissedGeneration,
	unsigned long long currentGeneration, bool active)
{
	return active && dismissedGeneration != 0 && dismissedGeneration == currentGeneration;
}

inline bool IsAutomaticDistanceEligible(double distanceNm, double maximumDistanceNm)
{
	return maximumDistanceNm <= 0.0 || (std::isfinite(distanceNm) && distanceNm >= 0.0 && distanceNm <= maximumDistanceNm);
}

inline size_t SequenceNumber(const std::vector<std::string>& sequence, const std::string& callsign)
{
	const auto aircraft = std::find(sequence.begin(), sequence.end(), callsign);
	return aircraft == sequence.end() ? 0 : static_cast<size_t>(aircraft - sequence.begin()) + 1;
}

template <typename IsEligible>
inline size_t FindSequencePredecessor(size_t index, IsEligible isEligible)
{
	for (size_t candidate = index; candidate > 0; --candidate) {
		if (isEligible(candidate - 1))
			return candidate - 1;
	}
	return index;
}

inline bool MoveSequenceAircraft(
	std::vector<std::string>& sequence,
	const std::string& callsign,
	const std::string& beforeCallsign)
{
	const auto source = std::find(sequence.begin(), sequence.end(), callsign);
	if (source == sequence.end() || callsign == beforeCallsign)
		return false;
	if (beforeCallsign.empty()) {
		if (source + 1 == sequence.end())
			return false;
	}
	else {
		const auto destination = std::find(sequence.begin(), sequence.end(), beforeCallsign);
		if (destination == sequence.end() || destination == source + 1)
			return false;
	}
	const std::string movedCallsign = *source;
	sequence.erase(source);
	const auto insertionPoint = beforeCallsign.empty() ? sequence.end()
		: std::find(sequence.begin(), sequence.end(), beforeCallsign);
	sequence.insert(insertionPoint, movedCallsign);
	return true;
}

inline bool MoveSequenceRelative(
	std::vector<std::string>& sequence,
	const std::string& callsign,
	const std::string& targetCallsign,
	bool placeBelow)
{
	auto source = std::find(sequence.begin(), sequence.end(), callsign);
	const auto target = std::find(sequence.begin(), sequence.end(), targetCallsign);
	if (source == sequence.end() || target == sequence.end() || source == target ||
		(!placeBelow && source + 1 == target) || (placeBelow && target + 1 == source))
		return false;
	const std::string movedCallsign = *source;
	sequence.erase(source);
	auto insertionPoint = std::find(sequence.begin(), sequence.end(), targetCallsign);
	if (placeBelow)
		++insertionPoint;
	sequence.insert(insertionPoint, movedCallsign);
	return true;
}

struct SequenceArrivalEstimate {
	std::string Callsign;
	double ArrivalSeconds;
	bool Locked;
	bool ClearedForIls = false;
};

struct SequenceOptimization {
	std::vector<std::string> Order;
	double AverageImprovementSeconds = 0.0;
	bool SearchComplete = true;
	struct Candidate {
		std::vector<std::string> Order;
		double AverageImprovementSeconds = 0.0;
	};
	std::vector<Candidate> Candidates;
};

inline bool AppendAutomaticArrival(std::vector<std::string>& order, const std::string& callsign)
{
	if (std::find(order.begin(), order.end(), callsign) != order.end())
		return false;
	order.push_back(callsign);
	return true;
}

inline bool IsCurrentSequenceProposal(const std::vector<std::string>& current,
	const std::vector<std::string>& proposalBase, const std::vector<std::string>& proposalOrder,
	const SequenceOptimization& refreshed, double minimumImprovementSeconds)
{
	return !proposalOrder.empty() && current == proposalBase && proposalOrder == refreshed.Order &&
		refreshed.AverageImprovementSeconds >= minimumImprovementSeconds;
}

inline bool ShouldEvaluateAutomaticSequence(double nowSeconds, double nextEvaluationSeconds,
	const std::vector<std::string>& current, const std::vector<std::string>& previous, bool accepting)
{
	return accepting || nowSeconds >= nextEvaluationSeconds || current != previous;
}

inline bool IsSingleForwardInsertion(const std::vector<std::string>& original,
	const std::vector<std::string>& proposed)
{
	if (original.size() != proposed.size())
		return false;
	size_t first = 0;
	while (first < original.size() && original[first] == proposed[first])
		++first;
	if (first == original.size())
		return false;
	const auto source = std::find(original.begin() + first + 1, original.end(), proposed[first]);
	if (source == original.end())
		return false;
	std::vector<std::string> candidate = original;
	std::rotate(candidate.begin() + first, candidate.begin() + (source - original.begin()),
		candidate.begin() + (source - original.begin()) + 1);
	return candidate == proposed;
}

inline bool SequenceProposalMove(const std::vector<std::string>& original,
	const std::vector<std::string>& proposed, std::string& moved, std::string& before)
{
	moved.clear();
	before.clear();
	if (!IsSingleForwardInsertion(original, proposed))
		return false;
	size_t first = 0;
	while (original[first] == proposed[first])
		++first;
	moved = proposed[first];
	before = original[first];
	return true;
}

inline std::vector<SequenceOptimization::Candidate> BestNonConflictingSequenceProposals(
	const std::vector<std::string>& base, std::vector<SequenceOptimization::Candidate> candidates,
	size_t maximumProposals)
{
	std::sort(candidates.begin(), candidates.end(), [](const SequenceOptimization::Candidate& first,
		const SequenceOptimization::Candidate& second) {
		return first.AverageImprovementSeconds > second.AverageImprovementSeconds;
	});
	std::set<std::string> involvedCallsigns;
	std::vector<SequenceOptimization::Candidate> selected;
	for (const SequenceOptimization::Candidate& candidate : candidates) {
		if (selected.size() >= maximumProposals)
			break;
		std::string moved;
		std::string before;
		if (!SequenceProposalMove(base, candidate.Order, moved, before) ||
			involvedCallsigns.count(moved) != 0 || involvedCallsigns.count(before) != 0)
			continue;
		involvedCallsigns.insert(moved);
		involvedCallsigns.insert(before);
		selected.push_back(candidate);
	}
	return selected;
}

inline bool SequenceProposalInvolves(const std::vector<std::string>& current,
	const std::vector<std::string>& base, const std::vector<std::string>& proposed,
	const std::string& callsign)
{
	std::string moved;
	std::string before;
	return !callsign.empty() && current == base && SequenceProposalMove(base, proposed, moved, before) &&
		(callsign == moved || callsign == before);
}

inline bool IsSameLiveSequenceProposal(const std::vector<std::string>& current,
	const std::vector<std::string>& base, const std::vector<std::string>& proposed,
	const std::vector<std::string>& selectedBase, const std::vector<std::string>& selectedProposal)
{
	return !proposed.empty() && current == base && base == selectedBase && proposed == selectedProposal;
}

inline bool BeginSequenceProposalPreview(const std::vector<std::string>& current,
	const std::vector<std::string>& base, const std::vector<std::string>& proposed,
	std::vector<std::string>& previewBase, std::vector<std::string>& previewOrder)
{
	std::string moved;
	std::string before;
	if (current != base || !SequenceProposalMove(base, proposed, moved, before))
		return false;
	previewBase = base;
	previewOrder = proposed;
	return true;
}

inline bool IsConservativeSequenceProposal(const std::vector<std::string>& original,
	const std::vector<std::string>& proposed, const std::map<std::string, double>& delayChanges)
{
	if (!IsSingleForwardInsertion(original, proposed) || delayChanges.size() != original.size())
		return false;
	size_t first = 0;
	while (first < original.size() && original[first] == proposed[first])
		++first;
	const auto source = std::find(original.begin() + first + 1, original.end(), proposed[first]);
	if (source == original.end() || static_cast<size_t>(source - original.begin()) - first > AutoSequenceMaximumForwardPositions)
		return false;
	for (const std::string& callsign : original) {
		const auto change = delayChanges.find(callsign);
		if (change == delayChanges.end() || !std::isfinite(change->second) ||
			change->second > AutoSequenceMaximumIndividualDelaySeconds)
			return false;
	}
	return true;
}

template <typename SeparationSeconds>
inline bool PreservesSequenceGapPairs(const std::vector<std::string>& order,
	const std::vector<std::pair<std::string, std::string>>& gapPairs, SeparationSeconds separationSeconds)
{
	for (const auto& pair : gapPairs) {
		const auto follower = std::find(order.begin(), order.end(), pair.second);
		if (follower == order.end())
			return false;
		auto preceding = follower;
		bool found = false;
		while (preceding != order.begin()) {
			--preceding;
			const double separation = separationSeconds(*preceding, pair.second);
			if (!std::isfinite(separation))
				return false;
			if (separation >= 0.0) {
				found = *preceding == pair.first;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

template <typename SeparationSeconds>
inline size_t GapSafeInsertionIndex(const std::vector<std::string>& order, const std::string& callsign,
	size_t preferredIndex, const std::vector<std::pair<std::string, std::string>>& gapPairs,
	SeparationSeconds separationSeconds)
{
	for (size_t index = (std::min)(preferredIndex, order.size()); index < order.size(); ++index) {
		std::vector<std::string> candidate = order;
		candidate.insert(candidate.begin() + index, callsign);
		if (PreservesSequenceGapPairs(candidate, gapPairs, separationSeconds))
			return index;
	}
	return order.size();
}

template <typename SeparationSeconds>
inline bool SequenceIndividualDelaysSeconds(const std::vector<SequenceArrivalEstimate>& arrivals,
	SeparationSeconds separationSeconds, std::vector<double>& delays)
{
	std::vector<double> scheduled;
	delays.clear();
	for (size_t index = 0; index < arrivals.size(); ++index) {
		if (!std::isfinite(arrivals[index].ArrivalSeconds) || arrivals[index].ArrivalSeconds < 0.0)
			return false;
		double slot = arrivals[index].ArrivalSeconds;
		for (size_t previous = 0; previous < index; ++previous) {
			const double separation = separationSeconds(arrivals[previous].Callsign, arrivals[index].Callsign);
			if (!std::isfinite(separation))
				return false;
			if (separation >= 0.0)
				slot = (std::max)(slot, scheduled[previous] + separation);
		}
		scheduled.push_back(slot);
		delays.push_back(slot - arrivals[index].ArrivalSeconds);
	}
	return true;
}

template <typename SeparationSeconds>
inline double SequenceTotalDelaySeconds(const std::vector<SequenceArrivalEstimate>& arrivals,
	SeparationSeconds separationSeconds)
{
	std::vector<double> delays;
	if (!SequenceIndividualDelaysSeconds(arrivals, separationSeconds, delays))
		return std::numeric_limits<double>::infinity();
	double totalDelay = 0.0;
	for (double delay : delays)
		totalDelay += delay;
	return totalDelay;
}

template <typename SeparationSeconds, typename IsOrderAllowed>
inline double SequenceProposalImprovementSeconds(const std::vector<SequenceArrivalEstimate>& arrivals,
	const std::vector<std::string>& proposed, SeparationSeconds separationSeconds, IsOrderAllowed isOrderAllowed,
	std::map<std::string, double>* delayChanges = nullptr)
{
	if (delayChanges != nullptr)
		delayChanges->clear();
	if (arrivals.size() < 2 || proposed.size() != arrivals.size() || !isOrderAllowed(proposed))
		return -std::numeric_limits<double>::infinity();
	std::vector<std::string> currentOrder;
	for (const auto& arrival : arrivals)
		currentOrder.push_back(arrival.Callsign);
	std::string moved;
	std::string before;
	if (!SequenceProposalMove(currentOrder, proposed, moved, before))
		return -std::numeric_limits<double>::infinity();
	const auto movingAircraft = std::find_if(arrivals.begin(), arrivals.end(), [&moved](const SequenceArrivalEstimate& arrival) {
		return arrival.Callsign == moved;
	});
	if (movingAircraft == arrivals.end() || movingAircraft->ClearedForIls)
		return -std::numeric_limits<double>::infinity();
	std::vector<SequenceArrivalEstimate> reordered;
	std::vector<bool> used(arrivals.size(), false);
	size_t lockedPrefix = 0;
	for (size_t index = 0; index < arrivals.size(); ++index) {
		if (arrivals[index].Locked)
			lockedPrefix = index + 1;
	}
	for (size_t index = 0; index < proposed.size(); ++index) {
		if (index < lockedPrefix && proposed[index] != arrivals[index].Callsign)
			return -std::numeric_limits<double>::infinity();
		const auto found = std::find_if(arrivals.begin(), arrivals.end(), [&proposed, index](const SequenceArrivalEstimate& arrival) {
			return arrival.Callsign == proposed[index];
		});
		if (found == arrivals.end())
			return -std::numeric_limits<double>::infinity();
		const size_t originalIndex = static_cast<size_t>(found - arrivals.begin());
		if (used[originalIndex])
			return -std::numeric_limits<double>::infinity();
		used[originalIndex] = true;
		reordered.push_back(*found);
	}
	std::vector<double> currentDelays;
	std::vector<double> proposedDelays;
	if (!SequenceIndividualDelaysSeconds(arrivals, separationSeconds, currentDelays) ||
		!SequenceIndividualDelaysSeconds(reordered, separationSeconds, proposedDelays))
		return -std::numeric_limits<double>::infinity();
	std::map<std::string, double> originalDelays;
	for (size_t index = 0; index < arrivals.size(); ++index)
		originalDelays[arrivals[index].Callsign] = currentDelays[index];
	double totalChange = 0.0;
	for (size_t index = 0; index < reordered.size(); ++index) {
		const double change = proposedDelays[index] - originalDelays[reordered[index].Callsign];
		totalChange += change;
		if (delayChanges != nullptr)
			(*delayChanges)[reordered[index].Callsign] = change;
	}
	return -totalChange / arrivals.size();
}

template <typename SeparationSeconds>
inline SequenceOptimization BestSequenceInsertionImprovement(
	const std::vector<SequenceArrivalEstimate>& arrivals, SeparationSeconds separationSeconds,
	const std::vector<std::vector<std::string>>& rejectedOrders = {}, size_t maxPairChecks = 25000000,
	const std::function<bool(const std::vector<std::string>&)>& isOrderAllowed = {})
{
	SequenceOptimization best;
	if (arrivals.size() < 2)
		return best;
	const size_t count = arrivals.size();
	std::vector<std::vector<double>> separations(count, std::vector<double>(count, -1.0));
	size_t firstEligible = 0;
	for (size_t index = 0; index < count; ++index) {
		if (!std::isfinite(arrivals[index].ArrivalSeconds) || arrivals[index].ArrivalSeconds < 0.0)
			return best;
		if (arrivals[index].Locked)
			firstEligible = index + 1;
		for (size_t other = 0; other < count; ++other) {
			if (index != other)
				separations[index][other] = separationSeconds(arrivals[index].Callsign, arrivals[other].Callsign);
		}
	}
	std::vector<size_t> order(count);
	for (size_t index = 0; index < count; ++index)
		order[index] = index;
	size_t remainingChecks = maxPairChecks;
	bool budgetExceeded = false;
	const auto totalDelay = [&arrivals, &separations, &remainingChecks, &budgetExceeded, count](
		const std::vector<size_t>& indices, double cutoff) {
		std::vector<double> scheduled(count);
		double delay = 0.0;
		for (size_t index = 0; index < count; ++index) {
			if (index > remainingChecks) {
				budgetExceeded = true;
				return std::numeric_limits<double>::infinity();
			}
			remainingChecks -= index;
			const size_t current = indices[index];
			double slot = arrivals[current].ArrivalSeconds;
			for (size_t previous = 0; previous < index; ++previous) {
				const double separation = separations[indices[previous]][current];
				if (!std::isfinite(separation))
					return std::numeric_limits<double>::infinity();
				if (separation >= 0.0)
					slot = (std::max)(slot, scheduled[previous] + separation);
			}
			scheduled[index] = slot;
			delay += slot - arrivals[current].ArrivalSeconds;
			if (delay >= cutoff)
				return delay;
		}
		return delay;
	};
	const double currentDelay = totalDelay(order, std::numeric_limits<double>::infinity());
	if (!std::isfinite(currentDelay)) {
		best.SearchComplete = !budgetExceeded;
		return best;
	}
	double bestDelay = currentDelay;
	const auto consider = [&]() {
		const double candidateDelay = totalDelay(order, currentDelay);
		if (candidateDelay < currentDelay) {
			std::vector<std::string> candidateOrder;
			for (size_t index : order)
				candidateOrder.push_back(arrivals[index].Callsign);
			if (std::find(rejectedOrders.begin(), rejectedOrders.end(), candidateOrder) == rejectedOrders.end() &&
				(!isOrderAllowed || isOrderAllowed(candidateOrder))) {
				const double improvement = (currentDelay - candidateDelay) / count;
				best.Candidates.push_back({ candidateOrder, improvement });
				if (candidateDelay < bestDelay) {
					bestDelay = candidateDelay;
					best.AverageImprovementSeconds = improvement;
					best.Order = candidateOrder;
				}
			}
		}
	};
	if (currentDelay <= 0.0)
		return best;
	for (size_t source = count; source > firstEligible + 1;) {
		--source;
		if (arrivals[source].ClearedForIls)
			continue;
		const size_t earliestDestination = (std::max)(firstEligible,
			source > AutoSequenceMaximumForwardPositions ? source - AutoSequenceMaximumForwardPositions : size_t(0));
		for (size_t destination = earliestDestination; destination < source; ++destination) {
			std::rotate(order.begin() + destination, order.begin() + source, order.begin() + source + 1);
			consider();
			std::rotate(order.begin() + destination, order.begin() + destination + 1, order.begin() + source + 1);
			if (budgetExceeded) {
				best.SearchComplete = false;
				return best;
			}
		}
	}
	return best;
}