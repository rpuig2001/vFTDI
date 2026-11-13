#include "../src/CallsignLookup.h"
#include "../src/SequenceOrder.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

void CheckRows(const char* path, const char* text, int expected)
{
	{
		std::ofstream config(path);
		config << text;
	}
	CCallsignLookup config(path);
	if (config.getDefaultVisibleRows() != expected) {
		std::cerr << "FAIL: expected " << expected << " rows for " << text << '\n';
		std::exit(1);
	}
}

int main(int argumentCount, char** arguments)
{
	if (argumentCount != 2 && argumentCount != 3)
		return 1;
	const char* path = arguments[1];
	CheckRows(path, "", 3);
	CheckRows(path, "[general]\ndefaultRows = 6\n", 6);
	CheckRows(path, "[general]\ndefaultRows = 1\n", 1);
	CheckRows(path, "[general]\ndefaultRows = 12\n", 12);
	CheckRows(path, "[general]\ndefaultRows = 0\n", 1);
	CheckRows(path, "[general]\ndefaultRows = -5\n", 1);
	CheckRows(path, "[general]\ndefaultRows = 100\n", 100);
	CheckRows(path, "[general]\ndefaultRows = 999\n", 999);
	CheckRows(path, "[general]\ndefaultRows = invalid\n", 3);
	CheckRows(path, "[general]\ndefaultRows = 6\ndefaultRows = invalid\n", 6);
	CheckRows(path, "[general]\ndefaultRows = 6\ndefaultRows = 8\n", 8);
	CheckRows(path, "[general]\ndefaultRows = 999999999999999999999\n", 3);
	std::vector<std::string> sequence = { "ALPHA", "BRAVO", "CHARLIE", "DELTA" };
	if (SequenceNumber(sequence, "ALPHA") != 1 || SequenceNumber(sequence, "DELTA") != 4 ||
		SequenceNumber(sequence, "MISSING") != 0 || SequenceNumber({}, "ALPHA") != 0) {
		std::cerr << "FAIL: sequence numbers must be one-based and absent flights must have no number\n";
		return 1;
	}
	if (!MoveSequenceAircraft(sequence, "DELTA", "BRAVO") ||
		sequence != std::vector<std::string>({ "ALPHA", "DELTA", "BRAVO", "CHARLIE" })) {
		std::cerr << "FAIL: moving an aircraft earlier should place it before the chosen callsign\n";
		return 1;
	}
	if (!MoveSequenceAircraft(sequence, "ALPHA", "CHARLIE") ||
		sequence != std::vector<std::string>({ "DELTA", "BRAVO", "ALPHA", "CHARLIE" })) {
		std::cerr << "FAIL: moving an aircraft later should place it before the chosen callsign\n";
		return 1;
	}
	if (!MoveSequenceAircraft(sequence, "BRAVO", "") ||
		sequence != std::vector<std::string>({ "DELTA", "ALPHA", "CHARLIE", "BRAVO" })) {
		std::cerr << "FAIL: moving an aircraft to the end should preserve the remaining order\n";
		return 1;
	}
	if (MoveSequenceAircraft(sequence, "BRAVO", "") ||
		MoveSequenceAircraft(sequence, "MISSING", "ALPHA") ||
		MoveSequenceAircraft(sequence, "ALPHA", "MISSING") ||
		sequence != std::vector<std::string>({ "DELTA", "ALPHA", "CHARLIE", "BRAVO" })) {
		std::cerr << "FAIL: no-op or stale move choices must not change the sequence\n";
		return 1;
	}
	if (!MoveSequenceRelative(sequence, "DELTA", "CHARLIE", true) ||
		sequence != std::vector<std::string>({ "ALPHA", "CHARLIE", "DELTA", "BRAVO" })) {
		std::cerr << "FAIL: moving after a selected row should insert immediately after it\n";
		return 1;
	}
	if (!MoveSequenceRelative(sequence, "BRAVO", "ALPHA", false) ||
		sequence != std::vector<std::string>({ "BRAVO", "ALPHA", "CHARLIE", "DELTA" })) {
		std::cerr << "FAIL: moving before a selected row should insert immediately before it\n";
		return 1;
	}
	if (MoveSequenceRelative(sequence, "ALPHA", "BRAVO", true) ||
		MoveSequenceRelative(sequence, "MISSING", "ALPHA", false) ||
		sequence != std::vector<std::string>({ "BRAVO", "ALPHA", "CHARLIE", "DELTA" })) {
		std::cerr << "FAIL: adjacent and stale relative moves should not reorder the sequence\n";
		return 1;
	}
	if (SequenceNumber(sequence, "BRAVO") != 1 || SequenceNumber(sequence, "DELTA") != 4) {
		std::cerr << "FAIL: sequence numbers must reflect the current order after moves\n";
		return 1;
	}
	if (argumentCount == 3) {
		CCallsignLookup release(arguments[2]);
		if (release.getDefaultVisibleRows() != 999) {
			std::cerr << "FAIL: Release default should allow 999 visible rows\n";
			return 1;
		}
	}
	std::remove(path);
	std::cout << "Panel row-count parsing, sequence numbering, and relative sequence-move checks passed.\n";
}