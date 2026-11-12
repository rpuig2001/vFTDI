#include "../src/CallsignLookup.h"
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
		config << "#SEQUENCEMODE DEPENDANT\n";
		config << "#RUNWAYPAIR EDDF 25L 25C ALTERNATING\n";
		config << "#RUNWAYPAIR EDDF 25L 25R DEPENDANT\n";
		config << "#RUNWAYPAIR EDDF 25C 25R DEPENDENT\n";
		config << "#RUNWAYPAIR eddf 7l 7r dependant\n";
		config << "#RUNWAYPAIR EDDF 08L 08R INDEPENDENT\n";
		config << "#RUNWAYPAIR EDDF 25R 25L UNKNOWN\n";
		config << "#RUNWAYPAIR EDDF 25L 25L DEPENDANT\n";
		config << "#RUNWAYPAIR EDDF 00L 25R DEPENDANT\n";
		config << "#RUNWAYPAIR EDDF 37L 25R DEPENDANT\n";
		config << "#RUNWAYPAIR EDDF 25L\n";
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
		output << "#SEQUENCEMODE ALTERNATING\n#SEQUENCEMODE UNKNOWN\n";
		output << "#RUNWAYPAIR EDDF 25L 25R DEPENDANT\n#RUNWAYPAIR EDDF 25R 25L alternating\n";
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
		output << "#PROFILE ALT\n#PROFILE DEP\n#PROFILE IND\n#DEFAULTPROFILE DEP\n";
		output << "#RUNWAYPAIR ALT EDDF 25L 25R ALTERNATING\n";
		output << "#RUNWAYPAIR DEP EDDF 25L 25R DEPENDANT\n";
		output << "#RUNWAYPAIR IND EDDF 25L 25R INDEPENDENT\n";
		output << "#RUNWAYPAIR IND EDDF 25L 25C ALTERNATING\n";
	}
	CCallsignLookup profiles(path);
	Check(profiles.getProfileNames() == std::vector<std::string>({ "ALT", "DEP", "IND" }),
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
	{
		std::ofstream output(path);
		output << "#PROFILE A\n#PROFILE BB\n#PROFILE CCC\n#PROFILE DDDD\n#PROFILE E\n#PROFILE F\n";
		output << "#DEFAULTPROFILE F\n";
	}
	CCallsignLookup boundedProfiles(path);
	Check(boundedProfiles.getProfileNames() == std::vector<std::string>({ "A", "BB", "CCC", "E", "F" }),
		"at most five profiles with one-to-three-letter names");
	Check(boundedProfiles.getDefaultProfile() == "F", "configured profile default");
	if (argumentCount == 3) {
		CCallsignLookup release(arguments[2]);
		Check(release.getProfileNames() == std::vector<std::string>({ "ALT", "DEP", "IND" }), "Release profiles");
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
			CCallsignLookup::RunwayPairMode::Dependant, "Release IND profile keeps 25C/25R Dependant");
		Check(release.getMinimumParallelDependentDistanceNm() == 1.5, "Release reduced spacing preserved");
	}
	std::remove(path);
	std::cout << "All runway-pair parsing, mode switching, normalization, and Alternating fallback checks passed.\n";
}