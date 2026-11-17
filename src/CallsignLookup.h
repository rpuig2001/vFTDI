#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class CCallsignLookup
{
public:
	enum class RunwayPairMode {
		Alternating,
		Dependant,
		Independent
	};

	struct RgbColor {
		int Red;
		int Green;
		int Blue;
	};

	explicit CCallsignLookup(const std::string& fileName);
	int getValue(const std::string& key) const;
	bool isMode2() const { return displayMode == 2; }
	double getMode2Setting(const std::string& key, double fallback) const;
	int getMinimumAltitude() const;
	static std::string normalizeRunway(const std::string& runway);
	bool isConfiguredArrivalRunway(const std::string& airport, const std::string& runway) const;
	bool hasConfiguredArrivalRunways(const std::string& airport) const;
	double getMaximumAutoDistanceNm() const;
	int getDefaultVisibleRows() const;
	std::string getProposalTagText() const;
	bool getProposalsEnabled() const { return proposalsEnabled; }
	int getMarkerSizePixels(bool finalTarget) const;
	double getMinimumDistanceNm() const;
	double getMinimumParallelDependentDistanceNm() const;
	double getFinalBufferNm() const;
	double getRequiredSpacingNm(char precedingWake, char succeedingWake,
		const char* precedingAircraftInfo, const char* succeedingAircraftInfo) const;
	bool getShowLocalizerRoute() const;
	bool toggleShowLocalizerRoute();
	bool getDependentMode() const;
	std::vector<std::string> getProfileNames() const;
	std::string getDefaultProfile() const;
	RunwayPairMode getRunwayPairMode(const std::string& profile,
		const std::string& precedingAirport, const std::string& precedingRunway,
		const std::string& succeedingAirport, const std::string& succeedingRunway) const;
	bool usesDependentSpacing(bool dependentMode,
		const std::string& precedingAirport, const std::string& precedingRunway,
		const std::string& succeedingAirport, const std::string& succeedingRunway) const;
	bool getWakeSeparation(char preceding, char succeeding, const char* precedingAircraftInfo,
		const char* succeedingAircraftInfo, double& separationNm) const;
	int getSpeedReductionDistanceNm() const;
	int getSpeedUntilReductionKts() const;
	bool getReduceSpeedOnClearance() const;
	double getLongApproachNm() const;
	double getLongApproachSpeedKts() const;
	int getFinalApproachSpeedKts(char wakeCategory) const;
	bool getLocalizerPointDistanceNm(const std::string& airportCode, const std::string& runwayName, double& distanceNm) const;
	bool getLocalizerCourseDegrees(const std::string& airportCode, const std::string& runwayName, double& courseDegrees) const;
	RgbColor getMarkerColor(const std::string& key) const;
	static RgbColor getDefaultColor(const std::string& key);
	std::vector<std::string> getAirportCodes() const;

	static bool Available;
	static CCallsignLookup* Lookup;

private:
	std::map<std::string, int> configValues;
	std::map<std::string, RgbColor> markerColors;
	std::map<std::string, double> mode2Settings;
	int displayMode = 1;
	std::map<std::string, double> localizerPointDistancesNm;
	std::map<std::string, double> localizerCoursesDegrees;
	std::set<std::pair<std::string, std::string>> configuredRunways;
	std::map<std::string, RunwayPairMode> legacyRunwayPairModes;
	std::map<std::string, std::map<std::string, RunwayPairMode>> profileRunwayPairModes;
	std::vector<std::string> profileNames;
	std::string defaultProfile;
	std::string proposalTagText = "P";
	bool proposalsEnabled = true;
	std::vector<std::string> airportCodes;
	std::map<std::string, char> recatCategories;
	std::map<std::string, double> wakeSeparations{
		{ "WTC_M_L", 5.0 },
		{ "WTC_H_L", 6.0 },
		{ "WTC_H_M", 5.0 },
		{ "WTC_H_H", 4.0 },
		{ "WTC_J_L", 8.0 },
		{ "WTC_J_M", 7.0 },
		{ "WTC_J_H", 6.0 }
	};
	std::map<std::string, double> recatEuWakeSeparations{
		{ "RECAT_A_A", 3.0 }, { "RECAT_A_B", 4.0 }, { "RECAT_A_C", 5.0 },
		{ "RECAT_A_D", 5.0 }, { "RECAT_A_E", 6.0 }, { "RECAT_A_F", 8.0 },
		{ "RECAT_B_B", 3.0 }, { "RECAT_B_C", 4.0 }, { "RECAT_B_D", 4.0 },
		{ "RECAT_B_E", 5.0 }, { "RECAT_B_F", 7.0 },
		{ "RECAT_C_B", 2.5 }, { "RECAT_C_C", 3.0 }, { "RECAT_C_D", 3.0 }, { "RECAT_C_E", 4.0 },
		{ "RECAT_C_F", 6.0 }, { "RECAT_D_F", 5.0 }, { "RECAT_E_F", 4.0 },
		{ "RECAT_F_F", 3.0 }
	};
	double minimumDistanceNm = 3.0;
	double maximumAutoDistanceNm = 0.0;
	double minimumParallelDependentDistanceNm = 3.0;
	double finalBufferNm = 1.0;
	double longApproachNm = 0.0;
	double longApproachSpeedKts = 250.0;
	bool showLocalizerRoute = false;
	bool dependentMode = false;
	bool useRecatEu = false;
	bool reduceSpeedOnClearance = true;
};