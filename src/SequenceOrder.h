#pragma once

#include <algorithm>
#include <string>
#include <vector>

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