#include "../src/CallsignLookup.h"
#include "../src/SequenceOrder.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

void Check(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main(int argumentCount, char** arguments)
{
	if (argumentCount != 2 && argumentCount != 3)
		return 1;
	const char* path = arguments[1];
	{
		std::ofstream config(path);
		config << "[general]\nsequenceMode = DEPENDANT\n";
		config << "[profile.ALT]\nEDDF.25L.25C = ALTERNATING\nEDDF.25L.25R = ALTERNATING\n";
		config << "[profile.DEP]\n";
		config << "EDDF.25L.25C = ALTERNATING\nEDDF.25L.25R = DEPENDANT\n";
		config << "EDDF.25C.25R = DEPENDENT\nEDDF.7l.7r = dependant\n";
		config << "EDDF.08L.08R = INDEPENDENT\nEDDF.25R.25L = UNKNOWN\n";
		config << "EDDF.25L.25L = DEPENDANT\nEDDF.00L.25R = DEPENDANT\n";
		config << "EDDF.37L.25R = DEPENDANT\nEDDF.25L = DEPENDANT\n";
	}
	CCallsignLookup config(path);
	Check(config.getDependentMode(), "configured default mode");
	Check(!config.usesDependentSpacing(true, "EDDF", "25L", "EDDF", "25C"), "25L/25C remain Alternating");
	Check(config.usesDependentSpacing(true, "EDDF", "25L", "EDDF", "25R"), "25L/25R dependent");
	Check(config.usesDependentSpacing(true, "EDDF", "25R", "EDDF", "25L"), "reversed pair uses same rule");
	Check(config.usesDependentSpacing(true, "EDDF", "25C", "EDDF", "25R"), "25C/25R dependent");
	Check(config.getRunwayPairMode("DEP", "EDDF", "08L", "EDDF", "08R") ==
		CCallsignLookup::RunwayPairMode::Independent, "configured independent runway pair");
	Check(config.getRunwayPairMode("DEP", "EDDF", "08R", "EDDF", "08L") ==
		CCallsignLookup::RunwayPairMode::Independent, "independent runway rule is symmetric");
	Check(config.getRunwayPairMode("ALT", "EDDF", "08L", "EDDF", "08R") ==
		CCallsignLookup::RunwayPairMode::Alternating, "global Alternating overrides independent runway rule");
	Check(config.getRunwayPairMode("DEP", "EDDF", "08L", "EDDF", "08C") ==
		CCallsignLookup::RunwayPairMode::Alternating, "unlisted pair remains Alternating");
	Check(!config.usesDependentSpacing(false, "EDDF", "25L", "EDDF", "25R"), "Alternating overrides dependent rule");
	Check(config.usesDependentSpacing(true, "eddf", "7L", "EDDF", "07r"), "case and runway number normalization");
	Check(!config.usesDependentSpacing(true, "EDDF", "25L", "EDDF", "25L"), "same runway always Alternating");
	Check(!config.usesDependentSpacing(true, "EDDF", "25L", "EDDF", "07R"), "unlisted pair Alternating");
	Check(!config.usesDependentSpacing(true, "EDDF", "25L", "EDDM", "25R"), "different destinations Alternating");
	Check(!config.usesDependentSpacing(true, "EDDF", "", "EDDF", "25R"), "missing runway Alternating");
	Check(!config.usesDependentSpacing(true, "", "25L", "", "25R"), "missing airports Alternating");
	Check(!config.usesDependentSpacing(true, "EDDF", "00L", "EDDF", "25R"), "invalid runway ignored");
	Check(!config.usesDependentSpacing(true, "EDDF", "37L", "EDDF", "25R"), "out-of-range runway ignored");
	Check(config.usesDependentSpacing(true, "EDDF", "25L", "EDDF", "25R"), "switch back to Dependant");
	Check(!config.usesDependentSpacing(true, "EDDF", "25L", "EDDF", "25C"), "runway reassignment changes pair mode");
	{
		std::ofstream output(path);
		output << "[general]\nsequenceMode = ALTERNATING\nsequenceMode = UNKNOWN\n";
		output << "[profile.DEP]\nEDDF.25L.25R = DEPENDANT\nEDDF.25R.25L = alternating\n";
	}
	CCallsignLookup overridden(path);
	Check(!overridden.getDependentMode(), "Alternating configuration and invalid mode fallback");
	Check(!overridden.usesDependentSpacing(true, "EDDF", "25L", "EDDF", "25R"), "last valid symmetric rule wins");
	{
		std::ofstream output(path);
	}
	CCallsignLookup defaults(path);
	Check(!defaults.getDependentMode(), "missing mode defaults to Alternating");
	Check(!defaults.usesDependentSpacing(true, "EDDF", "25L", "EDDF", "25R"), "no rules means Alternating");
	{
		std::ofstream output(path);
		output << "[profiles]\ndefault = DEP\n";
		output << "[profile.ALT]\nEDDF.25L.25R = ALTERNATING\n";
		output << "[profile.DEP]\nEDDF.25L.25R = DEPENDANT\n";
		output << "[profile.IND]\nEDDF.25L.25R = INDEPENDENT\nEDDF.25L.25C = ALTERNATING\nEDDF.25C.25R = INDEPENDENT\n";
		output << "[profile.IN2]\nEDDF.25L.25C = ALTERNATING\n";
	}
	CCallsignLookup profiles(path);
	Check(profiles.getProfileNames() == std::vector<std::string>({ "ALT", "DEP", "IND", "IN2" }),
		"named profile list");
	Check(profiles.getDefaultProfile() == "DEP", "default named profile");
	Check(profiles.getRunwayPairMode("ALT", "EDDF", "25L", "EDDF", "25R") ==
		CCallsignLookup::RunwayPairMode::Alternating, "ALT profile runway mode");
	Check(profiles.getRunwayPairMode("DEP", "EDDF", "25L", "EDDF", "25R") ==
		CCallsignLookup::RunwayPairMode::Dependant, "DEP profile runway mode");
	Check(profiles.getRunwayPairMode("IND", "EDDF", "25L", "EDDF", "25R") ==
		CCallsignLookup::RunwayPairMode::Independent, "IND profile runway mode");
	Check(profiles.getRunwayPairMode("IND", "EDDF", "25L", "EDDF", "25C") ==
		CCallsignLookup::RunwayPairMode::Alternating, "profile keeps 25L/25C Alternating");
	const std::vector<std::string> runways = { "25R", "25L", "25R", "25C", "25L", "25R", "25C" };
	const auto predecessor = [&](size_t index, const char* profile) {
		return FindSequencePredecessor(index, [&](size_t candidate) {
			return profiles.getRunwayPairMode(profile, "EDDF", runways[candidate], "EDDF", runways[index]) !=
				CCallsignLookup::RunwayPairMode::Independent;
		});
	};
	Check(predecessor(1, "IND") == 1, "25L has no eligible predecessor behind 25R");
	Check(predecessor(2, "IND") == 0, "25R skips independent 25L to find 25R");
	Check(predecessor(3, "IND") == 1, "25C skips independent 25R to find 25L");
	Check(predecessor(4, "IND") == 3, "25L follows alternating 25C");
	Check(predecessor(5, "IND") == 2, "25R skips both independent runways");
	Check(predecessor(6, "IND") == 4, "25C skips 25R to find the nearest alternating 25L");
	Check(predecessor(3, "ALT") == 2, "alternating profile retains adjacent predecessor");
	Check(predecessor(3, "DEP") == 2, "dependent pairs remain eligible predecessors");
	Check(profiles.getRunwayPairMode("IN2", "EDDF", "25L", "EDDF", "25C") ==
		CCallsignLookup::RunwayPairMode::Alternating, "numeric profile name runway mode");
	{
		std::ofstream output(path);
		output << "[profiles]\ndefault = F\n";
		output << "[profile.A]\n[profile.BB]\n[profile.CCC]\n[profile.DDDD]\n";
		output << "[profile.E]\n[profile.F]\n";
	}
	CCallsignLookup boundedProfiles(path);
	Check(boundedProfiles.getProfileNames() == std::vector<std::string>({ "A", "BB", "CCC", "E", "F" }),
		"at most five profiles with one-to-three-character alphanumeric names");
	Check(boundedProfiles.getDefaultProfile() == "F", "configured profile default");
	if (argumentCount == 3) {
		CCallsignLookup release(arguments[2]);
		Check(release.getProfileNames() == std::vector<std::string>({ "ALT", "DEP", "IND", "IN2" }), "INI profiles");
		Check(release.getDefaultProfile() == "DEP", "Release defaults to DEP profile");
		Check(release.getRunwayPairMode("ALT", "EDDF", "25L", "EDDF", "25R") ==
			CCallsignLookup::RunwayPairMode::Alternating, "Release ALT profile");
		Check(release.getRunwayPairMode("DEP", "EDDF", "25L", "EDDF", "25C") ==
			CCallsignLookup::RunwayPairMode::Alternating, "Release 25L/25C Alternating in DEP profile");
		Check(release.getRunwayPairMode("DEP", "EDDF", "25L", "EDDF", "25R") ==
			CCallsignLookup::RunwayPairMode::Dependant, "Release DEP profile");
		Check(release.getRunwayPairMode("IND", "EDDF", "25L", "EDDF", "25R") ==
			CCallsignLookup::RunwayPairMode::Independent, "Release IND profile");
		Check(release.getRunwayPairMode("IND", "EDDF", "25L", "EDDF", "25C") ==
			CCallsignLookup::RunwayPairMode::Alternating, "Release IND profile keeps 25L/25C Alternating");
		Check(release.getRunwayPairMode("IND", "EDDF", "25C", "EDDF", "25R") ==
			CCallsignLookup::RunwayPairMode::Independent, "INI IND profile keeps 25C/25R Independent");
		Check(release.getMinimumParallelDependentDistanceNm() == 1.5, "Release reduced spacing preserved");
		double course = 0.0;
		Check(release.getLocalizerCourseDegrees("EDDF", "25L", course) && course == 249.0,
			"INI runway course");
	}
	std::remove(path);
	std::cout << "All runway-pair parsing, mode switching, normalization, and Alternating fallback checks passed.\n";
}