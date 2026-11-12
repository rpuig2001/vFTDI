#pragma once

#include <map>
#include <string>
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
	int getMinimumAltitude() const;
	int getDefaultVisibleRows() const;
	int getMarkerSizePixels(bool finalTarget) const;
	double getMinimumDistanceNm() const;
	double getMinimumParallelDependentDistanceNm() const;
	double getFinalBufferNm() const;
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
	bool getWakeSeparation(char preceding, char succeeding, double& separationNm) const;
	int getSpeedReductionDistanceNm() const;
	int getSpeedUntilReductionKts() const;
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
	std::map<std::string, double> localizerPointDistancesNm;
	std::map<std::string, double> localizerCoursesDegrees;
	std::map<std::string, RunwayPairMode> legacyRunwayPairModes;
	std::map<std::string, std::map<std::string, RunwayPairMode>> profileRunwayPairModes;
	std::vector<std::string> profileNames;
	std::string defaultProfile;
	std::vector<std::string> airportCodes;
	std::map<std::string, double> wakeSeparations{
		{ "WTC_M_L", 5.0 },
		{ "WTC_H_L", 6.0 },
		{ "WTC_H_M", 5.0 },
		{ "WTC_H_H", 4.0 },
		{ "WTC_J_L", 8.0 },
		{ "WTC_J_M", 7.0 },
		{ "WTC_J_H", 6.0 }
	};
	double minimumDistanceNm = 3.0;
	double minimumParallelDependentDistanceNm = 3.0;
	double finalBufferNm = 1.0;
	double longApproachNm = 0.0;
	double longApproachSpeedKts = 250.0;
	bool showLocalizerRoute = false;
	bool dependentMode = false;
};